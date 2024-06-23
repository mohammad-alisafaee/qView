#!/usr/bin/env pwsh

param (
    $Prefix = "/usr"
)

$qtVersion = [version]((qmake --version -split '\n')[1] -split ' ')[3]
Write-Host "Detected Qt Version $qtVersion"

if ($IsWindows) {
    dist/scripts/vcvars.ps1
} elseif ($IsMacOS) {
    if ($qtVersion -ge [version]"6.5.3") {
        # GitHub macOS 13/14 runners use Xcode 15.0.x by default which has a known linker issue causing crashes if the artifact is run on macOS <= 12
        sudo xcode-select --switch /Applications/Xcode_15.3.app
    } else {
        # Keep older Qt versions on Xcode 14 due to concern over QTBUG-117484
        sudo xcode-select --switch /Applications/Xcode_14.3.1.app
    }
}

$argDeviceArchs = $IsMacOS -and $env:buildArch -eq 'Universal' ? "QMAKE_APPLE_DEVICE_ARCHS=x86_64 arm64" : $null
qmake PREFIX="$Prefix" DEFINES+="$env:nightlyDefines" $argDeviceArchs

if ($IsWindows) {
    nmake
} else {
    make
}
