# Runs an E2E step script against the built game.iso in Docker (no screen needed).
# Usage: tests\e2e\run_e2e.ps1 [-Steps level_bounds.steps] [-Bios path\to\saturn_bios.bin] [-Out folder]
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

docker build -q -t circuit-breaker-e2e $PSScriptRoot | Out-Null
docker run --rm `
	-v "${project}:/game:ro" `
	-v "${Bios}:/bios/bios.bin:ro" `
	-v "${Out}:/out" `
	circuit-breaker-e2e sh /game/tests/e2e/run.sh "/game/tests/e2e/$Steps"
"Screenshots in $Out"
