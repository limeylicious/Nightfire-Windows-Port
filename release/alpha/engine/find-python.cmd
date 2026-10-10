@echo off
rem Sets NF_PY to a Python 3.8 or newer: "python" on PATH, else the "py -3" launcher.
set "NF_PY="
python -c "import sys; sys.exit(sys.version_info < (3, 8))" >nul 2>&1
if not errorlevel 1 (
  set "NF_PY=python"
  exit /b 0
)
py -3 -c "import sys; sys.exit(sys.version_info < (3, 8))" >nul 2>&1
if not errorlevel 1 (
  set "NF_PY=py -3"
  exit /b 0
)
echo Python 3.8 or newer was not found. Install it from https://www.python.org/downloads/
echo (tick "Add python.exe to PATH" in the installer), then try again.
exit /b 1
