param([string]$Executable = "")

$ErrorActionPreference = "Stop"
$packageRoot = Split-Path -Parent $PSScriptRoot
if (-not $Executable) {
    $Executable = Join-Path $packageRoot "binary\Better RGB Adaptive v3.6.0.exe"
}
if (-not (Test-Path -LiteralPath $Executable)) { throw "EXE not found: $Executable" }

$file = Get-Item -LiteralPath $Executable
$signature = Get-AuthenticodeSignature -LiteralPath $Executable
$hash = Get-FileHash -LiteralPath $Executable -Algorithm SHA256

if ($file.VersionInfo.FileVersion -ne "3.6.0") {
    throw "Unexpected file version: $($file.VersionInfo.FileVersion)"
}
if ($file.Length -lt 100KB) { throw "Unexpected EXE size: $($file.Length)" }

Write-Host "Path: $($file.FullName)"
Write-Host "Size: $($file.Length) bytes"
Write-Host "Version: $($file.VersionInfo.FileVersion)"
Write-Host "Signature: $($signature.Status) (NotSigned is expected for this release)"
Write-Host "SHA-256: $($hash.Hash)"
