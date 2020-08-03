@echo off
echo This batch file performs the tools install/export, build and download phases for ESP32.
echo It is simplest to do this together in one batch file as the processes share enviroment
echo variables.
echo.

setlocal EnableDelayedExpansion

rem Local variables
set clean_build_dir=
set esp-idf_dir=
set code_dir=
set com_port=
set return_code=1
set component_dir=runner
set component_name=ubxlib_runner

rem Process command line parameters
set pos=0
:parameters
    rem Retrieve arg removing quotes from paths (so that we can concatenate paths here)
    set arg=%~1
    rem pos represents the number of a positional argument
    if not "%arg%"=="" (
        if "%arg%"=="/c" (
            set clean_build_dir=YES
        ) else if "%pos%"=="0" (
            set esp-idf_dir=%arg%
            set /A pos=pos+1
        ) else if "%pos%"=="1" (
            set code_dir=%arg%
            set /A pos=pos+1
        ) else if "%pos%"=="2" (
            set build_dir=%arg%
            set /A pos=pos+1
        ) else if "%pos%"=="3" (
            set com_port=%arg%
            set /A pos=pos+1
        ) else (
            echo %~n0: ERROR can't understand parameter "%arg%".
            goto usage
        )
        shift /1
        goto parameters
    )

rem Check esp-idf directory
if "%esp-idf_dir%"=="" (
    echo %~n0: ERROR must specify the esp-idf directory.
    goto usage
)
echo %~n0: esp-idf_dir is "%esp-idf_dir%".

rem Check code directory
if "%code_dir%"=="" (
    echo %~n0: ERROR must specify a code directory.
    goto usage
)
echo %~n0: code_dir is "%code_dir%".

rem Check build directory
if "%build_dir%"=="" (
    echo %~n0: ERROR must specify a build directory.
    goto usage
)
echo %~n0: build_dir is "%build_dir%".

rem Check COM port
if "%com_port%"=="" (
    echo %~n0: ERROR must specify a COM port, e.g. COM1.
    goto usage
)
echo %~n0: COM port is %com_port%.

rem To run installation processes and kill any launched executables we need
rem administrator privileges, so check for that here, see:
rem https://stackoverflow.com/questions/1894967/how-to-request-administrator-access-inside-a-batch-file/10052222#10052222
if "%PROCESSOR_ARCHITECTURE%"=="amd64" (
    >nul 2>&1 "%SYSTEMROOT%\SysWOW64\cacls.exe" "%SYSTEMROOT%\SysWOW64\config\system"
) else (
    >nul 2>&1 "%SYSTEMROOT%\system32\cacls.exe" "%SYSTEMROOT%\system32\config\system"
)
rem If error flag is set, we do not have admin.
if not !ERRORLEVEL! EQU 0 (
    echo %~n0: ERROR administrator privileges are required, please run as administrator.
    goto end
)

rem Build start
:build_start
    rem Deal with the build directory
    if exist "%build_dir%" (
        if not "%clean_build_dir%"=="" (
            echo %~n0: cleaning build directory "%build_dir%"...
            del /s /q /f "%build_dir%\*" 1>nul
            for /f "delims=" %%f in ('dir /ad /b "%build_dir%"') do rd /s /q "%build_dir%\%%f" 1>nul
        )
    )
    if not exist "%build_dir%" (
        echo %~n0: build_directory "%build_dir%" does not exist, creating it...
        md "%build_dir%"
        if not exist "%build_dir%" (
            echo %~n0: ERROR unable to create build directory "%build_dir%" ^(you must use backslash \, not /^).
            goto usage
        )
    )
    echo %~n0: the esp-idf tools will be installed in IDF_TOOLS_PATH ^(%IDF_TOOLS_PATH%^).
    echo %~n0: calling esp-idf install.bat and export.bat from "%esp-idf_dir%"...
    pushd %esp-idf_dir%
    echo %~n0: %CD%
    call install.bat
    call export.bat
    popd
    echo %~n0: environment variables will be:
    set
    echo %~n0: building tests in "%build_dir%" and then downloading them over %com_port%...
    rem print Python version for debug purposes
    python --version
    rem calling python blah.py here rather than just blah.py as that uses the first
    rem Python instance on the path (which export.bat may have altered) rather
    rem than the one associated with the extension .py in the Windows registry
    @echo on
    python %esp-idf_dir%\tools\idf.py -p %com_port% -C %code_dir%\port\platform\espressif\esp32\sdk\esp-idf\%component_dir% -B %build_dir% -D TEST_COMPONENTS="%component_name%" size flash
    @echo off
    set return_code=!ERRORLEVEL!
    if not !ERRORLEVEL! EQU 0 (
        echo %~n0: ERROR build or download failed.
    )
    goto end

rem Usage string
:usage
    echo.
    echo Usage: %~n0 [/c] esp-idf_dir code_dir build_dir comport
    echo.
    echo where:
    echo.
    echo - /c indicates that the build directory should be cleaned first.
    echo - esp-idf_dir is the location of esp-idf.
    echo - code_dir is the path to the root directory of ubxlib.
    echo - build_dir is the directory in which to do building.
    echo - comport is the port where the device under test is connected (e.g. COM1).
    echo.
    echo All paths must be absolute and must use Windows style directory separators.
    echo Before calling this batch file the environment variables IDF_TOOLS_PATH
    echo and U_FLAGS should be configured as required.
    echo.
    echo Note that the installation of tools requires administrator privileges and 
    echo hence this batch file MUST be run as administrator.
    goto end

rem Done
:end
    echo.
    echo %~n0: end with return code !return_code!.
    exit /b !return_code!
