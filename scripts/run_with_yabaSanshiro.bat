@ECHO Off
SET EMULATOR_DIR=..\..\Emulators

if exist game.iso (
"%EMULATOR_DIR%\YabaSanshiro\yabasanshiro.exe" -a -i game.cue
) else (
echo Please compile first !
)
