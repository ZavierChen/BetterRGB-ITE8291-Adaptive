param([Parameter(Mandatory=$true)][string]$ToolchainBin, [Parameter(Mandatory=$true)][string]$BuildDirectory)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$clang = Join-Path $ToolchainBin 'clang.exe'
$source = Join-Path $root 'source'
$catalog = [IO.File]::ReadAllText((Join-Path $source 'ui_strings.def'))
$entries = [regex]::Matches($catalog, 'UI_STRING\("((?:[^"\\]|\\.)*)", "((?:[^"\\]|\\.)*)"\)')
$translations = @{}
foreach ($entry in $entries) {
    $zh = $entry.Groups[1].Value; $en = $entry.Groups[2].Value
    if ($translations.ContainsKey($zh)) { throw "Duplicate key: $zh" }
    $translations[$zh] = $en
    $format = '%(?:[-+ #0]*\d*(?:\.\d+)?[hl]*[diuoxXfFeEgGcs]|%)'
    $zhFormats = [regex]::Matches($zh,$format).Value -join '|'
    $enFormats = [regex]::Matches($en,$format).Value -join '|'
    if ($zhFormats -ne $enFormats) { throw "Format mismatch: $zh" }
}
$code = [IO.File]::ReadAllText((Join-Path $source 'better_rgb.c'))
foreach ($match in [regex]::Matches($code, 'ui_text\((L"(?:[^"\\]|\\.)*"(?:\s*L"(?:[^"\\]|\\.)*")*)\)')) {
    $key = ([regex]::Matches($match.Groups[1].Value, 'L"((?:[^"\\]|\\.)*)"') | ForEach-Object {$_.Groups[1].Value}) -join ''
    if (-not $translations.ContainsKey($key)) { throw "Missing translation: $key" }
}
$exe = Join-Path $build 'ui_smoke.exe'
& $clang '-std=c11' '-O0' '-g' '-Wall' '-Wextra' '-Wpedantic' '-Werror' '-I' $source (Join-Path $PSScriptRoot 'ui_smoke.c') (Join-Path $build 'i18n.o') (Join-Path $build 'color_math.o') (Join-Path $build 'resource.o') (Join-Path $build 'hid.o') '-o' $exe '-lsetupapi' '-lhid' '-lm' '-lcomdlg32' '-lcomctl32' '-lshell32' '-ladvapi32' '-lole32' '-loleaut32' '-lwbemuuid' '-lpdh' '-lgdi32' '-luser32' '-lkernel32'
if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
Push-Location $build
try {
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "UI smoke test failed: $LASTEXITCODE" }
} finally { Pop-Location }
Write-Host "PASS: $($entries.Count) translations and all format signatures."
