@echo off
rem Installs cpsc and its HTML manual for the current user (no administrator needed).
rem Rules: ASCII only, CRLF line endings, and no at-sign names (Release.cmake uses configure_file).
setlocal EnableExtensions DisableDelayedExpansion

rem The script directory must be read before any shift. It ends with a backslash.
set "CPSC_SCRIPT_DIR=%~dp0"
set "CPSC_RC=0"
set "CPSC_PREFIX="
set "CPSC_SOURCE="
set "CPSC_PAUSE="
set "CPSC_SHORTCUT="
set "CPSC_PATH_ADDED="

rem Double-clicked (no arguments, started with cmd /c): keep the window open at the end.
if "%~1"=="" call :detect_double_click

:parse
if "%~1"=="" goto parsed
if /i "%~1"=="--prefix" goto opt_value
if /i "%~1"=="--source-dir" goto opt_value
if /i "%~1"=="-h" goto usage
if /i "%~1"=="--help" goto usage
if /i "%~1"=="/?" goto usage
set "CPSC_MSG=unknown argument: %~1 (see --help)"
goto fail

:opt_value
if "%~2"=="" set "CPSC_MSG=%~1 requires a directory" & goto fail
set "CPSC_ARG2=%~2"
if "%CPSC_ARG2:~0,2%"=="--" set "CPSC_MSG=%~1 requires a directory, got %~2" & goto fail
if /i "%~1"=="--prefix" set "CPSC_PREFIX=%~2"
if /i "%~1"=="--source-dir" set "CPSC_SOURCE=%~2"
shift
shift
goto parse

:usage
echo Usage: install.cmd [--prefix DIR] [--source-dir DIR]
echo(
echo Install cpsc.exe and its HTML manuals to DIR, add DIR to the user PATH and
echo create a Start Menu shortcut to the manual.
echo The default prefix is %%LOCALAPPDATA%%\Programs\cpsc.
echo The source directory defaults to the directory containing this script and
echo must contain cpsc.exe, doc\cpsc.html, doc\language\cpsc-language-overview.html
echo and doc\language\function and keyword HTML pages.
echo Use --source-dir build when running this script from the source checkout.
goto finish

:parsed
if defined DESTDIR set "CPSC_MSG=DESTDIR staging is unsupported on Windows; use --prefix instead." & goto fail

rem Prefix: default, then normalise to an absolute path without a trailing backslash.
if defined CPSC_PREFIX goto prefix_set
if not defined LOCALAPPDATA set "CPSC_MSG=Cannot locate the current user application directory; specify --prefix." & goto fail
set "CPSC_PREFIX=%LOCALAPPDATA%\Programs\cpsc"
:prefix_set
set "CPSC_P=%CPSC_PREFIX:/=\%"
set "CPSC_OK="
if "%CPSC_P:~1,2%"==":\" set "CPSC_OK=1"
if "%CPSC_P:~0,2%"=="\\" set "CPSC_OK=1"
if not defined CPSC_OK set "CPSC_MSG=--prefix must be an absolute Windows path without ; = ! %% or ^." & goto fail
rem A substitution cannot match =, so split on every forbidden character with for /f instead.
rem The leading x keeps the first token nonempty; a percent sign is written as two here.
for /f "delims==;!^%%" %%A in ("x%CPSC_P%") do if not "%%A"=="x%CPSC_P%" set "CPSC_MSG=--prefix must be an absolute Windows path without ; = ! %% or ^." & goto fail
for %%I in ("%CPSC_P%") do set "CPSC_P=%%~fI"
if "%CPSC_P:~-1%"=="\" if not "%CPSC_P:~-2%"==":\" set "CPSC_P=%CPSC_P:~0,-1%"
set "CPSC_KEY=%CPSC_P%"
if "%CPSC_KEY:~-1%"=="\" set "CPSC_KEY=%CPSC_KEY:~0,-1%"

rem Source: default is the script directory.
if defined CPSC_SOURCE goto source_set
set "CPSC_SOURCE=%CPSC_SCRIPT_DIR%"
:source_set
set "CPSC_S=%CPSC_SOURCE:/=\%"
for %%I in ("%CPSC_S%") do set "CPSC_S=%%~fI"
if "%CPSC_S:~-1%"=="\" if not "%CPSC_S:~-2%"==":\" set "CPSC_S=%CPSC_S:~0,-1%"
rem Prefix the attributes with a dash so the substring below never sees an undefined variable.
set "CPSC_ATTR=-"
for %%I in ("%CPSC_S%") do set "CPSC_ATTR=-%%~aI"
if not "%CPSC_ATTR:~1,1%"=="d" set "CPSC_MSG=Cannot open source directory: %CPSC_S%" & goto fail
if not exist "%CPSC_S%\cpsc.exe" set "CPSC_MSG=Missing source file: %CPSC_S%\cpsc.exe; build cpsc and the doc target first, or extract the whole ZIP first." & goto fail
if not exist "%CPSC_S%\doc\cpsc.html" set "CPSC_MSG=Missing source file: %CPSC_S%\doc\cpsc.html; build the doc target first." & goto fail
if not exist "%CPSC_S%\doc\language\cpsc-language-overview.html" set "CPSC_MSG=Missing language overview: %CPSC_S%\doc\language\cpsc-language-overview.html; build the doc target first." & goto fail
if not exist "%CPSC_S%\doc\language\function\cpsc-function-*.html" set "CPSC_MSG=Missing language manuals: %CPSC_S%\doc\language\function; build the doc target first." & goto fail
if not exist "%CPSC_S%\doc\language\keyword\cpsc-keyword-*.html" set "CPSC_MSG=Missing language manuals: %CPSC_S%\doc\language\keyword; build the doc target first." & goto fail

rem Collisions: the directories must not be files and the target files must not be directories.
set "CPSC_CHECK=%CPSC_P%"
call :check_not_file
if errorlevel 1 goto fail
set "CPSC_CHECK=%CPSC_P%\doc"
call :check_not_file
if errorlevel 1 goto fail
set "CPSC_CHECK=%CPSC_P%\doc\language"
call :check_not_file
if errorlevel 1 goto fail
set "CPSC_CHECK=%CPSC_P%\doc\language\function"
call :check_not_file
if errorlevel 1 goto fail
set "CPSC_CHECK=%CPSC_P%\doc\language\keyword"
call :check_not_file
if errorlevel 1 goto fail
set "CPSC_CHECK=%CPSC_P%\cpsc.exe"
call :check_not_dir
if errorlevel 1 goto fail
set "CPSC_CHECK=%CPSC_P%\doc\cpsc.html"
call :check_not_dir
if errorlevel 1 goto fail

rem Copy, unless installing in place.
if /i "%CPSC_S%"=="%CPSC_P%" goto copied
mkdir "%CPSC_P%\doc\language\function" 2>nul
mkdir "%CPSC_P%\doc\language\keyword" 2>nul
copy /y "%CPSC_S%\cpsc.exe" "%CPSC_P%\cpsc.exe" >nul
if errorlevel 1 goto copy_failed
copy /y "%CPSC_S%\doc\cpsc.html" "%CPSC_P%\doc\cpsc.html" >nul
if errorlevel 1 goto copy_failed
copy /y "%CPSC_S%\doc\language\cpsc-language-overview.html" "%CPSC_P%\doc\language\cpsc-language-overview.html" >nul
if errorlevel 1 goto copy_failed
copy /y "%CPSC_S%\doc\language\function\cpsc-function-*.html" "%CPSC_P%\doc\language\function\" >nul
if errorlevel 1 goto copy_failed
copy /y "%CPSC_S%\doc\language\keyword\cpsc-keyword-*.html" "%CPSC_P%\doc\language\keyword\" >nul
if errorlevel 1 goto copy_failed
:copied

rem Count the language manuals: the overview plus every function and keyword page.
set /a CPSC_COUNT=1
for %%F in ("%CPSC_S%\doc\language\function\cpsc-function-*.html" "%CPSC_S%\doc\language\keyword\cpsc-keyword-*.html") do set /a CPSC_COUNT+=1

rem Start Menu shortcut through PowerShell. Failure (no PowerShell, constrained language) only warns.
set "CPSC_LNK_TARGET=%CPSC_P%\doc\cpsc.html"
set "CPSC_LNK_DIR=%CPSC_P%\doc"
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command "$ErrorActionPreference='Stop'; $d=Join-Path ([Environment]::GetFolderPath('Programs')) 'cpsc'; [void][IO.Directory]::CreateDirectory($d); $l=(New-Object -ComObject WScript.Shell).CreateShortcut((Join-Path $d 'cpsc Help.lnk')); $l.TargetPath=$env:CPSC_LNK_TARGET; $l.WorkingDirectory=$env:CPSC_LNK_DIR; $l.Description='cpsc command-line manual'; $l.Save()" >nul 2>&1
if errorlevel 1 goto lnk_failed
set "CPSC_SHORTCUT=1"
goto lnk_done
:lnk_failed
setlocal EnableDelayedExpansion
>&2 echo(install.cmd: warning: could not create the Start Menu shortcut; open !CPSC_P!\doc\cpsc.html directly.
endlocal
:lnk_done

rem Read the user PATH only (never the combined PATH). Delayed expansion is still off here.
set "CPSC_PTYPE=REG_EXPAND_SZ"
set "CPSC_UPATH="
for /f "tokens=1,2,*" %%A in ('reg query HKCU\Environment /v Path 2^>nul') do if /i "%%A"=="Path" (set "CPSC_PTYPE=%%B" & set "CPSC_UPATH=%%C")
if not "%CPSC_PTYPE%"=="REG_SZ" if not "%CPSC_PTYPE%"=="REG_EXPAND_SZ" set "CPSC_MSG=Cannot update the user PATH; add %CPSC_P% manually." & goto fail

rem From here on, delayed expansion keeps PATH text (quotes, ampersands, percent signs) out of the parser.
setlocal EnableDelayedExpansion
set "CPSC_NOQ="
if defined CPSC_UPATH set "CPSC_NOQ=!CPSC_UPATH:"=!"
set "CPSC_CHK=;"
if defined CPSC_NOQ set "CPSC_CHK=;!CPSC_NOQ:/=\!;"
set "CPSC_CHK=!CPSC_CHK:\;=;!"
if /i not "!CPSC_CHK:;%CPSC_KEY%;=!"=="!CPSC_CHK!" goto path_done
if not "!CPSC_NOQ!"=="!CPSC_UPATH!" set "CPSC_MSG=user PATH contains quotation marks; add %CPSC_P% manually." & goto fail
set "CPSC_NEW=!CPSC_UPATH!"
if defined CPSC_NEW if not "!CPSC_NEW:~-1!"==";" set "CPSC_NEW=!CPSC_NEW!;"
set "CPSC_NEW=!CPSC_NEW!%CPSC_P%"
if not "!CPSC_NEW:~8000,1!"=="" set "CPSC_MSG=user PATH too long to update from cmd; add %CPSC_P% manually." & goto fail
rem A trailing backslash would escape the closing quote of reg add, so double it.
if "!CPSC_NEW:~-1!"=="\" set "CPSC_NEW=!CPSC_NEW!\"
reg add HKCU\Environment /v Path /t !CPSC_PTYPE! /d "!CPSC_NEW!" /f >nul
if errorlevel 1 set "CPSC_MSG=Cannot update the user PATH; add %CPSC_P% manually." & goto fail
set "CPSC_PATH_ADDED=1"
rem Setting and deleting a throwaway variable makes Windows broadcast the environment change.
setx CPSC_INSTALL_REFRESH 1 >nul 2>&1
reg delete HKCU\Environment /v CPSC_INSTALL_REFRESH /f >nul 2>&1
:path_done

echo(Installed !CPSC_P!\cpsc.exe
echo(Installed !CPSC_P!\doc\cpsc.html and !CPSC_COUNT! language manuals in !CPSC_P!\doc\language
if defined CPSC_SHORTCUT echo(Open the manual from Start Menu ^> cpsc ^> cpsc Help.
if defined CPSC_PATH_ADDED echo(Added !CPSC_P! to your user PATH.
if not defined CPSC_PATH_ADDED echo(!CPSC_P! is already in your user PATH.
echo(Open a new terminal to use cpsc by name (existing terminals keep their old PATH).
goto finish

:copy_failed
set "CPSC_MSG=Cannot copy the files to %CPSC_P%. Check the destination permissions and supplied paths."
goto fail

:check_not_file
if not exist "%CPSC_CHECK%" exit /b 0
set "CPSC_ATTR=-"
for %%I in ("%CPSC_CHECK%") do set "CPSC_ATTR=-%%~aI"
if "%CPSC_ATTR:~1,1%"=="d" exit /b 0
set "CPSC_MSG=Installation directory is occupied by a file: %CPSC_CHECK%"
exit /b 1

:check_not_dir
if not exist "%CPSC_CHECK%" exit /b 0
set "CPSC_ATTR=-"
for %%I in ("%CPSC_CHECK%") do set "CPSC_ATTR=-%%~aI"
if not "%CPSC_ATTR:~1,1%"=="d" exit /b 0
set "CPSC_MSG=Installation file is occupied by a directory: %CPSC_CHECK%"
exit /b 1

:detect_double_click
setlocal EnableDelayedExpansion
set "CPSC_LINE=!cmdcmdline!"
set "CPSC_FLAG="
if /i not "!CPSC_LINE: /c =!"=="!CPSC_LINE!" set "CPSC_FLAG=1"
endlocal & set "CPSC_PAUSE=%CPSC_FLAG%"
exit /b 0

:fail
setlocal EnableDelayedExpansion
>&2 echo(install.cmd: !CPSC_MSG!
endlocal
set "CPSC_RC=1"
goto finish

:finish
if defined CPSC_PAUSE pause
exit /b %CPSC_RC%
