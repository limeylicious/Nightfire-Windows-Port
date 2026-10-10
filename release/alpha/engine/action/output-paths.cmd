@echo off
rem Keep the project working directory: game_files and analysis paths depend on it.
if not exist "logs" mkdir "logs"
if not exist "captures\screenshots" mkdir "captures\screenshots"
if not exist "captures\textures" mkdir "captures\textures"
if not "%NIGHTFIRE_NO_AUTODUMPS140%"=="1" (
  if not defined RECOMP_FB_DUMP set "RECOMP_FB_DUMP=captures\screenshots\nightfire-framebuffer"
  if not defined RECOMP_TEX_DUMP set "RECOMP_TEX_DUMP=captures\textures\nightfire-texture"
) else (
  set "RECOMP_FB_DUMP="
  set "RECOMP_TEX_DUMP="
)
if not defined NIGHTFIRE_KERNEL_LOG set "NIGHTFIRE_KERNEL_LOG=logs\xbox_kernel.log"
