// SPDX-License-Identifier: MIT
// Off-game checks of native/src/interest.hpp. Build and run: bash native/test/run.sh
#include "../src/interest.hpp"

#include <cstdio>
#include <string_view>

using namespace interest;

static int g_failed = 0;
#define CHECK(x)                                                    \
    do {                                                            \
        if (!(x)) {                                                 \
            std::printf("FAIL line %d: %s\n", __LINE__, #x);        \
            ++g_failed;                                             \
        }                                                           \
    } while (0)

// A session where the drivers finish in the given order (0-based positions) and the first ones score.
static Session session(std::initializer_list<uint16_t> order, std::initializer_list<int> points)
{
    Session s;
    int position = 0;
    for (uint16_t d : order)
        s.results.push_back({d, position++});
    auto d = order.begin();
    for (int p : points)
        s.points.push_back({*d++, p});
    return s;
}

int main()
{
    const uint16_t me = 203, mate = 164, other = 174;

    // practice (no points), qualifying (pole pays 2), sprint, feature
    std::vector<Session> weekend = {
        session({other, me, mate}, {}),
        session({me, other, mate}, {2}),
        session({other, mate, me}, {10, 8, 6}),
        session({me, other, mate}, {25, 18, 15}),
    };
    Round r;
    CHECK(summarise(weekend, me, mate, &r));
    CHECK(r.quali == 1 && r.teammate_quali == 3);
    CHECK(r.races.size() == 2 && r.done);
    CHECK(r.races[0].place == 3 && r.races[0].points == 6 + 2 && r.races[0].teammate == 2);
    CHECK(r.races[1].place == 1 && r.races[1].points == 25 && r.races[1].teammate == 3);

    // only qualifying run so far: a round with no race yet
    Round early;
    CHECK(summarise({weekend[0], weekend[1]}, me, mate, &early));
    CHECK(early.quali == 1 && early.races.empty() && !early.done);
    // a feature-only weekend is done after its one race; a sprint alone is not
    Round feature_only, sprint_only;
    CHECK(summarise({weekend[1], weekend[3]}, me, mate, &feature_only) && feature_only.done &&
          feature_only.races.size() == 1 && feature_only.races[0].points == 27);
    CHECK(summarise({weekend[1], weekend[2]}, me, mate, &sprint_only) && !sprint_only.done);
    CHECK(targets({feature_only}).met == 1 && targets({feature_only}).next_round == 2);
    CHECK(targets({sprint_only}).met == 0 && targets({sprint_only}).next_round == 1);
    CHECK(continues(early, r) && !continues(r, early));

    // a driver who isn't in the round
    Round none;
    CHECK(!summarise(weekend, 999, 0, &none));

    // no teammate known
    Round solo;
    CHECK(summarise(weekend, me, 0, &solo) && solo.teammate_quali == 0 && solo.races[0].teammate == 0);

    Form f = form({r}, 0);
    CHECK(f.rounds == 1 && f.races == 2 && f.points == 33 && f.wins == 1 && f.podiums == 1 && f.poles == 1);
    CHECK(f.compared == 3 && f.ahead == 2);
    CHECK(f.score == 80 + 3 + 1 + 2 + 3); // capped points, a win, a podium, a pole, 2 of 3 ahead
    CHECK(form({r}, 1).score == 99);
    CHECK(form({}, 1).score == 0 && form({early}, 1).rounds == 0);

    // a midfield season: 6 points a round, level with the teammate
    Round mid;
    mid.quali = 9;
    mid.teammate_quali = 10;
    mid.races = {{8, 1, 7}, {7, 5, 9}};
    mid.done = true;
    std::vector<Round> season(12, mid);
    Form m = form(season, 9);
    CHECK(m.points == 72 && m.score == 32 + (24 * 20 - 36 * 10) / 36);

    CHECK(licence_points(1) == 40 && licence_points(3) == 40 && licence_points(4) == 30 && licence_points(10) == 3);
    CHECK(licence_points(0) == 0 && licence_points(11) == 0);
    CHECK(percent(45, 90) == 50 && percent(95, 90) == 100 && percent(10, 0) == 100);

    // merge: the game holds the whole season
    Round a = mid, b = r, c = early;
    CHECK(merge({a}, {a, b}) == (std::vector<Round>{a, b}));
    CHECK(merge({a, c}, {a, b}) == (std::vector<Round>{a, b}));
    // a loaded save that lost the first round: its one round is the kept last one, further on
    CHECK(merge({a, c}, {b}) == (std::vector<Round>{a, b}));
    // the same, with a new round started
    CHECK(merge({a, b}, {mid}) == (std::vector<Round>{a, b, mid}));
    CHECK(merge({a, b}, {}) == (std::vector<Round>{a, b}));

    // academy targets: the kind turns with the round, the level follows the points so far
    CHECK(goal_for({}, 0).kind == kQuali && goal_for({}, 0).level == 0);
    CHECK(goal_for({r}, 1).kind == kPoints && goal_for({r}, 1).level == 2); // 33 points in one round
    CHECK(goal_for({mid}, 1).level == 1);                                   // 6 a round
    CHECK(goal_for({mid, mid, mid}, 3).kind == kTeammate);
    CHECK(goal_for({solo, solo, solo}, 3).kind == kPoints); // no teammate seen
    CHECK(met({kQuali, 0}, mid) && !met({kQuali, 1}, mid));
    CHECK(met({kPoints, 1}, mid) && !met({kPoints, 2}, mid) && met({kPoints, 2}, r));
    CHECK(met({kFeature, 0}, mid) && !met({kFeature, 1}, mid) && met({kFeature, 2}, r));
    CHECK(met({kTeammate, 0}, mid) && !met({kTeammate, 1}, r) && !met({kTeammate, 2}, mid));
    CHECK(!met({kFeature, 0}, early) && !met({kPoints, 0}, early));

    // nothing judged yet: the first target, also while round 1 is under way
    Targets t0 = targets({});
    CHECK(t0.next_round == 1 && t0.last == -1 && std::string_view(describe(t0.next)) == "Qualify in the top 12");
    CHECK(targets({early}).next_round == 1 && targets({early}).met == 0);
    // a good season: the fourth target is the hard teammate one, and the sprint was lost to him
    Targets good = targets({r, r, r, r});
    CHECK(good.met == 3 && good.missed == 1 && good.streak == 1 && good.next_round == 5 && good.dropped_after == 0);
    // a round with nothing: three in a row and the academy is gone, and stays gone
    Round bad;
    bad.quali = 20;
    bad.teammate_quali = 10;
    bad.races = {{18, 0, 9}, {19, 0, 8}};
    bad.done = true;
    Targets warn = targets({r, bad, bad});
    CHECK(warn.met == 1 && warn.streak == 2 && warn.last == 0 && warn.next_round == 4 && !warn.dropped_after);
    Targets out = targets({r, bad, bad, bad, r, r});
    CHECK(out.dropped_after == 4 && out.next_round == 0 && out.met == 1 && out.missed == 3);
    CHECK(targets({bad, bad, r, bad}).streak == 1);
    // the round under way isn't judged before its second race
    Round half = bad;
    half.races.pop_back();
    half.done = false;
    CHECK(targets({bad, bad, half}).streak == 2 && targets({bad, bad, half}).next_round == 3);

    std::printf(g_failed ? "%d check(s) failed\n" : "all checks passed\n", g_failed);
    return g_failed ? 1 : 0;
}
