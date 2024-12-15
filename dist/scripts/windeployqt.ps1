param (
    $NightlyVersion = ""
)

$qtVersion = [version](qmake -query QT_VERSION)
Write-Host "Detected Qt version $qtVersion"

# Ship OpenSSL for older Qt versions. Starting 6.8, windeployqt doesn't deploy the OpenSSL backend
# by default; although it's easy to opt in, the Schannel backend seems solid enough at this point.
if ($qtVersion -lt [version]'6.8' -and $env:buildArch -ne 'Arm64') {
    # Download and extract OpenSSL
    if ($qtVersion -lt [version]'6.5') {
        $openSslDownloadUrl = "https://download.firedaemon.com/FireDaemon-OpenSSL/openssl-1.1.1w.zip"
        $openSslSubfolder = "openssl-1.1\"
        $openSslFilenameVersion = "1_1"
    } else {
        $openSslDownloadUrl = "https://download.firedaemon.com/FireDaemon-OpenSSL/openssl-3.4.0.zip"
        $openSslSubfolder = ""
        $openSslFilenameVersion = "3"
    }
    Write-Host "Downloading $openSslDownloadUrl"
    $ProgressPreference = 'SilentlyContinue'
    Invoke-WebRequest -Uri $openSslDownloadUrl -OutFile openssl.zip
    7z x -y openssl.zip -o"openssl"

    # Copy to output dir
    if ($env:buildArch -eq 'X86') {
        copy "openssl\${openSslSubfolder}x86\bin\libssl-$openSslFilenameVersion.dll" bin
        copy "openssl\${openSslSubfolder}x86\bin\libcrypto-$openSslFilenameVersion.dll" bin
    } else {
        copy "openssl\${openSslSubfolder}x64\bin\libssl-$openSslFilenameVersion-x64.dll" bin
        copy "openssl\${openSslSubfolder}x64\bin\libcrypto-$openSslFilenameVersion-x64.dll" bin
    }
}

# Run windeployqt
$isCrossCompile = $env:buildArch -eq 'Arm64'
$winDeployQt = $isCrossCompile ? "$env:QT_HOST_PATH\bin\windeployqt" : "windeployqt"
$argQtPaths = $isCrossCompile ? "--qtpaths=$env:QT_ROOT_DIR\bin\qtpaths.bat" : $null
& $winDeployQt $argQtPaths --no-compiler-runtime "bin\qView.exe"

if ($qtVersion -ge [version]'6.8.1') {
    # Copy font so windows11 style can work on Windows 10
    New-Item -ItemType Directory -Path "bin\fonts" -Force
    Copy-Item -Path "dist\win\fonts\Segoe Fluent Icons.ttf" -Destination "bin\fonts"
}

$imfDir = "bin\imageformats"
if ((Test-Path "$imfDir\kimg_tga.dll") -and (Test-Path "$imfDir\qtga.dll")) {
    # Prefer kimageformats TGA plugin which supports more formats
    Write-Output "Removing duplicate TGA plugin"
    Remove-Item "$imfDir\qtga.dll"
}

if ($NightlyVersion -eq '') {
    # Call innomake if we are not building a nightly version (no version passed)
    & "dist/scripts/innomake.ps1"
}
