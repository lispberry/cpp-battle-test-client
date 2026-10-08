# Formatting and clang-tidy checks; arguments go to tools\scripts\check.py (e.g. `tidy --all`, `format --fix`).
#   tools\scripts\win\check.ps1 <format|tidy> [--all] [--fix]
# clang-tidy cannot load plugins on Windows, so the project's own sw-include-style check is skipped here.
. "$PSScriptRoot\common.ps1"

if (Get-Command uv -ErrorAction SilentlyContinue)
{
	Invoke-Native uv (@('run', 'tools/scripts/check.py') + $args)
}
else
{
	Invoke-Native python (@('tools/scripts/check.py') + $args)
}
