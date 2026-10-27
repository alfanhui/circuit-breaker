@ECHO Off
REM Quick debugging: YabaSanshiro with its emulated BIOS (-nb) boots straight into the game,
REM skipping the Sega intro and licence screen. Real hardware always shows them, so check
REM boot behaviour with the real BIOS (run_with_yabaSanshiro.bat / run_with_mednafen.bat) too.
SET EMULATOR_DIR=..\..\Emulators

if exist game.cue (
"%EMULATOR_DIR%\YabaSanshiro\yabasanshiro.exe" -a -nb -i game.cue
) else (
echo Please compile first !
)
