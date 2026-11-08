# F2 Revamp

Formerly F2 Feeder Career 2026 (`community.f2-feeder-career`). From 0.8.0 the addon id is `miguelvf3.f2-revamp`: careers saved with the old addon need it, so start a new career.


A Swofty Launcher addon for F1 25 with the 2026 season: a Driver Career that starts in the 2026 Formula 2 season and moves up to Formula 1 for 2027. It has its own tile in the **Addons** row of the career select, with its own game mode and save slots. The game's own 2026 career is unchanged.

| Phase | What you race |
|---|---|
| F2 2026 | The `f2_2026` grid (Câmara, Herta, Montoya...) on the real 2026 F2 calendar, Melbourne first, with Madrid. 14, 8 or 3 rounds. |
| F1 2027 | The 2026 cars on the FIA 2027 calendar: 22 rounds, or 24 with the optional Portugal (Portimão) and Turkey (Istanbul Park). Shorter 16 and 10-round seasons. GP and championship titles read 2027. |

Later F1 seasons repeat the year, as in the game's own careers.

## Requirements

- Swofty Launcher with `swofty.core` 0.2.13 or later.
- Optional track addons, for the two rounds F1 25 has no circuit for. Each round has its own setting on the addon, off by default. Turn it on with its track addon before starting a career:
  - `portimao` needs `mod.portimao` (Portimão, by Pajko);
  - `istanbul_park` needs `track.istanbul_park` (Istanbul Park, by Apoc & RK), in the Swofty addon catalogue.

## Playing it

1. Install the folder and turn the addon on in a profile, with any track addons you want.
2. Driver Career > **Addons** row > **F2 2026 > F1 2027**.
3. Pick a driver. **Start in F2** races the F2 season first; **Skip to F1** goes straight to F1 2027.
   - A created driver picks an academy (the 2026 ones, with Audi and Cadillac). Its recognition bonus shows on the contract screen.
   - Existing drivers have no academy (the base game works the same way).
4. Season length: **Full Season** or **Custom Season** (24, 16 or 10 rounds) give the 2027 calendar.

## After F2: offers and promotions

Your final F2 place decides which F1 teams sign you at Team Select. A team that won't shows a popup naming the teams open to you.

| F2 place | Teams open to you |
|---|---|
| P1-P3 (enough super licence points) | Your easy team, Haas, Racing Bulls and your academy's teams |
| P4 or lower | Only your easy team |

Your easy team depends on your academy:

| Academy | Easy team |
|---|---|
| Ferrari | Haas |
| Mercedes | Aston Martin |
| Red Bull | Racing Bulls |
| Alpine, Williams, Audi, Cadillac | Their own team |
| McLaren, or no academy | Cadillac or Aston Martin |

A created driver uses the academy picked when creating the driver. **Skip to F1** has no F2 season, so every team is open.

At the first F1 Team HQ, the best AI drivers of your F2 season take the seats of Ocon (Haas), Lawson and Lindblad (Racing Bulls), with ratings set by their F2 place. A driver the game refuses (an F1 reserve, say) is skipped for the next one.

## Known issues

- Ties on points can rank you one place off the game's standings in the offer popup.
- After a promotion, Driver Details > Rivals can still show the released driver as your teammate rival, and a promoted driver can be missing from the Driver Rating Comparison.
- Repeating F2 after a poor season isn't possible yet: the F2 championship runs one season.

## How it works

`db/career.json` and `lang/career.json` are written by `tools/gen.py`; change the generator, not the files. The DLL is `native/src/f2_feeder.cpp`.

**Tile:** a core *career content* (`swofty/career_content.hpp`) with the data group `f1_career_f2_26` (suffix `FB27`). `Game_CareerWizard_StartsInF1` answers "no" for that group, so the wizard offers Skip to F1 / Start in F2.

**The career** is built like Progressive Career's, from copies of the game's 2026 career rows:
- the 8 data group rows and the group's entrant list;
- `shared_components`, the F1 championship (`F1_driver_career_f2_26`, statId 39) and the mode (`driver_career_f2_26`, statId 84), with its own `save_data.name`;
- the `F2_Default` setup list (F2, then F1) and the F2 championship as a second reference, as in `driver_career_25`;
- `F1CAREER_`, `F2CAREER_` and `ICONS_` lists in the 2026 content set.

**F2 runs in `F2_career_23`**, the only F2 championship no F1 25 mode uses (copies of F2 championships crashed). The grid is set on the championship (`f2_2026`), the calendar in the mode's `F2_Default` setup: event list `F2_career_21`, reshaped like F2 2024's, dated 2026. Its academies component names the `2026` academy group (it had none).

The Start in F2 scene and the Official Driver screen show 2026 drivers. Driver looks are found by the career's year, so drivers who only raced in 2025 don't appear.

**F2 calendar:** the event list's `season_0.track_event_list` names the season's list (`F2_2026_full`). `season_2026` has F2 season length rows (Long/Medium/Short) and `track_selections`, plus the short lists `F2_2026_8` and `F2_2026_3`.
- Round dates come from the race weekends (`schedule.all_track_events`): `rw_<track>` is dated 2025, `rw_<track>_26` 2026. F2 2026 has its own, `rw_f2_<track>_26` (stat_id 137–150), copies of the 2026 ones.
- Before the first track load the game rebuilds the season's weekends (`0x140bc9360`). It keeps only the calendar events whose `stat_id` is in the championship's `ComTrackList_0.default_track_list`, in that list's order. On `F2_career_23` that was `F2_2024_full`, so the season ran in 2024 order (Bahrain first, no Madrid, 13 rounds), and with 2026 events it had no weekends and crashed. The addon points it at `F2_feeder_order`, with the F2 2026 events.

**F1 2027 calendar:** the first F1 season races the FIA 2027 calendar (`F1_2027_full`, 24 rounds) on the game's 2026 data.
- Race weekends `rw_f1_<track>_27` (stat_id 151–174; Portugal and Turkey last, 173 and 174) are copies of the 2026 ones, each dated the Thursday of its 2027 weekend but in 2026: the game's season runs 11 Feb to 21 Dec 2026. A real 2027 date would need a `season_2027` row (the game only has `season_2023` to `season_2026`) and `season_2027` next to `season_2026` in some 40 fields.
- The list sorts by date (`sort_by_date`), like `F1_2026_full`. The short seasons `F1_2027_16` and `F1_2027_10` are the 2026 ones moved to 2027, with Monaco for Zandvoort.
- Portimão fills the game's `portimao` slot, so a copy of `rw_portimao` races there. Istanbul Park is a new track: its race weekend is Spa's with activity `istanbul_park`, and the addon adds that race weekend activity (`schedule.activities.activity_race_weekend`, a copy of Spa's, activity stat_id 814). Without it the season dropped the round.
- The F1 season keeps the game's event list (`F1_career_26`) and takes its list from the season length choice, as a stock career does. There are season length rows `F1_2027_Long/Medium/Short` and a `track_selections` row `F1_2027` (the Custom Season track pool). The game takes the first row that fits the mode and season, so while this career is in focus (its wizard, or the career loaded) the DLL sets the 2026 ones aside (wrapping `Game_SeasonLength_ApplyPreset`; preset type `0xff`, pool setup modes `0`) and puts them back afterwards.
- While the career is loaded, the DLL shows the GP titles and the championship name as 2027 with `swofty::lang`. The texts come from `native/src/titles_2027.inc`, generated from `tools/titles_2026.json` (Progressive Career's `data/titles.json`). F2 rounds use other keys (`lng_track_<code>_f2_title`).

**Saves:** the career has its own save container (`driver_career_f2_26`). Without the addon, the launcher keeps those containers and writes them back unchanged, so Steam Cloud can stay on.

## Lessons (for whoever continues)

- **A copied F2 `championships.Championship` crashes the boot** in `Game_ModeChampionship_Build`, even with only the name changed. Copies of the F1 championship work (Progressive does it). For F2, reuse a championship no mode uses.
- **An F1 championship can't be turned into F2 through the database.** `F1_career_23` with `game_setup_modes f2` crashes on entering the mode (`0xcb7330`): the game still gives it the F1 R&D component (`development_events`) and nothing in F2 wires it up.
- **A copied F1 event list that names its own track list per season** builds the weekends, but opens contract negotiations at the first Team HQ visit: `Career_ContractRenewal_ReplaceOldPosition` leaves the player with no team (`0x3fff`) and the Team HQ crashes. Take the list from the season length choice instead (0.6.4).
- **A race weekend's `activity` must name a race weekend activity row.** A track addon that adds a new track adds none, and the season drops the round silently.
- **Choose Driver screen:** the F2 tab lists only drivers who are both in the `career_f2_*_for_f1_*` list and on the championship's grid.
- **Saves:** changing which championships a mode uses breaks careers saved before (`Game_SaveData_DeserialiseContainer` on profile load). Steam Cloud brings the old save back if it's on.

## Build and test

```bash
SWOFTY_REPO=/path/to/SwoftyLauncher native/build.sh
```

The build needs zig (`winget install zig.zig`) and a SwoftyLauncher clone. In the launcher, install the folder, turn the addon on in a test profile with the two track addons, and clear `%LOCALAPPDATA%\Swofty\cache\overlay\<profile>` before testing a new version.

## Credits and licence

AGPL-3.0-or-later. The career layout and `tools/titles_2026.json` come from Swofty's Progressive Career (AGPL-3.0-or-later). Portimão by Pajko and Istanbul Park by Apoc & RK are separate addons.
