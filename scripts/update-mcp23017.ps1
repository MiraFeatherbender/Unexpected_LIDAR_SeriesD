<#
PowerShell helper to update the MCP23017 submodule to latest upstream.
Run from the repository root in PowerShell.

Example:
  .\scripts\update-mcp23017.ps1
#>

param(
    [string]$Path = 'components/mcp23017'
)

Write-Host "Updating submodule at $Path to tracked remote branch"
git submodule update --remote --merge $Path

Write-Host 'Staging and committing submodule pointer update (if any)'
git add $Path
if (git commit -m "Update mcp23017 submodule" -q) {
    Write-Host 'Committed submodule update.'
} else {
    Write-Host 'No changes to commit.'
}
