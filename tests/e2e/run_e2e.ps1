# Runs an E2E step script against the built game.iso in Docker (no screen needed).
# Usage: tests\e2e\run_e2e.ps1 [-Steps level_bounds.steps] [-Bios path\to\saturn_bios.bin] [-Out folder]
#
# Where the game boots is a build setting (BOOT_ON_FOOT in src/features.h: 1 stood in the corridor,
# 0 in the arena), and every film says which it needs by how it starts: wait_boot (the arena's
# navy) or wait_up (the corridor's red). If the build boots the other way, BOOT_ON_FOOT is set
# for the film and the game built, then put back and built again afterwards, even if the film fails
param(
	[string]$Steps = "level_bounds.steps",
	[string]$Bios = "$PSScriptRoot\..\..\..\..\Emulators\YabaSanshiro\bios.bin",
	[string]$Out = "$PSScriptRoot\..\build\e2e"
)
$ErrorActionPreference = "Stop"
$project = (Resolve-Path "$PSScriptRoot\..\..").Path
$Bios = (Resolve-Path $Bios).Path
New-Item -ItemType Directory -Force $Out | Out-Null
$Out = (Resolve-Path $Out).Path

# The boot this film needs (1: on foot, 0: the arena, -1: either), from its first wait
$steps_text = [System.IO.File]::ReadAllText("$PSScriptRoot\$Steps")
$needs = if ($steps_text -match "(?m)^wait_up") { 1 } elseif ($steps_text -match "(?m)^wait_boot") { 0 } else { -1 }
$features_h = "$project\src\features.h"
$built = if ([System.IO.File]::ReadAllText($features_h) -match "(?m)^#define BOOT_ON_FOOT (\d+)") { [int]$Matches[1] } else { 0 }
# A city tower to boot on ("boot_at 0650": its address on the floor plan; BOOT_AT_TOWER), or none
$tower = if ($steps_text -match "(?m)^boot_at ([0-9A-Fa-f]{4})") { "0x" + $Matches[1] } else { "-1" }
$tower_built = if ([System.IO.File]::ReadAllText($features_h) -match "(?m)^#define BOOT_AT_TOWER (\S+)") { $Matches[1] } else { "-1" }
# The CD block's hub ("boot_at hub": BOOT_AT_HUB 1, stood short of its lift's pad), or not. A
# "boot_at 0010" would stand you on the pad itself, and the lift would take you at once
$hub = if ($steps_text -match "(?m)^boot_at hub\s*$") { "1" } else { "0" }
$hub_built = if ([System.IO.File]::ReadAllText($features_h) -match "(?m)^#define BOOT_AT_HUB (\d+)") { $Matches[1] } else { "0" }
# The benchmark (a "bench" line: BENCH 1, src/game/bench.h), or not
$bench = if ($steps_text -match "(?m)^bench\s*$") { "1" } else { "0" }
$bench_built = if ([System.IO.File]::ReadAllText($features_h) -match "(?m)^#define BENCH (\d+)") { $Matches[1] } else { "0" }
# A glitch-check build (a "holes_from" line: GLITCH_CHECK 1, the magenta back colour), or not
$check = if ($steps_text -match "(?m)^holes_from") { "1" } else { "0" }
$check_built = if ([System.IO.File]::ReadAllText($features_h) -match "(?m)^#define GLITCH_CHECK (\d+)") { $Matches[1] } else { "0" }
$switch = (($needs -ge 0) -and ($needs -ne $built)) -or ($tower -ne $tower_built) -or ($bench -ne $bench_built) -or ($check -ne $check_built) -or ($hub -ne $hub_built)

# Sets BOOT_ON_FOOT and builds. Written back without a byte-order mark: PowerShell's own
# Set-Content would add one (SKILLS.md)
function Set-OnFoot([int]$on_foot, [string]$at, [string]$is_bench, [string]$is_check, [string]$at_hub)
{
	$src = [System.IO.File]::ReadAllText($features_h)
	$src = $src -replace "(?m)^#define BOOT_ON_FOOT \d+", "#define BOOT_ON_FOOT $on_foot"
	$src = $src -replace "(?m)^#define BOOT_AT_TOWER \S+", "#define BOOT_AT_TOWER $at"
	$src = $src -replace "(?m)^#define BENCH \d+", "#define BENCH $is_bench"
	$src = $src -replace "(?m)^#define GLITCH_CHECK \d+", "#define GLITCH_CHECK $is_check"
	$src = $src -replace "(?m)^#define BOOT_AT_HUB \d+", "#define BOOT_AT_HUB $at_hub"
	[System.IO.File]::WriteAllText($features_h, $src)
	Push-Location $project
	# (stderr joined inside cmd: under "Stop", a compiler warning on PowerShell's stderr ended the script)
	# A failed build leaves the old game.iso, and the film then runs on it: stop instead. (make says
	# "Error 1", not ": error"; game.iso open in an emulator stops make's clean, SKILLS.md)
	$started = Get-Date
	try { cmd /c "scripts\compile.bat 2>&1" | Select-String -Pattern ": error|\*\*\*" } finally { Pop-Location }
	if (-not (Test-Path "$project\game.iso") -or (Get-Item "$project\game.iso").LastWriteTime -lt $started)
	{ throw "The build failed: game.iso wasn't made again (open in an emulator?)" }
}

$film_exit = 0
docker build -q -t circuit-breaker-e2e $PSScriptRoot | Out-Null
try
{
	# Inside the try: if this build fails, the finally still puts BOOT_ON_FOOT back (it was left
	# switched, twice, 2026-11-09)
	if ($switch)
	{
		$on = if ($needs -ge 0) { $needs } else { $built }
		"Building with BOOT_ON_FOOT $on, BOOT_AT_TOWER $tower, BENCH $bench, GLITCH_CHECK $check, BOOT_AT_HUB $hub for $Steps"
		Set-OnFoot $on $tower $bench $check $hub
	}
	docker run --rm `
		-v "${project}:/game:ro" `
		-v "${Bios}:/bios/bios.bin:ro" `
		-v "${Out}:/out" `
		circuit-breaker-e2e sh /game/tests/e2e/run.sh "/game/tests/e2e/$Steps"
	# The film's own verdict (run.sh: the benchmark's wait_verdict exits 2 on a fail, 3 with none)
	$film_exit = $LASTEXITCODE
}
finally
{
	if ($switch)
	{
		"Building with BOOT_ON_FOOT $built, BOOT_AT_TOWER $tower_built, BENCH $bench_built, GLITCH_CHECK $check_built, BOOT_AT_HUB $hub_built again"
		Set-OnFoot $built $tower_built $bench_built $check_built $hub_built
	}
}
"Screenshots in $Out"
exit $film_exit
