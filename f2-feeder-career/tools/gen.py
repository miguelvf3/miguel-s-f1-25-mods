#!/usr/bin/env python3
"""Writes db/career.json and lang/career.json for F2 Feeder Career.

One career, F2 2026 -> F1 2027, with its own Addons tile, game mode, data group and save container,
built like Progressive Career's (copies of the stock 2026 driver career's rows). Its F2 season races
in F2_career_23, the F1 23/24 career championship F1 25 doesn't offer: copies of an F2 championship
crashed the game at boot (Game_ModeChampionship_Build). Its F1 seasons are dated 2026: the season
lists only take game_setup.seasons rows, and season_2027 crashed the boot (0x142a0ddb0 builds their
masks unchecked). The stock 2026 career is left as it is.

Up to 0.4.9 there was a second career, F2 2025 -> F1 2026 (group f1_career_f2_25); 0.5.0 dropped it.
"""
import itertools
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
ACCOLADE_DRIVERS = ["emmo_fittipaldi-jr", "tasanapol_inthraphuvasak", "noel_leon", "roman_bilinski", "colton_herta",
                    "rafael_camara", "mari_boya", "martinius_stenshorne", "laurens_vanhoepen", "nicolas_varrone"]
# Oliver Goethe's goals in the stock 2026 accolades: accolade and its three tiers.
ACCOLADE_GOALS = [("race_starts", (1, 3, 5)), ("top_10_finishes", (1, 2, 4)), ("top_5_finishes", (1, 2, 3)),
                  ("championship_points", (2, 3, 4)), ("top_5_championship", (1, 2, 4))]
SHARED = "championships.shared_components"
CS = "content_sets.content_set[_metadata_=2026]"
OFFICIAL = f"{CS}.official_entrant_list"
TL = "schedule.track_lists"
DY = "schedule.dynamic_event_data.default_ui_track_list_data"
TS = f"{DY}.track_selections"
F1_SEASONS = 10
# The career races its F2 season in F2_career_23, the only F2 championship no F1 25 mode uses (copies
# of an F2 championship crash the boot). The grid lives in the championship; the mode's F2_Default
# setup names the calendar.
F2_CHAMP, F2_GRID = "F2_career_23", "f2_2026"

CAREERS = [
    {
        # key "b" and the FB27 suffix kept from 0.4.x, so its strings and saves stay as they were.
        "key": "b", "group": "f1_career_f2_26", "mode": "driver_career_f2_26", "suffix": "FB27",
        "mode_stat": 84, "champ_stat": 39, "list_stat": 68,
        "f1_champ": "F1_driver_career_f2_26", "f1_events": "F1_career_26", "f1_track_list": "F1_2027_full",
        "f2_events": "F2_career_21", "f2_year": 2026,
        # The Start in F2 scene's three cars (SpawnedF2): car spec and driver.
        "scene_car": "f2_car_26",
        "scene": [("invicta_26 - driver_01", "rafael_camara"), ("hitech_26 - driver_02", "colton_herta"),
                  ("prema_26 - driver_01", "sebastian_montoya")],
        "title": "F2 2026 > F1 2027",
        "description": "Start in the 2026 Formula 2 season with its real grid and calendar, then move up to Formula 1 on the 2026 cars and line-ups.",
    },
]

# Season length lists per F2 year: (label, events). The full lists ship with the game.
F2_SHORT = {
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

# Before a season's first track load the game rebuilds its weekends (0x140bc9360): it keeps the calendar
# entries whose event stat id is in the championship's ComTrackList_0.default_track_list (not in the key
# catalogue), in that list's order. F2_career_23's is F2_2024_full: the rounds raced in the F2 2024 order
# (Bahrain first, Madring dropped: 13), and the rw_f2_*_26 weekends matched nothing, which left the
# season with no weekends and crashed the first track load (Frida trace, 2026-10-31). Its list is the
# F2 2026 weekends.
F2_ORDER = "F2_feeder_order"
EVENT_STAT = 137

# Race dates (month, day) of the F2 rounds, the F1 weekends they support, from the game's f1_2026 set.
# The stock F2 track data sets carry 2023 dates (in f2_2026_sprint Madring shares Catalunya's 3 June).
# They turned out not to order the calendar (the race weekends' dates and F2_ORDER do), but they are
# what the F2 track data says. An F2 weekend reads the f2_<year>_sprint set; f2_<year> gets them too.
F2_DATES = {
    2026: {"melbourne": (3, 8), "bahrain": (4, 12), "jeddah": (4, 19), "monaco": (6, 7), "catalunya": (6, 14),
           "austria": (6, 28), "silverstone": (7, 5), "spa_francorchamps": (7, 19), "hungaroring": (7, 26),
           "monza": (9, 6), "madrid": (9, 13), "baku": (9, 26), "losail": (11, 29), "abu_dhabi": (12, 6)},
}

# The F1 season after F2 2026 races the 2027 calendar (formula1.com/en/racing/2027, 24 rounds) on the
# game's 2026 data: new race weekends, copies of the 2026 ones, dated the Thursday of each 2027 weekend
# but in 2026. A 2027 date would need a season_2027 row (game_setup.seasons stops at 2026) and that
# season next to season_2026 in some 40 fields; the career's season runs 11 February to 21 December
# 2026. F1_2026_full sorts its rounds by date (sort_by_date), and so does the copy.
# (track, source race weekend, month, day)
F1_2027 = [("bahrain", "rw_bahrain_26", "March", 11), ("jeddah", "rw_jeddah_26", "March", 18),
           ("melbourne", "rw_melbourne_26", "April", 1), ("suzuka", "rw_suzuka_26", "April", 8),
           ("shanghai", "rw_shanghai_26", "April", 15), ("miami", "rw_miami_26", "April", 29),
           ("montreal", "rw_montreal_26", "May", 20), ("monaco", "rw_monaco_26", "June", 3),
           ("silverstone", "rw_silverstone_26", "July", 1),
           ("austria", "rw_austria_26", "July", 8), ("spa_francorchamps", "rw_spa_francorchamps_26", "July", 22),
           ("hungaroring", "rw_hungaroring_26", "July", 29), ("monza", "rw_monza_26", "September", 2),
           ("madrid", "rw_madrid_26", "September", 9), ("baku", "rw_baku_26", "September", 23),
           ("singapore", "rw_singapore_26", "October", 7), ("texas", "rw_texas_26", "October", 21),
           ("mexico", "rw_mexico_26", "October", 28), ("brazil", "rw_brazil_26", "November", 4),
           ("las_vegas", "rw_las_vegas_26", "November", 17), ("losail", "rw_losail_26", "December", 2),
           ("abu_dhabi", "rw_abu_dhabi_26", "December", 9)]
# Portugal and Turkey race on track addons, as optional rounds: F1 25 has neither circuit's files. `requires`
# has no optional form and the Addon Browser refuses an addon it can't resolve (Portimao isn't there), so each
# is a db setting named after the track, off by default: 22 rounds, up to 24 with both on.
# mod.portimao (Pajko) fills the stock portimao slot (its data cloned from Zandvoort; without it a copy of
# rw_portimao made a calendar round and no season weekend, 0.6.0), and track.istanbul_park (Apoc & RK)
# adds istanbul_park cloned from Spa, so its race weekend is Spa's with the activity changed. A race
# weekend's activity names a schedule.activities.activity_race_weekend row (the stock portimao one is
# there; with no istanbul_park row the season left the round out, 0.7.0: 23 rounds), so this one copies
# Spa's at a free activity stat_id (they're sparse: Madrid's is 813). Listed last so the other rounds keep
# their stat_ids. (track, source race weekend, month, day, activity)
F1_2027_ADDON_TRACKS = [("portimao", "rw_portimao", "June", 17, None),
                        ("istanbul_park", "rw_spa_francorchamps_26", "September", 30, "istanbul_park")]
ACTIVITY_STAT = 814
TRACK_ADDONS = {"portimao": "mod.portimao", "istanbul_park": "track.istanbul_park"}
F1_EVENT_STAT = EVENT_STAT + len(F2_2026_TRACKS)
# The 16 and 10 race seasons: F1_2026_16 and F1_2026_10's tracks, Monaco for Zandvoort.
F1_2027_16 = ["melbourne", "shanghai", "suzuka", "bahrain", "miami", "montreal", "austria", "silverstone",
              "spa_francorchamps", "monaco", "monza", "madrid", "brazil", "las_vegas", "losail", "abu_dhabi"]
F1_2027_10 = ["melbourne", "suzuka", "bahrain", "austria", "silverstone", "spa_francorchamps", "monza", "madrid",
              "brazil", "las_vegas"]

# In that season the race weekend and championship titles read 2027: f2_feeder.dll overrides these
# keys while the career is loaded (titles_2027.inc). tools/titles_2026.json is Progressive Career's
# data/titles.json (Swofty, AGPL-3.0-or-later): every language's stock text of the 2026 titles. The F2
# rounds read other keys (lng_track_<code>_f2_title).
TITLE_YEAR = 2027

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
    # The stock 2026 accolades have no row for ten F2 2026 drivers, and the Accolades screen was empty
    # for one of them (Camara, 0.7.5 test). They get Oliver Goethe's goals, the stock row of an F2 rookie.
    # add-row copies only the row's own two keys here (the catalogue has no entry for the goals), so
    # the goals are written out.
    drivers = f"accolades.AccoladeDataGroup[_metadata_={g}].driver_data"
    for d in ACCOLADE_DRIVERS:
        add(drivers, f"{drivers}.AccoladeDriver[_metadata_=oliver_goethe]", _metadata_=d, driver=d)
        row = f"{drivers}.AccoladeDriver[_metadata_={d}]"
        put(row, "_AccoladeDriverGoal_instancecount_", len(ACCOLADE_GOALS), 0)
        for n, (accolade, tiers) in enumerate(ACCOLADE_GOALS):
            goal = f"AccoladeDriverGoal_{n}"
            put(row, f"{goal}._metadata_", f"accolade_{n}", 2)
            put(row, f"{goal}.accolade", accolade, 4)
            put(row, f"{goal}.initial_value", 0, 1)
            for k, v in zip(("first_tier", "second_tier", "third_tier"), tiers):
                put(row, f"{goal}.{k}", v, 0)
    # The entrant list stat_id indexes the game's entrant list registry, which ends at the stock lists'
    # highest id (104): a list above it is not found and pressing the tile crashed in the router.
    # 53 and 68 are unused stock ids below it.
    # Its references bring in the grid, the driver pool, the reserves and the F2 drivers, whose F2 teams the
    # game clears at career creation so they're F1 free agents the market can sign. The stock list names
    # the F2 2025 grid (career_f2_2025_for_f1_2026); this career's F2 is 2026, so it names f2_2026.
    add("entrants.entrant_lists", "entrants.entrant_lists.entrant_list[_metadata_=f1_career_2026]",
        _metadata_=g, stat_id=c["list_stat"],
        entrant_list_references=f"f1_2026,f1_career_2026_driver_pool,f1_career_2026_reserve_drivers,{F2_GRID}")

    # The F1 seasons are the stock career's (F1_career_26): the Custom Season screen picks the track list,
    # F1_2027_Long among them (calendars()). A copy naming F1_2027_full in each season (0.6.0-0.6.3) built
    # the 2027 weekends, but at the first Team HQ visit the game opened contract negotiations, signed the
    # player's driver to his team and emptied the player's own seat (Career_ContractRenewal_ReplaceOldPosition,
    # team 0x3fff), and the Team HQ crashed on the missing team (Frida, 2026-11-02).

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
        # Its academies component names no academy group (F2_2024_career's names 2025, F1_driver_career_26's
        # 2026). A player's F2 academy is said to lose its recognition bonus in F1 (EA forum); try the group
        # the F1 season reads.
        put(f2, "ComDriverAcademies_0.data_group", "2026", 4)
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


def race_weekend(name, src, stat_id, date=None, activity=None):
    """A race weekend copied from src, dated like it or (year, month, day), at its track or activity."""
    ev = "schedule.all_track_events"
    add(ev, f"{ev}.fixed_event[_metadata_={src}]", _metadata_=name, stat_id=stat_id,
        **({"activity": activity} if activity else {}))
    # date_time isn't in the key catalogue, so add-row leaves it out; the boot reads all seven fields
    # of every race weekend (a missing one crashed in Trk_TrackData_LoadTrackListNodes).
    own = dict(zip(("year", "month", "day"), date or ()))
    for field, type_ in (("year", 0), ("month", 3), ("day", 0), ("hour", 0), ("minute", 0), ("second", 0),
                         ("daylight_saving", 5)):
        op = {"op": "set", "select": f"{ev}.fixed_event[_metadata_={name}]", "field": f"date_time.{field}",
              "create": True, "type": type_}
        if field in own:
            op["value"] = str(own[field])
        else:
            op["value_from"] = f"{ev}.fixed_event[_metadata_={src}].date_time.{field}"
        ops.append(op)


def when(condition, build):
    """Keeps the ops build() writes only under these db settings."""
    start = len(ops)
    build()
    for op in ops[start:]:
        op["when"] = condition


def calendars():
    for k, t in enumerate(F2_2026_TRACKS):
        race_weekend(f"rw_f2_{t}_26", f"rw_{t}_26", EVENT_STAT + k)
    f1 = []
    acts = "schedule.activities"
    for k, (t, src, month, day) in enumerate(F1_2027):
        f1.append(f"rw_f1_{t}_27")
        race_weekend(f1[-1], src, F1_EVENT_STAT + k, (2026, month, day))

    def addon_round(k, t, src, month, day, activity):
        if activity:
            add(acts, f"{acts}.activity_race_weekend[_metadata_=spa_francorchamps]", _metadata_=activity,
                track=activity, stat_id=ACTIVITY_STAT)
        race_weekend(f"rw_f1_{t}_27", src, F1_EVENT_STAT + k, (2026, month, day), activity)

    for k, (t, src, month, day, activity) in enumerate(F1_2027_ADDON_TRACKS, len(F1_2027)):
        when({t: True}, lambda: addon_round(k, t, src, month, day, activity))
        when({t: True}, lambda: ops.append({"op": "warn", "message": f"F2 Feeder: the {t} round is on, which needs {TRACK_ADDONS[t]}."}))
    add(TL, f"{TL}.track_event_list[_metadata_=F1_2026_full]", _metadata_=CAREERS[0]["f1_track_list"])
    sel = f"{TL}.track_event_list[_metadata_={CAREERS[0]['f1_track_list']}]"
    for i, e in enumerate(f1):
        put(f"{sel}.fixed_event_reference_{i}", "_metadata_", f"fixed_event_reference_{e}", 2)
        put(f"{sel}.fixed_event_reference_{i}", "event_name", e, 4)
    # The list's length and the rounds after the fixed ones depend on which of the two settings are on. The list
    # sorts by date, so their place in it doesn't matter.
    def long_list(on):
        rounds = f1 + [f"rw_f1_{t}_27" for t in on]
        put(sel, "_fixed_event_reference_instancecount_", len(rounds), 0)
        for i, e in enumerate(rounds[len(f1):], len(f1)):
            put(f"{sel}.fixed_event_reference_{i}", "_metadata_", f"fixed_event_reference_{e}", 2)
            put(f"{sel}.fixed_event_reference_{i}", "event_name", e, 4)
        put(f"{DY}.custom_dynamic_available_track_list_data[_metadata_=F1_2027_Long]", "max_num_tracks", len(rounds))
        # The list is a copy of the 24 round stock one: the rows past its end go, or they read as dropped rounds.
        for i in range(len(rounds), len(f1) + len(F1_2027_ADDON_TRACKS)):
            for field in ("_metadata_", "event_name"):
                ops.append({"op": "remove-record", "key": f"{sel}.fixed_event_reference_{i}.{field}"})

    # The shorter seasons: the 2026 ones' rounds in 2027, Zandvoort (gone in 2027) swapped for Monaco.
    lists = {"Long": (CAREERS[0]["f1_track_list"], len(f1))}
    for kind, label, tracks in (("Medium", "F1_2027_16", F1_2027_16), ("Short", "F1_2027_10", F1_2027_10)):
        track_list(f"F1_2026_{label[-2:]}", label, [f"rw_f1_{t}_27" for t in tracks])
        lists[kind] = (label, len(tracks))
    # The season lengths the wizard and the Custom Season screen offer (each with its track list, count and
    # transfer window), copies of the 2026 ones.
    for kind, (label, count) in lists.items():
        add(DY, f"{DY}.custom_dynamic_available_track_list_data[_metadata_=F1_2026_{kind}]",
            _metadata_=f"F1_2027_{kind}", track_list_name=label, max_num_tracks=count)
    addon_tracks = [t for t, *_ in F1_2027_ADDON_TRACKS]
    for choice in itertools.product((False, True), repeat=len(addon_tracks)):
        when(dict(zip(addon_tracks, choice)), lambda: long_list([t for t, on in zip(addon_tracks, choice) if on]))
    # The Custom Season screen's track pool for it (F1_2026's is F1_2026_full_plus). The game takes the
    # first preset and pool that fit the mode and season, so f2_feeder.dll sets the 2026 ones aside while
    # this career is in focus.
    add(TS, f"{TS}.tracks_selection[_metadata_=F1_2026]",
        **{"_metadata_": "F1_2027", "default": CAREERS[0]["f1_track_list"],
           "season_0.track_event_list": CAREERS[0]["f1_track_list"]})
    set_events("F2_2026_full", F2_2026_FULL)
    add(TL, f"{TL}.track_event_list[_metadata_=F2_2024_full]", _metadata_=F2_ORDER)
    sel = f"{TL}.track_event_list[_metadata_={F2_ORDER}]"
    put(sel, "_fixed_event_reference_instancecount_", len(F2_2026_FULL), 0)
    for i, e in enumerate(F2_2026_FULL):
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
    # The stock F2 2024 lines cover season_2023 to season_2025.
    add(TS, f"{TS}.tracks_selection[_metadata_=F1_2026]",
        **{"_metadata_": "F2_2024", "game_setup_modes": "f2", "seasons": "season_2023,season_2024,season_2025",
           "default": "F2_2024_full", "season_0.track_event_list": "F2_2024_full"})


for c in CAREERS:
    career(c)
# The driver pools (the Icons tab) name Tsolov, who races F2 2026 here. The game keeps every pool row in one
# map and leaves those drivers out of the Driver Rating Comparison unless they're the player (0x1416da8e0), so
# he never showed there. His row goes in the 2025 and 2026 pools, which every career shares: the pool's last
# entrant takes it and the count drops by one.
for pool, row, last, name, driver in (("f1_2025_driver_pool", 12, 18, "blakeley", "lucas_blakeley"),
                                      ("f1_2026_driver_pool", 10, 17, "rasmussen", "frederik_rasmussen")):
    list_row = f"entrants.entrant_lists.entrant_list[_metadata_={pool}]"
    put(f"{list_row}.entrant_{row}", "_metadata_", name)
    put(f"{list_row}.entrant_{row}", "entrant_reference", driver)
    put(list_row, "_entrant_instancecount_", last - 1)
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


def c_string(s):
    return '"' + "".join(chr(b) if 0x20 <= b < 0x7f and chr(b) not in '"\\' else f"\\{b:03o}"
                         for b in s.encode("utf-8")) + '"'


titles = json.loads((ROOT / "tools" / "titles_2026.json").read_text(encoding="utf-8"))
year_from = titles["year"]
overrides = [("*", k, v.replace(year_from, str(TITLE_YEAR))) for k, v in titles["all"].items()]
overrides += [(language, k, v.replace(year_from, str(TITLE_YEAR)))
              for language, table in titles["per"].items() for k, v in table.items()]
(ROOT / "native" / "src" / "titles_2027.inc").write_text(
    "// Written by tools/gen.py from tools/titles_2026.json: {language, key, text}.\n"
    + "".join(f"{{{c_string(lng)}, {c_string(k)}, {c_string(v)}}},\n" for lng, k, v in overrides),
    encoding="utf-8")
print(f"{len(ops)} database ops, {len(lang)} strings, {len(overrides)} title overrides")
