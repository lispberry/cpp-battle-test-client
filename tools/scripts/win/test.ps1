# Configure, build and run the tests with a CMake preset (Visual Studio generator).
#   tools\scripts\win\test.ps1 [-Preset debug|release]    (default: debug; asan is for clang/gcc only)
param([ValidateSet('debug', 'release')][string]$Preset = 'debug')
. "$PSScriptRoot\common.ps1"

Invoke-Native cmake '--preset', $Preset
Invoke-Native cmake '--build', '--preset', $Preset
Invoke-Native ctest '--preset', $Preset
