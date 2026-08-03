param(
    [string]$ToolchainBin = $env:LLVM_MINGW_BIN,
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
$packageRoot = Split-Path -Parent $PSScriptRoot
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $packageRoot "out"
}

function Resolve-Tool([string]$Name) {
    if ($ToolchainBin) {
        $candidate = Join-Path $ToolchainBin $Name
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    throw "Cannot find $Name. Add LLVM-MinGW bin to PATH or set LLVM_MINGW_BIN."
}

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $Program $($Arguments -join ' ')"
    }
}

$clang = Resolve-Tool "clang.exe"
$windres = Resolve-Tool "windres.exe"
$source = Join-Path $packageRoot "source"
$hidRoot = Join-Path $packageRoot "third_party\hidapi"
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$resourceObject = Join-Path $OutputDirectory "resource.o"
$applicationObject = Join-Path $OutputDirectory "better_rgb.o"
$hidObject = Join-Path $OutputDirectory "hid.o"
$executable = Join-Path $OutputDirectory "Better RGB Adaptive v3.4.5.exe"

Invoke-Checked $windres @(
    (Join-Path $source "resource.rc"), "-O", "coff", "-o", $resourceObject
)

# Treat every warning in first-party code as a release build failure.
Invoke-Checked $clang @(
    "-std=c11", "-O2", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
    "-I", $source, "-c", (Join-Path $source "better_rgb.c"), "-o", $applicationObject
)

# hidapi 0.16.0 emits three upstream strict-prototype warnings; do not use -Werror here.
Invoke-Checked $clang @(
    "-std=c11", "-O2", "-Wall", "-Wextra", "-Wpedantic",
    "-I", (Join-Path $hidRoot "hidapi"), "-I", (Join-Path $hidRoot "windows"),
    "-c", (Join-Path $hidRoot "windows\hid.c"), "-o", $hidObject
)

Invoke-Checked $clang @(
    $applicationObject, $resourceObject, $hidObject, "-o", $executable, "-mwindows",
    "-lsetupapi", "-lhid", "-lm", "-lcomdlg32", "-lcomctl32", "-lshell32",
    "-ladvapi32", "-lole32", "-loleaut32", "-lwbemuuid", "-lpdh",
    "-lgdi32", "-luser32", "-lkernel32"
)

$file = Get-Item -LiteralPath $executable
$hash = Get-FileHash -LiteralPath $executable -Algorithm SHA256
Write-Host "Build succeeded: $($file.FullName)"
Write-Host "Version: $($file.VersionInfo.FileVersion)"
Write-Host "SHA-256: $($hash.Hash)"
