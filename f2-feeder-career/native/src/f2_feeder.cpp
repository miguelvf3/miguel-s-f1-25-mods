// SPDX-License-Identifier: AGPL-3.0-or-later
//
// F2 Feeder Career: a career in the Addons row of the Driver Career content select, with its own
// mode, data group and save container (db/career.json, written by tools/gen.py):
//   f1_career_f2_26  F2 2026 -> F1 2027
// The stock wizard skips the Skip to F1 / Start in F2 step when Game_CareerWizard_StartsInF1 answers
// yes, which it does for 2026-model data groups. Answer no for this one, so the wizard offers Start
// in F2 and runs the mode's F2_Default setup list.
//
// Not inside the career tile router (FeTile_Route_CareerItem): for a career content tile its "no"
// path reads an object these tiles don't have and crashed on the press (0x143858b90 reading
// null+0xf08). There the stock answer stands, the router picks the season length route
// (career_wizard_driver_career_to_f1), and the wizard library turns that into the experience step
// (career_wizard_driver_career), the route the router takes for a 2025 career.
//
// The F1 season races the 2027 calendar on the game's 2026 data. The season length presets
// (schedule.dynamic_event_data) offer F1_2027_Long next to F1_2026_Long and the game takes the first
// one of a type, so for this career the 2026 one sits out while a preset is applied. While the career
// is loaded, the F1 race weekend and championship titles read 2027 (titles_2027.inc, from
// tools/gen.py). The F2 rounds read other keys.
#include <swofty/career_content.hpp>
#include <swofty/career_wizard.hpp>
#include <swofty/events.hpp>
#include <swofty/game.hpp>
#include <swofty/lang.hpp>
#include <swofty/mixin.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace {

namespace content = swofty::career::content;
namespace wz = swofty::career::wizard;

thread_local bool g_in_router = false;
bool g_wizard_ours = false; // the last career wizard check was for one of these careers

struct Career {
    const char *id, *title, *description, *group, *suffix, *mode;
};

constexpr Career kCareers[] = {
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

// One season length preset: the 0x30-byte entries at [component+0x290]+0xc0..+0xc8 (Frida, 2026-11-02).
struct Preset {
    uint32_t tracks;
    const char *track_list, *language;
    uint8_t type;                   // 0 long, 1 medium, 2 short
    uint32_t setup_modes, seasons;  // bit masks
    uint32_t formats;
    int32_t transfer_window;
};
static_assert(sizeof(Preset) == 0x30 && offsetof(Preset, type) == 0x18 && offsetof(Preset, transfer_window) == 0x28);

// One track selection (schedule.dynamic_event_data...track_selections): the Custom Season screen's track
// pool, the 0x60-byte entries at [component+0x290]+0xe0..+0xe8 (Frida, 2026-11-02).
struct Selection {
    uint32_t setup_modes, seasons;  // bit masks
    uint8_t rest[0x50];             // weekend formats; the seasons' track lists (a map at +0x18)
    const char *pool;               // default
};
static_assert(sizeof(Selection) == 0x60 && offsetof(Selection, pool) == 0x58);

struct Swap {
    const char *from, *to;
};
constexpr Swap kPresetSwaps[] = {{"F1_2026_full", "F1_2027_full"}, {"F1_2026_16", "F1_2027_16"},
                                 {"F1_2026_10", "F1_2027_10"}};
constexpr const char *kPoolFrom = "F1_2026_full_plus", *kPoolTo = "F1_2027_full";

// The 2026 presets and track pool this career's 2027 ones stand in for, hidden while the career is in
// focus (a preset's type 0xff, the pool's setup modes 0): the game and the Custom Season screen each
// take the first that fits. Hiding the preset only inside ApplyPreset left the screen on the 2026 rounds
// with none of the applied 2027 ones selected (0.6.5), and the 2026 pool too (0.6.6).
Preset *g_presets[std::size(kPresetSwaps)] = {};
uint8_t g_preset_types[std::size(kPresetSwaps)] = {};
Selection *g_pool = nullptr;
uint32_t g_pool_modes = 0;

template <class T, class Name>
void find_pair(char *begin, char *end, size_t stride, Name name, const char *from_name, const char *to_name,
               T *&from, T *&to)
{
    for (char *p = begin; p && p + stride <= end; p += stride) {
        const char *n = name(reinterpret_cast<T *>(p));
        if (!n)
            continue;
        if (!std::strcmp(n, from_name))
            from = reinterpret_cast<T *>(p);
        else if (!std::strcmp(n, to_name))
            to = reinterpret_cast<T *>(p);
    }
}

void hide_2026(char *owner)
{
    if (!owner)
        return;
    char *const *presets = reinterpret_cast<char *const *>(owner + 0xc0);
    char *const *pools = reinterpret_cast<char *const *>(owner + 0xe0);
    for (size_t i = 0; i < std::size(kPresetSwaps); ++i) {
        Preset *from = nullptr, *to = nullptr;
        find_pair<Preset>(presets[0], presets[1], sizeof(Preset), [](Preset *p) { return p->track_list; },
                          kPresetSwaps[i].from, kPresetSwaps[i].to, from, to);
        if (g_presets[i] || !from || !to || from->type != to->type)
            continue;
        g_presets[i] = from;
        g_preset_types[i] = from->type;
        from->type = 0xff;
        swofty::log("[f2-feeder] season length %u: %s instead of %s", g_preset_types[i], kPresetSwaps[i].to,
                    kPresetSwaps[i].from);
    }
    Selection *pool_from = nullptr, *pool_to = nullptr;
    find_pair<Selection>(pools[0], pools[1], sizeof(Selection), [](Selection *s) { return s->pool; }, kPoolFrom,
                         kPoolTo, pool_from, pool_to);
    if (!g_pool && pool_from && pool_to) {
        g_pool = pool_from;
        g_pool_modes = pool_from->setup_modes;
        pool_from->setup_modes = 0;
        swofty::log("[f2-feeder] custom season tracks: %s instead of %s", kPoolTo, kPoolFrom);
    }
}

void show_2026(const char *why)
{
    for (size_t i = 0; i < std::size(kPresetSwaps); ++i) {
        if (!g_presets[i])
            continue;
        g_presets[i]->type = g_preset_types[i];
        g_presets[i] = nullptr;
        swofty::log("[f2-feeder] season length %u: %s back (%s)", g_preset_types[i], kPresetSwaps[i].from, why);
    }
    if (g_pool) {
        g_pool->setup_modes = g_pool_modes;
        g_pool = nullptr;
        swofty::log("[f2-feeder] custom season tracks: %s back (%s)", kPoolFrom, why);
    }
}

struct Title {
    const char *language, *key, *text;
};

constexpr Title kTitles[] = {
#include "titles_2027.inc"
};

bool g_titles = false;

void set_titles(bool on, const char *why)
{
    if (on == g_titles || !swofty::lang::available())
        return;
    size_t done = 0;
    for (const Title &t : kTitles) {
        swofty::lang::Result r = on ? swofty::lang::set(t.key, t.text, t.language) : swofty::lang::clear(t.key, t.language);
        done += r == swofty::lang::Result::ok;
    }
    swofty::lang::refresh();
    g_titles = on;
    swofty::log("[f2-feeder] 2027 titles %s (%s): %zu of %zu", on ? "on" : "off", why, done, std::size(kTitles));
}

void on_career(const SwoftyEvent *, void *)
{
    bool mine = swofty::career::is(kCareers[0].group);
    set_titles(mine, "career loaded or Team HQ");
    if (!mine)
        show_2026("another career");
}

void on_unloaded(const SwoftyEvent *, void *)
{
    set_titles(false, "career unloaded");
    show_2026("career unloaded");
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
    for (const char *ev : {SWOFTY_EVENT_CAREER_LOADED, SWOFTY_EVENT_CAREER_HUB})
        swofty::log("[f2-feeder] subscribe %s: %s", ev, swofty::events::name(swofty::events::subscribe(ev, on_career)));
    swofty::log("[f2-feeder] subscribe %s: %s", SWOFTY_EVENT_CAREER_UNLOADED,
                swofty::events::name(swofty::events::subscribe(SWOFTY_EVENT_CAREER_UNLOADED, on_unloaded)));
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
    g_wizard_ours = ours(group);
    if (!g_wizard_ours)
        show_2026("another career's wizard");
    if (stock && !g_in_router && g_wizard_ours) {
        swofty::log("[f2-feeder] %s: Start in F2 offered", group);
        return false;
    }
    return stock;
}

// The wizard's season length step (Skip to F1) runs before the career loads: then the wizard's check
// says whose career it is. The F2 -> F1 step runs in the loaded career.
SWOFTY_WRAP(Game_SeasonLength_ApplyPreset, void, void *component, uint8_t preset)
{
    std::string group = swofty::career::current_data_group();
    if (component && (group.empty() ? g_wizard_ours : ours(group.c_str())))
        hide_2026(*reinterpret_cast<char **>(static_cast<char *>(component) + 0x290));
    else
        show_2026("another career's season length");
    orig(component, preset);
}

SWOFTY_ADDON(register_tiles)
