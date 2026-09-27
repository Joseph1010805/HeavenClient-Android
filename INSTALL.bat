@echo off
REM ===========================================================================
REM  PUT THE GAME ON A HANDHELD - the double-clickable front door.
REM
REM  The installer is tools\install.sh, and it stays a bash script: it drives
REM  adb, converts Windows and device paths in opposite directions, and asks
REM  questions. That is the right shape for the job and the wrong thing to
REM  hand somebody - a .sh file does not run from Explorer, and a Windows
REM  player has no reason to own a terminal.
REM
REM  So this is a shim and not a second installer. There is one installer;
REM  this finds the bash that Git for Windows ships, hands install.sh to it,
REM  and keeps the window open at the end so a failure can be READ instead of
REM  vanishing with the console. Double-clicked scripts that close on error
REM  are how a reason gets thrown away, which is the one bug this project
REM  keeps finding.
REM
REM  Arguments pass straight through, so   INSTALL.bat --server
REM  is exactly   tools/install.sh --server .
REM ===========================================================================

setlocal
cd /d "%~dp0"

REM ---------------------------------------------------------------------------
REM  bash
REM
REM  Git for Windows puts it in one of these four places depending on whether
REM  it was installed for everybody or just this user, and on 32 vs 64 bit.
REM  Nothing else here needs Git itself - only the bash that comes with it.
REM ---------------------------------------------------------------------------
set "BASH="
for %%B in (
  "%ProgramFiles%\Git\bin\bash.exe"
  "%ProgramW6432%\Git\bin\bash.exe"
  "%ProgramFiles(x86)%\Git\bin\bash.exe"
  "%LOCALAPPDATA%\Programs\Git\bin\bash.exe"
) do if not defined BASH if exist %%B set "BASH=%%~B"

if not defined BASH (
  echo.
  echo   Git for Windows is not installed, and the installer needs the bash
  echo   that comes with it. It is a normal Windows download, no account:
  echo.
  echo       https://git-scm.com/download/win
  echo.
  echo   Install it with the default options, then double-click this again.
  echo.
  pause
  exit /b 1
)

REM ---------------------------------------------------------------------------
REM  NO QUESTIONS HERE. THE INSTALLER ASKS THE ONE THAT MATTERS.
REM
REM  This used to ask "put a server on it too?" before install.sh had even
REM  looked for a device - so it asked about hosting before anything had
REM  confirmed WHICH machine, and it made an installer decision into a
REM  player's problem. Every device can be the one that creates the game; the
REM  login screen has no HOST / JOIN pair any more. So every device gets a
REM  server, and the only question asked is the installer's own
REM  "Install to <device>?".
REM
REM  It also printed the cable warning that install.sh now prints itself, so
REM  the same paragraph appeared twice before anything had happened.
REM
REM  Arguments still pass straight through, so INSTALL.bat --device SERIAL
REM  behaves exactly as tools/install.sh would.
REM ---------------------------------------------------------------------------
set "ARGS=%*"
if "%ARGS%"=="" set "ARGS=--server"

echo.
"%BASH%" tools/install.sh %ARGS%
set RC=%ERRORLEVEL%

echo.
if not "%RC%"=="0" (
  echo   It stopped early. Whatever it printed above is the reason - the
  echo   installer is written to say what went wrong rather than just fail.
  echo.
)
pause
exit /b %RC%
