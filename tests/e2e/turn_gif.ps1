# Records a right turn with each 1st person turn camera in slow motion, and makes comparison GIFs.
# Usage: tests\e2e\turn_gif.ps1   (build game.iso first; GIFs land in tests\build\e2e)
$ErrorActionPreference = "Stop"
$out = "$PSScriptRoot\..\build\e2e"
& "$PSScriptRoot\run_e2e.ps1" -Steps turn_gif.steps -Out $out
$out = (Resolve-Path $out).Path
docker run --rm -v "${PSScriptRoot}:/e2e:ro" -v "${out}:/out" --entrypoint sh circuit-breaker-e2e /e2e/turn_gif.sh
