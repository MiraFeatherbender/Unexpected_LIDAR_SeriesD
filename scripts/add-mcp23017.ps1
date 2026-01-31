<#
PowerShell helper to add the MCP23017 component as a git submodule.
Run from the repository root in PowerShell.

Example:
  .\scripts\add-mcp23017.ps1
#>

param(
    [string]$Remote = 'https://github.com/MiraFeatherbender/MCP23017.git',
    [string]$Branch = 'main',
    [string]$Path = 'components/mcp23017'
)

Write-Host "Adding submodule $Remote (branch $Branch) -> $Path"
git submodule add -b $Branch $Remote $Path

Write-Host 'Initializing submodules...'
git submodule update --init --recursive

Write-Host 'Done. If happy with the result, run:'
Write-Host '  git add .gitmodules $Path'
Write-Host '  git commit -m "Add mcp23017 component as submodule"'
