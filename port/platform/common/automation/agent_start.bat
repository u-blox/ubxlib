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
set kill_python=
set debug_file=agent_service_output.txt

rem Process optional command line parameters
set pos=0
:parameters
    rem Retrieve arg removing quotes
    set arg=%~1
    rem pos represents the number of a positional argument
    if not "%arg%"=="" (
        if "%arg%"=="/k" (
            set kill_python=YES
        ) else if "%arg%"=="/?" (
            goto usage
        ) else if "%pos%"=="0" (
            set url_ubxlib=%arg%
            set /A pos=pos+1
        ) else if "%pos%"=="1" (
            set branch_ubxlib=%arg%
            set /A pos=pos+1
        ) else if "%pos%"=="2" (
            set debug_file=%arg%
            set /A pos=pos+1
        ) else (
            echo %~n0: ERROR can't understand parameter "%arg%".
            goto usage
        )
        shift /1
        goto parameters
    )

rem Check environment variables.
if "%U_AGENT_INSTANCES%"=="" (
    echo %~n0: ERROR environment variable U_AGENT_INSTANCES must be populated.
    goto usage
)

rem If requested, kill all existing Python processes first.
if not "%kill_python%"=="" (
    echo Killing existing Python processes...
    taskkill /IM Python.exe /F > nul 2>&1
)

rem Try to ensure a clean start
if exist %dir_ubxlib% (
    echo Removing existing %dir_ubxlib%...
    rd /s /q %dir_ubxlib%
)
rem Populate directory.
if not exist %dir_ubxlib% (
    echo Populating %dir_ubxlib% from %url_ubxlib%...
    call git clone %url_ubxlib% %dir_ubxlib%
)
echo Checking out branch "%branch_ubxlib%" of %url_ubxlib%...
pushd %dir_ubxlib%
call git fetch
call git checkout %branch_ubxlib%
popd

rem Same with Unity.
if exist %dir_unity% (
    echo Removing existing %dir_unity%...
    rd /s /q %dir_unity%
)
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
cmd /c python u_agent_service.py -u %~dp0%dir_unity% -d %~dp0%dir_work%\%debug_file% -n %COMPUTERNAME% %drive_subst%: %U_AGENT_INSTANCES%
popd
goto end

rem Usage string.
:usage
    echo.
    echo Usage: %~n0  [/?] [/k] [ubxlib_url] [ubxlib_branch] [debug_file]
    echo.
    echo where:
    echo.
    echo - [/k] optionally kill ALL python processes first; useful to get rid of zombies,
    echo - [ubxlib_url] optionally provides the URL to the ubxlib repo (default %url_ubxlib%),
    echo - [ubxlib_branch] optionally provides the branch of [ubxlib_url] to use (default "%branch_ubxlib%"),
    echo - [debug_file] optionally provides an alternative name for the window-ed debug file that the agent will write (default "%debug_file%"),
    echo - [/?] print this help and exit.
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