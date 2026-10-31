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
# Both careers race their F2 season in F2_career_23, the only F2 championship no F1 25 mode uses
# (copies of an F2 championship crash the boot). The grid lives in the championship, so they share
# it; each mode's F2_Default setup names its own calendar.
F2_CHAMP, F2_GRID = "F2_career_23", "f2_2026"

CAREERS = [
    {
        "key": "a", "group": "f1_career_f2_25", "mode": "driver_career_f2_25", "suffix": "FA26",
        "mode_stat": 83, "champ_stat": 38, "list_stat": 53,
        "f1_champ": "F1_driver_career_f2_25", "f1_events": "F1_career_26", "f1_year": None,
        "f2_events": "F2_career_23", "f2_year": 2025,
        # The Start in F2 scene's three cars (SpawnedF2): car spec and driver. Drivers need a look for
        # the career's 2026 season, which 2025-only F2 drivers lack.
        "scene_car": "f2_car_26",
        "scene": [("invicta_26 - driver_01", "rafael_camara"), ("hitech_26 - driver_02", "colton_herta"),
                  ("prema_26 - driver_01", "sebastian_montoya")],
        "title": "F2 2025 > F1 2026",
        "description": "Start in the 2025 Formula 2 season with its real grid and calendar, then move up to the 2026 Formula 1 season.",
    },
    {
        "key": "b", "group": "f1_career_f2_26", "mode": "driver_career_f2_26", "suffix": "FB27",
        "mode_stat": 84, "champ_stat": 39, "list_stat": 68,
        "f1_champ": "F1_driver_career_f2_26", "f1_events": "F1_career_26", "f1_year": None,
        "f2_events": "F2_career_21", "f2_year": 2026,
        "scene_car": "f2_car_26",
        "scene": [("invicta_26 - driver_01", "rafael_camara"), ("hitech_26 - driver_02", "colton_herta"),
                  ("prema_26 - driver_01", "sebastian_montoya")],
        "title": "F2 2026 > F1 2027",
        "description": "Start in the 2026 Formula 2 season with its real grid and calendar, then move up to Formula 1 on the 2026 cars and line-ups.",
    },
]

# Season length lists per F2 year: (label, events). The full lists ship with the game.
F2_SHORT = {
    2025: [("F2_2025_8", ["rw_melbourne", "rw_bahrain", "rw_austria", "rw_silverstone", "rw_spa_francorchamps",
                          "rw_monza", "rw_losail", "rw_abu_dhabi"]),
           ("F2_2025_3", ["rw_melbourne", "rw_silverstone", "rw_abu_dhabi"])],
    2026: [("F2_2026_8", ["rw_f2_melbourne_26", "rw_f2_bahrain_26", "rw_f2_austria_26", "rw_f2_silverstone_26",
                          "rw_f2_spa_francorchamps_26", "rw_f2_monza_26", "rw_f2_losail_26", "rw_f2_abu_dhabi_26"]),
           ("F2_2026_3", ["rw_f2_melbourne_26", "rw_f2_silverstone_26", "rw_f2_abu_dhabi_26"])],
}

# The stock F2_2026_full names the race weekends dated 2025 (rw_<track>), except rw_madrid_26. A career
# lays a season's rounds out by those weekends' date_time, and in the 2026 season the 2025 Melbourne
# weekend fell out (Frida trace of the calendar entries: 2025-03-13, 2025-04-10, ..., Madring 2026-09-10).
# So the F2 2026 rounds get race weekends of their own, copies of rw_<track>_26 with its 2026 date
# (stat ids after the stock 136).
F2_2026_TRACKS = ["melbourne", "bahrain", "jeddah", "monaco", "catalunya", "austria", "silverstone",
                  "spa_francorchamps", "hungaroring", "monza", "madrid", "baku", "losail", "abu_dhabi"]
F2_2026_FULL = [f"rw_f2_{t}_26" for t in F2_2026_TRACKS]
F2_2025_FULL = ["rw_melbourne", "rw_bahrain", "rw_jeddah", "rw_imola", "rw_monaco", "rw_catalunya", "rw_austria",
                "rw_silverstone", "rw_spa_francorchamps", "rw_hungaroring", "rw_monza", "rw_baku", "rw_losail",
                "rw_abu_dhabi"]  # stock F2_2025_full

# Before a season's first track load the game rebuilds its weekends (0x140bc9360): it keeps the calendar
# entries whose event stat id is in the championship's ComTrackList_0.default_track_list (not in the key
# catalogue), in that list's order. F2_career_23's is F2_2024_full: the rounds raced in the F2 2024 order
# (Bahrain first, Madring dropped: 13), and the rw_f2_*_26 weekends matched nothing, which left the
# season with no weekends and crashed the first track load (Frida trace, 2026-10-31). Both careers share
# F2_career_23, so its list names both seasons' weekends: each calendar holds only its own year's.
F2_ORDER = "F2_feeder_order"
EVENT_STAT = 137

# Race dates (month, day) of the F2 rounds, the F1 weekends they support. The game orders a career's
# rounds by each track's date in the season's track data set, and the stock F2 sets carry 2023 dates:
# Melbourne (1 April) came after Bahrain and Jeddah, and in f2_2026_sprint Madring shares Catalunya's
# 3 June, so the 14 rounds raced as 13. An F2 weekend (sprint and feature race) reads the f2_<year>_sprint
# set; the plain f2_<year> set gets the dates too.
# 2026 is the game's f1_2026 set; 2025 is the real calendar (f1_2025 has no dates of its own).
F2_DATES = {
    2025: {"melbourne": (3, 16), "bahrain": (4, 13), "jeddah": (4, 20), "imola": (5, 18), "monaco": (5, 25),
           "catalunya": (6, 1), "austria": (6, 29), "silverstone": (7, 6), "spa_francorchamps": (7, 27),
           "hungaroring": (8, 3), "monza": (9, 7), "baku": (9, 21), "losail": (11, 30), "abu_dhabi": (12, 7)},
    2026: {"melbourne": (3, 8), "bahrain": (4, 12), "jeddah": (4, 19), "monaco": (6, 7), "catalunya": (6, 14),
           "austria": (6, 28), "silverstone": (7, 5), "spa_francorchamps": (7, 19), "hungaroring": (7, 26),
           "monza": (9, 6), "madrid": (9, 13), "baku": (9, 26), "losail": (11, 29), "abu_dhabi": (12, 6)},
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

    # The F2 season (F2_CHAMP, shared): set up once. F1_career_23 turned into an F2 championship
    # crashed on entering the mode: the game still gave it the F1 R&D component (development_events)
    # and nothing wired it up. Clearing its save_data, OptionsSystemConfiguration and statId, and
    # adding it to the game's F2 championship list, did not stop that.
    if c is CAREERS[0]:
        f2 = f"championships.Championship[_metadata_={F2_CHAMP}]"
        put(f"{f2}.ComCareerSetup_0", "entrant_list", F2_GRID)
        put(f"{f2}.ComCareerSetup_0", "setup_data", g)
    ev = f"schedule.event_lists.event_list[_metadata_={c['f2_events']}]"
    if c["f2_events"] != "F2_career_23":  # shape it like F2_2024_career: one season, F2 start and end
        put(ev, "_season_instancecount_", 1)
        put(f"{ev}.season_0", "_fixed_event_reference_instancecount_", 2)
        for i, name in enumerate(("f2_start_season", "f2_end_season")):
            put(f"{ev}.season_0.fixed_event_reference_{i}", "_metadata_", f"{name}_ref")
            put(f"{ev}.season_0.fixed_event_reference_{i}", "event_name", name)
        for d, (day, month) in enumerate(((1, "February"), (25, "December"))):
            put(f"{ev}.season_0.date_time_{d}", "day", day)
            put(f"{ev}.season_0.date_time_{d}", "month", month)
    for d in (0, 1):
        put(f"{ev}.season_0.date_time_{d}", "year", c["f2_year"])
    # The Custom Season screen picks the track list by the season's year, but a new career raced the
    # F2 2024 order (Bahrain first, 8 rounds from F2_2024_8). Name the season's list outright, as
    # Classic Career does for its seasons (cs_calendar_<year>).
    put(f"{ev}.season_0", "track_event_list", f"F2_{c['f2_year']}_full")

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
    for i, (ch, evl) in enumerate(((F2_CHAMP, c["f2_events"]), (c["f1_champ"], c["f1_events"]))):
        put(lst, f"ChampionshipSetup_{i}._metadata_", "ChampionshipSetup", 2)
        put(lst, f"ChampionshipSetup_{i}.championship", ch, 4)
        put(lst, f"ChampionshipSetup_{i}.event_list", evl, 4)
    refs = f"{mode}.championships"
    put(refs, "ChampionshipReference_1._metadata_", "ChampionshipReference", 2)
    put(refs, "ChampionshipReference_1.Championship", F2_CHAMP, 4)
    put(refs, "_ChampionshipReference_instancecount_", 2, 0)

    ops.append({"op": "list-append", "select": CS, "field": "available_game_modes_0.game_mode_championships",
                "value": m})
    # The game loads an entrant list only when the content set's whitelist (row 0 of
    # available_entrant_lists, the only row read) names it or a loaded list references it; without it
    # the data group's registry slot stays empty and the tile press crashed in the router. F2_GRID
    # (f2_2026) is already in it.
    ops.append({"op": "list-append", "select": f"{CS}.available_entrant_lists_0", "field": "entrant_lists",
                "value": g})
    base = c["suffix"][:2]
    for name in (base, c["suffix"]):
        add(CS, f"{OFFICIAL}[name=F1CAREER_26]", name=f"F1CAREER_{name}", entrant_list="f1_2026")
        add(CS, f"{OFFICIAL}[name=F2CAREER_26]", name=f"F2CAREER_{name}", entrant_list=F2_GRID)
        add(CS, f"{OFFICIAL}[name=ICONS_26]", name=f"ICONS_{name}")

    spawned = f"career.career_setup.CareerSetupDataGroup[_metadata_={g}].SpawnedF2_0"
    for n, (spec, driver) in enumerate(c["scene"], 1):
        put(spawned, f"car_{n}", c["scene_car"])
        put(spawned, f"car_{n}_spec", spec)
        put(spawned, f"driver_{n}", driver)


def track_list(src, label, events):
    add(TL, f"{TL}.track_event_list[_metadata_={src}]", _metadata_=label)
    set_events(label, events)


def set_events(label, events):
    sel = f"{TL}.track_event_list[_metadata_={label}]"
    for i, e in enumerate(events):
        put(f"{sel}.fixed_event_reference_{i}", "_metadata_", f"fixed_event_reference_{e}")
        put(f"{sel}.fixed_event_reference_{i}", "event_name", e)


def calendars():
    ev = "schedule.all_track_events"
    for k, t in enumerate(F2_2026_TRACKS):
        add(ev, f"{ev}.fixed_event[_metadata_=rw_{t}_26]", _metadata_=f"rw_f2_{t}_26", stat_id=EVENT_STAT + k)
        # date_time isn't in the key catalogue, so add-row leaves it out; the boot reads all seven fields
        # of every race weekend (a missing one crashed in Trk_TrackData_LoadTrackListNodes).
        for field, type_ in (("year", 0), ("month", 3), ("day", 0), ("hour", 0), ("minute", 0), ("second", 0),
                             ("daylight_saving", 5)):
            ops.append({"op": "set", "select": f"{ev}.fixed_event[_metadata_=rw_f2_{t}_26]",
                        "field": f"date_time.{field}", "create": True, "type": type_,
                        "value_from": f"{ev}.fixed_event[_metadata_=rw_{t}_26].date_time.{field}"})
    set_events("F2_2026_full", F2_2026_FULL)
    add(TL, f"{TL}.track_event_list[_metadata_=F2_2024_full]", _metadata_=F2_ORDER)
    sel = f"{TL}.track_event_list[_metadata_={F2_ORDER}]"
    order = F2_2025_FULL + F2_2026_FULL
    put(sel, "_fixed_event_reference_instancecount_", len(order), 0)
    for i, e in enumerate(order):
        put(f"{sel}.fixed_event_reference_{i}", "_metadata_", f"fixed_event_reference_{e}", 2)
        put(f"{sel}.fixed_event_reference_{i}", "event_name", e, 4)
    put(f"championships.Championship[_metadata_={F2_CHAMP}].ComTrackList_0", "default_track_list", F2_ORDER, 4)
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
    ds = "track_data.game_setup.TrackDataSet"
    for year, dates in F2_DATES.items():
        for track, (month, day) in dates.items():
            for kind in ("", "_sprint"):
                sel = f"{ds}[_metadata_=f2_{year}{kind}].TrackData[_metadata_={track}].TrackGeneralData_0"
                put(sel, "weekend_year", year)
                put(sel, "weekend_month", month)
                put(sel, "weekend_day", day)
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
