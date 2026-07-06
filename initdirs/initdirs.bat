@echo off
setlocal enabledelayedexpansion

REM 1. We ask the user for the path to the root directory
set "ROOT_DIR=%USERPROFILE%\my_space"
set /p "input=Enter the path to the root directory (default %ROOT_DIR%): "
if not "!input!"=="" set "ROOT_DIR=!input!"

REM 2. Create a directory if it does not exist
if not exist "!ROOT_DIR!" (
    mkdir "!ROOT_DIR!" || (echo ERROR: Failed to create directory "!ROOT_DIR!" & exit /b 1)
)

REM 3. Let's go there
cd "!ROOT_DIR!" || (echo ERROR: Failed to navigate to directory "!ROOT_DIR!" & exit /b 1)

REM 4. Create the necessary directories
mkdir bin script daemon manifest files res || (echo ERROR: Failed to create directory & exit /b 1)

REM 5. Go to the bin directory
cd bin || (echo ERROR: Failed to navigate to directory bin & exit /b 1)

REM 6. Create symlinks to manifest and files
mklink /D manifest ..\manifest || (echo ERROR: Failed to create symlink to manifest & exit /b 1)
mklink /D files ..\files || (echo ERROR: Failed to create symlink to files & exit /b 1)

REM 7. Create files
type nul > "..\res\server.conf" || (echo ERROR: Failed to create create file server.conf & exit /b 1)
type nul > "..\res\client.conf" || (echo ERROR: Failed to create file client.conf & exit /b 1)
type nul > "..\res\nodes" || (echo ERROR: Failed to create file nodes & exit /b 1)
type nul > "..\res\clients" || (echo ERROR: Failed to create file clients & exit /b 1)
type nul > "..\res\my_contact" || (echo ERROR: Failed to create file my_contact & exit /b 1)

REM 8. Write string in files
(
    echo [nodes] = [!ROOT_DIR!\res\nodes]
    echo [clients] = [!ROOT_DIR!\res\clients]
) > "..\res\server.conf" || (echo ERROR: Failed to write to server.conf & exit /b 1)

(
    echo [nodes] = [!ROOT_DIR!\res\nodes]
) > "..\res\client.conf" || (echo ERROR: Failed to write to client.conf & exit /b 1)

REM 9. Create symlinks on client.conf and server.conf
mklink client.conf ..\res\client.conf || (echo ERROR: Failed to create symlink to client.conf & exit /b 1)
mklink server.conf ..\res\server.conf || (echo ERROR: Failed to create symlink to server.conf & exit /b 1)
mklink my_contact ..\res\my_contact || (echo ERROR: Failed to create symlink to my_contact & exit /b 1)

REM If all actions are successful, return OK
echo OK
