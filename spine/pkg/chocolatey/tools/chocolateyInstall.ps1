$ErrorActionPreference = 'Stop'

$packageName = 'logan-spine-mcp'
$version     = '0.8.1'
$url64       = "https://github.com/DeusData/logan-spine-mcp/releases/download/v${version}/logan-spine-mcp-windows-amd64.zip"
$checksum64  = 'a602ad090ed3f49d86c55472f73f27ad7055222806a82358f2e08513e027f00f'
$installDir  = Join-Path $env:ChocolateyBinRoot $packageName

Install-ChocolateyZipPackage `
  -PackageName   $packageName `
  -Url64bit      $url64 `
  -Checksum64    $checksum64 `
  -ChecksumType64 'sha256' `
  -UnzipLocation $installDir

# Shim the binary so it is on PATH
$binPath = Join-Path $installDir 'logan-spine-mcp.exe'
Install-BinFile -Name 'logan-spine-mcp' -Path $binPath
