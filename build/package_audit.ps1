param([string]$PackageRoot = "")

$ErrorActionPreference = "Stop"
if (-not $PackageRoot) { $PackageRoot = Split-Path -Parent $PSScriptRoot }
$root = (Resolve-Path -LiteralPath $PackageRoot).Path
$manifestPath = Join-Path $root "SHA256SUMS.txt"
$files = Get-ChildItem -LiteralPath $root -Recurse -File |
    Where-Object { $_.FullName -ne $manifestPath }

$forbiddenExtensions = @(".pdb", ".ilk", ".obj", ".o", ".key", ".pfx", ".p12")
$forbidden = $files | Where-Object { $forbiddenExtensions -contains $_.Extension.ToLowerInvariant() }
if ($forbidden) {
    $names = ($forbidden.FullName -join [Environment]::NewLine)
    throw "Forbidden build or secret artifacts found:`n$names"
}

$manifest = foreach ($file in $files | Sort-Object FullName) {
    $relative = $file.FullName.Substring($root.Length + 1).Replace('\', '/')
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    "{0}  {1}" -f $hash, $relative
}
$manifest | Set-Content -LiteralPath $manifestPath -Encoding UTF8
Write-Host "Audited $($files.Count) files. SHA256SUMS.txt generated."
