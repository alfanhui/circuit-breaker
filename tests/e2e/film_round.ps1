# Films one of the arena's rounds: builds the game booting straight into it (BOOT_ROUND in
# src/input.c, 1 to 5 here), runs a step script against it in Docker, and makes a contact sheet
# of its shots. BOOT_ROUND is put back to 0 and the game rebuilt afterwards, even if a step fails.
# Usage: tests\e2e\film_round.ps1 -Round 4 [-Steps layouts.steps]
#   shots:  tests\build\e2e\round<Round>\    sheet: tests\build\e2e\round<Round>_sheet.png
# (layouts.steps for a round's layout, closing.steps for round 5's walls closing in)
param(
	[Parameter(Mandatory = $true)][ValidateRange(1, 5)][int]$Round,
	[string]$Steps = "layouts.steps"
)
$ErrorActionPreference = "Stop"
$project = (Resolve-Path "$PSScriptRoot\..\..").Path
$input_c = "$project\src\input.c"
$shots = "$project\tests\build\e2e"

# Sets BOOT_ROUND (0-based) in input.c. Written back without a byte-order mark: PowerShell's own
# Set-Content would add one (SKILLS.md)
function Set-BootRound([int]$number, [int]$on_foot)
{
	$src = [System.IO.File]::ReadAllText($input_c)
	$src = $src -replace "(?m)^#define BOOT_ROUND \d+", "#define BOOT_ROUND $number"
	$src = $src -replace "(?m)^#define BOOT_ON_FOOT \d+", "#define BOOT_ON_FOOT $on_foot"
	[System.IO.File]::WriteAllText($input_c, $src)
}
# The rounds are filmed from the arena: not booted on foot in the corridor (BOOT_ON_FOOT), which
# is put back as it was afterwards
$was_on_foot = if ([System.IO.File]::ReadAllText($input_c) -match "(?m)^#define BOOT_ON_FOOT (\d+)") { [int]$Matches[1] } else { 0 }

function Build
{
	Push-Location $project
	# (stderr joined inside cmd: under "Stop", a compiler warning on PowerShell's stderr ended the script)
	# A failed build leaves the old game.iso, and the film then runs on it: stop instead (run_e2e.ps1)
	$started = Get-Date
	try { cmd /c "scripts\compile.bat 2>&1" | Select-String -Pattern ": error|\*\*\*" } finally { Pop-Location }
	if (-not (Test-Path "$project\game.iso") -or (Get-Item "$project\game.iso").LastWriteTime -lt $started)
	{ throw "The build failed: game.iso wasn't made again (open in an emulator?)" }
}

Set-BootRound ($Round - 1) 0
try
{
	Build
	New-Item -ItemType Directory -Force $shots | Out-Null
	# Last run's shots out (other rounds' sheets kept): only this run's go on the sheet
	$loose = { Get-ChildItem $shots -Filter *.png -File | Where-Object { $_.Name -notlike "*_sheet.png" } }
	& $loose | Remove-Item
	powershell -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\run_e2e.ps1" -Steps $Steps | Select-Object -Last 1
	$folder = "$shots\round$Round"
	New-Item -ItemType Directory -Force $folder | Out-Null
	Get-ChildItem $folder -File | Remove-Item
	& $loose | Move-Item -Destination $folder
	# The container expands the glob: a list passed from PowerShell made nothing (SKILLS.md)
	docker run --rm -v "${shots}:/out" --entrypoint sh circuit-breaker-e2e -c `
		"montage /out/round$Round/*.png -tile 4x -geometry 480x360+3+3 /out/round${Round}_sheet.png"
	"Sheet: $shots\round${Round}_sheet.png"
}
finally
{
	Set-BootRound 0 $was_on_foot
	Build
}
