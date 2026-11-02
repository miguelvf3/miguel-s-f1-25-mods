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
//
// Only a created driver picks an academy. A player who takes an existing driver gets that driver's
// real academy (kDriverAcademies) when the career loads: the academies component keeps a map of
// driver key -> academy, saved as assignedAcademies {driverStatId, academId}, and the entry goes in
// the way its save loader puts one in.
#include <swofty/career.hpp>
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

#include <windows.h>

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

// Real academies of the F2 2026 drivers, by entrants.drivers stat_id and driver_academy statId (group
// 2026: 0 Mercedes, 1 Ferrari, 2 Red Bull, 3 Alpine, 4 McLaren, 5 Williams, 6 Audi, 7 Cadillac).
// Sources: gridandformula.co.uk F1 academies and Formula Scout's 2026 F2 preview (March 2026). The
// game's academies take in their partner teams (Mercedes: Aston Martin, as forceindia_26; Ferrari: Haas;
// Red Bull: Racing Bulls), so Mari Boya, Aston Martin's, gets Mercedes'. Herta is Cadillac's test driver.
struct DriverAcademy {
    uint16_t driver, academy;
    const char *name;
};
constexpr DriverAcademy kDriverAcademies[] = {
    {203, 1, "Rafael Camara: Ferrari"},    {189, 1, "Dino Beganovic: Ferrari"},
    {174, 2, "Nikola Tsolov: Red Bull"},   {192, 3, "Gabriele Mini: Alpine"},
    {194, 3, "Alexander Dunne: Alpine"},   {153, 3, "Kush Maini: Alpine"},
    {202, 7, "Colton Herta: Cadillac"},    {204, 0, "Mari Boya: Mercedes (Aston Martin)"},
};

// The academies component (DriverAcademiesComponent, F1 25 Steam build d873a1c8; found statically,
// 2026-11-02): descriptor 0x14605c0b8; +0x278 the entity; +0x298..+0x2a0 its academies, 16-byte
// entries with the academy record at +8 (record +0: u16 statId); +0x2b0 the driver -> academy map,
// entries at +0x2c8 (16 bytes: u16 driver key, +8 academy record). The save loader (0x140d6ee39)
// calls 0x140e091e0(map, &slot, &key): status 1 the key was there, 2 or 3 a new slot (write the key),
// 0 a failure; then it stores the record in the slot.
constexpr uintptr_t kAcademiesDesc = 0x605c0b8, kAcademyMapFindOrAdd = 0xe091e0;
constexpr uint8_t kFindOrAddHead[] = {0x41, 0x57, 0x41, 0x56, 0x41, 0x55, 0x41, 0x54, 0x56, 0x57, 0x55, 0x53};

struct MapSlot {
    uint64_t index;
    uint32_t status;
    uint32_t pad;
};
using FindOrAdd = void(void *map, MapSlot *slot, const uint16_t *key);

template <class T> T at(const void *p, size_t offset)
{
    T v;
    std::memcpy(&v, static_cast<const char *>(p) + offset, sizeof v);
    return v;
}

// The player's driver key (kind << 14 | stat id) from the career entity's entrant rows (0x88 bytes,
// +0x48 bit 0x10000000 the human, +0x50 driver kind, +0x52 driver stat id).
bool player_driver(void *entity, uint16_t *key, uint16_t *stat)
{
    char *row = at<char *>(entity, 0x2e0), *end = at<char *>(entity, 0x2e8);
    if (!row || end < row || (end - row) % 0x88 || end - row > 0x88 * 200)
        return false;
    for (; row < end; row += 0x88) {
        if (!(at<uint32_t>(row, 0x48) & 0x10000000))
            continue;
        *stat = at<uint16_t>(row, 0x52);
        *key = static_cast<uint16_t>(at<uint8_t>(row, 0x50) << 14 | *stat);
        return true;
    }
    return false;
}

void give_real_academy()
{
    char *base = reinterpret_cast<char *>(GetModuleHandleW(nullptr));
    if (std::memcmp(base + kAcademyMapFindOrAdd, kFindOrAddHead, sizeof kFindOrAddHead)) {
        swofty::log("[f2-feeder] academy: another game build (the map insert isn't where it was); left alone");
        return;
    }
    void *entity = swofty::career::entity();
    char *comp = static_cast<char *>(swofty::career::component_by_desc(base + kAcademiesDesc));
    uint16_t key = 0, stat = 0;
    if (!entity || !comp || at<void *>(comp, 0x278) != entity || !player_driver(entity, &key, &stat))
        return;
    const DriverAcademy *real = nullptr;
    for (const DriverAcademy &d : kDriverAcademies)
        if (d.driver == stat)
            real = &d;
    if (!real)
        return;
    char *academy = nullptr;
    for (char *e = at<char *>(comp, 0x298), *end = at<char *>(comp, 0x2a0); e && e < end; e += 16) {
        char *record = at<char *>(e, 8);
        if (record && at<uint16_t>(record, 0) == real->academy)
            academy = record;
    }
    if (!academy) {
        swofty::log("[f2-feeder] academy: %s not in this career's academies", real->name);
        return;
    }
    MapSlot slot{};
    reinterpret_cast<FindOrAdd *>(base + kAcademyMapFindOrAdd)(comp + 0x2b0, &slot, &key);
    if (slot.status == 0) {
        swofty::log("[f2-feeder] academy: the map refused driver %u", stat);
        return;
    }
    char *entry = at<char *>(comp, 0x2c8) + 16 * slot.index;
    if (slot.status == 1 && at<void *>(entry, 8))
        return; // the driver already has one: a created driver's pick, or ours from an earlier load
    if (slot.status != 1)
        std::memcpy(entry, &key, sizeof key);
    std::memcpy(entry + 8, &academy, sizeof academy);
    swofty::log("[f2-feeder] academy: %s (driver %u, slot %llu, status %u)", real->name, stat,
                static_cast<unsigned long long>(slot.index), slot.status);
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
    else
        give_real_academy();
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
