#!/usr/bin/env pwsh

param (
    $Prefix = "/usr"
)

$qtVersion = ((qmake --version -split '\n')[1] -split ' ')[3]
Write-Host "Detected Qt Version $qtVersion"

if ($IsWindows) {
    dist/scripts/vcvars.ps1
} elseif ($IsMacOS) {
    if ($qtVersion -like '5.*') {
        # Keep Qt 5.x build on Xcode 14 due to concern over QTBUG-117484
        sudo xcode-select --switch /Applications/Xcode_14.3.1.app
    } else {
        # GitHub macOS 13/14 runners use Xcode 15.0.x by default which has a known linker issue causing crashes if the artifact is run on macOS <= 12
        sudo xcode-select --switch /Applications/Xcode_15.3.app
    }
}

$qmakeArgs = @(
    "PREFIX=""$Prefix""",
    "DEFINES+=""$env:nightlyDefines"""
)

if ($IsMacOS -and $qtVersion -notlike '5.*') {
    $qmakeArgs += "QMAKE_APPLE_DEVICE_ARCHS=""x86_64 arm64"""
}

Write-Host "Running 'qmake' w/ args: $qmakeArgs"
Invoke-Expression "qmake $qmakeArgs"

if ($IsWindows) {
    Write-Host "Running 'nmake'"
    nmake
} else {
    Write-Host "Running 'make'"
    make
}
