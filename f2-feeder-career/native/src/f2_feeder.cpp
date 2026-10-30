// SPDX-License-Identifier: AGPL-3.0-or-later
//
// F2 Feeder Career: two careers in the Addons row of the Driver Career content select, each with its
// own mode, data group and save container (db/career.json, written by tools/gen.py):
//   f1_career_f2_25  F2 2025 -> F1 2026
//   f1_career_f2_26  F2 2026 -> F1 2027
// The stock wizard skips the Skip to F1 / Start in F2 step when Game_CareerWizard_StartsInF1 answers
// yes, which it does for 2026-model data groups. Answer no for these two, so the wizard offers Start
// in F2 and runs the mode's F2_Default setup list.
#include <swofty/career_content.hpp>
#include <swofty/game.hpp>
#include <swofty/mixin.hpp>

#include <cstring>

namespace {

namespace content = swofty::career::content;

struct Career {
    const char *id, *title, *description, *group, *suffix, *mode;
};

constexpr Career kCareers[] = {
    {"f2-2025", "lng_f2f_a_title", "lng_f2f_a_description", "f1_career_f2_25", "FA26", "driver_career_f2_25"},
    {"f2-2026", "lng_f2f_b_title", "lng_f2f_b_description", "f1_career_f2_26", "FB27", "driver_career_f2_26"},
};

bool ours(const char *group)
{
    if (!group)
        return false;
    for (const Career &c : kCareers)
        if (!std::strcmp(group, c.group))
            return true;
    return false;
}

bool register_tiles()
{
    for (const Career &c : kCareers) {
        content::Result r = content::add({.id = c.id,
                                          .title = c.title,
                                          .description = c.description,
                                          .data_group = c.group,
                                          .suffix = c.suffix,
                                          .mode = c.mode,
                                          .model = content::Model::f1_2026,
                                          .careers = content::Career::driver});
        swofty::log("[f2-feeder] tile %s (%s): %s", c.id, c.group, swofty::career::name(r));
    }
    return true;
}

} // namespace

SWOFTY_WRAP(Game_CareerWizard_StartsInF1, bool, void *entrants, void *wizard_data, const char *group)
{
    bool stock = orig(entrants, wizard_data, group);
    if (stock && ours(group)) {
        swofty::log("[f2-feeder] %s: Start in F2 offered", group);
        return false;
    }
    return stock;
}

SWOFTY_ADDON(register_tiles)
