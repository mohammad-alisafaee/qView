#!/usr/bin/env pwsh

# This script will download binary plugins from the kimageformats-binaries repository using Github's API.

$qtVersion = [version]((qmake --version -split '\n')[1] -split ' ')[3]
Write-Host "Detected Qt Version $qtVersion"

$osName =
    $IsWindows ? 'Windows' :
    $IsMacOS ? 'macOS' :
    $IsLinux ? 'Linux' :
    $null

$binaryBaseUrl = "https://github.com/jdpurcell/kimageformats-binaries/releases/download/cont"

$pluginNames = @('QtApng', 'KImageFormats')

if ($pluginNames.count -eq 0) {
    Write-Host "the pluginNames array is empty."
}

foreach ($pluginName in $pluginNames) {
    $artifactName = "$pluginName-$osName-$qtVersion-$env:buildArch.zip"
    $downloadUrl = "$binaryBaseUrl/$artifactName"

    Write-Host "Downloading $downloadUrl"
    Invoke-WebRequest -URI $downloadUrl -OutFile $artifactName
    Expand-Archive $artifactName -DestinationPath $pluginName
    Remove-Item $artifactName
}

if ($IsWindows) {
    $out_frm = "bin/"
    $out_imf = "bin/imageformats"
} elseif ($IsMacOS) {
    $out_frm = "bin/qView.app/Contents/Frameworks"
    $out_imf = "bin/qView.app/Contents/PlugIns/imageformats"
} else {
    $out_frm = "bin/appdir/usr/lib"
    $out_imf = "bin/appdir/usr/plugins/imageformats"
}

New-Item -Type Directory -Path "$out_frm" -ErrorAction SilentlyContinue
New-Item -Type Directory -Path "$out_imf" -ErrorAction SilentlyContinue

# Copy QtApng
if ($pluginNames -contains 'QtApng') {
    if ($IsWindows) {
        cp QtApng/QtApng/output/qapng.dll "$out_imf/"
    } elseif ($IsMacOS) {
        cp QtApng/QtApng/output/libqapng.* "$out_imf/"
    } else {
        cp QtApng/QtApng/output/libqapng.so "$out_imf/"
    }
}

function CopyFrameworkDlls($mainDll, $otherDlls) {
    if (-not (Test-Path -Path KImageFormats/KImageFormats/output/$mainDll -PathType Leaf)) {
        return
    }
    foreach ($dll in @($mainDll) + $otherDlls) {
        cp KImageFormats/KImageFormats/output/$dll "$out_frm/"
    }
}

if ($pluginNames -contains 'KImageFormats') {
    $kfMajorVer = $qtVersion -ge [version]'6.5.0' ? 6 : 5
    if ($IsWindows) {
        mv KImageFormats/KImageFormats/output/kimg_*.dll "$out_imf/"
        CopyFrameworkDlls "KF$($kfMajorVer)Archive.dll" @("zlib1.dll")
        if ($env:buildArch -eq 'X86') {
            CopyFrameworkDlls "avif.dll" @("aom.dll")
            CopyFrameworkDlls "heif.dll" @("aom.dll", "libde265.dll")
        } else {
            CopyFrameworkDlls "avif.dll" @("dav1d.dll")
            CopyFrameworkDlls "heif.dll" @("libde265.dll")
        }
        CopyFrameworkDlls "raw.dll" @("lcms2.dll", "zlib1.dll")
        CopyFrameworkDlls "jxl.dll" @("brotlicommon.dll", "brotlidec.dll", "brotlienc.dll", "hwy.dll", "jxl_cms.dll", "jxl_threads.dll", "lcms2.dll")
        CopyFrameworkDlls "OpenEXR-3_2.dll" @("deflate.dll", "Iex-3_2.dll", "IlmThread-3_2.dll", "Imath-3_1.dll", "OpenEXRCore-3_2.dll")
    } elseif ($IsMacOS) {
        cp KImageFormats/KImageFormats/output/kimg_*.* "$out_imf/"
        cp KImageFormats/KImageFormats/output/libKF?Archive.?.dylib "$out_frm/"
    } else {
        cp KImageFormats/KImageFormats/output/kimg_*.so "$out_imf/"
        cp KImageFormats/KImageFormats/output/libKF?Archive.so.? "$out_frm/"
    }
}
