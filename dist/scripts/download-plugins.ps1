#!/usr/bin/env pwsh

$qtVersion = [version](qmake -query QT_VERSION)
Write-Host "Detected Qt version $qtVersion"

$osName =
    $IsWindows ? 'Windows' :
    $IsMacOS ? 'macOS' :
    $IsLinux ? 'Linux' :
    $null

$binaryBaseUrl = "https://github.com/jdpurcell/kimageformats-binaries/releases/download/cont"

$pluginNames = @('QtApng', 'KImageFormats')

foreach ($pluginName in $pluginNames) {
    $artifactName = "$pluginName-$osName-$qtVersion-$env:buildArch.zip"
    $downloadUrl = "$binaryBaseUrl/$artifactName"

    Write-Host "Downloading $downloadUrl"
    Invoke-WebRequest -Uri $downloadUrl -OutFile $artifactName
    Expand-Archive $artifactName -DestinationPath "."
    Remove-Item $artifactName
}

if ($IsWindows) {
    $out_frm = "bin"
    $out_imf = "bin/imageformats"
} elseif ($IsMacOS) {
    $out_frm = "bin/qView.app/Contents/Frameworks"
    $out_imf = "bin/qView.app/Contents/PlugIns/imageformats"
} else {
    $out_frm = "bin/appdir/usr/lib"
    $out_imf = "bin/appdir/usr/plugins/imageformats"
}

New-Item -Type Directory -Path $out_frm -Force
New-Item -Type Directory -Path $out_imf -Force

# Move QtApng library
if ($pluginNames -contains 'QtApng') {
    mv QtApng/output/* "$out_imf"
}

# Move KImageFormats libraries
if ($pluginNames -contains 'KImageFormats') {
    mv KImageFormats/output/kimg_* "$out_imf"
    mv KImageFormats/output/* "$out_frm"
}
