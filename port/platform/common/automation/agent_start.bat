@echo off
rem This batch file starts an agent service.  It should be run in the directory that houses the agent,
rem conventionally c:\agent.  Usage instructions can be found under the "usage" label below.

set url_ubxlib=https://github.com/u-blox/ubxlib
set dir_ubxlib=ubxlib
set branch_ubxlib=master
set url_unity=https://github.com/ThrowTheSwitch/Unity
set dir_unity=Unity
set branch_unity=master
set dir_work=work
set dir_start=%~dp0
set drive_subst=z

rem Handle optional command-line parameters.
if not "%~1"=="" (
    if "%~1"=="/?" goto usage
    set url_ubxlib=%~1
)
if not "%~2"=="" (
    set branch_ubxlib=%~2
)

rem Check environment variables.
if "%U_AGENT_INSTANCES%"=="" (
    echo %~n0: ERROR environment variable U_AGENT_INSTANCES must be populated.
    goto usage
)

rem Populate ubxlib directory.
if not exist %dir_ubxlib% (
    echo Populating %dir_ubxlib% from %url_ubxlib%...
    call git clone %url_ubxlib% %dir_ubxlib%
)
echo Checking out branch "%branch_ubxlib%" of %url_ubxlib%...
pushd %dir_ubxlib%
call git fetch
call git checkout %branch_ubxlib%
popd

rem Populate Unity.
if not exist %dir_unity% (
    echo Populating %dir_unity% from %url_unity%...
    call git clone %url_unity%
)
echo Checking out branch "%branch_unity%" of %url_unity%...
pushd %dir_unity%
call git fetch
call git checkout %branch_unity%
popd

rem Actually start the agent.
echo Starting agent...
if exist %drive_subst%:\ subst %drive_subst%: /D
if not exist %dir_work% mkdir %dir_work%
subst %drive_subst%: %~dp0%dir_work%
pushd %dir_ubxlib%\port\platform\common\automation
cmd /c python u_agent_service.py -u %~dp0%dir_unity% -n %COMPUTERNAME% %drive_subst%: %U_AGENT_INSTANCES%
popd
goto end

rem Usage string.
:usage
    echo.
    echo Usage: %~n0  /? [ubxlib_url] [ubxlib_branch]
    echo.
    echo where:
    echo.
    echo - [ubxlib_url] optionally provides the URL to the ubxlib repo (default %url_ubxlib%),
    echo - [ubxlib_branch] optionally provides the branch of [ubxlib_url] to use (default "%branch_ubxlib%"),
    echo - /? (on its own, no other parameters) print this help and exit.
    echo.
    echo Note: one environment variable should also be populated:
    echo.
    echo - U_AGENT_INSTANCES must contain a space-separated, unquoted, list of the instance numbers
    echo   this agent supports, e.g. 0.0 0.1 0.2 0.3 1 2 3 4 5.
    echo.
    echo Tip: to remove the irritating Y/N prompt when CTRL-C is pressed, add "< nul" to the end of
    echo the command-line, e.g. %~n0 ^< nul.
    echo.

rem Done.
:end
cd %dir_start%