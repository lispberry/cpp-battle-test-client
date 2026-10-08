# Re-record e2e expected output (tests\e2e\.expected\). Review `git diff` before committing the result.
#   tools\scripts\win\update-expected.ps1            re-record every existing .expected file
#   tools\scripts\win\update-expected.ps1 <name>     record tests\e2e\scenarios\<name>.txt, creating its file if needed
param([string]$Name = '')
. "$PSScriptRoot\common.ps1"

$target = if ($Name) { "update_expected_$Name" } else { 'update_expected' }
Invoke-Native cmake '--preset', 'debug' | Out-Null
Invoke-Native cmake '--build', '--preset', 'debug', '--target', $target
