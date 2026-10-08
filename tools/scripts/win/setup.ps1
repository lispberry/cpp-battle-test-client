# One-time setup on Windows: CMake, LLVM (clang-format, clang-tidy), uv and lefthook via winget, then installs the
# git hooks. Building also needs Visual Studio 2022 (or its Build Tools) with the "Desktop development with C++"
# workload, which this script only checks for.
. "$PSScriptRoot\common.ps1"

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$hasMsvc = (Test-Path $vswhere) -and (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
if (-not $hasMsvc)
{
	Write-Warning 'MSVC not found. Install Visual Studio 2022 Build Tools with the C++ workload: winget install Microsoft.VisualStudio.2022.BuildTools'
}

foreach ($package in 'Kitware.CMake', 'LLVM.LLVM', 'astral-sh.uv')
{
	winget install --exact --id $package --accept-source-agreements --accept-package-agreements
	# winget exits non-zero when the package is already installed; that is fine.
}

# Pick up the PATH entries the installers just added.
$env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')

Invoke-Native uv 'tool', 'install', 'lefthook'
Invoke-Native lefthook 'install'
Write-Host 'Done. Build and test with: tools\scripts\win\test.ps1'
