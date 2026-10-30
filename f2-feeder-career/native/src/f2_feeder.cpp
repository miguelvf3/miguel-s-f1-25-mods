// SPDX-License-Identifier: AGPL-3.0-or-later
//
// F2 Feeder Career: two careers in the Addons row of the Driver Career content select, each with its
// own mode, data group and save container (db/career.json, written by tools/gen.py):
//   f1_career_f2_25  F2 2025 -> F1 2026
//   f1_career_f2_26  F2 2026 -> F1 2027
// The stock wizard skips the Skip to F1 / Start in F2 step when Game_CareerWizard_StartsInF1 answers
// yes, which it does for 2026-model data groups. Answer no for these two, so the wizard offers Start
// in F2 and runs the mode's F2_Default setup list.
//
// Not inside the career tile router (FeTile_Route_CareerItem): for a career content tile its "no"
// path reads an object these tiles don't have and crashed on the press (0x143858b90 reading
// null+0xf08). There the stock answer stands, the router picks the season length route
// (career_wizard_driver_career_to_f1), and the wizard library turns that into the experience step
// (career_wizard_driver_career), the route the router takes for a 2025 career.
#include <swofty/career_content.hpp>
#include <swofty/career_wizard.hpp>
#include <swofty/game.hpp>
#include <swofty/mixin.hpp>

#include <cstring>

namespace {

namespace content = swofty::career::content;
namespace wz = swofty::career::wizard;

thread_local bool g_in_router = false;

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
        wz::Result w = wz::tile_route(c.group, "career_wizard_driver_career");
        swofty::log("[f2-feeder] tile %s (%s): %s; experience step route %s", c.id, c.group,
                    swofty::career::name(r), wz::name(w));
    }
    return true;
}

} // namespace

SWOFTY_WRAP(FeTile_Route_CareerItem, swofty::game::Action *, swofty::game::HomeItem *item,
            swofty::game::Action *out)
{
    g_in_router = true;
    swofty::game::Action *r = orig(item, out);
    g_in_router = false;
    return r;
}

SWOFTY_WRAP(Game_CareerWizard_StartsInF1, bool, void *entrants, void *wizard_data, const char *group)
{
    bool stock = orig(entrants, wizard_data, group);
    if (stock && !g_in_router && ours(group)) {
        swofty::log("[f2-feeder] %s: Start in F2 offered", group);
        return false;
    }
    return stock;
}

SWOFTY_ADDON(register_tiles)
