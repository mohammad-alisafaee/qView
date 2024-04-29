#!/usr/bin/bash

if [[ -n "$1" ]]; then
    VERSION=$0
else
    VERSION=$(LC_ALL=C sed -n -e '/^VERSION/p' qView.pro)
    VERSION=${VERSION: -3}
fi

cd bin

macdeployqt qView.app
codesign --sign - --deep qView.app
if [[ -n "$1" ]]; then
    BUILD_NAME=qView-JDP-$1
    hdiutil create -srcfolder "qView.app" -volname "$BUILD_NAME" -format UDSB "qView.sparsebundle"
    hdiutil convert "qView.sparsebundle" -format ULFO -o "$BUILD_NAME-macOS$2.dmg"
    rm -r qView.sparsebundle
else
    brew install create-dmg
    create-dmg --volname "qView-JDP $VERSION" --filesystem APFS --format ULFO --window-size 660 400 --icon-size 160 --icon "qView.app" 180 170 --hide-extension qView.app --app-drop-link 480 170 "qView-JDP-$VERSION.dmg" "qView.app"
fi

rm -r *.app
