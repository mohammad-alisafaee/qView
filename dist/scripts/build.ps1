#!/usr/bin/env pwsh

param (
    $Prefix = "/usr"
)

$qtVersion = [version]((qmake --version -split '\n')[1] -split ' ')[3]
Write-Host "Detected Qt Version $qtVersion"

if ($IsWindows) {
    dist/scripts/vcvars.ps1
} elseif ($IsMacOS) {
    if ($qtVersion -lt [version]"6.0") {
        # Keep Qt 5.x build on Xcode 14 due to concern over QTBUG-117484
        sudo xcode-select --switch /Applications/Xcode_14.3.1.app
    } else {
        # GitHub macOS 13/14 runners use Xcode 15.0.x by default which has a known linker issue causing crashes if the artifact is run on macOS <= 12
        sudo xcode-select --switch /Applications/Xcode_15.3.app
    }
}

$argDeviceArchs = $IsMacOS -and $qtVersion -ge [version]"6.0" ? "QMAKE_APPLE_DEVICE_ARCHS=x86_64 arm64" : $null;
qmake PREFIX="$Prefix" DEFINES+="$env:nightlyDefines" $argDeviceArchs

if ($IsWindows) {
    nmake
} else {
    make
}
