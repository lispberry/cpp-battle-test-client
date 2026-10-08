# Shared helpers for the scripts in tools/scripts/win. Dot-source it: . "$PSScriptRoot\common.ps1"
$ErrorActionPreference = 'Stop'
Set-Location (git rev-parse --show-toplevel)

# Runs a native command and stops on a non-zero exit code (PowerShell does not do that by itself).
function Invoke-Native
{
	param([Parameter(Mandatory)][string]$Command, [string[]]$Arguments = @())
	& $Command @Arguments
	if ($LASTEXITCODE -ne 0)
	{
		throw "'$Command $($Arguments -join ' ')' failed with exit code $LASTEXITCODE"
	}
}
