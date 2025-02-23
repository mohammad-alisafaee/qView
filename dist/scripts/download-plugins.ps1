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

function MoveLibraries($category, $destDir, $files) {
    foreach ($file in $files) {
        Write-Host "${category}: $($file.Name) ($($file.LastWriteTimeUtc.ToString("yyyy-MM-dd HH:mm:ss")))"
        Move-Item -Path $file.FullName -Destination $destDir
    }
}

# Deploy QtApng
if ($pluginNames -contains 'QtApng') {
    Write-Host "`nDeploying QtApng:"
    MoveLibraries 'imf' $out_imf (Get-ChildItem "QtApng/output")
}

# Deploy KImageFormats
if ($pluginNames -contains 'KImageFormats') {
    Write-Host "`nDeploying KImageFormats:"
    MoveLibraries 'imf' $out_imf (Get-ChildItem "KImageFormats/output" -Filter "kimg_*")
    MoveLibraries 'frm' $out_frm (Get-ChildItem "KImageFormats/output")
}

Write-Host ''
