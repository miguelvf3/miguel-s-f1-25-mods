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
#include <swofty/lang.hpp>
#include <swofty/mixin.hpp>
#include <swofty/popup.hpp>
#include <swofty/storage.hpp>
#include <swofty/systems/career.hpp>
#include <swofty/systems/ops.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
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

void keep_table(const std::string &text, const char *why)
{
    {
        std::lock_guard<std::mutex> lock(g_kept_lock);
        g_kept = text;
    }
    swofty::storage::write(kStandingsFile, text);
    swofty::json::Value v = swofty::json::Value::object();
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
struct Offer {
    size_t place = 0; // the player's F2 place; 0 when he didn't race an F2 season (Skip to F1)
    int academy = -1;
};

bool offers_seat(const Offer &offer, uint16_t team)
{
    bool academy = offer.academy >= 0 && offer.academy < static_cast<int>(std::size(kAcademyTeams));
    const uint16_t *easy = academy ? kEasyTeams[offer.academy] : kNoAcademyEasyTeams;
    if (easy[0] == team || easy[1] == team)
        return true;
    if (offer.place > kLicencePlaces)
        return false;
    if (std::find(std::begin(kOpenSeatTeams), std::end(kOpenSeatTeams), team) != std::end(kOpenSeatTeams))
        return true;
    return academy && (kAcademyTeams[offer.academy][0] == team || kAcademyTeams[offer.academy][1] == team);
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
    std::string body = std::string(refused) + " won't sign you after P" + std::to_string(offer.place) +
                       " in F2. Teams open to you: " + open + ".";
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
    swofty::log("[f2-feeder] offers: player %u is F2 P%zu, academy %d", player, offer.place, offer.academy);
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
    } open[std::size(kLeavingSeats)];
    size_t opens = 0;
    for (const LeavingSeat &l : kLeavingSeats)
        if (Seat s = seat_of(entity, l.driver); s.team == l.team && s.seat >= 0)
            open[opens++] = {&l, s.seat, false};
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
                if (!place(s, open[j]))
                    return;
                ++promoted;
                static constexpr uint16_t stats[] = {0, 1, 3, 4};
                size_t position = s - table;
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

void on_hub(const SwoftyEvent *e, void *user)
{
    on_career(e, user);
    if (swofty::career::is(kCareers[0].group) && known_build()) {
        promote_f2_drivers();
        fix_teammate_rival(swofty::career::entity());
    }
}

void on_unloaded(const SwoftyEvent *, void *)
{
    set_titles(false, "career unloaded");
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
    if (component && known_build() && (group.empty() ? g_wizard_ours : ours(group.c_str())))
        hide_2026(*reinterpret_cast<char **>(static_cast<char *>(component) + 0x290));
    else
        show_2026("another career's season length");
    orig(component, preset);
}

SWOFTY_ADDON(register_tiles)
