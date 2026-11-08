# Handoff: F2 Feeder Career (F1 25 + 2026 season)

Context for picking the work up in a new session. Read `f2-feeder-career/README.md` too (how it works, lessons) and `f2-feeder-career/REVAMP.md` (the F2 revamp research).

## Goal

A [Swofty Launcher](https://github.com/Swofty-Developments/SwoftyLauncher) addon with a Driver Career that starts in F2: **F2 2026 > F1 2027**. Up to 0.4.9 there was also an F2 2025 > F1 2026 career; it was dropped in 0.5.0 to focus on the 2026 season.

The game's 2026 career has no Start in F2; its 2025 career has one (F2 2024, then F1 2025).

## Current state: 0.7.2

- **F1 2027:** the FIA 2027 calendar, 24 rounds, with Portugal and Turkey on the track addons `mod.portimao` and `track.istanbul_park`, which the manifest requires. Tested on 2 Nov 2026 with Skip to F1 (Sainz, Williams): Full Season and Custom Season 24, 16 and 10 give the 2027 lists, Turkey shows as round 17, Portimão raced, and the Team HQ opens with no crash. GP and championship titles read 2027.
- **F2 > F1:** goes through the same season length choice. Seen on 2 Nov: the Custom Season lists the 2027 rounds but none selected until a tab is picked again; then the free Team Select.
- **Academies (0.7.2):** the F2 championship's academies component now names the `2026` group. A created driver gets the 2026 academies at Start in F2, and the bonus shows on the F1 contract screen (Mercedes: 29% with academy, 26% without).
- The Tsolov save (made before 0.6) loads and keeps its 2026 calendar.
- Later F1 seasons repeat the year, like the game's careers. Decided on 2 Nov: no 2028+ calendars.
- A frozen copy, `community.f2-feeder-career-stable` (0.7.1), is on in the `default` profile with the two track addons; the dev copy (id `miguelvf3.f2-revamp` from 0.8.0, `community.f2-feeder-career` before) is on in `test`. They conflict, so each profile has one.

### 0.6.0 to 0.6.3: the Team HQ crash

Up to 0.6.3 the addon used its own F1 event list naming `F1_2027_full` in each season. That opened contract negotiations at the first Team HQ visit, the game left the player with no team, and the Team HQ crashed on B. It wasn't Portimão, as 0.6.1 assumed. 0.6.4 went back to the game's event list, and 0.6.5 to 0.6.8 make the season length choice give the 2027 lists.

### Earlier milestones

- 0.4.9: the F2 season starts in Melbourne, ROUND 1/14, in the real 2026 order (checked with Frida); the 3-round season too. The F2 > F1 transition works (Custom Season, then Team Select, then the career intro).
- 0.5.0: career A removed; the DLL rebuilt with zig 0.17. Table indices moved and saves kept loading.

## How to test a version

1. Turn the `f2-feeder-career/` addon on in the launcher's `test` profile.
2. Clear the cache: `Remove-Item -Recurse -Force "$env:LOCALAPPDATA\Swofty\cache\overlay\test"`
3. Start from the launcher.
4. Logs: `swofty\loader.log` and `swofty\crash-*.txt` in the game folder (`C:\Program Files (x86)\Steam\steamapps\common\F1 25`), also copied to `%LOCALAPPDATA%\Swofty\sessions\<date>\`; `%LOCALAPPDATA%\Swofty\state\wrapper-run.log` for the build.
5. Game database: `swofty.exe db get "<path>"` and `swofty.exe db search <text>` in the launcher's `bin` folder. Uncatalogued keys read faster with `gamedb.read` and `gamedb.fnv1a64(path)` from the launcher's `tools/gamedb.py`.

## Lessons (don't repeat)

- **Copying an F2 `championships.Championship` crashes the boot** in `Game_ModeChampionship_Build`, even with only the name changed. Copies of the F1 championship (`F1_driver_career_26`) work, as in Progressive. For F2, reuse championships no F1 25 mode uses (`F2_career_23`).
- **Entrant lists:**
  - `stat_id` indexes a registry that ends at the highest stock id (104). Free ids below 105: 2–12, 14, 15, 17–29, 31–37, 39, 53 and 68. The addon uses 68.
  - The game only loads a list that is in `content_sets.content_set[_metadata_=2026].available_entrant_lists_0.entrant_lists`, or that a loaded list references.
- **`add-row` only copies keys the launcher's catalogue knows.** For the mode, the championship and the groups, use `tools/fields.json` (from Progressive). In nested lists without `_instancecount_`, create fields with `set` + `create` + `type`.
- **F2 calendar** (found with Frida, not by trial):
  - The calendar is built from the event list's `season_0.track_event_list`, each entry dated by its race weekend's `date_time`. Dates in `TrackDataSet` change nothing.
  - Before the first track load, `0x140bc9360` rebuilds `Season.weekends`, keeping only entries whose event `stat_id` is in `ComTrackList_0.default_track_list`, **in that list's order** (key found by brute-forcing fnv1a64 paths).
  - New race weekends need all 7 `date_time` fields (`add-row` copies none), or the boot crashes in `Trk_TrackData_LoadTrackListNodes`.
  - `season_2027` isn't in `game_setup.seasons`, and naming it in a `seasons` list crashes the boot (`0x142a0ddb0`).
- **F1 calendar:** see the README. The game takes the first season length preset and track pool that fit; a copied event list naming its own list opens contract negotiations and empties the player's seat; a new track needs a race weekend activity row.
- **Start in F2:** `Game_CareerWizard_StartsInF1` answers yes for the 2026 career. The addon answers no for its group, but **not inside `FeTile_Route_CareerItem`**: there the "no" path reads an object addon tiles don't have. In the router the stock answer stands and `wz::tile_route(group, "career_wizard_driver_career")` changes the route.
- **Saves:**
  - Changing which championships a mode uses breaks careers saved before (`Game_SaveData_DeserialiseContainer` on profile load).
  - Steam Cloud brings old saves back. To clear: turn the cloud off, delete `Steam\userdata\1013098445\3059520\remote\*`, make a new save, and turn it back on keeping the local copy.
- **The game font has no "→"**; use ">".
- **A crash report** (`swofty\crash-*.txt`) keeps the memory around the registers; that's where the empty `development_events` component showed up. A full dump (`SWOFTY_CRASH_DUMP=full`) needs tens of GB.

## Next steps

1. **F2 revamp**, in the order in `REVAMP.md`. Step 1 (academy group) is done in 0.7.2. Next: the F2 2026 grid in the driver market pool, then real academies for existing drivers, promotion by F2 result, contract offers instead of the free Team Select, and an F2 hub.
2. When the mod is finished: a note in the universal-modder knowledge base (`um kb new`); open the PR only with the owner's OK.
3. Publishing on Swofty is planned.

## Tools

- `f2-feeder-career/tools/gen.py` writes `db/career.json` and `lang/career.json`. Edit the generator, not the JSON.
- `f2-feeder-career/native/build.sh` builds `f2_feeder.dll`. Needs zig (`winget install zig.zig`) and a SwoftyLauncher clone (`SWOFTY_REPO`).
- `tools/dump-career-db.ps1` dumps calendars, championships and modes with `swofty db get`. Slow: about 500 calls.
- In SwoftyLauncher: `addons/progressive` (career as an addon tile), `re/db/functions.tsv` and `re/db/subsystems/*.md` (exe functions), `tools/gamedb/catalog/keys.tsv.gz` (database keys), `docs/std/systems/career.md` and `docs/reference/addons/in-game-ui.md`.
