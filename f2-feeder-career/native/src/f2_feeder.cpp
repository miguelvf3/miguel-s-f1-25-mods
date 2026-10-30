// SPDX-License-Identifier: AGPL-3.0-or-later
//
// F2 Feeder Career 2026: the stock wizard skips the Skip to F1 / Start in F2 step for the 2026 data
// group (Game_CareerWizard_StartsInF1 answers yes). Answer no for f1_career_2026 so the wizard offers
// Start in F2, which then runs driver_career_26's F2_Default setup list (db/career.json).
#include <swofty/game.hpp>
#include <swofty/mixin.hpp>

#include <cstring>

namespace {
constexpr const char *kGroup = "f1_career_2026";
}

SWOFTY_WRAP(Game_CareerWizard_StartsInF1, bool, void *entrants, void *wizard_data, const char *group)
{
    bool stock = orig(entrants, wizard_data, group);
    if (stock && group && !std::strcmp(group, kGroup)) {
        swofty::log("[f2-feeder] %s: Start in F2 offered", group);
        return false;
    }
    return stock;
}

SWOFTY_ADDON()
