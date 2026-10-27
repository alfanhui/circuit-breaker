@ECHO OFF
REM Builds and runs the host unit tests for the pure game modules (src/game).
REM Uses gcc if it is on PATH, otherwise MSVC via vcvars64.bat.
SETLOCAL
CD /D "%~dp0"
IF NOT EXIST build MKDIR build

SET SOURCES=test_main.c test_trail.c test_cycle.c test_press_latch.c ..\src\game\trail.c ..\src\game\cycle.c ..\src\game\press_latch.c

WHERE gcc >NUL 2>&1
IF %ERRORLEVEL%==0 (
	gcc -std=c99 -W -Wall -Wextra -Werror -o build\tests.exe %SOURCES% || EXIT /B 1
	GOTO run
)

SET VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
FOR /F "usebackq tokens=*" %%i IN (`%VSWHERE% -latest -products * -property installationPath`) DO SET VSDIR=%%i
IF NOT DEFINED VSDIR (
	ECHO No gcc on PATH and no Visual Studio found.
	EXIT /B 1
)
CALL "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >NUL 2>&1
cl /nologo /W4 /WX /Fobuild\ /Febuild\tests.exe %SOURCES% || EXIT /B 1

:run
build\tests.exe
