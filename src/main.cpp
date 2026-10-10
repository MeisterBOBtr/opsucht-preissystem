#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>
#include <pl/memory/Signature.hpp>
#include <pl/memory/Hook.hpp>
#include <dlfcn.h>

#include <bedrocktools/BedrockTools.hpp>
#include <bedrocktools/Api.hpp>
#include <bedrocktools/events/Events.hpp>
#include "frame_data.h"

#include <jni.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

// The mobile BedrockTools template used by this project does not ship the
// full LeviLamina mc/ header tree. These two small ABI-compatible view types
// are used only as storage passed to the already-resolved drawText function.
// RectangleArea is four floats; mce::Color is four floats.
struct OpsuchtRectangleArea {
    float x0;
    float x1;
    float y0;
    float y1;
};

struct OpsuchtColor {
    float r;
    float g;
    float b;
    float a;
};

namespace {

constexpr std::string_view kModuleId =
    "opvantis.overlay";

constexpr const char* kApiBaseUrl =
    "https://api.opsucht.net/auctions";
constexpr const char* kApiActiveUrl =
    "https://api.opsucht.net/auctions/active";
constexpr const char* kApiCategoriesUrl =
    "https://api.opsucht.net/auctions/categories";
constexpr const char* kApiStreamUrl =
    "https://api.opsucht.net/auctions/stream";

// The API is deliberately a fixed background data source. The HUD does not
// depend on the inventory screen to fetch prices. Active auctions are refreshed
// periodically and the last successful snapshot remains available if a later
// request fails. The stream endpoint is documented by OPSUCHT and kept as the
// future low-latency update path; polling is used here because it is safer for
// Android/JNI networking and does not hold a blocking SSE connection open.
constexpr int kApiRefreshSeconds = 30;
constexpr int kInventoryRefreshTicks = 40; // once every 2 seconds at 20 TPS to reduce main-thread work
constexpr int kPriceCacheTicks = 80; // keep resolved item prices for ~4 seconds
constexpr int kApiCategoriesRefreshSeconds = 300;

constexpr std::string_view kFontId =
    "opvantis.roboto";

/*
 * Nur noch zum Ermitteln der Item-Namensfunktion.
 *
 * WICHTIG:
 * Dieser Code installiert KEINEN Hook auf NormalTick.
 */
constexpr std::string_view kRawNameSig =
    "? ? ? A9 ? ? ? F9 ? ? ? A9 ? ? ? A9 FD 03 00 91 "
    "? ? ? F9 ? ? ? A9 ? ? ? F9 ? ? ? B4";

/*
 * Minecraft / BedrockTools Inventory-Struktur.
 *
 * Diese Werte entsprechen dem bisher verwendeten
 * Inventar-Lesecode.
 */
constexpr std::size_t kPlayerInventory =
    0x570;

constexpr std::size_t kPlayerInventoryContainer =
    0xB8;

constexpr std::size_t kFillingContainerItems =
    0x140;

constexpr std::size_t kItemStackCount =
    0x22;

constexpr std::size_t kItemStackValid =
    0x23;

constexpr std::size_t kItemStackSize =
    0x98;

struct PriceInfo {
    double sum = 0.0;
    int samples = 0;
    std::string displayName;
    bool displayKey = false;
};

struct InventoryEntry {
    std::string name;
    int amount = 0;
    double unitPrice = 0.0;
    double stackValue = 0.0;
    std::string debugDetails;
};

struct VectorLayout {
    std::uintptr_t begin = 0;
    std::uintptr_t end = 0;
    std::uintptr_t capacity = 0;
};

static std::string lower(
    std::string s
) {
    std::transform(
        s.begin(),
        s.end(),
        s.begin(),
        [](unsigned char c) {
            return static_cast<char>(
                std::tolower(c)
            );
        }
    );

    return s;
}

static std::string normalizeItemKeepPrefix(
    std::string s
) {
    s = lower(std::move(s));

    for (char& c : s) {
        if (c == '_' || c == ':' || c == '-' || c == '/') {
            c = ' ';
        }
    }

    std::string out;
    bool space = false;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!out.empty()) space = true;
            continue;
        }
        if (space) {
            out.push_back(' ');
            space = false;
        }
        out.push_back(c);
    }
    return out;
}

static std::string normalizeItem(
    std::string s
) {
    s = normalizeItemKeepPrefix(std::move(s));

    auto stripPrefix = [&](std::string_view prefix) {
        if (s.rfind(prefix, 0) == 0) {
            s.erase(0, prefix.size());
        }
    };
    stripPrefix("minecraft ");
    stripPrefix("item ");
    stripPrefix("geyser custom ");

    std::string out;
    bool space = false;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!out.empty()) space = true;
            continue;
        }
        if (space) {
            out.push_back(' ');
            space = false;
        }
        out.push_back(c);
    }
    return out;
}

static std::string displayItemName(
    const std::string& raw
) {
    std::string s = raw;

    if (
        s.rfind(
            "minecraft:",
            0
        ) == 0
    ) {
        s.erase(0, 10);
    }

    for (char& c : s) {
        if (c == '_') {
            c = ' ';
        }
    }

    bool upper = true;

    for (char& c : s) {

        if (
            upper &&
            std::isalpha(
                static_cast<unsigned char>(c)
            )
        ) {
            c = static_cast<char>(
                std::toupper(
                    static_cast<unsigned char>(c)
                )
            );

            upper = false;
        }
        else if (c == ' ') {
            upper = true;
        }
    }

    return s;
}

static std::string normalizeTechnicalItemKey(std::string s) {
    s = lower(std::move(s));

    // First normalize all common separators.  The Android client has been
    // observed to expose Geyser ids as `geyser_custom_...`, while the API may
    // expose the same id as `geyser custom ...`, `geyser-custom-...`, or without
    // the Geyser prefix at all.  Prefix removal therefore has to happen AFTER
    // separator normalization, not before it.
    for (char& c : s) {
        if (c == ':' || c == '/' || c == '-' || c == ' ') c = '_';
    }

    std::string compact;
    compact.reserve(s.size());
    bool lastUnderscore = false;
    for (char c : s) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc)) {
            compact.push_back(c);
            lastUnderscore = false;
        } else if (!lastUnderscore && !compact.empty()) {
            compact.push_back('_');
            lastUnderscore = true;
        }
    }
    while (!compact.empty() && compact.back() == '_') compact.pop_back();

    // Remove the presentation prefix in its normalized form.
    constexpr std::string_view geyserPrefix = "geyser_custom_";
    if (compact.rfind(geyserPrefix, 0) == 0) {
        compact.erase(0, geyserPrefix.size());
    }

    return compact;
}

static std::string shortDisplayNameFromTechnicalId(const std::string& raw) {
    std::string key = normalizeTechnicalItemKey(raw);
    if (key.empty()) return {};

    // Prefer the last meaningful identifier component. OPSUCHT custom IDs often
    // end in the human-readable item key, e.g. ..._golden_excalibur.
    const auto pos = key.rfind('_');
    if (pos != std::string::npos && pos + 1 < key.size()) {
        // Keep the final two components when they form a readable name.
        const auto prev = key.rfind('_', pos - 1);
        if (prev != std::string::npos && prev + 1 < key.size()) {
            key = key.substr(prev + 1);
        } else {
            key = key.substr(pos + 1);
        }
    }
    return displayItemName(key);
}

static std::string formatMoney(
    double value
) {
    std::ostringstream out;

    out << std::fixed
        << std::setprecision(0)
        << value;

    std::string s =
        out.str();

    int p =
        static_cast<int>(
            s.size()
        ) - 3;

    while (p > 0) {

        s.insert(
            static_cast<std::size_t>(p),
            "."
        );

        p -= 3;
    }

    return s + " $";
}

static std::string jsonString(
    const std::string& object,
    const std::vector<std::string>& keys
) {
    for (const auto& key : keys) {

        const std::string needle =
            "\"" + key + "\"";

        std::size_t p =
            object.find(needle);

        if (
            p ==
            std::string::npos
        ) {
            continue;
        }

        p =
            object.find(
                ':',
                p + needle.size()
            );

        if (
            p ==
            std::string::npos
        ) {
            continue;
        }

        ++p;

        while (
            p < object.size() &&
            std::isspace(
                static_cast<unsigned char>(
                    object[p]
                )
            )
        ) {
            ++p;
        }

        if (
            p >= object.size() ||
            object[p] != '"'
        ) {
            continue;
        }

        ++p;

        std::string result;
        bool escaped = false;

        for (
            ;
            p < object.size();
            ++p
        ) {

            const char c =
                object[p];

            if (escaped) {

                result.push_back(c);
                escaped = false;
            }
            else if (c == '\\') {

                escaped = true;
            }
            else if (c == '"') {

                return result;
            }
            else {

                result.push_back(c);
            }
        }
    }

    return {};
}

static std::string jsonObjectField(const std::string& object, const std::string& key);

static std::string jsonStringDeep(
    const std::string& object,
    const std::vector<std::string>& keys
) {
    // OPSUCHT's web client treats some API fields as either plain strings or
    // small objects containing displayName/name/value. Walk every occurrence
    // instead of stopping at the first object-valued occurrence.
    for (const auto& key : keys) {
        const std::string needle = "\"" + key + "\"";
        std::size_t search = 0;
        while ((search = object.find(needle, search)) != std::string::npos) {
            std::size_t p = object.find(':', search + needle.size());
            if (p == std::string::npos) break;
            ++p;
            while (p < object.size() && std::isspace(static_cast<unsigned char>(object[p]))) ++p;
            if (p >= object.size()) break;

            if (object[p] == '"') {
                ++p;
                std::string result;
                bool escaped = false;
                for (; p < object.size(); ++p) {
                    const char c = object[p];
                    if (escaped) { result.push_back(c); escaped = false; }
                    else if (c == '\\') escaped = true;
                    else if (c == '"') return result;
                    else result.push_back(c);
                }
            } else if (object[p] == '{') {
                const std::string nested = jsonObjectField(object, key);
                if (!nested.empty()) {
                    const std::string nestedValue = jsonString(nested, {"displayName", "display_name", "name", "title", "label", "value"});
                    if (!nestedValue.empty()) return nestedValue;
                }
            }
            search += needle.size();
        }
    }
    return {};
}

static double jsonNumber(
    const std::string& object,
    const std::vector<std::string>& keys
) {
    for (const auto& key : keys) {

        const std::string needle =
            "\"" + key + "\"";

        std::size_t p =
            object.find(needle);

        if (
            p ==
            std::string::npos
        ) {
            continue;
        }

        p =
            object.find(
                ':',
                p + needle.size()
            );

        if (
            p ==
            std::string::npos
        ) {
            continue;
        }

        ++p;

        while (
            p < object.size() &&
            std::isspace(
                static_cast<unsigned char>(
                    object[p]
                )
            )
        ) {
            ++p;
        }

        char* end = nullptr;

        const double value =
            std::strtod(
                object.c_str() + p,
                &end
            );

        if (
            end !=
                object.c_str() + p &&
            std::isfinite(value)
        ) {
            return value;
        }
    }

    return 0.0;
}

static std::string jsonObjectField(
    const std::string& object,
    const std::string& key
) {
    const std::string needle = "\"" + key + "\"";
    std::size_t p = object.find(needle);
    if (p == std::string::npos) return {};
    p = object.find(':', p + needle.size());
    if (p == std::string::npos) return {};
    ++p;
    while (p < object.size() && std::isspace(static_cast<unsigned char>(object[p]))) ++p;
    if (p >= object.size() || object[p] != '{') return {};

    const std::size_t start = p;
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (; p < object.size(); ++p) {
        const char c = object[p];
        if (inString) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') inString = false;
            continue;
        }
        if (c == '"') { inString = true; continue; }
        if (c == '{') ++depth;
        else if (c == '}') {
            --depth;
            if (depth == 0) return object.substr(start, p - start + 1);
        }
    }
    return {};
}

static std::vector<std::string> jsonAllStringValues(const std::string& object) {
    // Collect string values from the item object without assuming a fixed API
    // field name. This is deliberately limited to plausible item identifiers,
    // names and keys so the resolver can bridge future OPSUCHT API field names.
    std::vector<std::string> result;
    std::size_t i = 0;
    while (i < object.size()) {
        if (object[i] != '"') { ++i; continue; }
        ++i;
        std::string token;
        bool escaped = false;
        for (; i < object.size(); ++i) {
            const char c = object[i];
            if (escaped) {
                token.push_back(c);
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                ++i;
                break;
            } else {
                token.push_back(c);
            }
        }

        // A JSON string is a value only when the next non-space character is
        // not ':'. Keys are ignored. Keep useful-looking item strings.
        std::size_t j = i;
        while (j < object.size() && std::isspace(static_cast<unsigned char>(object[j]))) ++j;
        if (j < object.size() && object[j] == ':') continue;
        if (token.empty() || token.size() > 256) continue;
        bool useful = token.find(':') != std::string::npos ||
                      token.find('_') != std::string::npos ||
                      token.find(' ') != std::string::npos ||
                      token.find('-') != std::string::npos ||
                      token.find('/') != std::string::npos;
        if (!useful) continue;
        if (std::find(result.begin(), result.end(), token) == result.end()) {
            result.push_back(std::move(token));
        }
    }
    return result;
}

static std::vector<std::string>
jsonObjects(
    const std::string& json
) {
    std::vector<std::string> result;

    int depth = 0;

    bool inString = false;
    bool escaped = false;

    std::size_t start =
        std::string::npos;

    for (
        std::size_t i = 0;
        i < json.size();
        ++i
    ) {

        const char c =
            json[i];

        if (inString) {

            if (escaped) {
                escaped = false;
            }
            else if (c == '\\') {
                escaped = true;
            }
            else if (c == '"') {
                inString = false;
            }

            continue;
        }

        if (c == '"') {

            inString = true;
            continue;
        }

        if (c == '{') {

            if (depth == 0) {
                start = i;
            }

            ++depth;
        }
        else if (c == '}') {

            if (depth > 0) {
                --depth;
            }

            if (
                depth == 0 &&
                start !=
                    std::string::npos
            ) {

                result.push_back(
                    json.substr(
                        start,
                        i - start + 1
                    )
                );

                start =
                    std::string::npos;
            }
        }
    }

    return result;
}

static double jsonNumberDeep(
    const std::string& object,
    const std::vector<std::string>& keys
) {
    // First use the existing fast scalar parser. If the API wrapped a number
    // in an object, inspect its common value/amount/price/bid fields.
    const double direct = jsonNumber(object, keys);
    if (direct != 0.0) return direct;

    for (const auto& key : keys) {
        const std::string nested = jsonObjectField(object, key);
        if (nested.empty()) continue;
        const double value = jsonNumber(nested, {"value", "amount", "price", "bid", "currentBid"});
        if (value != 0.0) return value;
    }
    return 0.0;
}

class OpsuchtApi {

public:

    // OPVANTIS UI/performance test: no networking is started here.
    // The mod must not poll the OPSUCHT API. Supabase read-only loading will
    // be added as a separate step after the UI/rendering behavior is stable.
    void start(ll::mod::NativeMod& self) {
        stop();
        mSelf = &self;
        mStop = true;
        if (mSelf) {
            mSelf->getLogger().info(
                "OPVANTIS UI-Test: direkte Opsucht-API-Abfragen deaktiviert"
            );
        }
    }

    void stop() {
        mStop = true;
        // No background worker is started in this build.
        if (mThread.joinable()) {
            mThread.join();
        }
    }

    std::size_t priceCount() const {

        std::scoped_lock lock(
            mMutex
        );

        return mPrices.size();
    }

    std::size_t apiAuctionCount() const { return mApiAuctionCount.load(); }
    std::size_t apiCategoryCount() const { return mApiCategoryCount.load(); }
    std::size_t apiResponseBytes() const { return mApiResponseBytes.load(); }
    std::uint64_t priceGeneration() const { return mPriceGeneration.load(); }

    PriceInfo findMatch(const std::string& rawName) const {
        const std::string fullKey = normalizeItemKeepPrefix(rawName);
        std::string key = normalizeItem(rawName);

        std::scoped_lock lock(mMutex);

        // 1) Exact match against the normal API name.
        if (auto it = mPrices.find(key); it != mPrices.end()) return it->second;

        // 2) Exact match against API aliases (identifier/material).
        if (auto it = mAliases.find(key); it != mAliases.end()) return it->second;

        // 3) Vanilla material fallback.
        if (auto it = mMaterialPrices.find(key); it != mMaterialPrices.end()) return it->second;

        // 4) Geyser custom items: use the full technical identifier only.
        // Never fuzzy-match a custom identifier to a vanilla material. A custom
        // Netherite Pickaxe must never inherit the normal Netherite Pickaxe price.
        const std::string geyserPrefix = "geyser custom ";
        if (fullKey.rfind(geyserPrefix, 0) == 0) {
            const std::string technicalKey = normalizeTechnicalItemKey(rawName);
            if (auto it = mAliases.find(technicalKey); it != mAliases.end()) return it->second;

            // Also try the API-side form with/without the Geyser presentation
            // prefix.  The raw client id can be `geyser_custom_main_...`, while
            // OPSUCHT commonly identifies the same custom item as just
            // `main_...`.
            const std::string fullTechnicalKey = normalizeItemKeepPrefix(rawName);
            const std::string prefixlessTechnicalKey =
                fullTechnicalKey.rfind(geyserPrefix, 0) == 0
                    ? fullTechnicalKey.substr(geyserPrefix.size())
                    : fullTechnicalKey;
            if (auto it = mAliases.find(normalizeItem(prefixlessTechnicalKey)); it != mAliases.end()) return it->second;
            if (auto it = mAliases.find(fullKey); it != mAliases.end()) return it->second;
            if (auto it = mAliases.find(fullKey.substr(geyserPrefix.size())); it != mAliases.end()) return it->second;

            // OPSUCHT may expose the custom item by its human-readable catalog
            // name instead of the full Geyser technical identifier. Derive only
            // the readable suffix (for example
            // geyser_custom_main_misc_may26_golden_excalibur -> Golden Excalibur)
            // and use EXACT matching. Never fall back to the vanilla material.
            const std::string shortName = shortDisplayNameFromTechnicalId(rawName);
            const std::string shortKey = normalizeItem(shortName);
            if (!shortKey.empty()) {
                if (auto it = mPrices.find(shortKey); it != mPrices.end()) return it->second;
                if (auto it = mAliases.find(shortKey); it != mAliases.end()) return it->second;
            }

            return {};
        }

        // 5) Last-resort fuzzy matching is allowed only for ordinary vanilla
        // display names. It is never reached for a Geyser custom identifier.
        // "Item.netherite Pickaxe" when OPSUCHT exposes the same item under a
        // slightly different display/material spelling. This is deliberately
        // conservative and only accepts one clear winner.
        auto makeTokens = [](const std::string& value) {
            std::vector<std::string> tokens;
            std::istringstream in(value);
            std::string token;
            while (in >> token) {
                if (token.size() >= 3 && token != "item" && token != "minecraft" && token != "geyser" && token != "custom") {
                    tokens.push_back(token);
                }
            }
            return tokens;
        };

        const auto rawTokens = makeTokens(key);
        const PriceInfo* best = nullptr;
        double bestScore = 0.0;
        double secondScore = 0.0;

        auto consider = [&](const std::string& candidateKey, const PriceInfo& info) {
            if (info.samples <= 0 || candidateKey.empty()) return;
            const auto candidateTokens = makeTokens(candidateKey);
            if (rawTokens.empty() || candidateTokens.empty()) return;

            int hits = 0;
            for (const auto& a : rawTokens) {
                for (const auto& b : candidateTokens) {
                    if (a == b || a.find(b) != std::string::npos || b.find(a) != std::string::npos) {
                        ++hits;
                        break;
                    }
                }
            }
            const double score = static_cast<double>(hits) / static_cast<double>(std::max(rawTokens.size(), candidateTokens.size()));
            if (score > bestScore) {
                secondScore = bestScore;
                bestScore = score;
                best = &info;
            } else if (score > secondScore) {
                secondScore = score;
            }
        };

        for (const auto& [candidateKey, info] : mPrices) consider(candidateKey, info);
        for (const auto& [candidateKey, info] : mMaterialPrices) consider(candidateKey, info);
        for (const auto& [candidateKey, info] : mAliases) consider(candidateKey, info);

        if (best && bestScore >= 0.50 && (bestScore - secondScore >= 0.10 || bestScore >= 0.80)) {
            return *best;
        }

        return {};
    }

    double getPrice(const std::string& name) const {
        const auto match = findMatch(name);
        return match.samples > 0 ? match.sum / match.samples : 0.0;
    }

    std::string getDisplayName(const std::string& name) const {
        return findMatch(name).displayName;
    }

private:

    std::string httpGet(const char* endpoint) {

        if (!mSelf) {
            return {};
        }

        JavaVM* vm =
            mSelf->getJavaVM();

        if (!vm) {
            return {};
        }

        JNIEnv* env = nullptr;
        bool attached = false;

        if (
            vm->GetEnv(
                reinterpret_cast<void**>(
                    &env
                ),
                JNI_VERSION_1_6
            ) != JNI_OK
        ) {

            if (
                vm->AttachCurrentThread(
                    &env,
                    nullptr
                ) != JNI_OK
            ) {
                return {};
            }

            attached = true;
        }

        auto detach = [&] {

            if (attached) {
                vm->DetachCurrentThread();
            }
        };

        jclass urlClass =
            env->FindClass(
                "java/net/URL"
            );

        if (!urlClass) {
            detach();
            return {};
        }

        jmethodID ctor =
            env->GetMethodID(
                urlClass,
                "<init>",
                "(Ljava/lang/String;)V"
            );

        jmethodID open =
            env->GetMethodID(
                urlClass,
                "openConnection",
                "()Ljava/net/URLConnection;"
            );

        if (!ctor || !open) {
            detach();
            return {};
        }

        jstring urlString =
            env->NewStringUTF(
                endpoint
            );

        jobject url =
            env->NewObject(
                urlClass,
                ctor,
                urlString
            );

        env->DeleteLocalRef(
            urlString
        );

        if (
            env->ExceptionCheck() ||
            !url
        ) {

            env->ExceptionClear();
            detach();

            return {};
        }

        jobject connection =
            env->CallObjectMethod(
                url,
                open
            );

        if (
            env->ExceptionCheck() ||
            !connection
        ) {

            env->ExceptionClear();
            detach();

            return {};
        }

        jclass connectionClass =
            env->GetObjectClass(
                connection
            );

        jmethodID connectTimeout =
            env->GetMethodID(
                connectionClass,
                "setConnectTimeout",
                "(I)V"
            );

        jmethodID readTimeout =
            env->GetMethodID(
                connectionClass,
                "setReadTimeout",
                "(I)V"
            );

        jmethodID input =
            env->GetMethodID(
                connectionClass,
                "getInputStream",
                "()Ljava/io/InputStream;"
            );

        if (connectTimeout) {

            env->CallVoidMethod(
                connection,
                connectTimeout,
                5000
            );
        }

        if (readTimeout) {

            env->CallVoidMethod(
                connection,
                readTimeout,
                5000
            );
        }

        if (!input) {
            detach();
            return {};
        }

        jobject stream =
            env->CallObjectMethod(
                connection,
                input
            );

        if (
            env->ExceptionCheck() ||
            !stream
        ) {

            env->ExceptionClear();
            detach();

            return {};
        }

        jclass streamClass =
            env->GetObjectClass(
                stream
            );

        jmethodID read =
            env->GetMethodID(
                streamClass,
                "read",
                "([B)I"
            );

        jmethodID close =
            env->GetMethodID(
                streamClass,
                "close",
                "()V"
            );

        if (!read) {
            detach();
            return {};
        }

        jbyteArray buffer =
            env->NewByteArray(
                8192
            );

        std::string result;

        for (;;) {

            const jint n =
                env->CallIntMethod(
                    stream,
                    read,
                    buffer
                );

            if (
                env->ExceptionCheck()
            ) {

                env->ExceptionClear();
                break;
            }

            if (n <= 0) {
                break;
            }

            jbyte* bytes =
                env->GetByteArrayElements(
                    buffer,
                    nullptr
                );

            if (!bytes) {
                break;
            }

            result.append(
                reinterpret_cast<char*>(
                    bytes
                ),
                static_cast<std::size_t>(
                    n
                )
            );

            env->ReleaseByteArrayElements(
                buffer,
                bytes,
                JNI_ABORT
            );
        }

        if (close) {

            env->CallVoidMethod(
                stream,
                close
            );
        }

        detach();

        return result;
    }

    void worker() {

        auto lastCategoryRefresh = std::chrono::steady_clock::now() - std::chrono::seconds(kApiCategoriesRefreshSeconds);

        while (!mStop.load()) {

            // 1) Active auctions are the permanent price source.
            const std::string json =
                httpGet(kApiActiveUrl);

            mApiResponseBytes.store(json.size());
            if (json.empty()) {
                mApiAuctionCount.store(0);
            } else {
                std::size_t activeCount = 0;
                for (const auto& obj : jsonObjects(json)) {
                    // An auction object contains the nested "item" object.
                    // Nested item objects themselves do not, so this avoids
                    // counting the same auction multiple times.
                    if (obj.find("\"item\"") != std::string::npos) {
                        ++activeCount;
                    }
                }
                mApiAuctionCount.store(activeCount);
            }

            if (!json.empty()) {

                std::unordered_map<std::string, PriceInfo> sums;
                std::unordered_map<std::string, PriceInfo> aliasSums;
                std::unordered_map<std::string, std::string> materialOwners;
                std::unordered_map<std::string, PriceInfo> materialSums;
                std::unordered_map<std::string, bool> materialAmbiguous;

                // /auctions/active returns auction objects whose item data is nested
                // under "item".  The API price is on the auction itself.
                for (const auto& object : jsonObjects(json)) {
                    // The live API keeps the actual item data inside `item`.
                    // Read that object first so custom OPSUCHT/Geyser items keep
                    // their real display name and identifier.
                    const std::string itemObject = jsonObjectField(object, "item");
                    const std::string& itemData = itemObject.empty() ? object : itemObject;

                    const std::string displayName = jsonStringDeep(itemData, {
                        "displayName", "display_name", "itemDisplayName", "item_display_name",
                        "itemName", "item_name", "name"
                    });
                    const std::string material = jsonStringDeep(itemData, {
                        "material", "type", "typeName", "type_name"
                    });
                    const std::string itemIdentifier = jsonStringDeep(itemData, {
                        "itemId", "item_id", "identifier", "itemIdentifier", "item_identifier", "key", "id"
                    });
                    if (displayName.empty() && material.empty() && itemIdentifier.empty()) continue;

                    // Prefer the live bid. If nobody has bid yet, use the start bid;
                    // if that is also zero, use the instant-buy price.
                    double price = jsonNumberDeep(object, {"currentBid", "current_bid", "bid", "currentPrice", "current_price"});
                    if (price <= 0.0) price = jsonNumberDeep(object, {"startBid", "start_bid", "startPrice", "start_price"});
                    if (price <= 0.0) price = jsonNumberDeep(object, {"instantBuyPrice", "instant_buy_price", "buyNowPrice", "buy_now_price", "price"});
                    if (price <= 0.0) continue;

                    double quantity = jsonNumberDeep(itemData, {
                        "amount", "quantity", "itemAmount", "item_amount", "count"
                    });
                    if (quantity <= 0.0) {
                        quantity = jsonNumberDeep(object, {
                            "amount", "quantity", "itemAmount", "item_amount", "count"
                        });
                    }
                    const double unitPrice = quantity > 0.0 ? price / quantity : price;
                    if (unitPrice <= 0.0) continue;

                    const std::string shown = !displayName.empty() ? displayName : material;

                    if (!material.empty()) {
                        const std::string materialKey = normalizeItem(material);
                        if (!displayName.empty()) {
                            auto owner = materialOwners.find(materialKey);
                            if (owner == materialOwners.end()) {
                                materialOwners[materialKey] = normalizeItem(displayName);
                            } else if (owner->second != normalizeItem(displayName)) {
                                materialAmbiguous[materialKey] = true;
                            }
                        }
                        auto& materialEntry = materialSums[materialKey];
                        materialEntry.sum += unitPrice;
                        ++materialEntry.samples;
                        if (materialEntry.displayName.empty()) materialEntry.displayName = shown;
                    }

                    // Custom items are keyed by their actual OPSUCHT display name.
                    // Do not mix different custom items that share one vanilla material.
                    if (!displayName.empty()) {
                        auto& entry = sums[normalizeItem(displayName)];
                        entry.sum += unitPrice;
                        ++entry.samples;
                        entry.displayName = displayName;
                        entry.displayKey = true;
                    } else if (!material.empty()) {
                        // Vanilla/unnamed auction: material is a safe key.
                        auto& entry = sums[normalizeItem(material)];
                        entry.sum += unitPrice;
                        ++entry.samples;
                        if (entry.displayName.empty()) entry.displayName = shown;
                        entry.displayKey = false;
                    }

                    // Keep the API identifiers as aliases. This is the important
                    // bridge for Bedrock/Geyser custom item names: the inventory can
                    // expose an identifier while the API exposes the readable name.
                    const std::string apiShownName = !displayName.empty() ? displayName : shown;
                    // Store every useful API-side identifier as an alias. This lets
                    // the inventory bridge match either the technical item id or the
                    // readable OPSUCHT name without relying on the UI text.
                    auto addApiAlias = [&](const std::string& alias) {
                        if (alias.empty()) return;
                        auto addAlias = [&](const std::string& aliasKey) {
                            if (aliasKey.empty()) return;
                            auto& aliasEntry = aliasSums[aliasKey];
                            aliasEntry.sum += unitPrice;
                            ++aliasEntry.samples;
                            if (aliasEntry.displayName.empty()) aliasEntry.displayName = apiShownName;
                            aliasEntry.displayKey = !displayName.empty();
                        };
                        addAlias(normalizeItem(alias));
                        addAlias(normalizeItemKeepPrefix(alias));
                        const std::string technical = normalizeTechnicalItemKey(alias);
                        if (!technical.empty()) addAlias(technical);
                    };

                    for (const auto& alias : {itemIdentifier, material, displayName}) {
                        addApiAlias(alias);
                    }

                    // V2 identifier bridge: OPSUCHT may add a technical identifier
                    // under a field name that is not stable/documented. Index every
                    // useful string value inside the item object as an alias. The
                    // display name remains the canonical result; these extra aliases
                    // are only lookup keys and are never shown as prices themselves.
                    for (const auto& extra : jsonAllStringValues(itemData)) {
                        addApiAlias(extra);
                    }
                }

                std::unordered_map<std::string, PriceInfo> next;
                for (auto& [name, info] : sums) {
                    if (info.samples > 0) next.emplace(name, std::move(info));
                }

                std::unordered_map<std::string, PriceInfo> nextMaterial;
                for (auto& [materialKey, info] : materialSums) {
                    if (info.samples > 0) {
                        // Keep material prices even when several custom items share
                        // the same vanilla material. This is a fallback for Bedrock
                        // items whose custom identifier is not exposed by ItemStack.
                        if (materialAmbiguous[materialKey]) {
                            info.displayName.clear();
                        }
                        nextMaterial.emplace(materialKey, std::move(info));
                    }
                }

                std::unordered_map<std::string, PriceInfo> nextAliases;
                for (auto& [aliasKey, info] : aliasSums) {
                    if (info.samples > 0 && !aliasKey.empty()) {
                        nextAliases.emplace(aliasKey, std::move(info));
                    }
                }

                {
                    std::scoped_lock lock(
                        mMutex
                    );

                    mPrices = std::move(next);
                    mMaterialPrices = std::move(nextMaterial);
                    mAliases = std::move(nextAliases);
                    mPriceGeneration.fetch_add(1);
                }

                if (mSelf) {

                    mSelf->getLogger().info(
                        "OPSUCHT API: {} Preise | {} aktive Auktionen",
                        priceCount(),
                        apiAuctionCount()
                    );
                }
            }

            // 2) Categories are a secondary catalog source. They are not used
            // for pricing, but keeping them in the background gives the resolver
            // another official OPSUCHT vocabulary instead of relying on Geyser text.
            const auto now = std::chrono::steady_clock::now();
            if (now - lastCategoryRefresh >= std::chrono::seconds(kApiCategoriesRefreshSeconds)) {
                const std::string categories = httpGet(kApiCategoriesUrl);
                if (!categories.empty()) {
                    std::size_t count = 0;
                    // Count category-like named entries without assuming one
                    // particular JSON shape.
                    for (const auto& obj : jsonObjects(categories)) {
                        if (obj.find("\"name\"") != std::string::npos ||
                            obj.find("\"id\"") != std::string::npos ||
                            obj.find("\"key\"") != std::string::npos) {
                            ++count;
                        }
                    }
                    mApiCategoryCount.store(count);
                }
                lastCategoryRefresh = now;
            }

            // 3) Keep the last successful API snapshot in memory. A temporary
            // network failure therefore does not erase prices from the HUD.
            for (
                int i = 0;
                i < kApiRefreshSeconds &&
                !mStop.load();
                ++i
            ) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

    ll::mod::NativeMod* mSelf =
        nullptr;

    std::atomic_bool mStop{
        false
    };

    std::thread mThread;

    mutable std::mutex mMutex;

    std::unordered_map<
        std::string,
        PriceInfo
    > mPrices;

    std::unordered_map<
        std::string,
        PriceInfo
    > mMaterialPrices;

    std::unordered_map<
        std::string,
        PriceInfo
    > mAliases;

    std::atomic<std::size_t> mApiAuctionCount{0};
    std::atomic<std::size_t> mApiCategoryCount{0};
    std::atomic<std::size_t> mApiResponseBytes{0};
    std::atomic<std::uint64_t> mPriceGeneration{0};
};

class OpsuchtInventarwertMod {

public:

    static OpsuchtInventarwertMod&
    instance() {

        static OpsuchtInventarwertMod mod;

        return mod;
    }

    OpsuchtInventarwertMod()
        : mSelf(
            *ll::mod::NativeMod::current()
        ) {

        sInstance = this;
    }

    ~OpsuchtInventarwertMod() {

        sInstance = nullptr;
    }

    bool load() {

        mSelf.getLogger().info(
            "OPSUCHT Inventarwert geladen"
        );

        return true;
    }

    bool enable() {

        mEnabled = true;
        mStop = false;

        registerFont();
        registerFrameImage();

        const bool registered =
            pl::modmenu::ModuleBuilder(
                std::string(kModuleId),
                "OPSUCHT Inventarwert"
            )
            .modId(
                mSelf.getId()
            )
            .description(
                "Zeigt den aktuellen Wert des geöffneten Inventars anhand der OPSUCHT-Auktionspreise."
            )
            .defaultEnabled(true)
            .onToggle(
                [this](
                    std::string_view moduleId,
                    bool enabled
                ) {

                    if (
                        moduleId !=
                        kModuleId
                    ) {
                        return;
                    }

                    mEnabled = enabled;

                    if (enabled) {
                                        // Das Panel erscheint erst, wenn Bedrock den Container
                        // tatsächlich als geöffnet meldet.
                    } else {
                        clearOverlay();
                    }
                }
            )
            .registerModule();

        mSelf.getLogger().info(
            "OPSUCHT ModMenu Registrierung: {}",
            registered
                ? "OK"
                : "FEHLER"
        );

        if (!registered) {
            return false;
        }

        // Das Panel bleibt sichtbar, solange die Mod aktiviert ist.
        mApi.start(
            mSelf
        );

        /*
         * Kein eigener NormalTick-Hook mehr.
         *
         * BedrockTools liefert den Tick über
         * LocalPlayerTickEvent.
         */
        mSetupThread =
            std::thread(
                [this] {
                    setupRuntime();
                }
            );

        // Der Tick bleibt als direkter Hook, weil er in der bisherigen Version
        // zuverlässig die 36 Inventarslots geliefert hat. Für den Screen-Zustand
        // verwenden wir dagegen den offiziellen BedrockTools ScreenStateEvent.

        return true;
    }

    bool disable() {

        mEnabled = false;
        mStop = true;

        clearOverlay();

        if (
            mSetupThread.joinable()
        ) {
            mSetupThread.join();
        }

        if (mLocalPlayerSubscription != 0 || mScreenSubscription != 0) {
            if (const auto* api = bedrocktools::api::find()) {
                if (mLocalPlayerSubscription != 0) api->unsubscribe(mLocalPlayerSubscription);
                if (mScreenSubscription != 0) api->unsubscribe(mScreenSubscription);
            }
            mLocalPlayerSubscription = 0;
            mScreenSubscription = 0;
        }
        mInventoryOpen = false;

        mApi.stop();

        pl::modmenu::unregisterModule(
            kModuleId
        );

        return true;
    }

    bool unload() {
        return true;
    }

private:

    using RawNameFn = std::string(*)(const void*);
    using ItemStringFn = std::string(*)(const void*);
    using ItemDescriptionIdFn = const std::string&(*)(const void*);
    using TickFn = void(*)(void*);
    using ScreenViewRenderFn = void(*)(void*, void*);
    using ScreenFn = void*(*)(void*, void*, void*, void*, void*, void*, void*, void*);
    using DrawTextFn = void(*)(void*, void*, void*, std::string*, void*, float, int, void*, void*);


    static OpsuchtInventarwertMod* sInstance;
    static RawNameFn sRawName;
    static ItemStringFn sGetName;
    static ItemStringFn sGetRawNameId;
    static ItemStringFn sGetTypeName;
    static ItemStringFn sGetDescriptionName;
    static ItemStringFn sGetCustomName;
    static ItemDescriptionIdFn sItemDescriptionId;
    static TickFn sOriginalTick;
    static ScreenViewRenderFn sOriginalScreenViewRender;
    static ScreenFn sOriginalContainerOpen;
    static ScreenFn sOriginalContainerDtor;
    static DrawTextFn sOriginalDrawText;
    static void* sLastUiContext;
    static void* sLastFont;
    static void* sLastTextMeasure;
    static void* sLastCaretMeasure;

    static void directTickHook(void* actor) {
        auto* self = sInstance;
        if (self) {
            self->mUiPanelDrawnThisFrame.store(false);

            // ScreenStateEvent is not equally reliable on every BedrockTools/
            // Android build. Keep a small UI-text based fallback: the normal
            // inventory screen always draws labels such as "All recipes" or
            // "Crafting/Handwerk". If those labels stop appearing for a few
            // ticks, consider the inventory closed. This prevents both a
            // missing panel and a permanent HUD/ghost panel.
            const bool inventoryTextWasSeen =
                self->mInventoryTextSeenThisFrame.exchange(false);
            if (inventoryTextWasSeen) {
                self->mInventoryTextMissingFrames.store(0);
                self->mInventoryOpen.store(true);
            } else if (self->mInventoryOpen.load()) {
                // Do not hide the panel just because inventory labels were not
                // rendered during a few frames. Bedrock can switch UI passes
                // while the inventory is opening; the old timeout caused the
                // panel state to flicker or reset. Screen/container hooks remain
                // responsible for the actual open/close state.
                self->mInventoryTextMissingFrames.fetch_add(1);
            }

            if (self->mLocalPlayerSubscription == 0) {
                self->onTick(actor);
            }

            // Keep the overlay alive from the game tick as well as from the
            // text-render hook. The inventory screen can replace the normal
            // HUD text pass; relying only on drawText therefore made the panel
            // disappear exactly when the inventory was opened.
            if (self->mEnabled.load() &&
                sLastUiContext &&
                sLastTextMeasure &&
                sLastCaretMeasure &&
                !self->mUiPanelDrawnThisFrame.exchange(true)) {
                self->drawDirectInventoryOverlayWithArgs(
                    sLastUiContext,
                    sLastFont,
                    sLastTextMeasure,
                    sLastCaretMeasure
                );
            }
        }
        if (sOriginalTick) sOriginalTick(actor);
    }


    static void* directContainerOpenHook(void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7) {
        auto* self = sInstance;
        if (self) {
            self->mInventoryOpen.store(true);
            self->mUiPanelDrawnThisFrame.store(false);
        }
        return sOriginalContainerOpen ? sOriginalContainerOpen(a0, a1, a2, a3, a4, a5, a6, a7) : nullptr;
    }

    static void* directContainerDtorHook(void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7) {
        auto* self = sInstance;
        if (self) {
            self->mInventoryOpen.store(false);
            self->mUiPanelDrawnThisFrame.store(false);
        }
        return sOriginalContainerDtor ? sOriginalContainerDtor(a0, a1, a2, a3, a4, a5, a6, a7) : nullptr;
    }

    static void directDrawTextHook(void* ctx, void* font, void* rect, std::string* text, void* color, float alpha, int alignment, void* textData, void* caretData) {
        auto* self = sInstance;
        sLastUiContext = ctx;
        sLastFont = font;
        sLastTextMeasure = textData;
        sLastCaretMeasure = caretData;

        if (sOriginalDrawText) {
            sOriginalDrawText(ctx, font, rect, text, color, alpha, alignment, textData, caretData);
        }

        // Fallback inventory-screen detection. This is deliberately based on
        // Minecraft's own inventory labels, not on our panel text, so it cannot
        // keep the panel alive by itself.
        if (self && text && !text->empty()) {
            std::string uiText = *text;
            std::transform(uiText.begin(), uiText.end(), uiText.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            const bool inventoryLabel =
                uiText.find("all recipes") != std::string::npos ||
                uiText.find("alle rezepte") != std::string::npos ||
                uiText.find("crafting") != std::string::npos ||
                uiText.find("handwerk") != std::string::npos ||
                uiText.find("inventory") != std::string::npos ||
                uiText.find("inventar") != std::string::npos;

            if (inventoryLabel) {
                self->mInventoryTextSeenThisFrame.store(true);
                self->mInventoryTextMissingFrames.store(0);
                self->mInventoryOpen.store(true);
            }
        }

        // Draw exactly once per Minecraft tick. The previous version drew on
        // every drawText call, which caused the panel to duplicate and flicker.
        // Drawing after the inventory screen's own text is seen gives the panel
        // a late, stable place in the same safe drawText pass.
        // Das Panel ist absichtlich NICHT an den Inventory-/Screen-State
        // gebunden. Es bleibt sichtbar, solange die Mod aktiviert ist.
        // Dadurch kann die Inventarauslese im Hintergrund ständig aktualisiert
        // werden und das Panel verschwindet nicht beim Öffnen/Schließen des
        // Minecraft-Inventars.
        if (self && self->mEnabled.load() &&
            !self->mUiPanelDrawnThisFrame.exchange(true)) {
            self->drawDirectInventoryOverlayWithArgs(ctx, font, textData, caretData);
        }
    }

    /*
     * Sucht nur die Item-Namensfunktion.
     *
     * Es wird hier KEIN Minecraft-Tick gehookt.
     */
    void subscribeScreenStateEvent() {

        if (mScreenSubscription != 0) {
            return;
        }

        // The BedrockTools runtime may not be ready when enable() is called.
        // Retry from the existing runtime setup thread until the API is available.
        if (const auto* api = bedrocktools::api::find()) {
            if (mLocalPlayerSubscription == 0) {
                mLocalPlayerSubscription = api->subscribe(
                    bedrocktools::events::LocalPlayerTickEvent::type,
                    bedrocktools::events::EventPriority::Normal,
                    [](bedrocktools::events::EventType type, void* payload, void* userData) {
                        if (type != bedrocktools::events::LocalPlayerTickEvent::type || !payload || !userData) return;
                        auto* self = static_cast<OpsuchtInventarwertMod*>(userData);
                        const auto& event = *static_cast<bedrocktools::events::LocalPlayerTickEvent*>(payload);
                        if (event.player) self->onTick(event.player);
                    },
                    this
                );
                mSelf.getLogger().info(
                    "OPSUCHT LocalPlayerTickEvent subscription: {}",
                    mLocalPlayerSubscription != 0
                );
            }
            mScreenSubscription = api->subscribe(
                bedrocktools::events::ScreenStateEvent::type,
                bedrocktools::events::EventPriority::Normal,
                [](bedrocktools::events::EventType type, void* payload, void* userData) {
                    if (type != bedrocktools::events::ScreenStateEvent::type || !payload || !userData) {
                        return;
                    }

                    auto* self = static_cast<OpsuchtInventarwertMod*>(userData);
                    const auto& event =
                        *static_cast<bedrocktools::events::ScreenStateEvent*>(payload);

                    if (event.screen != bedrocktools::events::ScreenKind::Container) {
                        return;
                    }

                    const bool open =
                        event.phase == bedrocktools::events::ScreenPhase::Opened;

                    self->mInventoryOpen.store(open);
                    // Der Screen-State beeinflusst die Anzeige nicht mehr. Die Inventardaten
                    // werden unabhängig davon dauerhaft aktualisiert.
                },
                this
            );

            mSelf.getLogger().info(
                "OPSUCHT ScreenStateEvent subscription: {}",
                mScreenSubscription != 0
            );
        }
    }

    void setupRuntime() {

        while (!mStop.load()) {

            subscribeScreenStateEvent();

            const auto rawName =
                pl::memory::resolveSignature(
                    kRawNameSig,
                    "libminecraftpe.so"
                );

            if (rawName) {
                sRawName = reinterpret_cast<RawNameFn>(rawName);
                mRawNameResolved = true;
            }

            // ItemStackBase exposes several non-virtual name accessors.
            // Resolve them directly when the Android Bedrock binary exports them.
            // Different builds may expose ItemStackBase or ItemStack symbols, so
            // both mangled names are tried.
            auto resolveItemString = [](const char* baseSymbol, const char* stackSymbol) -> ItemStringFn {
                void* p = dlsym(RTLD_DEFAULT, baseSymbol);
                if (!p && stackSymbol) p = dlsym(RTLD_DEFAULT, stackSymbol);
                return reinterpret_cast<ItemStringFn>(p);
            };
            if (!sGetName) sGetName = resolveItemString(
                "_ZNK13ItemStackBase7getNameEv", "_ZNK9ItemStack7getNameEv");
            if (!sGetRawNameId) sGetRawNameId = resolveItemString(
                "_ZNK13ItemStackBase12getRawNameIdEv", "_ZNK9ItemStack12getRawNameIdEv");
            if (!sGetTypeName) sGetTypeName = resolveItemString(
                "_ZNK13ItemStackBase11getTypeNameEv", "_ZNK9ItemStack11getTypeNameEv");
            if (!sGetDescriptionName) sGetDescriptionName = resolveItemString(
                "_ZNK13ItemStackBase18getDescriptionNameEv", "_ZNK9ItemStack18getDescriptionNameEv");
            if (!sGetCustomName) sGetCustomName = resolveItemString(
                "_ZNK13ItemStackBase13getCustomNameEv", "_ZNK9ItemStack13getCustomNameEv");

            mSelf.getLogger().info(
                "OPSUCHT ItemStack bridge: name={} raw={} type={} desc={} custom={}",
                sGetName != nullptr, sGetRawNameId != nullptr, sGetTypeName != nullptr,
                sGetDescriptionName != nullptr, sGetCustomName != nullptr);

            // Official BedrockTools inventory path: ItemStack +0x8 -> counter -> Item,
            // then Item vtable slot 5 is Item::getDescriptionId() returning const std::string&.
            // We keep this as a fallback-independent name source for current Bedrock.

            const auto containerOpen = pl::memory::resolveSignature(
                "? ? ? A9 ? ? ? F9 FD 03 00 91 F3 03 00 AA ? ? ? 94 ? ? ? F9 E1 03 1F 2A ? ? ? 94",
                "libminecraftpe.so"
            );
            if (containerOpen && !mContainerOpenHook.installed()) {
                mContainerOpenHook = pl::memory::HookHandle(
                    reinterpret_cast<void*>(containerOpen),
                    reinterpret_cast<void*>(&directContainerOpenHook),
                    reinterpret_cast<void**>(&sOriginalContainerOpen)
                );
            }

            const auto containerDtor = pl::memory::resolveSignature(
                "? ? ? D1 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? 91 56 D0 3B D5 F3 03 00 AA ? ? ? F9 ? ? ? F9 ? ? ? 90 ? ? ? 91 ? ? ? F9 ? ? ? F9 ? ? ? 91 ? ? ? F9 ? ? ? 94",
                "libminecraftpe.so"
            );
            if (containerDtor && !mContainerDtorHook.installed()) {
                mContainerDtorHook = pl::memory::HookHandle(
                    reinterpret_cast<void*>(containerDtor),
                    reinterpret_cast<void*>(&directContainerDtorHook),
                    reinterpret_cast<void**>(&sOriginalContainerDtor)
                );
            }

            const auto drawText = pl::memory::resolveSignature(
                "? ? ? D1 ? ? ? A9 ? ? ? F9 ? ? ? 91 ? ? ? A9 E8 03 05 2A",
                "libminecraftpe.so"
            );
            if (drawText && !mDrawTextHook.installed()) {
                mDrawTextHook = pl::memory::HookHandle(
                    reinterpret_cast<void*>(drawText),
                    reinterpret_cast<void*>(&directDrawTextHook),
                    reinterpret_cast<void**>(&sOriginalDrawText)
                );
                mUiTextHookInstalled = mDrawTextHook.installed();
            }

            const auto tick = pl::memory::resolveSignature(
                "? ? ? FC ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? 91 ? ? ? D1 54 D0 3B D5 F3 03 00 AA ? ? ? F9 ? ? ? F8 ? ? ? 39",
                "libminecraftpe.so"
            );

            if (tick && !mTickHook.installed()) {
                mTickHook = pl::memory::HookHandle(
                    reinterpret_cast<void*>(tick),
                    reinterpret_cast<void*>(&directTickHook),
                    reinterpret_cast<void**>(&sOriginalTick)
                );
                mTickHookInstalled = mTickHook.installed();
            }

            // Screen open/close is supplied by BedrockTools::ScreenStateEvent.
            mReady = mTickHookInstalled.load();

            mSelf.getLogger().info(
                "OPSUCHT direkte Hooks: rawName={} tick={} | ScreenStateEvent aktiv",
                mRawNameResolved.load(), mTickHookInstalled.load()
            );

            if (mTickHookInstalled.load()) {
                return;
            }

            std::this_thread::sleep_for(
                std::chrono::seconds(1)
            );
        }
    }

    /*
     * DAS ist die eigentliche Inventarauslese.
     *
     * Sie bleibt vollständig erhalten:
     *
     * Player
     *  -> PlayerInventory
     *  -> Container
     *  -> ItemStack
     *  -> Name
     *  -> Anzahl
     *  -> OPSUCHT Preis
     *  -> Stackwert
     *  -> Gesamtwert
     */
    struct CachedPrice {
        PriceInfo info;
        std::uint64_t generation = 0;
        std::uint32_t expiresAtTick = 0;
    };

    PriceInfo cachedMatch(const std::string& candidate) {
        if (candidate.empty()) return {};

        const auto generation = mApi.priceGeneration();
        auto it = mPriceCache.find(candidate);
        if (it != mPriceCache.end() &&
            it->second.generation == generation &&
            static_cast<std::int32_t>(it->second.expiresAtTick - mTickCounter) > 0) {
            return it->second.info;
        }

        PriceInfo info = mApi.findMatch(candidate);
        CachedPrice cached;
        cached.info = info;
        cached.generation = generation;
        cached.expiresAtTick = mTickCounter + kPriceCacheTicks;
        mPriceCache[candidate] = std::move(cached);
        return info;
    }

    void clearPriceCacheIfApiChanged() {
        const auto generation = mApi.priceGeneration();
        if (generation != mLastPriceGeneration) {
            mPriceCache.clear();
            mLastPriceGeneration = generation;
        }
    }

    void onTick(
        void* actor
    ) {

        ++mDiagTicks;
        mDiagPlayer.store(actor ? 1 : 0);

        if (!mEnabled) {
            return;
        }

        // Die Inventarauslese läuft dauerhaft, solange die Mod aktiviert ist.
        // Das Panel ist ebenfalls dauerhaft sichtbar; der Nutzer kann die Mod
        // beim normalen Spielen selbst über den Mod-Schalter deaktivieren.
        // Dadurch hängt die Funktion nicht mehr vom fehleranfälligen
        // Inventory-/Screen-State ab.

        // Die Inventarauslese darf nicht davon abhängen, dass getRawNameId
        // bereits aufgelöst wurde. Wir wollen zuerst beweisen, dass Player,
        // Container und Slots tatsächlich erreichbar sind.
        if (!actor) {
            return;
        }

        /*
         * Nicht bei jedem Tick das komplette
         * Inventar durchlaufen.
         */
        if (
            ++mTickCounter % kInventoryRefreshTicks != 0
        ) {
            return;
        }

        std::vector<
            InventoryEntry
        > entries;

        double total = 0.0;

        int pricedStacks = 0;
        int pricedItems = 0;

        try {

            auto* player =
                reinterpret_cast<
                    std::byte*
                >(actor);

            /*
             * Player -> PlayerInventory
             */
            void* playerInventory =
                *reinterpret_cast<void**>(
                    player +
                    kPlayerInventory
                );

            mDiagInventory.store(playerInventory ? 1 : 0);
            mDiagContainer.store(0);
            mDiagVector.store(0);
            mDiagSlots.store(0);

            if (!playerInventory) {
                if (++mInventoryReadFailures % 30 == 0) {
                    mSelf.getLogger().info("OPSUCHT Inventar: PlayerInventory ist null");
                }
                return;
            }

            /*
             * PlayerInventory
             * -> FillingContainer
             */
            void* container =
                *reinterpret_cast<void**>(
                    reinterpret_cast<
                        std::byte*
                    >(
                        playerInventory
                    ) +
                    kPlayerInventoryContainer
                );

            mDiagContainer.store(container ? 1 : 0);

            if (!container) {
                return;
            }

            /*
             * FillingContainer
             * -> vector<ItemStack>
             */
            const auto vector =
                *reinterpret_cast<
                    const VectorLayout*
                >(
                    reinterpret_cast<
                        std::byte*
                    >(
                        container
                    ) +
                    kFillingContainerItems
                );

            if (
                !vector.begin ||
                vector.end < vector.begin
            ) {
                mDiagVector.store(0);
                mDiagSlots.store(0);
                return;
            }

            mDiagVector.store(1);

            const std::uintptr_t bytes =
                vector.end -
                vector.begin;

            /*
             * Normales Spielerinventar:
             * 36 Slots.
             */
            const std::size_t slotCount =
                std::min<std::size_t>(
                    bytes /
                        kItemStackSize,
                    36
                );

            mDiagSlots.store(slotCount);

            for (
                std::size_t slot = 0;
                slot < slotCount;
                ++slot
            ) {

                const auto stackAddress =
                    vector.begin +
                    slot *
                        kItemStackSize;

                const auto* stack =
                    reinterpret_cast<
                        const std::byte*
                    >(
                        stackAddress
                    );

                /*
                 * Ist der Stack gültig?
                 */
                const std::uint8_t valid =
                    *reinterpret_cast<
                        const std::uint8_t*
                    >(
                        stack +
                        kItemStackValid
                    );

                /*
                 * Anzahl im Stack
                 */
                const std::uint8_t amount =
                    *reinterpret_cast<
                        const std::uint8_t*
                    >(
                        stack +
                        kItemStackCount
                    );

                if (
                    !valid ||
                    amount == 0
                ) {
                    continue;
                }

                /*
                 * Primäre Namensquelle: ItemStackBase::ItemCounter (0x8) -> Item*
                 * und Item::getDescriptionId() (vtable slot 5). BedrockTools documents
                 * exactly this item pointer path and vtable index.
                 */
                std::string rawName;
                std::string stackDisplayName;
                std::vector<std::string> candidates;
                std::string debugDetails;
                std::uintptr_t debugItemPtr = 0;
                std::uintptr_t debugVtablePtr = 0;
                auto addCandidate = [&](ItemStringFn fn) {
                    if (!fn) return;
                    try {
                        std::string value = fn(stack);
                        if (!value.empty() && std::find(candidates.begin(), candidates.end(), value) == candidates.end()) {
                            candidates.push_back(std::move(value));
                        }
                    } catch (...) {}
                };

                // Read the stack custom-name separately. Some Geyser/OPSUCHT
                // stacks expose the technical Geyser identifier through getCustomName(),
                // so it must never automatically replace the readable API/Item name.
                if (sGetCustomName) {
                    try {
                        stackDisplayName = sGetCustomName(stack);
                    } catch (...) {}
                }
                addCandidate(sGetCustomName);
                addCandidate(sGetName);
                addCandidate(sGetDescriptionName);
                addCandidate(sGetRawNameId);
                addCandidate(sGetTypeName);

                try {
                    const auto counter = *reinterpret_cast<void* const*>(stack + 0x8);
                    const auto item = counter ? *reinterpret_cast<void* const*>(counter) : nullptr;
                    debugItemPtr = reinterpret_cast<std::uintptr_t>(item);
                    if (item) {
                        auto** vt = *reinterpret_cast<void***>(item);
                        debugVtablePtr = reinterpret_cast<std::uintptr_t>(vt);
                        if (vt && vt[5]) {
                            const auto& name = reinterpret_cast<ItemDescriptionIdFn>(vt[5])(item);
                            if (!name.empty() && std::find(candidates.begin(), candidates.end(), name) == candidates.end()) {
                                candidates.push_back(name);
                            }
                        }
                    }
                } catch (...) {}

                if (candidates.empty() && sRawName) {
                    try {
                        std::string value = std::string(sRawName(stack));
                        if (!value.empty()) candidates.push_back(std::move(value));
                    } catch (...) {}
                }

                // Add stable technical variants. The complete Geyser identifier
                // is the primary identity; readable names are display-only.
                const std::size_t candidateCountBeforeVariants = candidates.size();
                for (std::size_t ci = 0; ci < candidateCountBeforeVariants; ++ci) {
                    const auto& base = candidates[ci];
                    std::string compact = base;
                    for (char& c : compact) {
                        if (c == ':' || c == '/' || c == '-') c = '_';
                    }
                    if (compact != base && !compact.empty() &&
                        std::find(candidates.begin(), candidates.end(), compact) == candidates.end()) {
                        candidates.push_back(compact);
                    }
                    const auto colon = base.find(':');
                    if (colon != std::string::npos && colon + 1 < base.size()) {
                        const std::string tail = base.substr(colon + 1);
                        if (!tail.empty() && std::find(candidates.begin(), candidates.end(), tail) == candidates.end()) {
                            candidates.push_back(tail);
                        }
                    }
                }

                // V3 DIAGNOSTIC: preserve the raw client-side identity evidence instead
                // of guessing a market price. This lets us see which ItemStack fields
                // are actually different for OPSUCHT custom/enchant variants.
                if (debugItemPtr) {
                    std::ostringstream dbg;
                    dbg << "slot=" << slot
                        << " item=0x" << std::hex << debugItemPtr
                        << " vt=0x" << debugVtablePtr << std::dec;
                    for (std::size_t ci = 0; ci < candidates.size() && ci < 6; ++ci) {
                        dbg << " | c" << ci << "=" << candidates[ci];
                    }
                    dbg << " | bytes=";
                    // ItemStack size used by the current inventory bridge is 0x98.
                    // Capture the first 32 bytes as a stable diagnostic fingerprint.
                    for (std::size_t bi = 0; bi < 32; ++bi) {
                        const unsigned int b = static_cast<unsigned int>(
                            *reinterpret_cast<const std::uint8_t*>(stack + bi));
                        if (bi) dbg << ':';
                        dbg << std::hex << std::setw(2) << std::setfill('0') << b;
                    }
                    debugDetails = dbg.str();
                }

                // Resolve cheap/exact candidates first. For Geyser custom items we
                // MUST NOT fall back to the vanilla material: a custom OPSUCHT
                // netherite pickaxe can be worth millions while its Java base item
                // is only a normal netherite pickaxe.
                clearPriceCacheIfApiChanged();
                const bool isGeyserCustom = std::any_of(candidates.begin(), candidates.end(), [](const std::string& candidate) {
                    return normalizeItemKeepPrefix(candidate).rfind("geyser custom ", 0) == 0;
                });

                for (const auto& candidate : candidates) {
                    const PriceInfo match = cachedMatch(candidate);
                    if (match.samples > 0 && (match.sum / match.samples) > 0.0) {
                        rawName = candidate;
                        break;
                    }
                }

                // Never select a vanilla candidate for a Geyser custom stack.
                // Keeping it unpriced is safer than displaying a completely wrong
                // price such as 70,000 $ for a >100M $ custom item.
                if (isGeyserCustom && rawName.empty()) {
                    for (const auto& candidate : candidates) {
                        if (normalizeItemKeepPrefix(candidate).rfind("geyser custom ", 0) == 0) {
                            rawName = candidate;
                            break;
                        }
                    }
                }
                if (rawName.empty() && !candidates.empty()) rawName = candidates.front();

                if (rawName.empty()) {
                    ++mDiagRawFail;
                    InventoryEntry entry;
                    entry.name = "Unbekanntes Item";
                    entry.amount = static_cast<int>(amount);
                    entry.debugDetails = std::move(debugDetails);
                    entries.push_back(std::move(entry));
                    continue;
                }

                ++mDiagRawOk;

                const PriceInfo resolved = cachedMatch(rawName);
                const double price = resolved.samples > 0
                    ? resolved.sum / resolved.samples
                    : 0.0;

                if (price > 0.0) ++mDiagPriceMatches;

                InventoryEntry entry;

                std::string apiName = resolved.displayName;
                // Display identity is separate from price identity. Never show the
                // technical Geyser identifier (or its compact form) as the HUD name.
                const auto isTechnicalGeyserName = [](const std::string& value) {
                    const std::string n = normalizeItemKeepPrefix(value);
                    return n.rfind("geyser custom ", 0) == 0 ||
                           n.rfind("geyser_custom_", 0) == 0;
                };
                if (!stackDisplayName.empty() && !isTechnicalGeyserName(stackDisplayName)) {
                    entry.name = stackDisplayName;
                } else if (!apiName.empty() && !isTechnicalGeyserName(apiName)) {
                    entry.name = apiName;
                } else if (isGeyserCustom) {
                    entry.name = shortDisplayNameFromTechnicalId(rawName);
                } else {
                    entry.name = displayItemName(rawName);
                }
                if (entry.name.empty()) entry.name = rawName;

                entry.amount =
                    static_cast<int>(
                        amount
                    );

                entry.unitPrice =
                    price;

                entry.stackValue =
                    price *
                    static_cast<double>(
                        amount
                    );
                entry.debugDetails = std::move(debugDetails);

                entries.push_back(
                    std::move(entry)
                );

                total +=
                    entries.back()
                        .stackValue;

                if (price > 0.0) {
                    ++pricedStacks;
                    pricedItems +=
                        static_cast<int>(
                            amount
                        );
                }
            }

        }
        catch (...) {

            return;
        }

        /*
         * Gleiche Items aus mehreren Slots zusammenfassen.
         */
        {
            std::vector<InventoryEntry> merged;

            for (const auto& entry : entries) {
                auto it = std::find_if(
                    merged.begin(),
                    merged.end(),
                    [&](const InventoryEntry& existing) {
                        return normalizeItem(existing.name) ==
                               normalizeItem(entry.name);
                    }
                );

                if (it == merged.end()) {
                    merged.push_back(entry);
                } else {
                    it->amount += entry.amount;
                    it->stackValue += entry.stackValue;
                }
            }

            entries.swap(merged);
        }

        // Immer ein sichtbares Panel liefern, solange das Modul aktiviert ist.
        // So bleibt die UI auch dann sichtbar, wenn Minecraft gerade noch
        // keinen lesbaren Stack geliefert hat. Sobald Daten vorliegen, wird
        // die Tabelle automatisch damit ersetzt.
        {
            std::lock_guard<std::mutex> lock(mDataMutex);
            mLastEntries = entries;
            mLastTotal = total;
            mLastPricedStacks = pricedStacks;
            mLastPricedItems = pricedItems;
        }

        drawInventoryOverlay(
            entries,
            total,
            pricedStacks,
            pricedItems
        );
    }

    void refreshOverlayFromCache() {
        std::vector<InventoryEntry> entries;
        double total = 0.0;
        int pricedStacks = 0;
        int pricedItems = 0;

        {
            std::lock_guard<std::mutex> lock(mDataMutex);
            entries = mLastEntries;
            total = mLastTotal;
            pricedStacks = mLastPricedStacks;
            pricedItems = mLastPricedItems;
        }

        drawInventoryOverlay(
            entries,
            total,
            pricedStacks,
            pricedItems
        );
    }

    bool registerFont() {

        const char* paths[] = {

            "/system/fonts/Roboto-Regular.ttf",

            "/system/fonts/Roboto-Medium.ttf"
        };

        for (
            const char* path :
            paths
        ) {

            std::ifstream file(
                path,
                std::ios::binary
            );

            if (!file) {
                continue;
            }

            std::vector<
                unsigned char
            > data(
                (
                    std::istreambuf_iterator<
                        char
                    >(file)
                ),
                std::istreambuf_iterator<
                    char
                >()
            );

            if (
                !data.empty() &&
                pl::modmenu::registerFont(
                    std::string(kFontId),
                    data
                )
            ) {

                return true;
            }
        }

        return false;
    }

    static void drawText(
        std::vector<
            pl::modmenu::DrawCommand
        >& commands,
        float x,
        float y,
        float size,
        std::uint32_t color,
        const std::string& text
    ) {

        pl::modmenu::DrawCommand cmd;

        cmd.type =
            pl::modmenu::DrawCommandType::Text;

        cmd.x = x;
        cmd.y = y;

        cmd.size = size;

        cmd.color = color;

        cmd.text = text;

        cmd.fontId =
            std::string(kFontId);

        commands.push_back(
            std::move(cmd)
        );
    }

    static void drawRect(
        std::vector<
            pl::modmenu::DrawCommand
        >& commands,
        float x,
        float y,
        float w,
        float h,
        std::uint32_t color
    ) {

        pl::modmenu::DrawCommand cmd;

        cmd.type =
            pl::modmenu::DrawCommandType::RectFilled;

        cmd.x = x;
        cmd.y = y;

        cmd.w = w;
        cmd.h = h;

        cmd.color = color;

        commands.push_back(
            std::move(cmd)
        );
    }

    bool registerFrameImage() {
        static const std::string imageId = "opvantis.original_frame";
        const bool ok = pl::modmenu::registerImage(
            imageId,
            std::span<const unsigned char>(
                reinterpret_cast<const unsigned char*>(opvantis_frame::kRgba),
                opvantis_frame::kRgbaSize
            ),
            opvantis_frame::kWidth,
            opvantis_frame::kHeight
        );
        mSelf.getLogger().info("OPVANTIS Originalrahmen-Textur: {}", ok ? "OK" : "FEHLER");
        return ok;
    }

    void drawNativePanelAfterScreen(void*) {}

    static OpsuchtRectangleArea makePanelRect(float left, float top, float right, float bottom) {
        OpsuchtRectangleArea r{};
        float v[4] = {left, right, top, bottom};
        std::memcpy(&r, v, sizeof(v));
        return r;
    }

    void drawDirectInventoryOverlayWithArgs(void*, void*, void*, void*) {
        if (!mEnabled.load()) return;

        std::vector<InventoryEntry> entries;
        double total = 0.0;
        int pricedItems = 0;
        {
            std::lock_guard<std::mutex> lock(mDataMutex);
            entries = mLastEntries;
            total = mLastTotal;
            pricedItems = mLastPricedItems;
        }

        std::vector<pl::modmenu::DrawCommand> commands;
        commands.reserve(1);

        // Draw the actual original PNG texture as one GPU image command.
        // No pixel-by-pixel rectangles: this keeps the render path lightweight.
        pl::modmenu::DrawCommand frame{};
        frame.type = pl::modmenu::DrawCommandType::Image;
        frame.x = 18.0f;
        frame.y = 24.0f;
        frame.w = 330.0f;
        frame.h = 330.0f * 920.0f / 712.0f;
        frame.imageId = "opvantis.original_frame";
        commands.push_back(std::move(frame));

        pl::modmenu::submitDrawCommands(std::string(kModuleId), commands);
    }
    void drawInventoryOverlay(
        const std::vector<InventoryEntry>&,
        double,
        int,
        int
    ) {
        // Native Minecraft UI renderer is the only panel renderer.
        // This intentionally does nothing so no HUD/ghost panel is drawn.
    }

    void clearOverlay() {
        // The next frame replaces the Mod Menu draw list. Native inventory
        // rendering is controlled by mInventoryOpen and has no persistent
        // retained panel.
    }

private:

    ll::mod::NativeMod& mSelf;

    OpsuchtApi mApi;

    std::atomic_bool mStop{
        false
    };

    std::atomic_bool mEnabled{
        false
    };

    std::atomic_bool mInventoryOpen{
        false
    };

    pl::memory::HookHandle mTickHook;
    pl::memory::HookHandle mScreenViewRenderHook;
    pl::memory::HookHandle mContainerOpenHook;
    pl::memory::HookHandle mContainerDtorHook;
    pl::memory::HookHandle mDrawTextHook;
    std::atomic_bool mRawNameResolved{false};
    std::atomic_bool mTickHookInstalled{false};
    std::atomic_bool mUiRenderHookInstalled{false};
    std::atomic_bool mUiTextHookInstalled{false};
    std::atomic_bool mUiPanelDrawnThisFrame{false};
    std::atomic_bool mInventoryTextSeenThisFrame{false};
    std::atomic<int> mInventoryTextMissingFrames{0};

    std::atomic_bool mReady{
        false
    };

    std::thread mSetupThread;

    std::uint32_t mTickCounter =
        0;

    std::uint32_t mUiRefreshCounter =
        0;

    std::uint32_t mInventoryReadFailures =
        0;

    // Visible runtime diagnostics. These tell us exactly where the Bedrock
    // inventory read chain stops on the user's Minecraft build.
    std::atomic<std::uint64_t> mDiagTicks{0};
    std::atomic<int> mDiagPlayer{0};
    std::atomic<int> mDiagInventory{0};
    std::atomic<int> mDiagContainer{0};
    std::atomic<int> mDiagVector{0};
    std::atomic<std::size_t> mDiagSlots{0};
    std::atomic<std::uint64_t> mDiagRawOk{0};
    std::atomic<std::uint64_t> mDiagRawFail{0};
    std::atomic<std::uint64_t> mDiagPriceMatches{0};

    std::mutex mDataMutex;
    std::unordered_map<std::string, CachedPrice> mPriceCache;
    std::uint64_t mLastPriceGeneration = 0;
    std::vector<InventoryEntry> mLastEntries;
    double mLastTotal = 0.0;
    int mLastPricedStacks = 0;
    int mLastPricedItems = 0;

    bedrocktools::events::Subscription mScreenSubscription = 0;
    bedrocktools::events::Subscription mLocalPlayerSubscription = 0;
};

OpsuchtInventarwertMod*
    OpsuchtInventarwertMod::sInstance =
        nullptr;

OpsuchtInventarwertMod::RawNameFn
    OpsuchtInventarwertMod::sRawName = nullptr;
OpsuchtInventarwertMod::ItemStringFn OpsuchtInventarwertMod::sGetName = nullptr;
OpsuchtInventarwertMod::ItemStringFn OpsuchtInventarwertMod::sGetRawNameId = nullptr;
OpsuchtInventarwertMod::ItemStringFn OpsuchtInventarwertMod::sGetTypeName = nullptr;
OpsuchtInventarwertMod::ItemStringFn OpsuchtInventarwertMod::sGetDescriptionName = nullptr;
OpsuchtInventarwertMod::ItemStringFn OpsuchtInventarwertMod::sGetCustomName = nullptr;
OpsuchtInventarwertMod::ItemDescriptionIdFn
    OpsuchtInventarwertMod::sItemDescriptionId = nullptr;
OpsuchtInventarwertMod::TickFn
    OpsuchtInventarwertMod::sOriginalTick =
        nullptr;

OpsuchtInventarwertMod::ScreenViewRenderFn
    OpsuchtInventarwertMod::sOriginalScreenViewRender = nullptr;
OpsuchtInventarwertMod::ScreenFn
    OpsuchtInventarwertMod::sOriginalContainerOpen = nullptr;
OpsuchtInventarwertMod::ScreenFn
    OpsuchtInventarwertMod::sOriginalContainerDtor = nullptr;
OpsuchtInventarwertMod::DrawTextFn
    OpsuchtInventarwertMod::sOriginalDrawText = nullptr;
void* OpsuchtInventarwertMod::sLastUiContext = nullptr;
void* OpsuchtInventarwertMod::sLastFont = nullptr;
void* OpsuchtInventarwertMod::sLastTextMeasure = nullptr;
void* OpsuchtInventarwertMod::sLastCaretMeasure = nullptr;

PL_REGISTER_MOD(
    OpsuchtInventarwertMod,
    OpsuchtInventarwertMod::instance()
)

} // namespace
