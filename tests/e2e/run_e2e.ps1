# Runs an E2E step script against the built game.iso in Docker (no screen needed).
# Usage: tests\e2e\run_e2e.ps1 [-Steps level_bounds.steps] [-Bios path\to\saturn_bios.bin] [-Out folder]
#
# Where the game boots is a build setting (BOOT_ON_FOOT in src/input.c: 1 stood in the corridor,
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
$input_c = "$project\src\input.c"
$built = if ([System.IO.File]::ReadAllText($input_c) -match "(?m)^#define BOOT_ON_FOOT (\d+)") { [int]$Matches[1] } else { 0 }
$switch = ($needs -ge 0) -and ($needs -ne $built)

# Sets BOOT_ON_FOOT and builds. Written back without a byte-order mark: PowerShell's own
# Set-Content would add one (SKILLS.md)
function Set-OnFoot([int]$on_foot)
{
	$src = [System.IO.File]::ReadAllText($input_c)
	[System.IO.File]::WriteAllText($input_c, ($src -replace "(?m)^#define BOOT_ON_FOOT \d+", "#define BOOT_ON_FOOT $on_foot"))
	Push-Location $project
	# (stderr joined inside cmd: under "Stop", a compiler warning on PowerShell's stderr ended the script)
	# A failed build leaves the old game.iso, and the film then runs on it: stop instead. (make says
	# "Error 1", not ": error"; game.iso open in an emulator stops make's clean, SKILLS.md)
	$started = Get-Date
	try { cmd /c "scripts\compile.bat 2>&1" | Select-String -Pattern ": error|\*\*\*" } finally { Pop-Location }
	if (-not (Test-Path "$project\game.iso") -or (Get-Item "$project\game.iso").LastWriteTime -lt $started)
	{ throw "The build failed: game.iso wasn't made again (open in an emulator?)" }
}

docker build -q -t circuit-breaker-e2e $PSScriptRoot | Out-Null
try
{
	# Inside the try: if this build fails, the finally still puts BOOT_ON_FOOT back (it was left
	# switched, twice, 2026-11-09)
	if ($switch)
	{
		"Building with BOOT_ON_FOOT $needs for $Steps"
		Set-OnFoot $needs
	}
	docker run --rm `
		-v "${project}:/game:ro" `
		-v "${Bios}:/bios/bios.bin:ro" `
		-v "${Out}:/out" `
		circuit-breaker-e2e sh /game/tests/e2e/run.sh "/game/tests/e2e/$Steps"
}
finally
{
	if ($switch)
	{
		"Building with BOOT_ON_FOOT $built again"
		Set-OnFoot $built
	}
}
"Screenshots in $Out"
