#!/usr/bin/bash

if [[ -z "$1" ]]; then
    RELEASE_VER=$(LC_ALL=C sed -n -e '/^VERSION/p' qView.pro)
    RELEASE_VER=${RELEASE_VER: -3}
fi

sudo apt update
sudo apt install libfuse2

if [[ "$buildArch" == "X64" ]]; then
    ARCH_NAME="x86_64"
elif [[ "$buildArch" == "Arm64" ]]; then
    ARCH_NAME="aarch64"
else
    echo "Unsupported build architecture." >&2
    exit 1
fi

wget -c -nv "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$ARCH_NAME.AppImage"
wget -c -nv "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$ARCH_NAME.AppImage"
chmod a+x linuxdeploy-*.AppImage

mkdir -p bin/appdir/usr
make install INSTALL_ROOT=bin/appdir
cd bin
rm qview

../linuxdeploy-$ARCH_NAME.AppImage \
    --plugin qt \
    --appdir appdir \
    --output appimage

if [[ -n "$1" ]]; then
    mv *.AppImage qView-JDP-$1-Linux_$ARCH_NAME.AppImage
else
    mv *.AppImage qView-JDP-$RELEASE_VER-Linux_$ARCH_NAME.AppImage
fi
rm -r appdir
