#!/usr/bin/env pwsh

param (
    $Prefix = "/usr"
)

if ($IsWindows) {
    dist/scripts/vcvars.ps1
}

$qtVersion = ((qmake --version -split '\n')[1] -split ' ')[3]
Write-Host "Detected Qt Version $qtVersion"

$qmakeArgs = @(
    "PREFIX=""$Prefix""",
    "DEFINES+=""$env:nightlyDefines"""
)

if ($IsMacOS) {
    if ($qtVersion -like '5.*') {
        # QTBUG-117225
        $qmakeArgs += @("-early", "QMAKE_DEFAULT_LIBDIRS=""$(xcrun -show-sdk-path)/usr/lib""")
    } else {
        $qmakeArgs += "QMAKE_APPLE_DEVICE_ARCHS=""x86_64 arm64"""
    }
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
