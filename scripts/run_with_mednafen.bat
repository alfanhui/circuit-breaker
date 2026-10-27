@ECHO Off
SET EMULATOR_DIR=..\..\Emulators
SET MEDNAFEN_EXECUTABLE_PATH=%EMULATOR_DIR%\mednafen\mednafen.exe
REM Mednafen looks for its own BIOS file names (sega_101.bin / mpr-17933.bin); point it at ours instead
SET SATURN_BIOS=%cd%\%EMULATOR_DIR%\YabaSanshiro\bios.bin

if not exist %MEDNAFEN_EXECUTABLE_PATH% (
	echo ---
	echo Please install Mednafen here %EMULATOR_DIR%
	echo ---
	pause
	exit
)

if exist game.cue (
REM bios_sanity 0: skip Mednafen's check for its known BIOS dumps. Region JP: match the target console
"%MEDNAFEN_EXECUTABLE_PATH%" -ss.bios_jp "%SATURN_BIOS%" -ss.bios_na_eu "%SATURN_BIOS%" -ss.bios_sanity 0 -ss.region_autodetect 0 -ss.region_default jp -sound.volume 150 "%cd%\game.cue"
) else (
echo Please compile first !
)
