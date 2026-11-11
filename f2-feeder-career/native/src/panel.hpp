// SPDX-License-Identifier: AGPL-3.0-or-later
//
// The F2 Revamp career panel: a full-screen overlay (swofty/overlay.hpp, experimental) with the F2 table,
// the player's season, his super licence, the title rival and each F1 team's interest. The plugin fills a
// Snapshot on the game thread; the paint callback draws the last one on the render thread. F8 opens and
// closes it, and so does the hub tab's row (open()). Nothing is drawn while it is closed.
#pragma once
#include <swofty/overlay.hpp>
#include <swofty/overlay_ui.hpp>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

namespace f2panel {

namespace ov = swofty::overlay;
namespace ui = swofty::overlay::ui;

struct Row {
    std::string name;
    int points = 0;
    bool player = false;
};

struct Team {
    std::string name;
    int percent = 0; // 100 is an offer by the score alone
    bool offer = false;
    bool easy = false;
};

struct Snapshot {
    bool in_f2 = false;       // the F2 season is still running: the numbers are "as it stands"
    std::vector<Row> table;   // best first
    size_t place = 0;         // the player's, 1-based; 0 unknown
    std::string player, rival; // rival: "P2 Nikola Tsolov, 7 points behind"
    int rounds = 0, races = 0, points = 0, wins = 0, podiums = 0, poles = 0, ahead = 0, compared = 0, score = 0;
    bool has_form = false;
    int licence_prior = 0, licence_place = 0, licence_needed = 40;
    std::string academy; // "Ferrari  |  targets met: 3 of 5", or empty
    std::vector<std::string> academy_lines; // the last target, the next one, or the drop
    std::string academy_warning;            // two targets missed in a row
    std::vector<Team> teams;
    std::vector<std::string> promoted; // "Nikola Tsolov to Racing Bulls (F2 P2)"
    std::vector<Row> rookies;          // the first F1 season: the player and the promoted drivers, by F1 points
    std::string rookie_line;           // "Ahead of your teammate in 5 of 9 sessions this season"
};

inline std::mutex g_lock;
inline Snapshot g_snapshot;
inline std::atomic<bool> g_enabled{false}, g_open{false}, g_focused{false}, g_registered{false};
inline ui::State g_state;

inline void set(Snapshot s)
{
    std::lock_guard<std::mutex> lock(g_lock);
    g_snapshot = std::move(s);
}

// enabled: one of our careers is loaded and the hub option is on. Turning it off closes the panel.
inline void enable(bool on)
{
    g_enabled = on;
    if (!on)
        g_open = false;
}

inline void open(bool on)
{
    g_open = on && g_enabled;
}

constexpr uint32_t kDim = 0xffffffa8, kText = 0xffffffff, kAccent = 0xff4d4dff, kGood = 0x3ddc84ff,
                   kTrack = 0xffffff2a, kPlayer = 0xe1060038, kShade = 0x000000a0;

inline void heading(ui::Ui &u, float x, float y, float w, const char *text)
{
    ui::label(u, {x, y, w, u.u(26)}, text, kDim);
    ov::rect(u.p, x, y + u.u(28), w, u.u(2), kAccent);
}

inline void bar(ui::Ui &u, float x, float y, float w, float h, int percent, uint32_t rgba)
{
    ov::rect_rounded(u.p, x, y, w, h, h / 2, kTrack);
    float filled = w * static_cast<float>(percent < 0 ? 0 : percent > 100 ? 100 : percent) / 100.0f;
    if (filled >= h)
        ov::rect_rounded(u.p, x, y, filled, h, h / 2, rgba);
}

inline void paint(SwoftyPaint *p, void *)
{
    if (!g_enabled && !g_open && !g_focused)
        return;
    ui::Ui u(p, g_state);
    if (g_enabled && u.in.key_pressed(VK_F8))
        g_open = !g_open.load();
    if (g_open && (u.back || !g_enabled))
        g_open = false;
    bool open = g_open;
    if (open != g_focused.load() && ov::v3()) {
        ov::focus(open);
        g_focused = open;
    }
    if (!open)
        return;

    Snapshot s;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        s = g_snapshot;
    }
    float fw = 0, fh = 0;
    ov::size(p, &fw, &fh);
    ov::rect(p, 0, 0, fw, fh, kShade);
    float w = u.u(1500), h = u.u(900);
    ov::Rect c = ui::panel(u, {(fw - w) / 2, (fh - h) / 2, w, h}, s.in_f2 ? "F2 REVAMP  |  FORMULA 2 SEASON" : "F2 REVAMP  |  YOUR F2 SEASON");
    char line[160];

    // left: the table
    float lw = u.u(520), row = u.u(31), y = c.y;
    heading(u, c.x, y, lw, s.in_f2 ? "STANDINGS" : "FINAL STANDINGS");
    y += u.u(38);
    if (s.table.empty())
        ui::label(u, {c.x, y, lw, row}, "No F2 results yet.", kDim);
    for (size_t i = 0; i < s.table.size() && i < 24; ++i, y += row) {
        const Row &r = s.table[i];
        if (r.player)
            ov::rect_rounded(p, c.x, y + u.u(1), lw, row - u.u(2), u.u(6), kPlayer);
        std::snprintf(line, sizeof line, "%zu", i + 1);
        ui::label(u, {c.x + u.u(8), y, u.u(40), row}, line, kDim);
        ui::label(u, {c.x + u.u(56), y, lw - u.u(140), row}, r.name, kText);
        std::snprintf(line, sizeof line, "%d", r.points);
        ui::label(u, {c.x + lw - u.u(90), y, u.u(80), row}, line, kText, ov::align::right | ov::align::middle);
    }

    // right: the player's season
    float x = c.x + lw + u.u(50), rw = c.w - lw - u.u(50);
    y = c.y;
    heading(u, x, y, rw, "YOUR SEASON");
    y += u.u(40);
    if (s.place)
        std::snprintf(line, sizeof line, "%s  |  P%zu  |  %d points", s.player.c_str(), s.place, s.points);
    else
        std::snprintf(line, sizeof line, "%s", s.player.empty() ? "No F2 season in this career." : s.player.c_str());
    ui::label(u, {x, y, rw, row}, line, kText);
    y += row;
    if (s.has_form) {
        std::snprintf(line, sizeof line, "Rounds %d, races %d  |  Wins %d  |  Other podiums %d  |  Poles %d", s.rounds,
                      s.races, s.wins, s.podiums, s.poles);
        ui::label(u, {x, y, rw, row}, line, kDim);
        y += row;
        std::snprintf(line, sizeof line, "Ahead of your teammate in %d of %d sessions  |  Form score %d of 100", s.ahead,
                      s.compared, s.score);
        ui::label(u, {x, y, rw, row}, line, kDim);
        y += row;
    }
    if (!s.rival.empty()) {
        std::snprintf(line, sizeof line, "Title rival: %s", s.rival.c_str());
        ui::label(u, {x, y, rw, row}, line, kDim);
        y += row;
    }
    if (!s.academy.empty()) {
        std::snprintf(line, sizeof line, "Academy: %s", s.academy.c_str());
        ui::label(u, {x, y, rw, row}, line, kDim);
        y += row;
    }
    for (const std::string &l : s.academy_lines) {
        ui::label(u, {x, y, rw, row}, l, kDim);
        y += row;
    }
    if (!s.academy_warning.empty()) {
        ui::label(u, {x, y, rw, row}, s.academy_warning, kAccent);
        y += row;
    }

    y += u.u(14);
    heading(u, x, y, rw, "SUPER LICENCE");
    y += u.u(40);
    int licence = s.licence_prior + s.licence_place;
    if (s.place)
        std::snprintf(line, sizeof line, "%d points, %d needed  (%d from earlier seasons, %d for P%zu in F2)", licence,
                      s.licence_needed, s.licence_prior, s.licence_place, s.place);
    else
        std::snprintf(line, sizeof line, "%d points, %d needed  (from earlier seasons; none yet from F2)", licence,
                      s.licence_needed);
    ui::label(u, {x, y, rw, row}, line, licence >= s.licence_needed ? kGood : kText);
    y += row + u.u(2);
    bar(u, x, y, rw, u.u(12), s.licence_needed ? licence * 100 / s.licence_needed : 0,
        licence >= s.licence_needed ? kGood : kAccent);
    y += u.u(30);

    // the rookie season shows the rookies' table in place of the teams' interest, which is settled by then
    if (!s.rookies.empty()) {
        heading(u, x, y, rw, "ROOKIE STANDINGS (YOUR FIRST F1 SEASON)");
        y += u.u(40);
        for (size_t i = 0; i < s.rookies.size() && i < 8; ++i, y += row) {
            const Row &r = s.rookies[i];
            if (r.player)
                ov::rect_rounded(p, x, y + u.u(1), rw, row - u.u(2), u.u(6), kPlayer);
            std::snprintf(line, sizeof line, "%zu", i + 1);
            ui::label(u, {x + u.u(8), y, u.u(40), row}, line, kDim);
            ui::label(u, {x + u.u(56), y, rw - u.u(160), row}, r.name, kText);
            std::snprintf(line, sizeof line, "%d", r.points);
            ui::label(u, {x + rw - u.u(90), y, u.u(80), row}, line, kText, ov::align::right | ov::align::middle);
        }
        if (!s.rookie_line.empty()) {
            ui::label(u, {x, y, rw, row}, s.rookie_line, kDim);
            y += row;
        }
        y += u.u(10);
    }
    float name_w = u.u(190), tag_w = u.u(190), tr = u.u(30);
    if (s.rookies.empty() && !s.teams.empty()) {
        heading(u, x, y, rw, s.in_f2 ? "F1 TEAM INTEREST (IF THE SEASON ENDED NOW)" : "F1 TEAM INTEREST");
        y += u.u(40);
    }
    for (const Team &t : s.rookies.empty() ? s.teams : std::vector<Team>{}) {
        ui::label(u, {x, y, name_w, tr}, t.name, kText);
        bar(u, x + name_w, y + u.u(9), rw - name_w - tag_w - u.u(16), u.u(12), t.offer ? 100 : t.percent,
            t.offer ? kGood : kAccent);
        if (t.easy)
            std::snprintf(line, sizeof line, "Always open");
        else if (t.offer && t.percent < 100)
            std::snprintf(line, sizeof line, "Offer"); // by the table place alone (P1 to P3)
        else if (t.offer)
            std::snprintf(line, sizeof line, "%d%%  Offer", t.percent);
        else if (t.percent >= 100)
            std::snprintf(line, sizeof line, "Needs the licence");
        else
            std::snprintf(line, sizeof line, "%d%%", t.percent);
        ui::label(u, {x + rw - tag_w, y, tag_w, tr}, line, t.offer || t.easy ? kGood : kDim, ov::align::right | ov::align::middle);
        y += tr;
    }

    if (!s.promoted.empty()) {
        y += u.u(10);
        heading(u, x, y, rw, "MOVED UP TO F1 WITH YOU");
        y += u.u(40);
        for (const std::string &m : s.promoted) {
            ui::label(u, {x, y, rw, row}, m, kDim);
            y += row;
        }
    }

    ui::label(u, {c.x, c.y + c.h - u.u(30), c.w, u.u(30)}, "F8, Esc or B closes this panel", kDim,
              ov::align::right | ov::align::middle);
}

// Once: registers the paint callback. false when the overlay isn't there (feature off, older core).
inline bool start()
{
    if (g_registered)
        return true;
    if (!ov::available() || ov::on_paint(paint) != ov::Result::ok)
        return false;
    g_registered = true;
    return true;
}

}
