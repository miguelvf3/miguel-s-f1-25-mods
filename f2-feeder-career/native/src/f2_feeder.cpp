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
//
// At the first F1 season's Team HQ, the best AI drivers of the F2 season (kPromoted) take the seats of
// the drivers whose seats are open for 2027 (kLeavingSeats), read from the F2 season's standings.
#include <swofty/career.hpp>
#include <swofty/career_content.hpp>
#include <swofty/career_wizard.hpp>
#include <swofty/events.hpp>
#include <swofty/game.hpp>
#include <swofty/hub.hpp>
#include <swofty/lang.hpp>
#include <swofty/mixin.hpp>
#include <swofty/popup.hpp>
#include <swofty/services.hpp>
#include <swofty/storage.hpp>
#include <swofty/systems/career.hpp>
#include <swofty/systems/ops.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

#include <f2_revamp_api/career.hpp>

#include "interest.hpp"
#include "panel.hpp"

// The career panel (panel.hpp): on or off, and the hub tab's row, where any change opens or closes it.
SWOFTY_SETTING(bool, hub_panel, true);
// The academy's notices and the F2 review, as the game's message popup.
SWOFTY_SETTING(bool, notices, true);
SWOFTY_SETTING(std::string_view, panel_open, "closed");

namespace {

namespace content = swofty::career::content;
namespace wz = swofty::career::wizard;

void refresh_panel(); // the career panel's numbers, below
bool tell_notices(bool at_hub = false); // the academy's notices and the F2 review, below
std::atomic<int> g_notice_tries{0}; // frame ticks left to tell a notice the career wasn't ready for

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

// The offsets and addresses here come from one exe. Another build's PE timestamp
// leaves everything that reads them off; the tiles and titles go through Swofty, and keep working.
constexpr DWORD kKnownExeStamp = 1789118616;

bool known_build()
{
    auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(GetModuleHandleW(nullptr));
    auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(reinterpret_cast<const char *>(dos) + dos->e_lfanew);
    static const bool known = [nt] {
        bool same = nt->FileHeader.TimeDateStamp == kKnownExeStamp;
        swofty::log("[f2-feeder] game exe stamp %lu: %s", static_cast<unsigned long>(nt->FileHeader.TimeDateStamp),
                    same ? "known build" : "another build; calendar lists, academies and promotion stay off");
        return same;
    }();
    return known;
}

void hide_2026(char *owner)
{
    if (!known_build())
        return;
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
    if (!known_build())
        return;
    char *base = reinterpret_cast<char *>(GetModuleHandleW(nullptr));
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

// Promotion by result: in the first F1 season the best AI drivers of the F2 season take the seats open
// for 2027 in the news of 15 October 2026 (Camara replaces Ocon at Haas; both Racing Bulls seats open;
// entrants.drivers / entrants.teams stat ids of the 2026 data). An academy driver takes his academy's
// seat first (Ferrari: Haas, Red Bull: Racing Bulls); the others take the seats left, in this order.
struct LeavingSeat {
    uint16_t team, driver, academy;
    const char *name;
};
constexpr LeavingSeat kLeavingSeats[] = {
    {483, 17, 1, "Esteban Ocon (Haas)"},
    {482, 113, 2, "Liam Lawson (Racing Bulls)"},
    {482, 188, 2, "Arvid Lindblad (Racing Bulls)"},
};
constexpr size_t kPromoted = 3;
// Alone among the F2 2026 drivers, Tsolov keeps his F2 number (6, Hadjar's) in F1: the others have 99, and the
// seat change gives them a free one. His entrants.drivers number is 48.
constexpr uint16_t kTsolov = 174;
constexpr int kTsolovNumber = 48;

// A promoted driver's ratings by his final F2 position: experience, racecraft, awareness, pace (the
// career's statistics 0, 1, 3, 4; overall weights 0.25, 1, 0.25, 2). An AI driver doesn't grow in the F2
// season as the player does (Leon reached F1 at 57, the player at 74, 4 Nov), so these give an overall
// of about 75, 73, 71 and 70 for P1 to P4 and below. Set once, in the promotion; set_driver_ratings
// writes them into the driver's curves, so they grow from there.
struct PromotedRatings {
    float values[4];
};
constexpr PromotedRatings kPromotedRatings[] = {
    {{40, 74, 68, 80}}, {{38, 72, 66, 79}}, {{36, 70, 64, 77}}, {{34, 68, 62, 76}}};
constexpr uint16_t kNoTeam = 0x3fff;

// The career entity and its championships (re/db/structs CareerEntity, Championship, Season,
// F1SeasonStandings): +0x1040..+0x1048 16-byte entries {?, Championship*}, +0x1058 the current one.
// Championship +0x548..+0x550 16-byte season entries {?, Season*}, +0x560 the season index. Season
// +0x2a8 the drivers' standings: +0x20..+0x28 points rounds and +0xd0..+0xd8 result rounds (0x20
// bytes: +8..+0x10 sessions of 0x28 bytes: +8..+0x10 records), +0xa8..+0xb0 extra points. A points
// record is {u32 entrant key, i32 points}, a result record {u32 key, i32 0-based position, ...}; the
// key's high word is the driver stat id.
struct Standing {
    uint16_t driver = 0;
    int points = 0;
    uint8_t finishes[26] = {}; // finishes per position in the sessions that scored, for the countback
};

template <class F> void each_record(char *rounds_begin, char *rounds_end, size_t stride, F f)
{
    for (char *r = rounds_begin; r && r < rounds_end; r += 0x20)
        for (char *s = at<char *>(r, 8), *se = at<char *>(r, 0x10); s && s < se; s += 0x28)
            for (char *e = at<char *>(s, 8), *ee = at<char *>(s, 0x10); e && e < ee; e += stride)
                f(e);
}

Standing *standing_of(Standing *table, size_t &n, size_t cap, uint16_t driver)
{
    for (size_t i = 0; i < n; ++i)
        if (table[i].driver == driver)
            return &table[i];
    if (n == cap)
        return nullptr;
    table[n].driver = driver;
    return &table[n++];
}

// The F2 season's final order: the last season of the entity's championship that isn't the current
// one, sorted as the game's standings are: by points, then countback (most wins, then most P2s...).
size_t read_standings(char *f2, Standing *out, size_t cap);

// The finished F2 championship's results live only in the session the F2 season ends in: a loaded
// save leaves its standings empty, and Career_Rebuild_ChampionshipStandings finds nothing to rebuild
// them from (0.7.5 test). So the order read at the first Team HQ after F2 (before the player's Team
// Select) is kept for that driver, and a load in the first F1 season reads it. Since 0.8.1 it lives in the
// career's own addon data (career::saves, the core's per-career JSON that moves with the save), written at each
// F2 session end (the core writes the file at once; career.saving is a modify event the C subscribe refuses); the old storage file stays as a fallback for 0.8.0 careers.
constexpr const char *kStandingsFile = "f2_standings.txt";
constexpr const char *kAddonId = "miguelvf3.f2-revamp";

std::mutex g_kept_lock;
std::string g_kept; // the table this session kept

const char *result_name(swofty::career::Result r)
{
    return swofty::career::name(r);
}

// The career's data object: {"table": the kept table text, "promoted": [{driver, team, replaced, place}]}.
swofty::json::Value career_data()
{
    if (auto stored = swofty::career::saves::data(kAddonId).get_current(); stored && stored->value.is_object())
        return stored->value;
    return swofty::json::Value::object();
}

void keep_table(const std::string &text, const char *why)
{
    {
        std::lock_guard<std::mutex> lock(g_kept_lock);
        g_kept = text;
    }
    swofty::storage::write(kStandingsFile, text);
    swofty::json::Value v = career_data();
    v["table"] = swofty::json::Value(text);
    swofty::career::Result r = swofty::career::saves::data(kAddonId).set_current(v, 1);
    swofty::log("[f2-feeder] kept F2 table (%s): career data %s, career %s", why, result_name(r),
                swofty::career::saves::current().c_str());
}

// The career's own data first, else the 0.8.0 storage file.
bool read_kept(std::string &text)
{
    if (auto stored = swofty::career::saves::data(kAddonId).get_current()) {
        std::string_view t = stored->value["table"].as_string();
        if (!t.empty()) {
            text.assign(t);
            return true;
        }
    }
    return swofty::storage::read(kStandingsFile, text) == swofty::storage::Result::ok;
}

size_t f2_standings(void *entity, uint16_t player, Standing *out, size_t cap)
{
    char *current = at<char *>(entity, 0x1058), *f2 = nullptr;
    for (char *c = at<char *>(entity, 0x1040), *end = at<char *>(entity, 0x1048); c && c < end; c += 16)
        if (char *champ = at<char *>(c, 8); champ && champ != current)
            f2 = champ;
    if (!f2)
        return 0;
    if (size_t n = read_standings(f2, out, cap)) {
        std::string text = std::to_string(player) + "\n";
        for (size_t i = 0; i < n; ++i)
            text += std::to_string(out[i].driver) + " " + std::to_string(out[i].points) + "\n";
        keep_table(text, "Team HQ after F2");
        return n;
    }
    std::string text;
    if (!read_kept(text))
        return 0;
    size_t n = 0;
    unsigned saved_player = 0, driver = 0;
    int points = 0, used = 0;
    const char *p = text.c_str();
    if (std::sscanf(p, "%u%n", &saved_player, &used) != 1 || saved_player != player)
        return 0;
    for (p += used; n < cap && std::sscanf(p, "%u %d%n", &driver, &points, &used) == 2; p += used)
        out[n++] = {static_cast<uint16_t>(driver), points};
    swofty::log("[f2-feeder] promotion: F2 standings kept when the F2 season ended: %zu drivers", n);
    return n;
}

size_t read_standings(char *f2, Standing *out, size_t cap)
{
    char *sb = at<char *>(f2, 0x548), *se = at<char *>(f2, 0x550);
    if (!sb || se <= sb || (se - sb) % 16)
        return 0;
    char *season = at<char *>(se - 16, 8);
    if (!season)
        return 0;
    char *st = season + 0x2a8;
    size_t n = 0;
    auto add = [&](uint32_t key, int points) {
        if (Standing *s = standing_of(out, n, cap, static_cast<uint16_t>(key >> 16)))
            s->points += points;
    };
    each_record(at<char *>(st, 0x20), at<char *>(st, 0x28), 8, [&](char *e) { add(at<uint32_t>(e, 0), at<int32_t>(e, 4)); });
    for (char *e = at<char *>(st, 0xa8), *ee = at<char *>(st, 0xb0); e && e < ee; e += 8)
        add(at<uint32_t>(e, 0), at<int32_t>(e, 4));
    // Only sessions that scored count: qualifying has result rows too, in the same round and session slots.
    char *points_round = at<char *>(st, 0x20), *points_end = at<char *>(st, 0x28);
    for (char *r = at<char *>(st, 0xd0), *re = at<char *>(st, 0xd8); r && r < re && points_round < points_end;
         r += 0x20, points_round += 0x20) {
        char *ps = at<char *>(points_round, 8), *pse = at<char *>(points_round, 0x10);
        for (char *rs = at<char *>(r, 8), *rse = at<char *>(r, 0x10); rs && rs < rse && ps < pse; rs += 0x28, ps += 0x28) {
            bool scored = false;
            for (char *e = at<char *>(ps, 8), *ee = at<char *>(ps, 0x10); e && e < ee; e += 8)
                scored = scored || at<int32_t>(e, 4) > 0;
            if (!scored)
                continue;
            for (char *e = at<char *>(rs, 8), *ee = at<char *>(rs, 0x10); e && e < ee; e += 0x10) {
                int32_t position = at<int32_t>(e, 4);
                Standing *d = standing_of(out, n, cap, static_cast<uint16_t>(at<uint32_t>(e, 0) >> 16));
                if (d && position >= 0 && position < static_cast<int32_t>(std::size(d->finishes)))
                    ++d->finishes[position];
            }
        }
    }
    std::sort(out, out + n, [](const Standing &a, const Standing &b) {
        if (a.points != b.points)
            return a.points > b.points;
        return std::lexicographical_compare(b.finishes, b.finishes + std::size(b.finishes), a.finishes,
                                            a.finishes + std::size(a.finishes));
    });
    return n;
}

// A driver's F1 entrant row: its team stat id, or kNoTeam, and its race seat (0 or 1, else -1).
struct Seat {
    uint16_t team = kNoTeam;
    int seat = -1;
};

// The F2 2026 teams are entrants.teams artgp_26 .. invicta_26 (489-499).
bool f1_team(uint16_t team)
{
    return team != kNoTeam && (team < 489 || team > 499);
}

// A free agent keeps a team position, so a seat needs an F1 team too.
Seat seat_of(void *entity, uint16_t driver)
{
    for (char *row = at<char *>(entity, 0x2e0), *end = at<char *>(entity, 0x2e8); row && row < end; row += 0x88) {
        if (at<uint16_t>(row, 0x52) != driver)
            continue;
        uint16_t team = at<uint16_t>(row, 0x5a);
        int position = (at<uint16_t>(row, 0x80) ^ 0x100) & 0xff;
        return {team, f1_team(team) && position < 2 ? position : -1};
    }
    return {};
}

// Offers after F2: Team Select refuses a team that offers no seat. Its screen object (vtable 0x145729db0) takes
// every UI message in slot 10 (0x141a69a50: this, name, data): team_selected is the confirm, and +0x564 holds
// the focused team's key (Frida, 7 Nov). A created driver's academy isn't read yet, only kDriverAcademies'.
constexpr uintptr_t kTeamSelectVtable = 0x5729db0;
constexpr size_t kTeamSelectMessageSlot = 10;
constexpr uint32_t kTeamSelectedHash = 0xb2738f54;
constexpr uint16_t kOpenSeatTeams[] = {483, 482};  // Haas, Racing Bulls: the seats kLeavingSeats opens
constexpr size_t kLicencePlaces = 3;               // F2 P1-P3 carry the 40 super licence points
constexpr uint16_t kAcademyTeams[][2] = {{476, 480}, {477, 483}, {478, 482}, {481, 0},
                                         {484, 0},   {479, 0},   {485, 0},   {486, 0}};
// The easiest seat by academy (same order): a partner team, or the academy's own team where it isn't at the
// front. McLaren's is, so its drivers go where a driver with no academy goes: the two weakest teams.
constexpr uint16_t kEasyTeams[][2] = {{480, 0}, {483, 0}, {482, 0}, {481, 0}, {486, 480}, {479, 0}, {485, 0}, {486, 0}};
constexpr uint16_t kNoAcademyEasyTeams[2] = {486, 480};
struct TeamName {
    uint16_t team;
    const char *name;
};
constexpr TeamName kTeamNames[] = {{476, "Mercedes"}, {477, "Ferrari"},      {478, "Red Bull"}, {484, "McLaren"},
                                   {481, "Alpine"},   {479, "Williams"},     {485, "Audi"},     {480, "Aston Martin"},
                                   {482, "Racing Bulls"}, {483, "Haas"},     {486, "Cadillac"}};
// What each team asks of the F2 form score (interest.hpp): the front four, the midfield, the two teams with an
// open seat, and the newest team. A team of the player's academy asks kAcademyDiscount less.
struct TeamAsk {
    uint16_t team;
    int score;
};
constexpr TeamAsk kTeamAsks[] = {{476, 90}, {477, 90}, {478, 90}, {484, 90}, {479, 70}, {480, 70},
                                 {481, 70}, {485, 70}, {482, 50}, {483, 50}, {486, 35}};
constexpr int kAcademyDiscount = 15;
// Super licence points the real F2 2026 drivers bring from 2024 and 2025 (the F2 season is the third of the
// three that count). From the series' final standings; the drivers not listed have none, or weren't checked
// (Bilinski, Fittipaldi Jr., Varrone). TO VERIFY against the FIA's own tables before a release.
struct PriorLicence {
    uint16_t driver;
    int points;
};
constexpr PriorLicence kPriorLicence[] = {
    {203, 55}, // Camara: FRECA 2024 1st, F3 2025 1st
    {202, 34}, // Herta: IndyCar 2024 2nd, 2025 7th
    {174, 25}, // Tsolov: F3 2025 2nd
    {192, 25}, // Mini: F3 2024 2nd
    {204, 20}, // Boya: F3 2025 3rd
    {194, 20}, // Dunne: F2 2025 5th
    {189, 17}, // Beganovic: F3 2024 6th, F2 2025 7th
    {205, 12}, // Stenshorne: F3 2025 5th
    {164, 7},  // Duerksen: F2 2024 10th, 2025 9th
    {191, 7},  // Goethe: F3 2024 7th
    {199, 7},  // Inthraphuvasak: F3 2025 7th
    {200, 2},  // Leon: F3 2024 10th
};

struct Offer {
    size_t place = 0; // the player's F2 place; 0 when he didn't race an F2 season (Skip to F1)
    int academy = -1;
    bool has_form = false; // the career kept his F2 rounds (0.8.3 on)
    int score = 0;         // interest::Form::score
    int licence = 0;       // super licence points: what he brought plus the F2 place's
    int bonus = 0;         // academy targets met: its teams ask this much less
    bool dropped = false;  // three targets missed in a row: academy is -1 from then on
};

// Each academy target met takes this off what the academy's teams ask, up to the cap.
constexpr int kTargetBonus = 2, kTargetBonusMax = 10;

bool academy_team(const Offer &offer, uint16_t team)
{
    bool academy = offer.academy >= 0 && offer.academy < static_cast<int>(std::size(kAcademyTeams));
    return academy && (kAcademyTeams[offer.academy][0] == team || kAcademyTeams[offer.academy][1] == team);
}

bool easy_team(const Offer &offer, uint16_t team)
{
    bool academy = offer.academy >= 0 && offer.academy < static_cast<int>(std::size(kAcademyTeams));
    const uint16_t *easy = academy ? kEasyTeams[offer.academy] : kNoAcademyEasyTeams;
    return easy[0] == team || easy[1] == team;
}

// What the team asks of this player's score; 0 for a team that isn't in kTeamAsks.
int asked_by(const Offer &offer, uint16_t team)
{
    for (const TeamAsk &a : kTeamAsks)
        if (a.team == team)
            return a.score - (academy_team(offer, team) ? kAcademyDiscount + offer.bonus : 0);
    return 0;
}

// A team outside the player's academy counts his score for less, and the front teams (the ones that ask 90)
// for less still: a champion fills his academy's bar and the midfield's, not Mercedes' or McLaren's
// (in-game test, 9 Nov: a title gave every team 100%). A player with no academy has no team behind him, but
// none against him either. Per cent of the score: {midfield and back, front}.
constexpr int kOutsideAcademy[] = {80, 65}, kNoAcademy[] = {90, 75}, kFrontAsk = 90;

int score_for(const Offer &offer, uint16_t team)
{
    if (academy_team(offer, team))
        return offer.score;
    bool front = false;
    for (const TeamAsk &a : kTeamAsks)
        front = front || (a.team == team && a.score >= kFrontAsk);
    return offer.score * (offer.academy >= 0 ? kOutsideAcademy : kNoAcademy)[front ? 1 : 0] / 100;
}

// The team's interest as a percentage: 100 is an offer.
int interest_in(const Offer &offer, uint16_t team)
{
    if (easy_team(offer, team))
        return 100;
    int asked = asked_by(offer, team);
    return asked && offer.has_form ? interest::percent(score_for(offer, team), asked) : 0;
}

// The easy team always offers. The 0.7.8 rule stays: P1-P3 also get the open seats and the academy's teams.
// With the rounds kept, any team offers once the score reaches what it asks and the licence is there.
bool offers_seat(const Offer &offer, uint16_t team)
{
    if (easy_team(offer, team))
        return true;
    if (offer.place && offer.place <= kLicencePlaces &&
        (academy_team(offer, team) ||
         std::find(std::begin(kOpenSeatTeams), std::end(kOpenSeatTeams), team) != std::end(kOpenSeatTeams)))
        return true;
    int asked = asked_by(offer, team);
    return offer.has_form && asked && offer.licence >= interest::kLicenceNeeded && score_for(offer, team) >= asked;
}

void show_refusal(const Offer &offer, uint16_t team)
{
    std::string open;
    const char *refused = "That team";
    for (const TeamName &t : kTeamNames) {
        if (t.team == team)
            refused = t.name;
        if (offers_seat(offer, t.team))
            open += (open.empty() ? "" : ", ") + std::string(t.name);
    }
    std::string body = std::string(refused) + " won't sign you after P" + std::to_string(offer.place) + " in F2";
    if (offer.has_form && offer.licence < interest::kLicenceNeeded)
        body += " (super licence: " + std::to_string(offer.licence) + " of " +
                std::to_string(interest::kLicenceNeeded) + " points)";
    else if (offer.has_form)
        body += " (interest: " + std::to_string(interest_in(offer, team)) + "%)";
    body += ". Teams open to you: " + open + ".";
    swofty::popup::Result r = swofty::popup::show("No seat available", body);
    if (r != swofty::popup::Result::ok)
        swofty::log("[f2-feeder] offers: refusal popup: %s", swofty::popup::name(r));
}

// A driver's academy: the real drivers' table, else what the game's map holds (a created driver's pick). The
// map's slots run from +0x2c8 to +0x2d0; a record only counts if it's one of this career's academies.
int academy_of_driver(void *entity, uint16_t driver)
{
    for (const DriverAcademy &d : kDriverAcademies)
        if (d.driver == driver)
            return d.academy;
    char *comp = static_cast<char *>(swofty::career::component_by_desc(
        reinterpret_cast<char *>(GetModuleHandleW(nullptr)) + kAcademiesDesc));
    if (!comp || at<void *>(comp, 0x278) != entity)
        return -1;
    char *e = at<char *>(comp, 0x2c8), *end = at<char *>(comp, 0x2d0);
    if (!e || end < e || (end - e) % 16 || end - e > 16 * 256)
        return -1;
    for (; e < end; e += 16) {
        char *record = at<char *>(e, 8);
        if (!record || (at<uint16_t>(e, 0) & 0x3fff) != driver)
            continue;
        for (char *a = at<char *>(comp, 0x298), *ae = at<char *>(comp, 0x2a0); a && a < ae; a += 16)
            if (at<char *>(a, 8) == record)
                return at<uint16_t>(record, 0);
    }
    return -1;
}

// The player's F1 season is yet to start at Team Select, so his entrant row can be gone, and a loaded save has
// no F2 results (see kStandingsFile). What on_f2_session kept at the last race answers then.
size_t read_kept_place(uint16_t *player)
{
    std::string text;
    if (!read_kept(text))
        return 0;
    unsigned saved = 0, driver = 0;
    int points = 0, used = 0;
    const char *p = text.c_str();
    if (std::sscanf(p, "%u%n", &saved, &used) != 1)
        return 0;
    *player = static_cast<uint16_t>(saved);
    size_t i = 0;
    for (p += used; std::sscanf(p, "%u %d%n", &driver, &points, &used) == 2; p += used) {
        ++i;
        if (driver == saved)
            return i;
    }
    return 0;
}

// The human's row team, or 0 with no human row.
uint16_t player_team(void *entity)
{
    for (char *row = at<char *>(entity, 0x2e0), *end = at<char *>(entity, 0x2e8); row && row < end; row += 0x88)
        if (at<uint32_t>(row, 0x48) & 0x10000000)
            return at<uint16_t>(row, 0x5a);
    return 0;
}

size_t read_live_table(void *entity, Standing *table, size_t cap)
{
    char *current = at<char *>(entity, 0x1058);
    return current ? read_standings(current, table, cap) : 0;
}

// The player's F2 teammate: the other entrant row of his team; 0 with none.
uint16_t teammate_of(void *entity, uint16_t team)
{
    for (char *row = at<char *>(entity, 0x2e0), *end = at<char *>(entity, 0x2e8); row && row < end; row += 0x88)
        if (!(at<uint32_t>(row, 0x48) & 0x10000000) && at<uint16_t>(row, 0x5a) == team)
            return at<uint16_t>(row, 0x52);
    return 0;
}

// The player's rounds of the current championship's season, from its standings (layout above Standing): the
// result rounds and the points rounds run side by side, session by session.
std::vector<interest::Round> live_rounds(void *entity, uint16_t player, uint16_t teammate)
{
    std::vector<interest::Round> out;
    char *champ = at<char *>(entity, 0x1058);
    char *sb = champ ? at<char *>(champ, 0x548) : nullptr, *se = champ ? at<char *>(champ, 0x550) : nullptr;
    if (!sb || se <= sb || (se - sb) % 16)
        return out;
    char *season = at<char *>(se - 16, 8);
    if (!season)
        return out;
    char *st = season + 0x2a8;
    char *r = at<char *>(st, 0xd0), *re = at<char *>(st, 0xd8), *pr = at<char *>(st, 0x20), *pe = at<char *>(st, 0x28);
    if (!r || re < r || (re - r) % 0x20 || re - r > 0x20 * 64)
        return out;
    for (; r < re; r += 0x20, pr += 0x20) {
        std::vector<interest::Session> sessions;
        bool paid = pr && pr < pe;
        char *ps = paid ? at<char *>(pr, 8) : nullptr, *pse = paid ? at<char *>(pr, 0x10) : nullptr;
        char *rs = at<char *>(r, 8), *rse = at<char *>(r, 0x10);
        if (!rs || rse < rs || (rse - rs) % 0x28 || rse - rs > 0x28 * 16)
            continue;
        for (; rs < rse; rs += 0x28) {
            interest::Session s;
            char *e = at<char *>(rs, 8), *ee = at<char *>(rs, 0x10);
            if (e && ee >= e && (ee - e) % 0x10 == 0 && ee - e <= 0x10 * 64)
                for (; e < ee; e += 0x10)
                    s.results.push_back({static_cast<uint16_t>(at<uint32_t>(e, 0) >> 16), at<int32_t>(e, 4)});
            if (ps && ps < pse) {
                char *p = at<char *>(ps, 8), *pp = at<char *>(ps, 0x10);
                if (p && pp >= p && (pp - p) % 8 == 0 && pp - p <= 8 * 64)
                    for (; p < pp; p += 8)
                        s.points.push_back({static_cast<uint16_t>(at<uint32_t>(p, 0) >> 16), at<int32_t>(p, 4)});
                ps += 0x28;
            }
            sessions.push_back(std::move(s));
        }
        interest::Round round;
        if (interest::summarise(sessions, player, teammate, &round))
            out.push_back(std::move(round));
    }
    return out;
}

// The rounds in the career data: "rounds": [{"q": quali place, "tq": the teammate's, "races": [[place, points,
// teammate's place], ...]}], in the order they ran.
std::vector<interest::Round> kept_rounds(const swofty::json::Value &data)
{
    std::vector<interest::Round> out;
    if (const auto *a = data["rounds"].if_array())
        for (const swofty::json::Value &e : *a) {
            interest::Round r;
            r.quali = static_cast<int>(e["q"].as_int());
            r.teammate_quali = static_cast<int>(e["tq"].as_int());
            r.done = e["d"].as_int() != 0;
            if (const auto *races = e["races"].if_array())
                for (const swofty::json::Value &x : *races)
                    r.races.push_back({static_cast<int>(x[0].as_int()), static_cast<int>(x[1].as_int()),
                                       static_cast<int>(x[2].as_int())});
            out.push_back(std::move(r));
        }
    return out;
}

// At each F2 session end: the game's rounds go into the career data, for the offers at Team Select (a loaded
// save may not hold them any more) and for the hub.
void keep_rounds(void *entity, uint16_t player, uint16_t team)
{
    uint16_t teammate = teammate_of(entity, team);
    std::vector<interest::Round> live = live_rounds(entity, player, teammate);
    swofty::json::Value v = career_data();
    std::vector<interest::Round> kept = kept_rounds(v);
    std::vector<interest::Round> rounds = interest::merge(kept, live);
    interest::Form f = interest::form(rounds, 0);
    swofty::log("[f2-feeder] interest: rounds live %zu kept %zu -> %zu, teammate %u; %d races, %d points, %d wins, "
                "%d podiums, %d poles, ahead of teammate %d of %d, score %d",
                live.size(), kept.size(), rounds.size(), teammate, f.races, f.points, f.wins, f.podiums, f.poles,
                f.ahead, f.compared, f.score);
    interest::Targets t = interest::targets(rounds);
    swofty::log("[f2-feeder] targets: met %d missed %d (in a row %d), last %d, dropped after round %zu; round %zu: %s",
                t.met, t.missed, t.streak, t.last, t.dropped_after, t.next_round,
                t.next_round ? interest::describe(t.next) : "none");
    if (rounds == kept)
        return;
    swofty::json::Value list = swofty::json::Value::array();
    for (const interest::Round &r : rounds) {
        swofty::json::Value e = swofty::json::Value::object(), races = swofty::json::Value::array();
        e["q"] = swofty::json::Value(r.quali);
        e["tq"] = swofty::json::Value(r.teammate_quali);
        e["d"] = swofty::json::Value(r.done ? 1 : 0);
        for (const interest::Race &x : r.races)
            races.push_back(swofty::json::Value::array(
                {swofty::json::Value(x.place), swofty::json::Value(x.points), swofty::json::Value(x.teammate)}));
        e["races"] = std::move(races);
        list.push_back(std::move(e));
    }
    v["rounds"] = std::move(list);
    swofty::career::Result r = swofty::career::saves::data(kAddonId).set_current(v, 1);
    swofty::log("[f2-feeder] interest: rounds kept: career data %s", result_name(r));
}

int prior_licence(uint16_t driver)
{
    for (const PriorLicence &p : kPriorLicence)
        if (p.driver == driver)
            return p.points;
    return 0;
}

// The offer's score and licence from the kept rounds; place and academy are set already. The academy's
// targets count here: met ones ease its teams, three missed in a row and the player has no academy.
interest::Form fill_offer(Offer &offer, uint16_t player, const std::vector<interest::Round> &rounds)
{
    interest::Form f = interest::form(rounds, offer.place);
    if (offer.academy >= 0) {
        interest::Targets t = interest::targets(rounds);
        if (t.dropped_after) {
            offer.dropped = true;
            offer.academy = -1;
        } else {
            offer.bonus = std::min(kTargetBonusMax, t.met * kTargetBonus);
        }
    }
    offer.has_form = offer.place && f.rounds > 0;
    offer.score = f.score;
    offer.licence = interest::licence_points(offer.place) + prior_licence(player);
    return f;
}

// Read at the confirm itself: Team Select comes in the front end, with no career.hub first (0.7.8 tests, 7 Nov).
Offer read_offer()
{
    Offer offer;
    void *entity = swofty::career::entity();
    bool ours = swofty::career::is(kCareers[0].group);
    swofty::career::State state{};
    swofty::career::state(&state);
    uint16_t key = 0, player = 0;
    bool has_row = entity && player_driver(entity, &key, &player);
    uint16_t team = entity ? player_team(entity) : 0;
    Standing table[64];
    size_t n = entity ? read_live_table(entity, table, std::size(table)) : 0;
    swofty::log("[f2-feeder] offers: confirm: ours %d entity %p rows %zu human row %d team %u; core key %s has_player %u "
                "driver %u team %u; championships %zu, live table %zu",
                ours ? 1 : 0, entity, entity ? (at<char *>(entity, 0x2e8) - at<char *>(entity, 0x2e0)) / 0x88 : 0,
                has_row ? 1 : 0, team, state.key, state.has_player, state.player_driver, state.player_team,
                entity ? (at<char *>(entity, 0x1048) - at<char *>(entity, 0x1040)) / 16 : 0, n);
    if (!ours || !entity || (has_row && (team < 489 || team > 499)))
        return offer; // not our career, or a driver already in F1. A Skip to F1 has no kept table: see on_starting
    for (size_t i = 0; has_row && i < n; ++i)
        if (table[i].driver == player)
            offer.place = i + 1;
    if (!offer.place)
        offer.place = read_kept_place(&player);
    offer.academy = academy_of_driver(entity, player);
    std::vector<interest::Round> rounds = kept_rounds(career_data());
    interest::Form f = fill_offer(offer, player, rounds);
    swofty::log("[f2-feeder] offers: player %u is F2 P%zu, academy %d; rounds kept %zu (%d races, %d points, %d wins, "
                "%d poles), score %d, super licence %d; academy targets: bonus %d, dropped %d",
                player, offer.place, offer.academy, rounds.size(), f.races, f.points, f.wins, f.poles, offer.score,
                offer.licence, offer.bonus, offer.dropped ? 1 : 0);
    for (const TeamName &t : kTeamNames)
        swofty::log("[f2-feeder] offers: %s asks %d, interest %d%%, offer %d", t.name, asked_by(offer, t.team),
                    interest_in(offer, t.team), offers_seat(offer, t.team) ? 1 : 0);
    return offer;
}

// Every F2 session end and results screen keeps the table, so the last one holds the final order.
void on_f2_session(const SwoftyEvent *, void *)
{
    void *entity = swofty::career::entity();
    uint16_t key = 0, player = 0;
    if (!known_build() || !swofty::career::is(kCareers[0].group))
        return;
    bool has_row = entity && player_driver(entity, &key, &player);
    uint16_t team = entity ? player_team(entity) : 0;
    Standing table[64];
    size_t n = entity ? read_live_table(entity, table, std::size(table)) : 0, place = 0;
    swofty::log("[f2-feeder] offers: F2 session end: entity %p human row %d team %u live table %zu", entity,
                has_row ? 1 : 0, team, n);
    if (!has_row || team < 489 || team > 499 || !n)
        return;
    std::string text = std::to_string(player) + "\n";
    for (size_t i = 0; i < n; ++i) {
        text += std::to_string(table[i].driver) + " " + std::to_string(table[i].points) + "\n";
        if (table[i].driver == player)
            place = i + 1;
    }
    keep_table(text, "F2 session end");
    swofty::log("[f2-feeder] offers: F2 table kept: player P%zu of %zu, %d points", place, n,
                place ? table[place - 1].points : 0);
    keep_rounds(entity, player, team);
    refresh_panel();
    tell_notices();
}

using TeamSelectMessage = void *(void *screen, const uint32_t *const *name, void *data, void *extra);
TeamSelectMessage *g_team_select_message = nullptr;

void *team_select_message(void *screen, const uint32_t *const *name, void *data, void *extra)
{
    uint16_t team = static_cast<uint16_t>(at<uint32_t>(screen, 0x564) >> 16);
    // The same screen opens Start in F2 with the F2 teams: only an F1 pick is an offer.
    if (name && *name && **name == kTeamSelectedHash && team && (team < 489 || team > 499)) {
        if (Offer offer = read_offer(); offer.place) {
            bool yes = offers_seat(offer, team);
            swofty::log("[f2-feeder] offers: team %u %s (F2 P%zu)", team, yes ? "offers a seat" : "refused", offer.place);
            if (!yes) {
                show_refusal(offer, team);
                return nullptr;
            }
        }
    }
    return g_team_select_message(screen, name, data, extra);
}

// A new career starts with no F2 behind it: an earlier career's table must not answer its Team Select.
void on_starting(const SwoftyEvent *e, void *)
{
    auto *start = static_cast<const swofty::career::Starting *>(e ? e->data : nullptr);
    if (!start || start->kind != SWOFTY_CAREER_START_NEW || wz::group() != kCareers[0].group)
        return;
    {
        std::lock_guard<std::mutex> lock(g_kept_lock);
        g_kept.clear(); // a new career has no data of its own yet
    }
    swofty::storage::Result r = swofty::storage::write(kStandingsFile, "0");
    swofty::log("[f2-feeder] offers: new career, kept F2 table cleared: %d", static_cast<int>(r));
}

void hook_team_select()
{
    auto **slot = reinterpret_cast<TeamSelectMessage **>(reinterpret_cast<char *>(GetModuleHandleW(nullptr)) +
                                                         kTeamSelectVtable) + kTeamSelectMessageSlot;
    DWORD old = 0;
    if (!VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &old))
        return;
    g_team_select_message = *slot;
    *slot = team_select_message;
    VirtualProtect(slot, sizeof *slot, old, &old);
    swofty::log("[f2-feeder] offers: Team Select hooked");
}

// Silly Season's market (aleex0428), as proposed on 9 Nov: each promotion is told to it as a signing with a
// contract to the end of kContractUntil. Looked up with services::when, so it runs only when Silly Season is on.
struct SillySeasonMarket {
    static constexpr const char *name = "silly_season.market";
    static constexpr uint32_t version = 1;
    uint32_t size;
    int (*sign)(uint32_t driver_stat, uint32_t team_stat, int until);
};
constexpr int kContractUntil = 2028; // two seasons: 2027 and 2028
std::atomic<const SillySeasonMarket *> g_market{nullptr};
bool g_market_told = false; // this career's promotions were told to the market (once per loaded career)

std::vector<F2RevampPromotion> read_promotions()
{
    std::vector<F2RevampPromotion> out;
    swofty::json::Value v = career_data();
    if (const auto *a = v["promoted"].if_array())
        for (const swofty::json::Value &e : *a)
            out.push_back({static_cast<uint32_t>(e["driver"].as_int()), static_cast<uint32_t>(e["team"].as_int()),
                           static_cast<uint32_t>(e["replaced"].as_int()), static_cast<uint32_t>(e["place"].as_int())});
    if (v.contains("promoted"))
        return out;
    // A career promoted before 0.8.2 has no record: in its first F1 season, the F2 drivers (not the player) now
    // racing for a team whose seat kLeavingSeats opened are the promoted ones; the replaced driver is unknown.
    void *entity = swofty::career::entity();
    uint16_t key = 0, player = 0;
    char *current = entity ? at<char *>(entity, 0x1058) : nullptr;
    if (!current || at<uint32_t>(current, 0x560) != 0 || !player_driver(entity, &key, &player))
        return out;
    Standing table[64];
    size_t n = f2_standings(entity, player, table, std::size(table));
    for (size_t i = 0; i < n; ++i) {
        Seat s = seat_of(entity, table[i].driver);
        if (table[i].driver == player || s.seat < 0)
            continue;
        if (std::any_of(std::begin(kLeavingSeats), std::end(kLeavingSeats), [&](const LeavingSeat &l) { return l.team == s.team; }))
            out.push_back({table[i].driver, s.team, 0, static_cast<uint32_t>(i + 1)});
    }
    return out;
}

void tell_market(const F2RevampPromotion &p)
{
    const SillySeasonMarket *m = g_market.load();
    if (!m || m->size < offsetof(SillySeasonMarket, sign) + sizeof m->sign || !m->sign)
        return;
    int r = m->sign(p.driver_stat, p.team_stat, kContractUntil);
    swofty::log("[f2-feeder] market: driver %u signed by team %u until %d: %d", p.driver_stat, p.team_stat,
                kContractUntil, r);
}

void record_promotion(const F2RevampPromotion &p)
{
    swofty::json::Value v = career_data();
    if (!v["promoted"].is_array())
        v["promoted"] = swofty::json::Value::array();
    swofty::json::Value e = swofty::json::Value::object();
    e["driver"] = swofty::json::Value(p.driver_stat);
    e["team"] = swofty::json::Value(p.team_stat);
    e["replaced"] = swofty::json::Value(p.replaced_stat);
    e["place"] = swofty::json::Value(p.f2_place);
    v["promoted"].push_back(std::move(e));
    swofty::career::Result r = swofty::career::saves::data(kAddonId).set_current(v, 1);
    swofty::log("[f2-feeder] promotion recorded (driver %u, team %u): career data %s", p.driver_stat, p.team_stat,
                result_name(r));
    tell_market(p);
}

// Once per loaded career, at Team HQ: promotions made earlier (another session, or before Silly Season was on)
// are told again; the market takes a repeat as the same contract.
void tell_market_all()
{
    if (g_market_told || !g_market.load())
        return;
    g_market_told = true;
    for (const F2RevampPromotion &p : read_promotions())
        tell_market(p);
}

void promote_f2_drivers()
{
    void *entity = swofty::career::entity();
    uint16_t player_key = 0, player = 0;
    if (!entity || !player_driver(entity, &player_key, &player))
        return;
    char *current = at<char *>(entity, 0x1058);
    Standing table[64];
    size_t n = f2_standings(entity, player, table, std::size(table));
    if (!current || at<uint32_t>(current, 0x560) != 0)
        return; // promotions happen in the first F1 season only
    if (seat_of(entity, player).seat < 0)
        return; // the player's F1 team isn't signed yet: his pick comes first
    if (!n)
        return; // a Skip to F1 career: no F2 season
    struct Open {
        const LeavingSeat *leaving;
        int seat;
        bool taken;
        int refused = 0; // set_seat refusals of this seat on this visit
    } open[std::size(kLeavingSeats)];
    size_t opens = 0;
    for (const LeavingSeat &l : kLeavingSeats)
        if (Seat s = seat_of(entity, l.driver); s.team == l.team && s.seat >= 0)
            open[opens++] = {&l, s.seat, false, 0};
    if (!opens)
        return; // done on an earlier visit, or the player took the seats
    std::string order;
    for (size_t i = 0; i < n && i < 10; ++i)
        order += (i ? ", " : "") + std::to_string(table[i].driver) + ":" + std::to_string(table[i].points);
    swofty::log("[f2-feeder] promotion: F2 final order (driver:points) %s", order.c_str());
    auto academy_of = [](uint16_t driver) -> int {
        for (const DriverAcademy &d : kDriverAcademies)
            if (d.driver == driver)
                return d.academy;
        return -1;
    };
    // The core's set_seat (swofty::career::set_seat_of_team's request, by stat ids): a free agent takes
    // the seat and its driver leaves the grid.
    const SwoftyCareerOps *ops = swofty::career::detail::live_ops();
    if (!ops || ops->version < 4 || !ops->set_seat) {
        swofty::log("[f2-feeder] promotion: this core has no set_seat; left alone");
        return;
    }
    auto place = [&](const Standing *s, Open &o) -> bool {
        // career::swap_drivers first, in case a driver has a race seat; an F2 driver has none in the F1 season, so
        // the core refuses it (0.8.1 test, 8 Nov) and set_seat does the move.
        if (ops->swap_drivers) {
            int r = ops->swap_drivers(s->driver, o.leaving->driver);
            Seat now = seat_of(entity, s->driver);
            bool seated = r == 0 && now.team == o.leaving->team && now.seat >= 0;
            swofty::log("[f2-feeder] promotion: swap driver %u (F2 %d pts) with %s: result %d, now team %u seat %d%s",
                        s->driver, s->points, o.leaving->name, r, now.team, now.seat, seated ? "" : "; trying set_seat");
            if (seated) {
                o.taken = true;
                return true;
            }
        }
        std::string request = "{\"driver\":" + std::to_string(s->driver) + ",\"team\":" +
                              std::to_string(o.leaving->team) + ",\"seat\":" + std::to_string(o.seat) + "}";
        char report[1024] = {};
        size_t len = 0;
        int r = ops->set_seat({request.data(), request.size()}, report, sizeof report - 1, &len);
        swofty::log("[f2-feeder] promotion: driver %u (F2 %d pts) takes the seat of %s, seat %d: result %d %s",
                    s->driver, s->points, o.leaving->name, o.seat, r, report);
        o.taken = r == 0;
        if (!o.taken) {
            // The core refuses a driver with more than one row in the career's entrant table (Swofty bot, 11 Nov;
            // a player's report had every driver refused that way). Which rows: for the next report.
            std::string rows;
            size_t index = 0;
            for (char *row = at<char *>(entity, 0x2e0), *end = at<char *>(entity, 0x2e8); row && row < end;
                 row += 0x88, ++index)
                if (at<uint16_t>(row, 0x52) == s->driver)
                    rows += " [row " + std::to_string(index) + " team " + std::to_string(at<uint16_t>(row, 0x5a)) +
                            " flags 0x" + [](uint32_t v) { char b[12]; std::snprintf(b, sizeof b, "%x", v); return std::string(b); }(at<uint32_t>(row, 0x48)) + "]";
            swofty::log("[f2-feeder] promotion: driver %u refused; his entrant rows:%s", s->driver,
                        rows.empty() ? " none" : rows.c_str());
        }
        return o.taken;
    };
    // A refused driver (an F1 reserve, say) leaves his seat open for the next one in the table.
    bool tried[std::size(table)] = {};
    size_t promoted = 0;
    for (;;) {
        const Standing *chosen[kPromoted] = {};
        size_t picks = 0, seats = 0;
        for (size_t j = 0; j < opens; ++j)
            seats += !open[j].taken;
        for (size_t i = 0; i < n && picks < seats && promoted + picks < kPromoted; ++i) {
            const Standing &s = table[i];
            if (tried[i] || s.driver == player || seat_of(entity, s.driver).seat >= 0)
                continue; // tried, the player, or a driver already racing in F1
            chosen[picks++] = &s;
        }
        if (!picks)
            break;
        auto attempt = [&](const Standing *s, bool same_academy) {
            for (size_t j = 0; j < opens; ++j) {
                if (open[j].taken || (same_academy && open[j].leaving->academy != academy_of(s->driver)))
                    continue;
                tried[s - table] = true;
                if (!place(s, open[j])) {
                    // A seat that refuses two drivers in a row is the seat's problem (a player's report, 11 Nov:
                    // ten drivers refused for one seat on every visit): leave it for the next Team HQ visit.
                    if (++open[j].refused >= 2) {
                        open[j].taken = true;
                        swofty::log("[f2-feeder] promotion: the seat of %s refused two drivers; left for the next visit",
                                    open[j].leaving->name);
                    }
                    return;
                }
                ++promoted;
                size_t position = s - table;
                record_promotion({s->driver, open[j].leaving->team, open[j].leaving->driver, static_cast<uint32_t>(position + 1)});
                static constexpr uint16_t stats[] = {0, 1, 3, 4};
                const PromotedRatings &ratings = kPromotedRatings[std::min(position, std::size(kPromotedRatings) - 1)];
                int r = ops->set_driver_ratings(s->driver, stats, ratings.values, std::size(stats));
                swofty::log("[f2-feeder] promotion: driver %u ratings (F2 P%zu): result %d", s->driver, position + 1, r);
                if (s->driver == kTsolov) {
                    r = ops->set_car_number(kTsolov, kTsolovNumber, 0);
                    swofty::log("[f2-feeder] promotion: driver %u number %d: result %d", kTsolov, kTsolovNumber, r);
                }
                return;
            }
        };
        for (size_t i = 0; i < picks; ++i)
            attempt(chosen[i], true);
        for (size_t i = 0; i < picks; ++i)
            if (!tried[chosen[i] - table])
                attempt(chosen[i], false);
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
    refresh_panel();
    if (!mine) {
        show_2026("another career");
    }
    else {
        // The season length owner is the schedule component's (+0x290). A career loaded during F2 hasn't
        // been through ApplyPreset when the Custom Season screen opens, so its lists go here too.
        char *schedule = known_build() ? static_cast<char *>(swofty::career::component("k_Career_ScheduleComponentDesc"))
                                       : nullptr;
        if (schedule)
            hide_2026(at<char *>(schedule, 0x290));
        give_real_academy();
        g_notice_tries = tell_notices() ? 0 : 240;
    }
}

// The teammate rivalry (Driver Details > Rivals). The RivalManagerComponent (desc 0x1460631a0) holds one manager
// per driver in a vector at +0xbd8 (16-byte entries, manager at +8; the manager's driver id at +0x7a). A manager's
// rivalries are 16-byte shared pointers at +0x18..+0x20; a rivalry's type is its byte +0x10 (0 teammate, 1
// championship, 2 driver rating) and its vtable slot 3 (+0x18) returns its rival's key ({u8, u8, u16 driver}).
// 0x14124fad0 starts a teammate rivalry only for a manager without one (Game_Rivalry_Start 0x141249ce0, type 0),
// with the teammate's entrant key (row +0x50), so a promotion after the grid is set leaves the released driver
// as the rival (0.8.0). Read from the code, 9 Nov.
constexpr uintptr_t kRivalManagerDesc = 0x60631a0;

void fix_teammate_rival(void *entity)
{
    uint16_t key = 0, player = 0;
    if (!entity || !player_driver(entity, &key, &player))
        return;
    uint16_t team = player_team(entity);
    uint32_t mate_key = 0;
    for (char *row = at<char *>(entity, 0x2e0), *end = at<char *>(entity, 0x2e8); row && row < end; row += 0x88) {
        if (at<uint16_t>(row, 0x5a) != team || at<uint16_t>(row, 0x52) == player)
            continue;
        if (((at<uint16_t>(row, 0x80) ^ 0x100) & 0xff) < 2) {
            mate_key = at<uint32_t>(row, 0x50);
            break;
        }
    }
    uint16_t mate = static_cast<uint16_t>(mate_key >> 16);
    if (!mate_key || !f1_team(team))
        return;
    char *base = reinterpret_cast<char *>(GetModuleHandleW(nullptr));
    char *comp = static_cast<char *>(swofty::career::component_by_desc(base + kRivalManagerDesc));
    if (!comp || at<void *>(comp, 0x278) != entity)
        return;
    for (char *m = at<char *>(comp, 0xbd8), *me = at<char *>(comp, 0xbe0); m && m < me; m += 0x10) {
        char *mgr = at<char *>(m, 8);
        if (!mgr || at<uint16_t>(mgr, 0x7a) != player)
            continue;
        for (char *r = at<char *>(mgr, 0x18), *re = at<char *>(mgr, 0x20); r && r < re; r += 0x10) {
            char *rivalry = at<char *>(r, 0);
            if (!rivalry || at<uint8_t>(rivalry, 0x10) != 0)
                continue;
            using RivalKey = uint32_t *(*)(void *);
            uint32_t *rival = reinterpret_cast<RivalKey>(at<void **>(rivalry, 0)[3])(rivalry);
            if (!rival)
                return;
            uint16_t old = static_cast<uint16_t>(*rival >> 16);
            bool leaving = std::any_of(std::begin(kLeavingSeats), std::end(kLeavingSeats),
                                       [&](const LeavingSeat &l) { return l.driver == old; });
            if (old == mate || !leaving)
                return; // already the teammate, or a rival this addon didn't move
            swofty::log("[f2-feeder] rivals: teammate rival %u (key %08x) -> %u (key %08x)", old, *rival, mate, mate_key);
            *rival = mate_key;
            return;
        }
    }
}

// The f2_revamp.career service (api/f2_revamp_api/career.hpp).
int svc_in_f2()
{
    void *entity = swofty::career::entity();
    uint16_t key = 0, player = 0;
    if (!entity || !swofty::career::is(kCareers[0].group) || !player_driver(entity, &key, &player))
        return 0;
    uint16_t team = player_team(entity);
    return team >= 489 && team <= 499 ? 1 : 0;
}

uint32_t svc_standings(uint32_t *driver_stat, uint32_t cap)
{
    std::string text;
    if (!swofty::career::is(kCareers[0].group) || !read_kept(text))
        return 0;
    unsigned player = 0, driver = 0;
    int points = 0, used = 0;
    const char *p = text.c_str();
    if (std::sscanf(p, "%u%n", &player, &used) != 1 || !player)
        return 0; // "0": a new career's cleared table
    uint32_t n = 0;
    for (p += used; std::sscanf(p, "%u %d%n", &driver, &points, &used) == 2; p += used, ++n)
        if (driver_stat && n < cap)
            driver_stat[n] = driver;
    return n;
}

uint32_t svc_promoted(F2RevampPromotion *out, uint32_t cap)
{
    if (!swofty::career::is(kCareers[0].group))
        return 0;
    std::vector<F2RevampPromotion> all = read_promotions();
    for (size_t i = 0; out && i < all.size() && i < cap; ++i)
        out[i] = all[i];
    return static_cast<uint32_t>(all.size());
}

const F2RevampCareer kCareerService{sizeof(F2RevampCareer), &svc_in_f2, &svc_standings, &svc_promoted};

// The F2 2026 grid, for the panel (entrants.drivers stat ids).
struct DriverName {
    uint16_t driver;
    const char *name;
};
constexpr DriverName kF2Names[] = {
    {203, "Rafael Camara"},     {164, "Joshua Duerksen"},  {166, "Ritomo Miyata"},     {202, "Colton Herta"},
    {200, "Noel Leon"},         {174, "Nikola Tsolov"},    {189, "Dino Beganovic"},    {201, "Roman Bilinski"},
    {192, "Gabriele Mini"},     {191, "Oliver Goethe"},    {193, "Sebastian Montoya"}, {204, "Mari Boya"},
    {205, "Martinius Stenshorne"}, {194, "Alex Dunne"},    {153, "Kush Maini"},        {199, "Tasanapol Inthraphuvasak"},
    {198, "Emerson Fittipaldi Jr."}, {187, "Cian Shields"}, {207, "Nicolas Varrone"},  {167, "Rafael Villagomez"},
    {206, "Laurens van Hoepen"}, {197, "John Bennett"},
};
constexpr const char *kAcademyNames[] = {"Mercedes", "Ferrari", "Red Bull", "Alpine",
                                         "McLaren",  "Williams", "Audi",    "Cadillac"};

std::string driver_name(uint16_t driver, uint16_t player)
{
    for (const DriverName &d : kF2Names)
        if (d.driver == driver)
            return d.name;
    return driver == player ? "You" : "Driver " + std::to_string(driver);
}

const char *team_name(uint32_t team)
{
    for (const TeamName &t : kTeamNames)
        if (t.team == team)
            return t.name;
    return "an F1 team";
}

// The rookie season is the career season the player was first seen in F1 at Team HQ: its index is kept as
// "rookie_season" (whether the F2 season counts as index 0 is not known, so the index itself is kept).
void mark_rookie_season()
{
    if (svc_in_f2())
        return;
    auto pos = swofty::career::current_round();
    if (!pos)
        return;
    swofty::json::Value v = career_data();
    bool kept = v.contains("rookie_season");
    swofty::log("[f2-feeder] rookie: season index %d, round %d of %d; rookie season kept %d (%lld)", pos->season_index,
                pos->round, pos->rounds, kept ? 1 : 0, static_cast<long long>(v["rookie_season"].as_int(-1)));
    if (kept)
        return;
    v["rookie_season"] = swofty::json::Value(pos->season_index);
    swofty::career::Result r = swofty::career::saves::data(kAddonId).set_current(v, 1);
    swofty::log("[f2-feeder] rookie: season %d kept as the rookie season: career data %s", pos->season_index,
                result_name(r));
}

bool in_rookie_season(const swofty::json::Value &v)
{
    auto pos = swofty::career::current_round();
    return pos && v.contains("rookie_season") && v["rookie_season"].as_int(-1) == pos->season_index;
}

// The panel's numbers, from the career data (the kept table and rounds), so it reads the same in F2, at Team
// Select and in F1. Called on the game thread whenever they may have changed.
void refresh_panel()
{
    bool mine = swofty::career::is(kCareers[0].group);
    f2panel::enable(mine && hub_panel.get() && f2panel::start());
    if (!mine)
        return;
    f2panel::Snapshot s;
    void *entity = swofty::career::entity();
    s.in_f2 = svc_in_f2() != 0;
    std::string text;
    unsigned player = 0, driver = 0;
    int points = 0, used = 0;
    if (read_kept(text) && std::sscanf(text.c_str(), "%u%n", &player, &used) == 1) {
        for (const char *p = text.c_str() + used; std::sscanf(p, "%u %d%n", &driver, &points, &used) == 2; p += used) {
            s.table.push_back({driver_name(static_cast<uint16_t>(driver), static_cast<uint16_t>(player)), points,
                               driver == player});
            if (driver == player) {
                s.place = s.table.size();
                s.points = points;
            }
        }
    }
    uint16_t key = 0, stat = 0;
    if (!player && entity && player_driver(entity, &key, &stat))
        player = stat;
    s.player = player ? driver_name(static_cast<uint16_t>(player), static_cast<uint16_t>(player)) : "";
    if (s.place > 1) {
        const f2panel::Row &r = s.table[s.place - 2];
        s.rival = "P" + std::to_string(s.place - 1) + " " + r.name + ", " + std::to_string(r.points - s.points) + " points ahead";
    } else if (s.place == 1 && s.table.size() > 1) {
        const f2panel::Row &r = s.table[1];
        s.rival = "P2 " + r.name + ", " + std::to_string(s.points - r.points) + " points behind";
    }
    Offer offer;
    offer.place = s.place;
    offer.academy = entity && player ? academy_of_driver(entity, static_cast<uint16_t>(player)) : -1;
    int academy = offer.academy;
    std::vector<interest::Round> rounds = kept_rounds(career_data());
    interest::Form f = fill_offer(offer, static_cast<uint16_t>(player), rounds);
    s.has_form = offer.has_form;
    s.rounds = f.rounds, s.races = f.races, s.wins = f.wins, s.podiums = f.podiums, s.poles = f.poles;
    s.ahead = f.ahead, s.compared = f.compared, s.score = f.score;
    s.licence_prior = prior_licence(static_cast<uint16_t>(player));
    s.licence_place = interest::licence_points(s.place);
    s.licence_needed = interest::kLicenceNeeded;
    if (academy >= 0 && academy < static_cast<int>(std::size(kAcademyNames))) {
        std::string name = kAcademyNames[academy];
        interest::Targets t = interest::targets(rounds);
        int judged = t.met + t.missed;
        if (t.dropped_after) {
            s.academy = "none";
            s.academy_lines.push_back(name + " dropped you after round " + std::to_string(t.dropped_after) +
                                      ": three targets missed in a row");
        } else {
            s.academy = name;
            if (judged)
                s.academy += "  |  targets met: " + std::to_string(t.met) + " of " + std::to_string(judged);
            if (t.last >= 0)
                s.academy_lines.push_back(std::string(t.last ? "Last target met: " : "Last target missed: ") +
                                          interest::describe(t.last_goal));
            if (s.in_f2) {
                s.academy_lines.push_back("Target for round " + std::to_string(t.next_round) + ": " +
                                          interest::describe(t.next));
                if (t.streak == interest::kMissesToDrop - 1)
                    s.academy_warning = "Two targets missed in a row: one more and " + name + " drops you";
            }
        }
    }
    if (s.place)
        for (const TeamName &t : kTeamNames)
            s.teams.push_back({t.name, interest_in(offer, t.team), offers_seat(offer, t.team), easy_team(offer, t.team)});
    if (!s.in_f2) {
        std::vector<F2RevampPromotion> moved = read_promotions();
        std::sort(moved.begin(), moved.end(),
                  [](const F2RevampPromotion &a, const F2RevampPromotion &b) { return a.f2_place < b.f2_place; });
        for (const F2RevampPromotion &m : moved)
            s.promoted.push_back(driver_name(static_cast<uint16_t>(m.driver_stat), 0) + " to " + team_name(m.team_stat) +
                                 " (F2 P" + std::to_string(m.f2_place) + ")");
    }
    if (!s.in_f2 && entity && player && in_rookie_season(career_data())) {
        std::vector<uint32_t> rookies{player};
        for (const F2RevampPromotion &m : read_promotions())
            rookies.push_back(m.driver_stat);
        Standing live[64];
        size_t n = read_live_table(entity, live, std::size(live));
        for (size_t i = 0; i < n; ++i)
            if (std::find(rookies.begin(), rookies.end(), live[i].driver) != rookies.end())
                s.rookies.push_back({driver_name(live[i].driver, static_cast<uint16_t>(player)) + "  (P" +
                                         std::to_string(i + 1) + " in F1)",
                                     live[i].points, live[i].driver == player});
        uint16_t team = player_team(entity);
        interest::Form f1 = interest::form(live_rounds(entity, static_cast<uint16_t>(player), teammate_of(entity, team)), 0);
        if (f1.compared)
            s.rookie_line = "Ahead of your teammate in " + std::to_string(f1.ahead) + " of " +
                            std::to_string(f1.compared) + " sessions this season";
    }
    f2panel::set(std::move(s));
}

// What the player should hear once, as the game's message popup (inbox mails can't be kept to this career yet:
// REVAMP.md). In F2: the academy's verdict on the round just judged and the next target; "told" in the career
// data is the number of judged rounds told, plus one. In F1: the F2 season's review, once ("review").
// false: the career isn't ready to tell (no player row yet), try again later. The F2 review waits for Team HQ
// (at_hub), where the promotions are made first, so it can name who moved up.
bool tell_notices(bool at_hub)
{
    if (!notices.get() || !swofty::career::is(kCareers[0].group) || !swofty::popup::available())
        return true;
    void *entity = swofty::career::entity();
    uint16_t key = 0, player = 0;
    if (!entity || !player_driver(entity, &key, &player))
        return false;
    swofty::json::Value v = career_data();
    std::vector<interest::Round> rounds = kept_rounds(v);
    std::string title, body;
    if (svc_in_f2()) {
        int academy = academy_of_driver(entity, player);
        if (academy < 0 || academy >= static_cast<int>(std::size(kAcademyNames)))
            return true;
        interest::Targets t = interest::targets(rounds);
        int judged = t.met + t.missed;
        if (v["told"].as_int() > judged)
            return true;
        std::string name = kAcademyNames[academy];
        title = name + " academy";
        if (t.last >= 0)
            body = std::string(t.last ? "Target met: " : "Target missed: ") + interest::describe(t.last_goal) + ".\n";
        if (t.dropped_after) {
            body += "That is three targets missed in a row. " + name + " has dropped you from its academy.";
        } else {
            if (t.streak == interest::kMissesToDrop - 1)
                body += "Two missed in a row: one more and you are out of the academy.\n";
            body += "Target for round " + std::to_string(t.next_round) + ": " + interest::describe(t.next) + ".";
        }
        v["told"] = swofty::json::Value(judged + 1);
    } else {
        if (!at_hub || v["review"].as_int() || v["table"].as_string().empty())
            return true;
        uint16_t kept_player = 0;
        size_t place = read_kept_place(&kept_player);
        if (!place)
            return true;
        interest::Form f = interest::form(rounds, place);
        title = "Your F2 season";
        body = "You finished P" + std::to_string(place) + " in Formula 2";
        if (f.rounds)
            body += ": " + std::to_string(f.points) + " points, wins " + std::to_string(f.wins) +
                    ", other podiums " + std::to_string(f.podiums) + ", poles " + std::to_string(f.poles);
        body += ".";
        std::string moved;
        for (const F2RevampPromotion &m : read_promotions())
            moved += (moved.empty() ? "" : ", ") + driver_name(static_cast<uint16_t>(m.driver_stat), 0) + " (" +
                     team_name(m.team_stat) + ")";
        if (!moved.empty())
            body += "\nMoved up to F1 with you: " + moved + ".";
        v["review"] = swofty::json::Value(1);
    }
    swofty::popup::Result shown = swofty::popup::show(title, body);
    if (shown != swofty::popup::Result::ok) {
        swofty::log("[f2-feeder] notice: no popup (%s): %s", swofty::popup::name(shown), title.c_str());
        return true;
    }
    swofty::career::Result r = swofty::career::saves::data(kAddonId).set_current(v, 1);
    swofty::log("[f2-feeder] notice: %s: %s (career data %s)", title.c_str(), body.c_str(), result_name(r));
    return true;
}

// F8, Esc and B open and close the panel in the paint callback; the hub tab's row follows here, on the game
// thread, so its next change does what it says.
void panel_tick(double, void *)
{
    static int wait = 0;
    if (++wait < 30)
        return;
    wait = 0;
    // a new career goes straight to its first session: the notice career.loaded couldn't tell, and the panel
    if (g_notice_tries > 0) {
        bool told = tell_notices();
        int left = told ? 0 : g_notice_tries - 1;
        g_notice_tries = left;
        if (told)
            refresh_panel();
        if (told || !left)
            swofty::log("[f2-feeder] notice: after career loaded: %s", told ? "career ready" : "gave up waiting");
    }
    bool open = f2panel::g_open;
    if (swofty::hub::available() && (panel_open.get() == "open") != open)
        swofty::hub::set_option(kAddonId, "panel_open", open ? "open" : "closed");
}

void on_hub(const SwoftyEvent *e, void *user)
{
    on_career(e, user);
    if (swofty::career::is(kCareers[0].group) && known_build()) {
        promote_f2_drivers();
        fix_teammate_rival(swofty::career::entity());
        tell_market_all();
        mark_rookie_season();
        tell_notices(true);
        refresh_panel();
    }
}

void on_unloaded(const SwoftyEvent *, void *)
{
    set_titles(false, "career unloaded");
    g_market_told = false;
    g_notice_tries = 0;
    f2panel::enable(false);
    show_2026("career unloaded");
}

bool register_tiles()
{
    if (known_build()) {
        hook_team_select();
    }
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
    swofty::log("[f2-feeder] panel: overlay %s, frame tick %s", f2panel::start() ? "ok" : "not available",
                swofty::events::name(swofty::events::on_frame(panel_tick)));
    swofty::log("[f2-feeder] subscribe %s: %s", SWOFTY_EVENT_CAREER_LOADED,
                swofty::events::name(swofty::events::subscribe(SWOFTY_EVENT_CAREER_LOADED, on_career)));
    swofty::log("[f2-feeder] subscribe %s: %s", SWOFTY_EVENT_CAREER_HUB,
                swofty::events::name(swofty::events::subscribe(SWOFTY_EVENT_CAREER_HUB, on_hub)));
    swofty::log("[f2-feeder] subscribe %s: %s", SWOFTY_EVENT_CAREER_STARTING,
                swofty::events::name(swofty::events::subscribe(SWOFTY_EVENT_CAREER_STARTING, on_starting)));
    swofty::log("[f2-feeder] subscribe %s: %s", SWOFTY_EVENT_SESSION_END,
                swofty::events::name(swofty::events::subscribe(SWOFTY_EVENT_SESSION_END, on_f2_session)));
    swofty::log("[f2-feeder] subscribe %s: %s", SWOFTY_EVENT_RESULTS,
                swofty::events::name(swofty::events::subscribe(SWOFTY_EVENT_RESULTS, on_f2_session)));
    swofty::log("[f2-feeder] subscribe %s: %s", SWOFTY_EVENT_CAREER_UNLOADED,
                swofty::events::name(swofty::events::subscribe(SWOFTY_EVENT_CAREER_UNLOADED, on_unloaded)));
    swofty::log("[f2-feeder] service %s v%u: %s", F2RevampCareer::name, F2RevampCareer::version,
                swofty::services::name(swofty::services::publish(kCareerService)));
    swofty::log("[f2-feeder] lookup %s: %s", SillySeasonMarket::name,
                swofty::services::name(swofty::services::when<SillySeasonMarket>([](const SillySeasonMarket &m) {
                    g_market = &m;
                    swofty::log("[f2-feeder] market: %s found", SillySeasonMarket::name);
                })));
    return true;
}

} // namespace

// The hub tab's row opens and closes the panel.
SWOFTY_ON_CHANGE(panel_open, std::string_view v)
{
    f2panel::open(v == "open");
}

SWOFTY_ON_CHANGE(hub_panel, bool)
{
    refresh_panel();
}

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
    if (component && known_build() && (group.empty() ? g_wizard_ours : ours(group.c_str())))
        hide_2026(*reinterpret_cast<char **>(static_cast<char *>(component) + 0x290));
    else
        show_2026("another career's season length");
    orig(component, preset);
}

SWOFTY_ADDON(register_tiles)
