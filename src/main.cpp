#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>

#include <algorithm>
#include <vector>

#include "frame_data.h"

namespace {
constexpr const char *kModuleId = "opsucht_inventarwert.panel";
constexpr const char *kImageId = "opsucht_inventarwert.frame";

class OpsuchtInventarwert {
public:
    static OpsuchtInventarwert &instance() {
        static OpsuchtInventarwert mod;
        return mod;
    }

    OpsuchtInventarwert()
        : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod &getSelf() const { return mSelf; }

    bool load() {
        getSelf().getLogger().info("Opsucht Inventarwert – Rahmen wird geladen.");
        return true;
    }

    bool enable() {
        mFrame = opsucht_frame::decodeRgba();
        if (mFrame.empty()) {
            getSelf().getLogger().error("Rahmen konnte nicht dekodiert werden.");
            return false;
        }

        if (!pl::modmenu::registerImage(
                kImageId,
                mFrame,
                opsucht_frame::kWidth,
                opsucht_frame::kHeight)) {
            getSelf().getLogger().error("Rahmen konnte nicht registriert werden.");
            return false;
        }

        if (!pl::modmenu::ModuleBuilder(kModuleId, "Opsucht Inventarwert")
                .modId(getSelf().getId())
                .description("Zeigt den Opsucht-Inventarwert an.")
                .defaultEnabled(true)
                .registerModule()) {
            getSelf().getLogger().error("HUD-Modul konnte nicht registriert werden.");
            return false;
        }

        zeichneRahmen();
        getSelf().getLogger().info("Opsucht Inventarwert aktiviert.");
        return true;
    }

    bool disable() {
        pl::modmenu::submitDrawCommands(kModuleId, {});
        pl::modmenu::unregisterModule(kModuleId);
        return true;
    }

    bool unload() {
        mFrame.clear();
        mFrame.shrink_to_fit();
        return true;
    }

private:
    void zeichneRahmen() {
        const auto screen = pl::modmenu::getHudSurfaceSize();

        // Schritt 1: Nur der feste Rahmen.
        // Die spätere Tabelle wird hier zusätzlich als eigene DrawCommand-Liste
        // darübergelegt.
        constexpr float panelHeight = 465.0f;
        constexpr float panelWidth = 360.0f;

        float x = 10.0f;
        float y = (screen.height > panelHeight)
                      ? (screen.height - panelHeight) * 0.5f
                      : 5.0f;

        // Falls der Bildschirm schmaler als das Panel ist, links bündig bleiben.
        x = std::max(0.0f, std::min(x, screen.width - panelWidth));

        const std::vector<pl::modmenu::DrawCommand> commands = {{
            {
                .type = pl::modmenu::DrawCommandType::Image,
                .x = x,
                .y = y,
                .w = panelWidth,
                .h = panelHeight,
                .imageId = kImageId,
            },
        }};

        pl::modmenu::submitDrawCommands(kModuleId, commands);
    }

    ll::mod::NativeMod &mSelf;
    std::vector<unsigned char> mFrame;
};

} // namespace

PL_REGISTER_MOD(OpsuchtInventarwert, OpsuchtInventarwert::instance())
