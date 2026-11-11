// SPDX-License-Identifier: MIT
//
// How much the F1 teams want the player after his F2 season: his rounds, a form score from them, the
// super licence points, and what each team asks. No game types here, so native/test/interest_test.cpp
// runs it off the game.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>

namespace interest {

// Places are 1-based; 0 is unknown (didn't take part, or no teammate).
struct Race {
    int place = 0, points = 0, teammate = 0;
};

struct Round {
    int quali = 0, teammate_quali = 0;
    std::vector<Race> races;
    bool done = false; // its feature race is run (a round may be the feature race alone)

    bool operator==(const Round &o) const
    {
        if (done != o.done || quali != o.quali || teammate_quali != o.teammate_quali || races.size() != o.races.size())
            return false;
        for (size_t i = 0; i < races.size(); ++i)
            if (races[i].place != o.races[i].place || races[i].points != o.races[i].points ||
                races[i].teammate != o.races[i].teammate)
                return false;
        return true;
    }
};

// One session of a round as the game's standings keep it: every driver's 0-based position, and the
// points it paid.
struct Session {
    std::vector<std::pair<uint16_t, int>> results, points;
};

// A session is a race when it paid a win's points (sprint 10, feature 25); pole pays 2.
constexpr int kRacePoints = 8, kFeaturePoints = 20;

inline int place_in(const Session &s, uint16_t driver)
{
    for (const auto &[d, position] : s.results)
        if (d == driver)
            return position + 1;
    return 0;
}

inline int points_in(const Session &s, uint16_t driver)
{
    int n = 0;
    for (const auto &[d, points] : s.points)
        if (d == driver)
            n += points;
    return n;
}

// The player's round from its sessions, in the order they ran. Qualifying is the last session with
// results before the first race (practice comes before it); its pole points count with the first race.
// false when the player has no result in the round yet.
inline bool summarise(const std::vector<Session> &sessions, uint16_t player, uint16_t teammate, Round *out)
{
    Round r;
    int carried = 0;
    bool seen = false;
    for (const Session &s : sessions) {
        int best = 0;
        for (const auto &[d, points] : s.points)
            best = std::max(best, points);
        int place = place_in(s, player);
        seen = seen || place > 0;
        if (best >= kRacePoints) {
            r.races.push_back({place, points_in(s, player) + carried, teammate ? place_in(s, teammate) : 0});
            carried = 0;
            r.done = r.done || best >= kFeaturePoints;
        } else if (r.races.empty() && !s.results.empty()) {
            r.quali = place;
            r.teammate_quali = teammate ? place_in(s, teammate) : 0;
            carried = points_in(s, player);
        }
    }
    if (!seen)
        return false;
    *out = std::move(r);
    return true;
}

// b is a's round with the same or more sessions run.
inline bool continues(const Round &a, const Round &b)
{
    if (a.quali != b.quali || a.races.size() > b.races.size())
        return false;
    for (size_t i = 0; i < a.races.size(); ++i)
        if (a.races[i].place != b.races[i].place || a.races[i].points != b.races[i].points)
            return false;
    return true;
}

// The rounds to keep, from the ones kept so far and the ones the game holds now. The game's list is the
// whole season in the normal case. If it is shorter (a loaded save that lost the earlier rounds), its
// rounds are the latest ones: its last round is the kept last round further on, or a new one.
inline std::vector<Round> merge(std::vector<Round> kept, const std::vector<Round> &live)
{
    if (live.size() >= kept.size())
        return live;
    if (live.empty())
        return kept;
    size_t offset = kept.size() - live.size();
    if (!continues(kept.back(), live.back()))
        ++offset;
    kept.resize(offset + live.size());
    std::copy(live.begin(), live.end(), kept.begin() + static_cast<std::ptrdiff_t>(offset));
    return kept;
}

struct Form {
    int rounds = 0, races = 0, points = 0, wins = 0, podiums = 0, poles = 0, ahead = 0, compared = 0;
    int score = 0; // 0 to 100
};

// The score: points per round (15 a round, a champion's pace, gives 80), wins, other podiums and poles,
// the share of sessions ahead of the teammate, and the final place in the table (0 while the season runs).
inline Form form(const std::vector<Round> &rounds, size_t final_place)
{
    Form f;
    for (const Round &r : rounds) {
        if (r.races.empty())
            continue;
        ++f.rounds;
        f.poles += r.quali == 1;
        if (r.quali > 0 && r.teammate_quali > 0) {
            ++f.compared;
            f.ahead += r.quali < r.teammate_quali;
        }
        for (const Race &race : r.races) {
            ++f.races;
            f.points += race.points;
            f.wins += race.place == 1;
            f.podiums += race.place == 2 || race.place == 3;
            if (race.place > 0 && race.teammate > 0) {
                ++f.compared;
                f.ahead += race.place < race.teammate;
            }
        }
    }
    if (!f.rounds)
        return f;
    int score = std::min(80, f.points * 80 / (f.rounds * 15));
    score += std::min(12, f.wins * 3) + std::min(6, f.podiums) + std::min(6, f.poles * 2);
    if (f.compared)
        score += (f.ahead * 20 - f.compared * 10) / f.compared; // -10 to +10
    score += final_place == 1 ? 10 : final_place == 2 ? 7 : final_place == 3 ? 5 : 0;
    f.score = std::clamp(score, 0, 100);
    return f;
}

// The academy's target for one round. Nothing of it is stored: the target of round i comes from the rounds
// before it, so a reloaded save reads the same season the same way.
enum Kind { kQuali, kPoints, kFeature, kTeammate, kKinds };
struct Goal {
    int kind = kQuali, level = 0; // level 0 to 2: harder as the player scores more
};

constexpr const char *kGoalText[kKinds][3] = {
    {"Qualify in the top 12", "Qualify in the top 8", "Qualify in the top 4"},
    {"Score points this round", "Score points in every race of the round", "Score 15 points or more in the round"},
    {"Finish the feature race in the top 10", "Finish the feature race in the top 6",
     "Finish the feature race on the podium"},
    {"Beat your teammate in the feature race", "Beat your teammate in every race of the round",
     "Beat your teammate in qualifying and in every race"},
};

inline const char *describe(const Goal &g)
{
    return kGoalText[g.kind % kKinds][std::clamp(g.level, 0, 2)];
}

// A round counts once its feature race is run, or a later round exists.
inline bool judged(const std::vector<Round> &rounds, size_t i)
{
    return i < rounds.size() && (rounds[i].done || i + 1 < rounds.size());
}

// The kind turns with the round; the level follows the points per round so far (5 a round: medium, 12: hard).
// With no teammate seen in the rounds before, the teammate target gives way to the points one.
inline Goal goal_for(const std::vector<Round> &rounds, size_t index)
{
    int points = 0, n = 0;
    bool teammate = index == 0;
    for (size_t i = 0; i < index && i < rounds.size(); ++i) {
        ++n;
        teammate = teammate || rounds[i].teammate_quali > 0;
        for (const Race &race : rounds[i].races) {
            points += race.points;
            teammate = teammate || race.teammate > 0;
        }
    }
    Goal g;
    g.kind = static_cast<int>(index % kKinds);
    if (g.kind == kTeammate && !teammate)
        g.kind = kPoints;
    g.level = !n ? 0 : points >= n * 12 ? 2 : points >= n * 5 ? 1 : 0;
    return g;
}

inline bool met(const Goal &g, const Round &r)
{
    constexpr int kQualiTop[] = {12, 8, 4}, kFeatureTop[] = {10, 6, 3};
    int level = std::clamp(g.level, 0, 2), total = 0;
    bool every = !r.races.empty(), ahead_every = !r.races.empty();
    auto ahead = [](int place, int teammate) { return place > 0 && (teammate == 0 || place < teammate); };
    for (const Race &race : r.races) {
        total += race.points;
        every = every && race.points > 0;
        ahead_every = ahead_every && ahead(race.place, race.teammate);
    }
    switch (g.kind) {
    case kQuali:
        return r.quali > 0 && r.quali <= kQualiTop[level];
    case kPoints:
        return level == 0 ? total > 0 : level == 1 ? every : total >= 15;
    case kFeature:
        return !r.races.empty() && r.races.back().place > 0 && r.races.back().place <= kFeatureTop[level];
    default:
        if (level == 0)
            return !r.races.empty() && ahead(r.races.back().place, r.races.back().teammate);
        return ahead_every && (level == 1 || ahead(r.quali, r.teammate_quali));
    }
}

// The season's targets so far. Three misses in a row and the academy drops the player, for good.
constexpr int kMissesToDrop = 3;
struct Targets {
    int met = 0, missed = 0, streak = 0; // streak: misses in a row up to now
    int last = -1;                       // the last judged round: 1 met, 0 missed, -1 none yet
    Goal last_goal;
    size_t dropped_after = 0; // the round (1-based) that got the player dropped; 0: still in
    size_t next_round = 0;    // the round (1-based) the next target is for; 0 once dropped
    Goal next;
};

inline Targets targets(const std::vector<Round> &rounds)
{
    Targets t;
    size_t i = 0;
    for (; judged(rounds, i); ++i) {
        t.last_goal = goal_for(rounds, i);
        t.last = met(t.last_goal, rounds[i]) ? 1 : 0;
        t.last ? ++t.met : ++t.missed;
        t.streak = t.last ? 0 : t.streak + 1;
        if (t.streak >= kMissesToDrop) {
            t.dropped_after = i + 1;
            return t;
        }
    }
    t.next_round = i + 1;
    t.next = goal_for(rounds, i);
    return t;
}

// FIA super licence points for the final F2 place (Appendix L, Supplement 1): 40 are needed.
constexpr int kLicenceNeeded = 40;
inline int licence_points(size_t place)
{
    constexpr int kPoints[] = {40, 40, 40, 30, 20, 10, 8, 6, 4, 3};
    return place >= 1 && place <= std::size(kPoints) ? kPoints[place - 1] : 0;
}

// A team offers a seat when the score reaches what it asks, as a percentage (100 = an offer).
inline int percent(int score, int asked)
{
    return asked <= 0 ? 100 : std::min(100, score * 100 / asked);
}

}
