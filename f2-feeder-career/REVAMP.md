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

## Suggested order

1. **Academy bonus test** (`2026` group on the F2 academies). Done.
2. **F2 drivers in the market** (data only: `drivers_available_from_team_setup` with the F2 2026 grid). Changes the career without a DLL.
3. **Real academy for existing drivers.** Starts with Frida on the Academy step and the component.
4. **Promotion by result** (DLL at the end of F2).
5. **Contract offers instead of the Team Select.**
6. **F2 hub.**

## Sources

- Guides and forum: [Clutchpoints, F1 25 Driver Career Guide](https://clutchpoints.com/gaming/f1-25-driver-career-guide); [Yahoo Tech, F1 25 Driver Career Guide](https://tech.yahoo.com/gaming/articles/f1-25-driver-career-guide-143836977.html); [EA forum: Academy choice in F2 is not valid for F1](https://forums.ea.com/idea/f1-25-bug-reports-en/academy-choice-in-f2-is-not-valid-for-f1/12295483); [EA forum: F1 25 career mode needs more freedom](https://forums.ea.com/discussions/f1-25-general-discussion-en/f1-25-career-mode-needs-more-freedom-f2-career--full-driver-market/13705477); [EA forum: patch 1.04 thread](https://forums.ea.com/discussions/f1-25-general-discussion-en/f1%C2%AE-25---patch-1-04-discussion-thread/12277624/replies/12292462); [EA forum: current rookies from F2](https://forums.ea.com/idea/f1-25-bug-reports-en/cannot-select-current-f1-rookies-when-starting-driver-career-from-f2/12294630); [EA, F1 25 career deep dive](https://www.ea.com/games/f1/f1-25/news/f1-25-career-deep-dive).
- Swofty: `re/db/subsystems/career.md`, `re/db/functions.tsv`, `docs/std/systems/career.md`, `docs/reference/addons/in-game-ui.md`.
- Game database: `driver_academies`, `driver_market.driver_market`, `recognition`, `entrants.drivers`, and the championships' components (read by path with `gamedb.fnv1a64`).
