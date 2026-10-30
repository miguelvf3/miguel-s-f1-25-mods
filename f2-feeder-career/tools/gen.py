#!/usr/bin/env python3
"""Writes db/career.json and lang/career.json for F2 Feeder Career.

Two careers, each its own Addons tile, game mode, data group and save container, built like
Progressive Career's (copies of the stock 2026 driver career's rows):

  A  F2 2025 -> F1 2026   F2 on F2_career_23 (F1 23/24 career championship F1 25 doesn't offer)
  B  F2 2026 -> F1 2027   F2 on F1_career_23 (same), turned into an F2 championship. Its F1 seasons
                          are dated 2026: the season lists only take game_setup.seasons rows, and
                          season_2027 crashed the boot (0x142a0ddb0 builds their masks unchecked).

Copies of an F2 championship crashed the game at boot (Game_ModeChampionship_Build), so each F2
season takes over an unused stock championship instead. The stock 2026 career is left as it is.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

DATA_GROUP_TABLES = [
    ("career.career_setup", "CareerSetupDataGroup"),
    ("accolades", "AccoladeDataGroup"),
    ("career.leverage", "LeverageDataGroup"),
    ("career.my_team_renown_and_form", "MyTeamRenownAndFormDataGroup"),
    ("career.my_team_sponsors", "MyTeamSponsorsDataGroup"),
    ("recognition", "RecognitionDataGroup"),
    ("resource_points", "data_group"),
    ("specialist_manager", "dataGroup"),
]
SHARED = "championships.shared_components"
CS = "content_sets.content_set[_metadata_=2026]"
OFFICIAL = f"{CS}.official_entrant_list"
TL = "schedule.track_lists"
DY = "schedule.dynamic_event_data.default_ui_track_list_data"
TS = f"{DY}.track_selections"
F1_SEASONS = 10

CAREERS = [
    {
        "key": "a", "group": "f1_career_f2_25", "mode": "driver_career_f2_25", "suffix": "FA26",
        "mode_stat": 83, "champ_stat": 38, "list_stat": 53,
        "f1_champ": "F1_driver_career_f2_25", "f1_events": "F1_career_26", "f1_year": None,
        "f2_champ": "F2_career_23", "f2_events": "F2_career_23", "f2_year": 2025, "f2_grid": "f2_2025",
        "f2_choose": None,  # keeps F2CAREER_26's career_f2_2025_for_f1_2026
        "title": "F2 2025 > F1 2026",
        "description": "Start in the 2025 Formula 2 season with its real grid and calendar, then move up to the 2026 Formula 1 season.",
    },
    {
        "key": "b", "group": "f1_career_f2_26", "mode": "driver_career_f2_26", "suffix": "FB27",
        "mode_stat": 84, "champ_stat": 39, "list_stat": 68,
        "f1_champ": "F1_driver_career_f2_26", "f1_events": "F1_career_26", "f1_year": None,
        "f2_champ": "F1_career_23", "f2_events": "F2_career_21", "f2_year": 2026, "f2_grid": "f2_2026",
        "f2_choose": "f2_2026",
        "title": "F2 2026 > F1 2027",
        "description": "Start in the 2026 Formula 2 season with its real grid and calendar, then move up to Formula 1 on the 2026 cars and line-ups.",
    },
]

# Season length lists per F2 year: (label, events). The full lists ship with the game.
F2_SHORT = {
    2025: [("F2_2025_8", ["rw_melbourne", "rw_bahrain", "rw_austria", "rw_silverstone", "rw_spa_francorchamps",
                          "rw_monza", "rw_losail", "rw_abu_dhabi"]),
           ("F2_2025_3", ["rw_melbourne", "rw_silverstone", "rw_abu_dhabi"])],
    2026: [("F2_2026_8", ["rw_melbourne", "rw_bahrain", "rw_austria", "rw_silverstone", "rw_spa_francorchamps",
                          "rw_monza", "rw_losail", "rw_abu_dhabi"]),
           ("F2_2026_3", ["rw_melbourne", "rw_silverstone", "rw_abu_dhabi"])],
}

ops = []


def add(table, src, **set_):
    op = {"op": "add-row", "table": table, "from": src}
    if set_:
        op["set"] = {k: str(v) for k, v in set_.items()}
    ops.append(op)


def put(select, field, value, type_=None):
    op = {"op": "set", "select": select, "field": field, "value": str(value)}
    if type_ is not None:
        op.update(create=True, type=type_)
    ops.append(op)


def career(c):
    g, m = c["group"], c["mode"]
    for table, word in DATA_GROUP_TABLES:
        add(table, f"{table}.{word}[_metadata_=f1_career_2026]", _metadata_=g)
    # The entrant list stat_id indexes the game's entrant list registry, which ends at the stock lists'
    # highest id (104): a list above it is not found and pressing the tile crashed in the router.
    # 53 and 68 are unused stock ids below it.
    add("entrants.entrant_lists", "entrants.entrant_lists.entrant_list[_metadata_=f1_career_2026]",
        _metadata_=g, stat_id=c["list_stat"])

    if c["f1_year"]:
        add("schedule.event_lists", "schedule.event_lists.event_list[_metadata_=F1_career_26]",
            _metadata_=c["f1_events"])
        sel = f"schedule.event_lists.event_list[_metadata_={c['f1_events']}]"
        for s in range(F1_SEASONS):
            for d in (0, 1):
                put(f"{sel}.season_{s}.date_time_{d}", "year", c["f1_year"])

    shared = f"shared_driver_career_{c['key']}_f2f_components"
    add(SHARED, f"{SHARED}.SharedComponents[_metadata_=shared_driver_career_26_components]",
        **{"_metadata_": shared, "ComAccolades_0.data_group": g, "ComResourcePoints_0.data_group": g,
           "ComSchedule_0.event_list": c["f1_events"]})
    add("championships", "championships.Championship[_metadata_=F1_driver_career_26]",
        **{"_metadata_": c["f1_champ"], "statId": c["champ_stat"], "event_list": c["f1_events"],
           "ComCareerSetup_0.entrant_list": g, "ComCareerSetup_0.setup_data": g,
           "ComInfluence_0.data_group": g, "ComSpecialists_0.specialist_data": g,
           "shared_components": f"shared_driver_career_components,shared_driver_career_offline_components,{shared}"})

    # The F2 season: a stock championship no F1 25 mode uses.
    f2 = f"championships.Championship[_metadata_={c['f2_champ']}]"
    if c["f2_champ"] != "F2_career_23":  # an F1 23 championship turned into an F2 one, like F2_career_23
        put(f2, "game_setup_modes", "f2")
        put(f2, "event_list", c["f2_events"])
        put(f2, "_OptionsSystemConfiguration_instancecount_", 0)
    put(f"{f2}.ComCareerSetup_0", "entrant_list", c["f2_grid"])
    put(f"{f2}.ComCareerSetup_0", "setup_data", g)
    ev = f"schedule.event_lists.event_list[_metadata_={c['f2_events']}]"
    if c["f2_events"] != "F2_career_23":  # shape it like F2_2024_career: one season, F2 start and end
        put(ev, "_season_instancecount_", 1)
        put(f"{ev}.season_0", "track_event_list", "")
        put(f"{ev}.season_0", "_fixed_event_reference_instancecount_", 2)
        for i, name in enumerate(("f2_start_season", "f2_end_season")):
            put(f"{ev}.season_0.fixed_event_reference_{i}", "_metadata_", f"{name}_ref")
            put(f"{ev}.season_0.fixed_event_reference_{i}", "event_name", name)
        for d, (day, month) in enumerate(((1, "February"), (25, "December"))):
            put(f"{ev}.season_0.date_time_{d}", "day", day)
            put(f"{ev}.season_0.date_time_{d}", "month", month)
    for d in (0, 1):
        put(f"{ev}.season_0.date_time_{d}", "year", c["f2_year"])

    mode = f"game_modes.ModeChampionship[_metadata_={m}]"
    setups = f"{mode}.championship_setups"
    add("game_modes", "game_modes.ModeChampionship[_metadata_=driver_career_26]",
        **{"allowedComponentSaveGroups": "18446744073709551615", "_metadata_": m, "statId": c["mode_stat"],
           "save_data.name": m, "event_list": c["f1_events"],
           "championship_setups.ChampionshipSetupList_0.ChampionshipSetup_0.championship": c["f1_champ"],
           "championship_setups.ChampionshipSetupList_0.ChampionshipSetup_0.event_list": c["f1_events"],
           "championships.ChampionshipReference_0.Championship": c["f1_champ"]})
    # F2_Default like driver_career_25's: the F2 season, then F1.
    add(setups, f"{setups}.ChampionshipSetupList_0", _metadata_="F2_Default")
    lst = f"{setups}.ChampionshipSetupList[_metadata_=F2_Default]"
    put(lst, "_ChampionshipSetup_instancecount_", 2, 0)
    for i, (ch, evl) in enumerate(((c["f2_champ"], c["f2_events"]), (c["f1_champ"], c["f1_events"]))):
        put(lst, f"ChampionshipSetup_{i}._metadata_", "ChampionshipSetup", 2)
        put(lst, f"ChampionshipSetup_{i}.championship", ch, 4)
        put(lst, f"ChampionshipSetup_{i}.event_list", evl, 4)
    refs = f"{mode}.championships"
    put(refs, "ChampionshipReference_1._metadata_", "ChampionshipReference", 2)
    put(refs, "ChampionshipReference_1.Championship", c["f2_champ"], 4)
    put(refs, "_ChampionshipReference_instancecount_", 2, 0)

    ops.append({"op": "list-append", "select": CS, "field": "available_game_modes_0.game_mode_championships",
                "value": m})
    # The game loads an entrant list only when the content set's whitelist (row 0 of
    # available_entrant_lists, the only row read) names it or a loaded list references it; without it
    # the data group's registry slot stays empty and the tile press crashed in the router.
    ops.append({"op": "list-append", "select": f"{CS}.available_entrant_lists_0", "field": "entrant_lists",
                "value": f"{g},{c['f2_grid']}"})
    base = c["suffix"][:2]
    for name in (base, c["suffix"]):
        add(CS, f"{OFFICIAL}[name=F1CAREER_26]", name=f"F1CAREER_{name}", entrant_list="f1_2026")
        f2row = {"name": f"F2CAREER_{name}"}
        if c["f2_choose"]:
            f2row["entrant_list"] = c["f2_choose"]
        add(CS, f"{OFFICIAL}[name=F2CAREER_26]", **f2row)
        add(CS, f"{OFFICIAL}[name=ICONS_26]", name=f"ICONS_{name}")

    spawned = f"career.career_setup.CareerSetupDataGroup[_metadata_={g}].SpawnedF2_0"
    for n, (spec, driver) in enumerate((("invicta_25 - driver_01", "leonardo_fornaroli"),
                                        ("mpmotorsport_25 - driver_02", "richard_verschoor"),
                                        ("campos_25 - driver_01", "arvid_lindblad")), 1):
        put(spawned, f"car_{n}", "f2_car_25")
        put(spawned, f"car_{n}_spec", spec)
        put(spawned, f"driver_{n}", driver)


def track_list(src, label, events):
    add(TL, f"{TL}.track_event_list[_metadata_={src}]", _metadata_=label)
    sel = f"{TL}.track_event_list[_metadata_={label}]"
    for i, e in enumerate(events):
        put(f"{sel}.fixed_event_reference_{i}", "_metadata_", f"fixed_event_reference_{e}")
        put(f"{sel}.fixed_event_reference_{i}", "event_name", e)


def calendars():
    # F2 season lengths and track order follow the F2 season's year (its event list's dates).
    for year, lists in F2_SHORT.items():
        track_list("F2_2024_8", *lists[0])
        track_list("F2_2024_3", *lists[1])
        for kind, label in (("Long", f"F2_{year}_full"), ("Medium", lists[0][0]), ("Short", lists[1][0])):
            add(DY, f"{DY}.custom_dynamic_available_track_list_data[_metadata_=F2_2024_{kind}]",
                _metadata_=f"F2_{year}_{kind}", seasons=f"season_{year}", track_list_name=label)
        add(TS, f"{TS}.tracks_selection[_metadata_=F1_2026]",
            **{"_metadata_": f"F2_{year}", "game_setup_modes": "f2", "seasons": f"season_{year}",
               "default": f"F2_{year}_full", "season_0.track_event_list": f"F2_{year}_full"})
    for kind in ("Long", "Medium", "Short"):
        put(f"{DY}.custom_dynamic_available_track_list_data[_metadata_=F2_2024_{kind}]", "seasons",
            "season_2023,season_2024")
    add(TS, f"{TS}.tracks_selection[_metadata_=F1_2026]",
        **{"_metadata_": "F2_2024", "game_setup_modes": "f2", "seasons": "season_2023,season_2024",
           "default": "F2_2024_full", "season_0.track_event_list": "F2_2024_full"})


for c in CAREERS:
    career(c)
calendars()

# Key names of the copied rows the launcher's key catalogue lacks (championship_setups and the like).
# Taken from Progressive Career's db/career.json (Swofty, AGPL-3.0-or-later), which copies the same
# stock rows: the f1_career_2026 data groups, F1_career_26, shared_driver_career_26_components,
# F1_driver_career_26 and driver_career_26.
fields = json.loads((ROOT / "tools" / "fields.json").read_text(encoding="utf-8"))
fields[OFFICIAL] = ["_metadata_", "entrant_list", "name"]
db = {"schema_version": 1, "fields": fields, "ops": ops}
(ROOT / "db" / "career.json").write_text(json.dumps(db, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")

lang = []
for c in CAREERS:
    lang += [{"op": "lang.set", "language": "*", "key": f"lng_f2f_{c['key']}_title", "value": c["title"]},
             {"op": "lang.set", "language": "*", "key": f"lng_f2f_{c['key']}_description", "value": c["description"]}]
(ROOT / "lang").mkdir(exist_ok=True)
(ROOT / "lang" / "career.json").write_text(
    json.dumps({"schema_version": 1, "ops": lang}, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
print(f"{len(ops)} database ops, {len(lang)} strings")
