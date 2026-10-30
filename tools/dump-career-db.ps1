# Dumps the game database rows the F2 Feeder Career addon needs into career-db-dump.txt.
# Run from the Swofty Launcher's bin folder:
#   powershell -ExecutionPolicy Bypass -File <path>\dump-career-db.ps1
param(
    [string]$Swofty = ".\swofty.exe",
    [string]$Out = "$PWD\career-db-dump.txt"
)

$roots = @(
    "schedule.event_lists.event_list",
    "schedule.track_lists.track_event_list",
    "schedule.event_lists.event_list[_metadata_=F2_2024_career]",
    "schedule.event_lists.event_list[_metadata_=F1_career_26]",
    "schedule.event_lists.event_list[_metadata_=F1_career_25]",
    "championship_events.Championships",
    "game_modes.ModeChampionship[_metadata_=driver_career_25]",
    "game_modes.ModeChampionship[_metadata_=driver_career_26]",
    "championships.Championship[_metadata_=F2_2024_career]",
    "frontend_scene.scene_rooms_defines.career_rooms.career_wizard"
)
# Track lists whose label matches this are dumped in full.
$trackListMatch = "F2|career_2[456]|_25|_26"
$maxDepth = 6

function Dump([string]$path, [int]$depth) {
    $lines = & $Swofty db get $path 2>&1 | ForEach-Object { "$_" }
    "### $path" | Add-Content $Out
    $lines | Add-Content $Out
    if ($depth -ge $maxDepth) { return }
    foreach ($l in $lines) {
        if ($l -match '^\s{2}(\S+)\s{2}table, (\d+) rows$') {
            $name = $Matches[1]; $n = [int]$Matches[2]
            if ($n -gt 40) { continue }
            for ($i = 0; $i -lt $n; $i++) { Dump "$path.$($name)_$i" ($depth + 1) }
        }
    }
}

Remove-Item $Out -ErrorAction SilentlyContinue
foreach ($r in $roots) { Dump $r 0 }

# The track lists by label: dump the F2 and 2025/2026 ones.
$lists = & $Swofty db get "schedule.track_lists" 2>&1 | ForEach-Object { "$_" }
$rows = & $Swofty db get "schedule.track_lists.track_event_list" 2>&1 | ForEach-Object { "$_" }
"### track list rows" | Add-Content $Out
$lists + $rows | Add-Content $Out
for ($i = 0; $i -lt 40; $i++) {
    $p = "schedule.track_lists.track_event_list_$i"
    $head = & $Swofty db get $p 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { break }
    if (($head -join "`n") -match $trackListMatch) { Dump $p 0 } else { "### $p (skipped)" | Add-Content $Out; $head | Add-Content $Out }
}
Write-Host "wrote $Out"
