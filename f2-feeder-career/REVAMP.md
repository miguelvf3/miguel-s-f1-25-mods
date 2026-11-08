# F2 revamp: research (2 Nov 2026)

How F1 25's F2 career works inside, and what each part of the revamp could change. Sources: the web (guides, EA forum), Swofty's reverse engineering notes (`re/db`), the game database (`test` profile, 0.7.1) and the Swofty SDK docs.

## What the game does today

**For the player** (guides and EA forum):
- A Driver Career can start in F2 or straight in F1. A created driver picks one of seven academies (Mercedes, Ferrari, Red Bull, Alpine, McLaren, Williams, Sauber). The academy doesn't guarantee a seat; it only raises *recognition* with that team.
- The first F1 contract runs one season. Picking a team takes a driver's seat.
- Known bug: an academy picked in F2 reaches F1 with its recognition bonus at zero.
- The AI hires F2 drivers into F1 (a patch 1.04 bug is that they keep their F2 numbers), but the F2 season isn't simulated in the background: their results aren't used.

**Academies** (database):
- `driver_academies.driver_academy_group` has two groups, `2025` (7 academies) and `2026` (8: Audi and Cadillac join). Each academy has `name`, `statId` and `teams`, the F1 teams tied to it (Ferrari: `ferrari_26, haas_26`).
- No table ties an existing driver to an academy. The driver row (`entrants.drivers`, e.g. `rafael_camara`) has no academy field. The academy is a career choice: the wizard's "Academy" step (`Team_Build_AcademySelect`, tiles `f1_career_academy_select_tile`), stored in the championship's `ComDriverAcademies` component.
- `ComDriverAcademies` names an academy group: `F1_driver_career_26` (and our F1) use `2026`, the stock 2025 career's F2 (`F2_2024_career`) uses `2025`. `F2_career_23`, where our F2 runs, had **none** until 0.7.2.
- Recognition: `recognition.RecognitionDataGroup_N` (`Team_Load_Recognition`) holds per team `ai_weighting`, `team_comparison_driver` and weightings, plus driver modifiers and years-with-team data.

**Contracts and the Team Select** (`re/db/subsystems/career.md`):
- At the end of a season, `Career_Decide_ContractNegotiation` (0x142d57d10) picks the path. If the player's contract ends this season, the game opens `StateDriverCareerContractNegotiationsTeamSelect` (`Game_ContractTeamSelect_*`, 0x1436c46d0), then the agent (Intro, Update, Outro). With no contract, it goes straight to the agent.
- Contracts: `CareerDriverContractsComponent` and `DriverCareerContractsManagerComponent` (next season's contracts at +0x408). Signing is `Career_Sign_DriverContract` (0x140d79ff0) and `Career_Sign_DriverToTeam` (0x140d8fcb0). `Career_ContractRenewal_ReplaceOldPosition` (0x140d766b0) is what left the player with no team in 0.6.x.
- Rating ranges, secret meetings and counteroffers are tables every career shares (`driver_contracts.NCS`, `career.generic_contracts`).
- Swofty's F1 1990 addon skips the contract flow (`contract_skip.cpp`) and Progressive Career moves seats (`seats.cpp`): both are examples.

**Driver market:**
- `ComNCSDriverMarket` (group `2026` on our F1) reads `driver_market.driver_market.DataGroup_N` (`Team_Load_DriverMarket`, 0x140dcdb00): firings, retirements, the rating range each team wants, and `drivers_available_from_team_setup`.
- `drivers_available_from_team_setup` is a fixed list: 3 groups of 6 drivers from outside F1 (e.g. Fornaroli, Bennett). That's how F2 drivers reach F1 today, regardless of their results.
- The market's checks log to `DRIVER_MARKET`: budget, the team's championship position and rating (`Career_DriverMarket_CheckEligibility`, `_CheckTeamRequirements`, `_CheckAffordable`).

**Hub:** the F2 season doesn't go through the Team HQ: races follow one another. Swofty can add screens (`operations.screens`, `.ui` files with native view models) and Team HQ laptop tabs (`hub_tab`), but the laptop only exists in the F1 hub.

## What the Swofty SDK already offers

- `career::set_seat` / `set_seat_of_team`, `swap_drivers`, `apply_lineup` and `set_driver_ratings`: put a driver in a seat, swap two drivers and change ratings in the loaded career.
- `career::markets` (`career.market`): retirements, firings and each team's rating ask, per market group. Experimental in 1.6.4.
- Career events: `career.season.start`, `career.loaded`, `career.hub` (the DLL already uses the last two).
- `hub_tabs` and `operations.screens` for UI.

## Each part of the revamp

| Part | How | Difficulty |
|---|---|---|
| Real academy for existing drivers (Câmara at Ferrari, Tsolov at Red Bull) | Find where the career stores the chosen academy (`ComDriverAcademies`, in the save). When a career starts with an existing driver, the DLL writes their academy from our own driver > academy table. First check whether the wizard's Academy step can show for existing drivers (`force_options` and `replace_steps` in `career.wizard`). | Medium: needs Frida on the wizard and the component |
| Academy bonus lost from F2 to F1 | Give `F2_career_23`'s `ComDriverAcademies` the `2026` group. Data only. | Low: done in 0.7.2 |
| AI drivers moving up from F2 to F1 | Data only: replace `drivers_available_from_team_setup` (2026) with the F2 2026 grid, and the market hires them. With the DLL: at the end of F2, read the standings and put the best into the market or a seat (`set_seat_of_team`), with new ratings (`set_driver_ratings`). | Low (data) to medium (DLL) |
| Contract offers instead of the free Team Select | The Team Select is the `ContractNegotiationsTeamSelect` step, opened when the player's contract ends. Options: filter the teams the screen shows (the academy's, and those whose rating range takes the player), or skip the screen and sign the offer the agent picks. Handle with care: this system left the player with no team in 0.6.x. | High |
| F2 season hub | Make F2 go through the Team HQ (check with Frida whether an F2 championship can get the hub flow), or a new Swofty screen between races with the calendar and standings. | High: last |

## Results

**Academy (0.7.2, 2 Nov):** `F2_career_23`'s `ComDriverAcademies` now uses the `2026` group. Start in F2 with a created driver shows the Academy step with the 2026 academies (Audi, Cadillac). The bonus shows on the **contract screen**: at Mercedes, 29% recognition with an academy (Inc. 4% Bonus), 26% without (Inc. 1% Bonus). The Team Select Insights only compare the two drivers' ratings and don't show the academy. Whether the bonus was lost without the `2026` group wasn't tested.

**F2 drivers as F1 free agents (0.7.3, 2 Nov):** the free agent pool doesn't come from `drivers_available_from_team_setup` but from the career entrant list's references: `f1_2026` (grid), `f1_career_2026_driver_pool` (icons and young drivers), `f1_career_2026_reserve_drivers`, and an F2 grid whose teams `Game_CareerSetup_ValidateEntrantTeams` clears at career creation. The stock 2026 list names `career_f2_2025_for_f1_2026` (F2 2025); ours now names `f2_2026`. Frida at a new career's creation: the 22 F2 2026 drivers (Câmara, Herta, Montoya, Tsolov...) are free agents and the F2 2025 ones (Fornaroli, Browning...) are gone. Tsolov is listed twice (the driver pool has him too). Not yet seen in game: whether the market signs them.

**Real academy for existing drivers (0.7.4, 2 Nov):** no game table ties an existing driver to an academy (only the suit art names it). `DriverAcademiesComponent` keeps a driver key > academy map (saved as `assignedAcademies`), and on career load the DLL adds the player's entry from its own table (Câmara and Beganović Ferrari, Tsolov Red Bull, Minì, Dunne and Maini Alpine, Herta Cadillac, Boya Mercedes). The game's academies already include the partner teams (Mercedes with Aston Martin, Ferrari with Haas, Red Bull with Racing Bulls). In game, Câmara on Skip to F1: the log shows `academy: Rafael Camara: Ferrari (driver 203, slot 7, status 2)`, and the Ferrari contract screen shows 38% recognition against Leclerc with "Inc. 5% Bonus". The Team Select Insights, opened before the hub, still show 36% and "Inc. 1% Bonus".

**Promotion by result (0.7.5, 2-4 Nov; ran in game on 4 Nov):** with Câmara at Haas, the log shows Tsolov (61 pts) in Lawson's seat and León (44 pts) in Lindblad's, both `result ok`, the second with a number change. The F2 standings were read live (22 drivers). at the first F1 season's Team HQ, once the player has signed for an F1 team, the best 3 AI drivers of the F2 season take the seats open for 2027 in the news (Ocon at Haas, Lawson and Lindblad at Racing Bulls; until 3 Nov Ocon, Bottas, Lawson and Stroll), an academy driver first at his academy's team, through the core's `set_seat`. The F2 order is the F2 championship's last season standings: points, then countback over the sessions that scored (a three-way tie on 6 Nov showed qualifying has result rows too; countback gives the game's order, León, Tsolov, Bilinski). Found on the way:
- The game passes through the Team HQ once *before* the F2 > F1 Team Select (`career.hub` fires with the player still on his F2 team), hence the wait for the player's F1 team.
- A free agent's entrant row keeps a team position: a seat needs an F1 team too.
- The finished F2 championship's standings exist only in the session the F2 season ends. After a load they are empty and `Career_Rebuild_ChampionshipStandings` finds no results to rebuild them from. The DLL keeps the order it reads in its storage (`f2_standings.txt`, with the player's driver) and reads it back after a load.
- Tsolov was missing from the Driver Rating Comparison, promoted or not. The list (0x1416da8e0) sorts the 117 driverData entries and drops every driver in a map of driver pool rows unless he's the player. The map holds the rows of `f1_2025_driver_pool` and `f1_2026_driver_pool` (objects named after the row, `tsolov`, `rasmussen`), and Tsolov is in both, as a young driver in the Icons tab. Removing him only from this career's lists didn't help: the content set loads the stock pools for every career. 0.7.5 takes his row out of both pools, so he also leaves the Icons tab of every career while the addon is on. Still open: on 7 Nov he was still missing after an F1 GP in a career started before this fix, and a career started with it hasn't been checked. The user set it aside as minor, to look at again with the next batch of tests. His `icon_driver_nikola_tsolov` option stays in Driver Icon Settings (stock data).
- In game on 4 Nov: both promoted drivers show in the grid reveal in Racing Bulls suits, León at 57 overall in the comparison; Lawson and Lindblad are free agents. Accolades shows the five goals, and the calendar is the 2027 one (Bahrain first, Portugal and Turkey in), though the Custom Season screen showed the 2026 list.
- The standings are right after the grid reveal: Lawson and Lindblad only show at other teams before it, and the promoted drivers then appear with 0 points. No fix needed.

**Accolades (0.7.5, 3 Nov, build checked, not in game):** the empty Accolades panel was Câmara's: the stock 2026 accolades (`AccoladeDataGroup` `f1_career_2026`, copied for this career) have no row for ten F2 2026 drivers (Fittipaldi, Inthraphuvasak, León, Bilinski, Herta, Câmara, Boya, Stenshorne, van Hoepen, Varrone). They get Oliver Goethe's five goals. `add-row` copied only the row's own two keys (no catalogue entry for the goals), so the generator writes the goals with `set`.

- Game build guard (0.7.7): the DLL compares the exe's PE timestamp (1789118616) once. On another build it logs that and leaves off what reads game offsets: the calendar list swap, academies and promotion. The tiles and titles keep working. Exe checked on disk; the log line is still to confirm in game.

## Suggested order

1. **Academy bonus test** (`2026` group on the F2 academies). Done.
2. **F2 drivers in the market** (data only: `drivers_available_from_team_setup` with the F2 2026 grid). Changes the career without a DLL.
3. **Real academy for existing drivers.** Starts with Frida on the Academy step and the component.
4. **Promotion by result** (DLL at the end of F2).
5. **Contract offers instead of the Team Select.**
6. **F2 hub.**

## Plan for the next steps (3 Nov)

Research done offline (Swofty main at `91b49d7`, 4 Nov: `re/db`, `docs/std`). Each step says what is known and what needs the game.

### To test first (0.7.5)

1. New career, Start in F2, 3 races; finish F2, sign for a team, reach the Team HQ. Check the 3 promoted drivers in the grid (Haas, Racing Bulls), the log lines `[f2-feeder] promotion:` and that `f2_standings.txt` exists in the addon folder.
2. Save, quit, load: no second promotion (the leaving drivers are gone from their seats).
3. Tsolov in the Driver Rating Comparison when he isn't promoted.
4. Accolades with Câmara: the panel shows five goals. If the old save still shows it empty, a new career.
5. Skip to F1 career: no promotion (no F2 season), academy as in 0.7.4.

### 4b. Rating bonus for the F2 top finishers

**Built (0.7.5, 4 Nov, not tested in game):** the simpler route won. `set_driver_ratings` writes into the driver's curves, so the promotion sets the promoted drivers' ratings once, by final F2 position (`kPromotedRatings`): overall about 75, 73, 71 and 70 for P1, P2, P3 and P4 or below (overall = (0.25 exp + racecraft + 0.25 awareness + 2 pace) / 3.5). The player gets to about 74 through the F2 season's XP; León reached F1 at 57. The research below is kept for reference: the `driver.ratings.read` route isn't used.

- The career keeps a driver's ratings in `DriverStatisticsComponent` (`driverData`, 0x50-byte entries per driver key; each statistic is an evolution curve plus `dynamicLevel` and `temporaryBoostValue`). The game reads them with `Career_Get_DriverStatValue` (0x141140ee0) and `Career_Get_DriverOverallRating` (0x14113d120), both L2.
- Swofty's `driver.ratings.read` event (`drivers::events::RatingsRead`, before) lets an addon change each value the career reads, per driver and statistic. Progressive Career uses it for its season ratings. That is the route: no need to read the current value first.
- Plan: when the promotion runs, keep the F2 top 5 with their positions in the storage file. A `driver.ratings.read` handler, only in this career (`.in(owner)`), raises those drivers' values. To decide: the size, whether experience changes too, and whether it fades over seasons.
- How far apart F2 and F1 are (`driver_statistics` holders, the `season_2026` sets; a curve is `x:value` over the career, the second `0:` point the start):

  | Driver | Experience | Racecraft | Awareness | Pace |
  |---|---|---|---|---|
  | F2 2026 (Câmara, Goethe, Tsolov, Mini, Bilinski, Herta...) | 31-38 | 49-68 | 57-62 | 64-78 |
  | Lindblad (F1 rookie 2026) | 32 | 70 | 60 | 72 |
  | Lawson | 73 | 79 | 71 | 81 |
  | Ocon | 83 | 85 | 84 | 84 |

  An F2 driver's curve rises to about 80 over the career, so a promoted rookie starts at the back.
- The market won't sign them on its own: each team asks for a driver rating (`driver_market` group 2026: Audi 82, Williams 83, Haas 84, Racing Bulls 85, Alpine 86, Cadillac 87, up to McLaren 99), and an F2 driver's overall is around 60. Even the 0.7.3 free agents stay unsigned unless the promotion (or a rating floor) moves them.
- So the bonus works better as a floor than a plus: `value = max(value, floor)`, with the floor by F2 position near a real rookie's (Lindblad: racecraft 70, pace 72). Proposal: P1 racecraft 72 / awareness 66 / pace 78; P2 70 / 64 / 76; P3 68 / 62 / 74; P4-5 66 / 60 / 72. The curve still grows from there.
- Check: the handler needs the std-2 event headers (`swofty/systems/drivers.hpp`); `driver_id` names `game/<label>`, so the DLL needs label <-> stat id (`swofty::career::detail::live_ops()->driver_stat`).

### 4c. Numbers and looks of the promoted drivers

- Every F2 2026 driver has F1 number 99 in `entrants.drivers` (Tsolov 48), so two promoted drivers share 99. The game gives a free number on its own signings (`Career_DriverNumbers_AssignFree`); whether the core's `set_seat` does is to check in the 0.7.5 test. If not: `career::set_car_number` (table version 4) after each promotion, with a number of our own per driver (to choose), else a free one.
- Looks: check in the test whether a promoted driver races in his F2 team's suit; Swofty's `set_look` names a look for a driver.

### 5. Contract offers instead of the free Team Select

Known:
- The end-of-season contract path: `Career_Decide_ContractNegotiation` (0x142d57d10) goes to `StateDriverCareerContractNegotiationsTeamSelect` when the player's contract ends this season, else to the agent (Intro, Update, Outro). With no contract record (`Career_Needs_DriverContract`) it goes to the agent.
- The Driver Career Team Select list is built by 0x141873a30, which first calls `Game_DriverContracts_BuildOffers` (0x140d80ef0) for the player: offers at contracts manager +0x3c8..+0x3d0 (0x48-byte entries), none when `Game_DriverContracts_RankTeam` has no valid rank. `Game_TeamSelect_FocusTeam` / `ShowTeamDrivers` fill the focused team.
- The Team Select state is absent from the start-of-career flow, so the screen after F2 (Custom Season, Team Select, career intro) may be the wizard's team select instead. Unknown which.

Traced in game (4 Nov, end of F2 with Câmara): `... > StateFrontendWorld > StateFrontend > StateOnlineFrontend`, then the Team Select list builder 0x141873a30 runs in the front end, without `Game_DriverContracts_BuildOffers` and without `Game_ContractTeamSelect_OnEnter`. Then `StateLaunch > StateCareerHQWorld > PreHQCareerDataSync > FirePlayers > StateTeamHQ > StateTeamHQUpdate > ContractNegotiations* > ContractNegotiationsDecision > Intro`: the contract scene runs from the Team HQ flow, through the agent path. So the screen after F2 is the career start's Team Select, not the end-of-season one: the place to filter teams is the builder itself (its team list, vm+0x2d0). `career.hub` fires at `StateTeamHQUpdate`, with the player already on his F1 team.

Still needs the game (Frida): which flow state shows the Team Select after F2 (answered above); whether 0x141873a30 runs there; what the offers list holds at that moment; whether a team tile can be hidden or locked (the list the builder fills).

Design to decide: which teams offer a seat. Proposal: the academy's teams always; the teams whose driver market rating range (`team_driver_rating_range_requirements`, `driver_contracts.NCS.rating_ranges`) takes the player's rating; and for an F2 champion or top 3, the teams with a leaving driver (the promotion seats). The others are hidden or locked.

Offer rule for the first version, approved by the user (7 Nov), from the player's final F2 position:
- Easy team, by academy: Ferrari Haas; Mercedes Aston Martin; Red Bull Racing Bulls; Alpine, Williams, Audi and Cadillac their own team; McLaren or no academy Cadillac and Aston Martin.
- P4 or lower: only the easy team. P1-P3 (40 super licence points): the easy team, Haas, Racing Bulls and the teams of the academy. A refused team shows a popup with the open teams. This stands in for repeating F2 until that exists, and ignores the 40-point limit for now.
- Whether a team without an offer can be hidden or locked is the first thing to find out with Frida.

Traced on 7 Nov (Skip to F1 Team Select; same builder as after F2): the screen object has vtable 0x145729db0, and slot 10 (0x141a69a50) takes every UI message as (this, name, data). `team_selected_changed` (hash 0x9c4d00fd) is a focus change and `team_selected` (0xb2738f54) the confirm; +0x564 holds the focused team's key. Skipping that call for a team refuses it and the screen stays usable: tested live with Haas and Racing Bulls only. The team list itself sits at +0x3d0 with a 0x14 stride (read only; hiding a tile is untested). 0.7.8 refuses teams without an offer this way. Not done: on-screen text, a created driver's academy (only kDriverAcademies), and the lowest constructors' team, which is Haas because no F1 season has run yet.

Agreed with the user (3 Nov):
- **Another F2 season when no team offers a seat.** The career repeats F2 instead of a free pick. Needs: whether the mode can run its F2 championship again (the `F2_Default` setup list runs F2 then F1; a second F2 season may need a second F2 setup or a reset of `F2_career_23`'s season), and the F2 grid for 2027 (the same 2026 grid, minus the promoted).
- **Super licence as the gate.** FIA points for the F2 championship: 40, 40, 40, 30, 20, 10, 8, 6, 4, 3 for P1-P10, and 40 are needed (over three seasons; also at least six F2 events). So the F2 top 3 always qualify; P4 (30) and P5 (20) qualify only with points from before (an existing driver's real F3 or F2 results; a created driver has none). A second F2 season adds its points to the first.

### 6. F2 hub

- The Team HQ (and its laptop, where Swofty's `hub.tab` adds pages) exists only in the F1 part of a Driver Career. Between F2 weekends the game goes from one race to the next.
- Options: (a) a Swofty screen (`operations.screens`, a `.ui` file with a native view model) shown between F2 rounds, with the calendar, standings and the academy; (b) make the F2 championship go through the Team HQ flow (`game_modes.ModeChampionship` flags such as `return_to_fe_between_weekends`; the hub state is F1's), riskier.
- Traced in game (Frida on `FeFlow_Set_StateStatus`, 0.7.5, two F2 rounds): after a weekend's race, `... > Complete > PostRaceCareerHub > CareerRewards > PostRaceCareerHub > Exit > StateFrontendFrameLock > StateUnloadSession > WeekendSessionManager > StateSmokeTestSkipFEPreLaunchHold > StateCerberusServer > StateCareerHubSelector > StateLoadSession > PracticeAndQualifying`. Between a sprint and the feature race the session stays loaded (`Exit > Race`).
- **`StateCareerHubSelector` is the place to look at for a hub:** the game runs it between weekends with the session unloaded, and the name says it picks whether a hub comes next. No Swofty note covers it yet. Next: find its vtable from the name string and read its `onUpdate` links (what sends F1 to the Team HQ and F2 to `StateLoadSession`), statically, without the game. Read `docs/reference/addons/in-game-ui.md` for the screen API too.

**Option (c), seen in Silly Season (`alexs.silly-season` 1.0.0, 6 Nov; from its Addon Browser page, the addon itself not opened):** a full-screen panel on Swofty's experimental overlay (`swofty/overlay.hpp`, an immediate-mode 2D paint callback: rectangles, lines, text, images), opened with a key (F7 / Back + Y), plus a Team HQ tab. An overlay doesn't need the game's UI files or a flow state, so a calendar, standings and academy panel could work during F2 now. It can't advance time or start a session, so the FP1 events still need `StateCareerHubSelector`. Limits: experimental (`experimental_features: overlay`, an "experimental API" badge), and it may not draw with frame generation on. The user will ask the author (Alex Sánchez) before reusing anything.

**Overlap with Silly Season:** it simulates F2, moves academy juniors up, retires and sacks drivers, and sends mails and press conferences; it needs launcher 1.8.16 and covers 2025 and 2026 careers. Check together: whether it works in this mod's own data group, and whether both change seats (our promotion against its market). Its option to let the game run the market may be the way to keep both on.

### Later: FP1 outings (user's idea, 3 Nov)

- For AI juniors in the F1 career: Swofty's `career.season` changes have a `stand_in` kind (from a round, until a round), but a change covers whole weekends, not FP1 only. An FP1-only swap needs a session hook (the entrants of a practice session), unresearched.
- For the player during the F2 season: an F1 practice session inside the F2 championship is not something the career does; it would need an extra F1 weekend in the F2 calendar or a separate event. High difficulty, research after the F2 hub.
- The user's shape for it (3 Nov): a special event in the F2 hub, on an F1 weekend that F2 doesn't race (as in Mexico 2026, where academy juniors drove FP1). If the F2 hub gets a Team HQ-like time advance, the event appears on that weekend's date: the player drives one F1 practice session for his academy's team, then the hub carries on to the next F2 round. To research then: starting a single F1 practice session from the hub (a one-session event, like the Grand Prix or invitational event setups) with the player in an F1 seat, and what it gives back (recognition with the academy team, ratings).

## Later ideas

- **F1 FP1 outings for academy drivers.** During the F2 season, now and then the player drives FP1 for his academy's F1 team, as real academy juniors do (usually one junior per academy at a given weekend). AI academy drivers would get FP1 outings too, in place of a race driver. See the plan above.
- **Reserve or test driver** when there is no race seat: sign as an F1 team's reserve (the career has reserve rows: an entrant with team position 2 or more, `f1_career_2026_reserve_drivers`; `Career_Sign_DriverToTeam` fills a seat from a team's first reserve) and race F2 again, with more FP1 outings. Unknown whether the player's own row can sit in a reserve slot.
- **Abu Dhabi young driver test**, a special event in the F2 hub after the last round, like the FP1 outings.
- **An academy that reacts to results**: dropped after a poor season, an offer from another academy after a good one; told through the career's emails and news (the `emails` and `news` tables of the career data groups).
- **Mid-season call-up**: when an F1 race driver leaves or is injured, an academy junior stands in (Swofty's `career.season` changes, kind `stand_in`, from a round until a round).
- **Track acclimatisation practice programme misbehaves in F2** (reported 4 Nov, to look into). The programme reads its corner targets from the track's resource (`Game_PracticeTrackAcclimatisation_CopyTargets`, 0x14299a4f0); with no resource or empty corner data it has no targets, and Swofty's guard only stops the crash. Programme folder by season: `practice_programmes_26` for `season_2026`. Symptom at Qatar: the gates show, but none responds and the HUD stays on gate 16, the last one (before the final straight's DRS), even at the start of a lap; only the DRS press at the end registers. So it has targets but its current target is stuck. `Game_PracticeTrackAcclimatisation_Start` (0x1429a06b0) picks the current target once, from the car's lap distance, so two things to rule out: the save-data path (the weekend was quit in practice and reloaded, and the programme restores its progress from the save), and leaving the garage at a lap distance near the lap's end. Compare with a fresh practice at Qatar, another track and the stock F2 career before changing anything; it may not be this mod's.
- **Custom Season after F2 showed the 2026 list when the career was reloaded during F2** (4 Nov): the DLL hid the 2026 presets only inside `Game_SeasonLength_ApplyPreset`, which first runs when the player advances. 0.7.5 hides them when the career loads too: the owner object is the schedule component's (`ScheduleComponent` +0x290). To confirm in game.
- **Optional 2027 tracks** (0.7.6, user's idea, 6 Nov). `requires` has no optional form and the Addon Browser refuses an addon it can't resolve, so both tracks left `requires` for `load_after`, and two `db` toggles, `portimao` and `istanbul_park`, off by default, add each round through `when` ops. The calendar has 22 rounds, 23 with one and 24 with both; the 16 and 10 round seasons don't change. Builds checked on 7 Nov for all four combinations: list count, rows and `max_num_tracks` match, and the rows past the list's end are removed. With Istanbul on and its addon off the launcher warns at build that the game won't start; Portimao has no such check, so the addon prints its own warning. Tested in game on 7 Nov: both on, the game opens; both off with the track addons off, it opens and a new career's Custom Season shows 22 rounds. With an option on and its addon off the game didn't open, so the description and warning matter. A `db` setting applies on rebuild, so it's chosen before a career: changing it mid-career is untested.
- Not planned: simulating F2 seasons after the player's (the player's focus is F1 by then).

## The 2027 grid (news of 15 Oct 2026)

[RacingNews365](https://racingnews365.com/complete-f1-2027-confirmed-driver-line-ups) and [The Race](https://www.the-race.com/formula-1/2027-f1-grid-every-confirmed-driver/): Câmara replaces Ocon at Haas (Bearman expected to stay); both Racing Bulls seats are open (Lawson and Lindblad); Mercedes not announced but expected unchanged. Stroll (Aston Martin) and Bottas (Cadillac) are confirmed for 2027, so the 0.7.5 list (Ocon, Bottas, Lawson, Stroll) doesn't match. 0.7.5 now uses Ocon (Haas), Lawson and Lindblad (Racing Bulls): three seats for the top 3 (the user's choice, 3 Nov). Câmara, a Ferrari junior, takes Haas first, as he does in reality.
- **One standings file per career.** `f2_standings.txt` is one file for every career of this mod (keyed by the player's driver only). Two careers with the same driver ending F2 at the same time would share it.

## Sources

- Guides and forum: [Clutchpoints, F1 25 Driver Career Guide](https://clutchpoints.com/gaming/f1-25-driver-career-guide); [Yahoo Tech, F1 25 Driver Career Guide](https://tech.yahoo.com/gaming/articles/f1-25-driver-career-guide-143836977.html); [EA forum: Academy choice in F2 is not valid for F1](https://forums.ea.com/idea/f1-25-bug-reports-en/academy-choice-in-f2-is-not-valid-for-f1/12295483); [EA forum: F1 25 career mode needs more freedom](https://forums.ea.com/discussions/f1-25-general-discussion-en/f1-25-career-mode-needs-more-freedom-f2-career--full-driver-market/13705477); [EA forum: patch 1.04 thread](https://forums.ea.com/discussions/f1-25-general-discussion-en/f1%C2%AE-25---patch-1-04-discussion-thread/12277624/replies/12292462); [EA forum: current rookies from F2](https://forums.ea.com/idea/f1-25-bug-reports-en/cannot-select-current-f1-rookies-when-starting-driver-career-from-f2/12294630); [EA, F1 25 career deep dive](https://www.ea.com/games/f1/f1-25/news/f1-25-career-deep-dive).
- Swofty: `re/db/subsystems/career.md`, `re/db/functions.tsv`, `docs/std/systems/career.md`, `docs/reference/addons/in-game-ui.md`.
- Game database: `driver_academies`, `driver_market.driver_market`, `recognition`, `entrants.drivers`, and the championships' components (read by path with `gamedb.fnv1a64`).

### Team Select state (7 Nov tests)

- `career.hub` does not fire before Team Select after F2, and the core has no `career.season.end` (subscribe returns invalid).
- At the confirm the entity is live with 2 championships, but it has no human row and an empty live F2 table on a loaded save; the core state is empty too.
- So the F2 order is kept in `f2_standings.txt` at each F2 `session.end` and `results`, and Team Select reads that file. A final-race capture is not tested yet.
- The refusal itself works: with P1 and Ferrari, only Ferrari, Haas and Racing Bulls opened. That P1 came from an old file, not from a real result.
- An old `f2_standings.txt` with 3 columns gets misread and produced wrong promotions in that test career.
- Attaching Frida to a long-running game crashed it (access violation in frida-agent, 11:56).

Known gap (7 Nov, left as it is by the user): ties in the kept F2 table can differ from the game. A player tied on 2 points with Herta read P13, and the game showed P14. The rule is unknown.

### Repeating F2 (7 Nov probe)

- `F2_Default` with F2, F2, F1 boots, and a second F2 Custom Season follows the first (user test on the test profile; a throwaway change to the installed copy only).
- User rule: repeat only below P10, with the same 2026 season and grid, and nobody moves to F1 from F2 in those repeats.
- 0x1407c26f0 walks the order list at entity +0x1068..+0x1070 and makes the first unfinished championship current through Career_Set_CurrentChampionship (0x1407be1d0). A wrap there could send a qualified player past the spare F2 entries to F1. Not tried yet.
- With several F2 championships, `read_live_table` must read the current one, not the first with results.

Frida on the hung game (7 Nov): the order list held the same Championship pointer twice, then F1, and the entity had 2 championships. So rows naming F2_career_23 share one object, its one season is reused, and the first session of the second F2 never ends. The F1 skip code can't work on such a list, so it is not called and F2_SEASONS is back to 1. A repeat needs a different championship in each row.

Step 6 lead (8 Nov, Swofty docs `reference/addons/in-game-ui.md`): `hub_tab` in the manifest adds a settings-style tab for a data group to the career hub laptop (`laptop_container` in `career_hub_flow`) and to Team HQ, with Progressive Career as the example. The F2 phase has no hub screen to put it on: Advance after a round goes straight to the next weekend's first session (user, 8 Nov), so an F2 hub needs a screen of its own. `operations.screens` adds whole screens.

Repeat F2 dropped (8 Nov, with the user): a second season in the F2 event list did nothing. Frida after that F2: the F2 championship held one season (status +0x538 = 2, complete) while the F1 one held ten. F2_2024_career as a second row was never tried. A player at P4 or lower goes to the easy team.

Retest (8 Nov) with `_season_instancecount_` = 2 on F2_career_21, checked in the built database: the F2 still ended after one season and went to Team Select (Tsolov P2, Red Bull academy: Mercedes refused, Racing Bulls took him). Repeat stays dropped unless the core can keep a championship open.

Bug (8 Nov): with the player at Racing Bulls, Driver Details > Rivals shows Liam Lawson as the teammate rival after the promotion released him. The rivalry isn't updated by set_seat; look for where it is set (maybe at the grid reveal).

### Request: F2 2025 > F1 2026 (8 Nov, from the Overtake thread)

Asked by a player after 0.8.0 went up. It existed up to 0.4.9 and was dropped in 0.5.0 (f2a145e) to focus on 2026.
- Both careers would need an F2 championship: F2_career_23 is the only free one and holds its grid (f2_2026), and copied F2 championships crash the boot.
- The offers and promotions are 2026 data: kLeavingSeats (Ocon, Lawson, Lindblad), the 2026 academy group with Audi and Cadillac, kDriverAcademies. A 2025 career needs its own.
- F2 2025 grid `f2_2025` and `career_f2_2025_for_f1_2026` exist in the game; the F1 2026 calendar is stock.
- Priority: after the 2026 fixes (known issues in README).
