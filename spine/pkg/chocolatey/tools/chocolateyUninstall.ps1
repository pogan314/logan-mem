$ErrorActionPreference = 'Stop'

$packageName = 'logan-spine-mcp'
$installDir  = Join-Path $env:ChocolateyBinRoot $packageName

Uninstall-BinFile -Name 'logan-spine-mcp'

if (Test-Path $installDir) {
  Remove-Item $installDir -Recurse -Force
}
