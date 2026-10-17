#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>
#include <pl/memory/Signature.hpp>
#include <pl/memory/Hook.hpp>
#include <dlfcn.h>

#include <bedrocktools/BedrockTools.hpp>
#include <bedrocktools/Api.hpp>
#include <bedrocktools/events/Events.hpp>

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

constexpr const char* kSupabasePricesUrl =
    "https://opeinegbcqqqreuxpdqh.supabase.co/functions/v1/opvantis-inventory-prices";

// Read-only snapshot from Supabase. The mod never writes to Supabase and does
// not call the OPSUCHT API directly. The endpoint averages the five newest
// price records for each article and returns the article name plus aliases.
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

static std::string jsonArrayField(const std::string& object, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    std::size_t p = object.find(needle);
    if (p == std::string::npos) return {};
    p = object.find(':', p + needle.size());
    if (p == std::string::npos) return {};
    ++p;
    while (p < object.size() && std::isspace(static_cast<unsigned char>(object[p]))) ++p;
    if (p >= object.size() || object[p] != '[') return {};
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
        if (c == '[') ++depth;
        else if (c == ']') {
            --depth;
            if (depth == 0) return object.substr(start, p - start + 1);
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

    // Start a background, read-only Supabase snapshot loader. Rendering never
    // performs network requests, and the previous price snapshot remains in
    // memory if a later request fails.
    void start(ll::mod::NativeMod& self) {
        stop();
        mSelf = &self;
        mStop = false;
        mThread = std::thread([this]() { worker(); });
        if (mSelf) {
            mSelf->getLogger().info(
                "OPVANTIS: read-only Supabase-Preisquelle gestartet"
            );
        }
    }

    void stop() {
        mStop = true;
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

        // IMPORTANT: Handle Geyser custom identifiers BEFORE generic aliases and
        // vanilla-material fallback. Several different OPSUCHT items share the
        // same Bedrock material (for example DIAMOND_PICKAXE), so a base-material
        // match must never win for a custom identifier.
        const std::string geyserPrefix = "geyser custom ";
        if (fullKey.rfind(geyserPrefix, 0) == 0 || rawName.rfind("geyser_custom_", 0) == 0) {
            const std::string technicalKey = normalizeTechnicalItemKey(rawName);
            if (!technicalKey.empty()) {
                if (auto it = mAliases.find(technicalKey); it != mAliases.end()) return it->second;
                if (auto it = mPrices.find(technicalKey); it != mPrices.end()) return it->second;
            }

            // Only an exact catalog-name match is allowed as a secondary route.
            // No generic material lookup and no fuzzy match for custom items.
            const std::string shortName = shortDisplayNameFromTechnicalId(rawName);
            const std::string shortKey = normalizeItem(shortName);
            if (!shortKey.empty()) {
                if (auto it = mPrices.find(shortKey); it != mPrices.end()) return it->second;
                if (auto it = mAliases.find(shortKey); it != mAliases.end()) return it->second;
            }

            // Dynamic catalog matching for custom Geyser IDs. OPSUCHT may expose
            // a readable market name while Bedrock exposes only a technical ID
            // (e.g. ..._golden_excalibur). Compare the identifier's words against
            // every priced Supabase catalog name. Accept only a unique, strong
            // name hit; never use the shared vanilla material as a fallback.
            const std::string catalogSearchKey = normalizeItem(rawName);
            PriceInfo customBest{};
            std::string customBestName;
            bool customAmbiguous = false;
            for (const auto& [catalogKey, info] : mPrices) {
                if (info.samples <= 0 || info.displayName.empty()) continue;
                const std::string displayKey = normalizeItem(info.displayName);
                if (displayKey.empty() || displayKey.size() < 4) continue;
                // Require the full catalog name to appear as a contiguous phrase
                // in the technical ID. This works across arbitrary catalog sizes
                // without a manually maintained seven-item mapping.
                if (catalogSearchKey.find(displayKey) != std::string::npos) {
                    if (customBestName.empty()) {
                        customBest = info;
                        customBestName = displayKey;
                    } else if (customBestName != displayKey) {
                        customAmbiguous = true;
                        break;
                    }
                }
            }
            if (!customAmbiguous && !customBestName.empty()) return customBest;
            return {};
        }

        // 1) Exact match against the normal API name.
        if (auto it = mPrices.find(key); it != mPrices.end()) return it->second;

        // 2) Exact match against API aliases (identifier/material).
        if (auto it = mAliases.find(key); it != mAliases.end()) return it->second;

        // 3) Vanilla material fallback.
        if (auto it = mMaterialPrices.find(key); it != mMaterialPrices.end()) return it->second;

        // 4) Geyser custom items: use the full technical identifier only.
        // Never fuzzy-match a custom identifier to a vanilla material. A custom
        // Netherite Pickaxe must never inherit the normal Netherite Pickaxe price.
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

    // Strict catalog lookup for a readable custom display name. Unlike
    // findMatch(), this never uses fuzzy matching or a vanilla-material fallback.
    // This is essential when several OPSUCHT custom items share one Bedrock type
    // (for example two talismans represented by the same totem item).
    PriceInfo findExactMatch(const std::string& rawName) const {
        if (rawName.empty()) return {};
        const std::string key = normalizeItem(rawName);
        const std::string fullKey = normalizeItemKeepPrefix(rawName);
        std::scoped_lock lock(mMutex);
        if (auto it = mPrices.find(key); it != mPrices.end()) return it->second;
        if (auto it = mAliases.find(key); it != mAliases.end()) return it->second;
        if (auto it = mPrices.find(fullKey); it != mPrices.end()) return it->second;
        if (auto it = mAliases.find(fullKey); it != mAliases.end()) return it->second;
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
        while (!mStop.load()) {
            const std::string json = httpGet(kSupabasePricesUrl);
            mApiResponseBytes.store(json.size());

            const std::string itemsJson = jsonArrayField(json, "items");
            if (!itemsJson.empty() && json.find("\"success\":true") != std::string::npos) {
                std::unordered_map<std::string, PriceInfo> nextPrices;
                std::unordered_map<std::string, PriceInfo> nextAliases;
                std::unordered_map<std::string, PriceInfo> nextMaterials;
                std::unordered_map<std::string, std::string> materialOwners;
                std::unordered_map<std::string, bool> ambiguousMaterials;
                std::size_t loaded = 0;

                for (const auto& object : jsonObjects(itemsJson)) {
                    const std::string name = jsonString(object, {"name"});
                    const std::string opsuchtName = jsonString(object, {"opsucht_name"});
                    const std::string minecraftId = jsonString(object, {"minecraft_id"});
                    const std::string recognitionKey = jsonString(object, {"erkennungsschluessel"});
                    if (name.empty() && opsuchtName.empty() && minecraftId.empty() && recognitionKey.empty()) continue;

                    const double avgPrice = jsonNumber(object, {"avg_price"});
                    const int samples = static_cast<int>(jsonNumber(object, {"samples"}));
                    if (avgPrice <= 0.0 || samples <= 0) continue;

                    PriceInfo info;
                    info.sum = avgPrice * static_cast<double>(samples);
                    info.samples = samples;
                    // Prefer the explicit OPSUCHT catalog identity for display.
                    // `name` can be a technical/base label, while `opsucht_name`
                    // is the market-facing name used to distinguish custom items.
                    info.displayName = !opsuchtName.empty() ? opsuchtName : name;
                    info.displayKey = true;

                    auto addKey = [&](std::unordered_map<std::string, PriceInfo>& target,
                                      const std::string& value) {
                        if (value.empty()) return;
                        const std::string key = normalizeItem(value);
                        if (!key.empty()) target[key] = info;
                        const std::string fullKey = normalizeItemKeepPrefix(value);
                        if (!fullKey.empty()) target[fullKey] = info;
                        const std::string technicalKey = normalizeTechnicalItemKey(value);
                        if (!technicalKey.empty()) target[technicalKey] = info;
                    };

                    addKey(nextPrices, name);
                    addKey(nextAliases, name);
                    addKey(nextAliases, opsuchtName);
                    // Keep the Supabase recognition key available as a dynamic
                    // catalog alias too. It is not treated as a material ID.
                    addKey(nextAliases, recognitionKey);
                    // Do NOT add minecraft_id as an article alias. Multiple OPSUCHT
                    // custom items (e.g. talismans, tool/armor tiers) can share one
                    // Bedrock base ID; storing that ID here made the last article
                    // overwrite the others. minecraft_id is handled only by the
                    // unambiguous material fallback below.

                    // Only expose a material fallback when that material maps to
                    // one unambiguous article. This prevents custom items from
                    // inheriting the price of a different item with the same base.
                    if (!minecraftId.empty()) {
                        const std::string materialKey = normalizeItem(minecraftId);
                        auto owner = materialOwners.find(materialKey);
                        const std::string articleKey = normalizeItem(!name.empty() ? name : opsuchtName);
                        if (owner == materialOwners.end()) {
                            materialOwners[materialKey] = articleKey;
                            nextMaterials[materialKey] = info;
                        } else if (owner->second != articleKey) {
                            ambiguousMaterials[materialKey] = true;
                            nextMaterials.erase(materialKey);
                        }
                    }
                    ++loaded;
                }

                {
                    std::scoped_lock lock(mMutex);
                    mPrices = std::move(nextPrices);
                    mAliases = std::move(nextAliases);
                    mMaterialPrices = std::move(nextMaterials);
                    mApiAuctionCount.store(loaded);
                    mApiCategoryCount.store(0);
                    mPriceGeneration.fetch_add(1);
                }

                if (mSelf) {
                    mSelf->getLogger().info(
                        "OPVANTIS Supabase: {} Artikel mit Preisen geladen ({} Bytes)",
                        loaded, json.size()
                    );
                }
            } else if (mSelf) {
                mSelf->getLogger().warn(
                    "OPVANTIS Supabase: Preisabruf fehlgeschlagen; letzter Snapshot bleibt aktiv"
                );
            }

            for (int i = 0; i < kApiRefreshSeconds && !mStop.load(); ++i) {
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

        const bool registered =
            pl::modmenu::ModuleBuilder(
                std::string(kModuleId),
                "OPSUCHT Inventarwert"
            )
            .modId(
                mSelf.getId()
            )
            .description(
                "Test 18: Panel wird nur bei ScreenStateEvent Container/Opened angezeigt."
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
    using ScreenViewRenderFn = void(*)(void*, void*, void*, void*, void*, void*, void*, void*);
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

    static void directScreenViewRenderHook(
        void* self, void* a2, void* a3, void* a4,
        void* a5, void* a6, void* a7, void* a8
    ) {
        auto* mod = sInstance;
        // Keep the game's original screen rendering untouched.
        if (sOriginalScreenViewRender) {
            sOriginalScreenViewRender(self, a2, a3, a4, a5, a6, a7, a8);
        }
        if (mod) {
            mod->mScreenViewRenderCalls.fetch_add(1, std::memory_order_relaxed);

            // Test 25 safety change: do not reuse drawText arguments from a
            // previous UI call here. Their lifetime is not guaranteed after
            // the original drawText invocation has returned. The overlay is
            // drawn only from directDrawTextHook using that call's live args.
        }
    }

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

            // Test 25 safety change: never draw using cached UI pointers from
            // the game tick. Only draw from directDrawTextHook while its
            // arguments belong to the current Minecraft text-render call.
        }
        if (sOriginalTick) sOriginalTick(actor);
    }


    static void* directContainerOpenHook(void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7) {
        auto* self = sInstance;
        if (self) {
            self->mContainerOpenCalls.fetch_add(1);
            self->mContainerVisible.store(true);
            self->addDiagLog("Container OPEN hook (rechte Testtafel aktiviert)");
            self->mUiPanelDrawnThisFrame.store(false);
        }
        return sOriginalContainerOpen ? sOriginalContainerOpen(a0, a1, a2, a3, a4, a5, a6, a7) : nullptr;
    }

    static void* directContainerDtorHook(void* a0, void* a1, void* a2, void* a3, void* a4, void* a5, void* a6, void* a7) {
        auto* self = sInstance;
        if (self) {
            self->mContainerCloseCalls.fetch_add(1);
            self->mContainerVisible.store(false);
            self->addDiagLog("Container CLOSE hook (rechte Testtafel deaktiviert)");
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

            self->mDrawTextCalls.fetch_add(1);
            if (inventoryLabel) {
                self->mInventoryLabelHits.fetch_add(1);
                self->mInventoryTextSeenThisFrame.store(true);
                self->mInventoryTextMissingFrames.store(0);
                self->mInventoryOpen.store(true);
                std::string clipped = uiText.substr(0, 48);
                self->addDiagLog(std::string("UI-TEXT: ") + clipped);
            } else if (
                uiText.find("chest") != std::string::npos ||
                uiText.find("truhe") != std::string::npos ||
                uiText.find("container") != std::string::npos ||
                uiText.find("take half") != std::string::npos ||
                uiText.find("nehmen") != std::string::npos ||
                uiText.find("craft") != std::string::npos ||
                uiText.find("recipe") != std::string::npos ||
                uiText.find("search") != std::string::npos ||
                uiText.find("suchen") != std::string::npos
            ) {
                self->addDiagLog(std::string("UI-TEXT: ") + uiText.substr(0, 48));
            }
        }

        // Test 26: draw after each original UI text call so later inventory
        // labels do not immediately cover the overlay. The overlay calls the
        // saved original function directly, so it does not recurse through this
        // hook. Do not cache or retain any of these call-local pointers.
        if (self && self->mEnabled.load()) {
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
                    self->mScreenEvents.fetch_add(1);
                    self->mLastScreenKind.store(static_cast<int>(event.screen));
                    self->mLastScreenPhase.store(static_cast<int>(event.phase));
                    self->addDiagLog(std::string("Screen type=") + std::to_string(static_cast<int>(event.screen)) +
                        " phase=" + std::to_string(static_cast<int>(event.phase)));

                    if (event.screen != bedrocktools::events::ScreenKind::Container) {
                        self->mNonContainerScreenEvents.fetch_add(1);
                        self->addDiagLog("Screen non-container");
                        return;
                    }
                    self->mContainerScreenEvents.fetch_add(1);
                    self->addDiagLog("Screen container");

                    const bool open =
                        event.phase == bedrocktools::events::ScreenPhase::Opened;

                    self->mInventoryOpen.store(open);
                    self->mUiPanelDrawnThisFrame.store(false);
                    self->addDiagLog(open ? "PANEL-STATUS: OPENED event" : "PANEL-STATUS: CLOSED event");
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

            // Test 21: Observe the native ScreenView::render pass. This is
            // deliberately diagnostic-only: call the original exactly once and
            // count invocations without changing the game's rendering order.
            const auto screenViewRender = pl::memory::resolveSignature(
                "? ? ? FC ? ? ? 6D ? ? ? 6D ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? 91 ? ? ? D1 48 D0 3B D5 FC 03 00 AA",
                "libminecraftpe.so"
            );
            if (screenViewRender && !mScreenViewRenderHook.installed()) {
                mScreenViewRenderHook = pl::memory::HookHandle(
                    reinterpret_cast<void*>(screenViewRender),
                    reinterpret_cast<void*>(&directScreenViewRenderHook),
                    reinterpret_cast<void**>(&sOriginalScreenViewRender)
                );
                mScreenViewRenderHookInstalled.store(mScreenViewRenderHook.installed());
                mSelf.getLogger().info(
                    "OPVANTIS ScreenViewRender hook installed: {}",
                    mScreenViewRenderHookInstalled.load()
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

                // Collect candidate identities internally for strict catalog matching; these details
                // are not displayed in the final inventory panel.
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

                // First resolve readable names exactly against the Supabase
                // catalog. Do this before any technical/base identifiers so a
                // generic NETHERITE_AXE or totem ID cannot win over the real
                // OPSUCHT custom name.
                if (!stackDisplayName.empty()) {
                    const PriceInfo namedMatch = mApi.findExactMatch(stackDisplayName);
                    if (namedMatch.samples > 0 && namedMatch.sum > 0.0) {
                        rawName = stackDisplayName;
                    }
                }

                auto looksTechnical = [](const std::string& value) {
                    const std::string n = normalizeItemKeepPrefix(value);
                    if (n.rfind("minecraft:", 0) == 0 ||
                        n.rfind("geyser custom ", 0) == 0 ||
                        n.rfind("geyser_custom_", 0) == 0 ||
                        n.rfind("item ", 0) == 0) return true;
                    // Names such as NETHERITE_AXE are technical/base labels,
                    // not the visible OPSUCHT market name.
                    bool hasUnderscore = value.find('_') != std::string::npos;
                    bool hasLetter = false;
                    bool allLettersUpper = true;
                    for (unsigned char c : value) {
                        if (std::isalpha(c)) {
                            hasLetter = true;
                            if (!std::isupper(c)) allLettersUpper = false;
                        }
                    }
                    return hasUnderscore && hasLetter && allLettersUpper;
                };

                if (rawName.empty()) {
                    for (const auto& candidate : candidates) {
                        if (candidate.empty() || looksTechnical(candidate)) continue;
                        const PriceInfo namedMatch = mApi.findExactMatch(candidate);
                        if (namedMatch.samples > 0 && namedMatch.sum > 0.0) {
                            rawName = candidate;
                            break;
                        }
                    }
                }

                for (const auto& candidate : candidates) {
                    if (!rawName.empty()) break;
                    // For Geyser/OPSUCHT custom items, never let a generic base
                    // material such as totem, axe, helmet, or chestplate win.
                    // Only try the full technical custom identity; the strict
                    // display-name lookup above handles readable custom names.
                    if (isGeyserCustom &&
                        normalizeItemKeepPrefix(candidate).rfind("geyser custom ", 0) != 0) {
                        continue;
                    }
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
                // Do not silently pick candidates.front(): candidate order is not
                // a reliable item identity and can turn a base material into a
                // false article/price. If nothing matched, keep the slot unpriced.
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

                // Final safety net: never assign a price from a technical base
                // identifier alone. If Bedrock does not expose a readable custom
                // name or a unique Geyser identifier, leave it unpriced rather
                // than showing another OPSUCHT item's price.
                const std::string rawNormalized = normalizeItemKeepPrefix(rawName);
                bool rawIsTechnicalBase =
                    rawNormalized.rfind("minecraft:", 0) == 0 ||
                    rawNormalized.rfind("minecraft ", 0) == 0 ||
                    rawNormalized.rfind("item ", 0) == 0;
                if (rawNormalized.rfind("geyser custom ", 0) == 0 ||
                    rawNormalized.rfind("geyser_custom_", 0) == 0) {
                    rawIsTechnicalBase = false;
                } else {
                    const bool hasUnderscore = rawName.find('_') != std::string::npos;
                    bool hasLetter = false;
                    bool allLettersUpper = true;
                    for (unsigned char c : rawName) {
                        if (std::isalpha(c)) {
                            hasLetter = true;
                            if (!std::isupper(c)) allLettersUpper = false;
                        }
                    }
                    if (hasUnderscore && hasLetter && allLettersUpper) rawIsTechnicalBase = true;
                }
                const PriceInfo resolved = rawIsTechnicalBase ? PriceInfo{} : cachedMatch(rawName);
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
                // Items are considered registered only when the Supabase catalog
                // match contains a price. If no article/price match exists, use the
                // exact requested label instead of implying that the item is known.
                // Keep the frozen Test31 layout untouched; this changes text only.
                if (resolved.samples <= 0 || price <= 0.0) {
                    // Final user-facing behavior: never expose internal diagnostic
                    // strings and never guess a price when catalog identity is unclear.
                    entry.name = "Item nicht erfasst";
                } else if (!apiName.empty() && !isTechnicalGeyserName(apiName)) {
                    entry.name = apiName;
                } else if (!stackDisplayName.empty() && !isTechnicalGeyserName(stackDisplayName)) {
                    entry.name = stackDisplayName;
                } else if (isGeyserCustom) {
                    entry.name = shortDisplayNameFromTechnicalId(rawName);
                } else {
                    entry.name = displayItemName(rawName);
                }
                if (entry.name.empty()) entry.name = "Item nicht erfasst";

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

    void drawNativePanelAfterScreen(void*) {}

    static OpsuchtRectangleArea makePanelRect(float left, float top, float right, float bottom) {
        OpsuchtRectangleArea r{};
        float v[4] = {left, right, top, bottom};
        std::memcpy(&r, v, sizeof(v));
        return r;
    }

    void addDiagLog(const std::string& message) {
        std::lock_guard<std::mutex> lock(mDiagLogMutex);
        mDiagLog.push_back(message);
        if (mDiagLog.size() > 6) mDiagLog.erase(mDiagLog.begin());
    }

    void drawDiagnosticOverlay() {}

    void drawDirectInventoryOverlayWithArgs(void* uiContext, void* font, void* textMeasure, void* caretMeasure) {
        // TEST 24: intentionally use Minecraft's original drawText function,
        // not ModMenu submitDrawCommands(), to test whether the overlay is drawn
        // on top of the inventory UI. This test is text-only by design.
        if (!mEnabled.load() || !uiContext || !sOriginalDrawText || !font || !textMeasure || !caretMeasure) return;

        std::vector<InventoryEntry> entries;
        double total = 0.0;
        int pricedItems = 0;
        {
            std::lock_guard<std::mutex> lock(mDataMutex);
            entries = mLastEntries;
            total = mLastTotal;
            pricedItems = mLastPricedItems;
        }

        // Compact design-only test: preserve the known working direct drawText
        // path, but align columns, remove the auction subtitle, and reserve room
        // for the total by limiting the visible list. Price/data logic is unchanged.
        constexpr std::size_t kVisibleRows = 18;
        std::ostringstream out;
        out << "OPVANTIS | INVENTARWERT\n";
        out << "ITEM                 STUECK  PREIS/ST.\n";
        out << "---------------------------------------\n";
        if (entries.empty()) {
            out << "Inventar wird gelesen...\n";
        } else {
            const std::size_t shown = std::min<std::size_t>(entries.size(), kVisibleRows);
            for (std::size_t i = 0; i < shown; ++i) {
                std::string name = entries[i].name.empty() ? "Unbekannt" : entries[i].name;
                if (name.size() > 20) name = name.substr(0, 17) + "...";
                out << std::left << std::setw(20) << name
                    << std::right << std::setw(6) << entries[i].amount
                    << std::setw(11)
                    << (entries[i].unitPrice > 0.0 ? formatMoney(entries[i].unitPrice) : "-")
                    << "\n";
            }
            if (entries.size() > shown) out << "+ " << (entries.size() - shown) << " weitere Items\n";
        }
        out << "---------------------------------------\n";
        out << "GESAMTWERT: " << formatMoney(total) << "\n";
        out << "BEWERTETE ITEMS: " << pricedItems;

        // Keep the existing renderer and position; adjust only visual layout.
        std::vector<pl::modmenu::DrawCommand> commands;
        commands.reserve(40);
        constexpr float x = 18.0f, y = 42.0f, w = 420.0f, h = 350.0f;
        constexpr std::uint32_t bg = 0xE6101119;
        constexpr std::uint32_t accent = 0xFFD5A8FF;
        constexpr std::uint32_t border = 0xFF7435B0;
        constexpr std::uint32_t muted = 0xFFB8C0CC;
        constexpr std::uint32_t white = 0xFFF2F2F2;
        drawRect(commands, x, y, w, h, bg);
        drawRect(commands, x, y, w, 3.0f, border);
        drawRect(commands, x, y, 3.0f, h, border);
        drawRect(commands, x + w - 3.0f, y, 3.0f, h, border);
        drawRect(commands, x, y + h - 3.0f, w, 3.0f, border);
        drawText(commands, x + 12.0f, y + 10.0f, 15.0f, accent, "OPVANTIS | INVENTARWERT");
        drawText(commands, x + 12.0f, y + 36.0f, 9.0f, accent, "ITEM");
        drawText(commands, x + 246.0f, y + 36.0f, 9.0f, accent, "STUECK");
        drawText(commands, x + 322.0f, y + 36.0f, 9.0f, accent, "PREIS/ST.");
        drawText(commands, x + 12.0f, y + 52.0f, 9.0f, muted, "-----------------------------------------------");
        if (entries.empty()) {
            drawText(commands, x + 12.0f, y + 70.0f, 10.0f, white, "Inventar wird gelesen...");
        } else {
            const std::size_t shown = std::min<std::size_t>(entries.size(), kVisibleRows);
            for (std::size_t i = 0; i < shown; ++i) {
                const auto& e = entries[i];
                std::string name = e.name.empty() ? "Unbekannt" : e.name;
                if (name.size() > 25) name = name.substr(0, 22) + "...";
                std::ostringstream qty;
                qty << e.amount;
                const float rowY = y + 66.0f + 13.0f * static_cast<float>(i);
                drawText(commands, x + 12.0f, rowY, 9.0f, white, name);
                drawText(commands, x + 250.0f, rowY, 9.0f, white, qty.str());
                drawText(commands, x + 322.0f, rowY, 9.0f, white,
                         e.unitPrice > 0.0 ? formatMoney(e.unitPrice) : "-");
            }
            if (entries.size() > shown) {
                drawText(commands, x + 12.0f, y + 70.0f + 17.0f * static_cast<float>(shown),
                         8.0f, muted, "+ weitere Items in Inventar erkannt");
            }
        }
        drawRect(commands, x + 10.0f, y + h - 55.0f, w - 20.0f, 1.0f, border);
        drawText(commands, x + 12.0f, y + h - 43.0f, 10.0f, muted, "GESAMTWERT");
        drawText(commands, x + 132.0f, y + h - 43.0f, 11.0f, accent, formatMoney(total));
        drawText(commands, x + 12.0f, y + h - 25.0f, 8.0f, muted,
                 "BEWERTETE ITEMS: " + std::to_string(pricedItems));
        pl::modmenu::submitDrawCommands(std::string(kModuleId) + ".opvantis-panel", commands);

        // Native text is what remains visible above the inventory. Draw each
        // line separately so colors and fixed columns survive that render path.
        // Keep the compact font and 18 rows; no price/data logic is changed.
        const OpsuchtColor nativeAccent{0.84f, 0.66f, 1.0f, 1.0f};
        const OpsuchtColor nativeMuted{0.70f, 0.68f, 0.76f, 1.0f};
        const OpsuchtColor nativeWhite{0.96f, 0.95f, 0.99f, 1.0f};
        auto nativeAt = [&](float left, float top, float right,
                            const std::string& text, const OpsuchtColor& color) {
            OpsuchtRectangleArea rect = makePanelRect(left, top, right, top + 14.0f);
            if (sOriginalDrawText) {
                void* nativeColor = const_cast<OpsuchtColor*>(&color);
                sOriginalDrawText(uiContext, font, &rect,
                                  const_cast<std::string*>(&text), nativeColor,
                                  0.72f, 0, textMeasure, caretMeasure);
            }
        };
        auto nativeLine = [&](float top, const std::string& text, const OpsuchtColor& color) {
            nativeAt(18.0f, top, 360.0f, text, color);
        };

        nativeLine(42.0f, "OPVANTIS | INVENTARWERT", nativeAccent);
        // Each header/value uses its own fixed x-coordinate. Do not use spaces
        // for alignment: Minecraft's font is proportional, so columns drift.
        nativeAt(18.0f, 59.0f, 165.0f, "ITEM", nativeAccent);
        nativeAt(180.0f, 59.0f, 235.0f, "STUECK", nativeAccent);
        nativeAt(260.0f, 59.0f, 360.0f, "PREIS/ST.", nativeAccent);
        nativeLine(73.0f, "-----------------------------------------------", nativeMuted);
        if (entries.empty()) {
            nativeLine(88.0f, "Inventar wird gelesen...", nativeWhite);
        } else {
            const std::size_t shownNative = std::min<std::size_t>(entries.size(), kVisibleRows);
            for (std::size_t i = 0; i < shownNative; ++i) {
                const auto& e = entries[i];
                std::string name = e.name.empty() ? "Unbekannt" : e.name;
                if (name.size() > 25) name = name.substr(0, 22) + "...";
                const float rowY = 87.0f + 13.0f * static_cast<float>(i);
                nativeAt(18.0f, rowY, 170.0f, name, nativeWhite);
                nativeAt(180.0f, rowY, 235.0f, std::to_string(e.amount), nativeWhite);
                nativeAt(260.0f, rowY, 360.0f,
                         e.unitPrice > 0.0 ? formatMoney(e.unitPrice) : "-", nativeWhite);
            }
        }
        nativeLine(326.0f, "GESAMTWERT: " + formatMoney(total), nativeAccent);
        nativeLine(341.0f, "BEWERTETE ITEMS: " + std::to_string(pricedItems), nativeMuted);
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
    std::atomic_bool mScreenViewRenderHookInstalled{false};
    std::atomic<std::uint64_t> mScreenViewRenderCalls{0};
    std::atomic<std::uint64_t> mPostScreenPanelSubmissions{0};
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
    std::atomic<std::uint64_t> mScreenEvents{0};
    std::atomic<std::uint64_t> mContainerScreenEvents{0};
    std::atomic<std::uint64_t> mNonContainerScreenEvents{0};
    std::atomic<std::uint64_t> mContainerOpenCalls{0};
    std::atomic<std::uint64_t> mContainerCloseCalls{0};
    std::atomic_bool mContainerVisible{false};
    std::atomic<std::uint64_t> mDrawTextCalls{0};
    std::atomic<std::uint64_t> mInventoryLabelHits{0};
    std::atomic<int> mLastScreenKind{-1};
    std::atomic<int> mLastScreenPhase{-1};
    std::mutex mDiagLogMutex;
    std::vector<std::string> mDiagLog;

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
