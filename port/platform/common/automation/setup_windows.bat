@echo off

SET SCRIPTS_DIR=%~dp0

echo|set /p="Looking for python: "
WHERE python > nul
IF %ERRORLEVEL% NEQ 0 (
    echo NOT found
    echo Please install Python 3
)
echo Found

echo|set /p="Looking for pip3: "
WHERE pip3 > nul
IF %ERRORLEVEL% NEQ 0 (
    echo NOT found
    echo Please make sure pip3 is is in your PATH variable
)
echo Found

pip3 install -r %SCRIPTS_DIR%/requirements.txt
pip3 install windows-curses