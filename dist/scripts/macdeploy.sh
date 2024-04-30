#!/usr/bin/bash

if [[ -n "$1" ]]; then
    VERSION=$0
else
    VERSION=$(LC_ALL=C sed -n -e '/^VERSION/p' qView.pro)
    VERSION=${VERSION: -3}
fi

if [[ -n "$APPLE_DEVID_APP_CERT_DATA" ]]; then
    CODESIGN_CERT_PATH=$RUNNER_TEMP/codesign.p12
    KEYCHAIN_PATH=$RUNNER_TEMP/codesign.keychain-db
    KEYCHAIN_PASS=$(uuidgen)

    echo -n "$APPLE_DEVID_APP_CERT_DATA" | base64 --decode -o "$CODESIGN_CERT_PATH"

    security create-keychain -p "$KEYCHAIN_PASS" "$KEYCHAIN_PATH"
    security unlock-keychain -p "$KEYCHAIN_PASS" "$KEYCHAIN_PATH"
    security import "$CODESIGN_CERT_PATH" -P "$APPLE_DEVID_APP_CERT_PASS" -A -t cert -f pkcs12 -k "$KEYCHAIN_PATH"
    security set-key-partition-list -S apple-tool:,apple: -s -k "$KEYCHAIN_PASS" "$KEYCHAIN_PATH"
    security list-keychains -d user -s "$KEYCHAIN_PATH"

    CODESIGN_CERT_NAME=$(openssl pkcs12 -in "$CODESIGN_CERT_PATH" -passin pass:"$APPLE_DEVID_APP_CERT_PASS" -nokeys -clcerts -info 2>&1 | openssl x509 -noout -subject -nameopt multiline | grep commonName | awk -F'= ' '{print $2}')
else
    CODESIGN_CERT_NAME=-
fi

cd bin

macdeployqt qView.app
if [[ "$APPLE_NOTARIZE_REQUESTED" == "true" ]]; then
    APP_IDENTIFIER=$(/usr/libexec/PlistBuddy -c "Print CFBundleIdentifier" "qView.app/Contents/Info.plist")
    codesign --sign "$CODESIGN_CERT_NAME" --deep --options runtime --timestamp "qView.app"
else
    codesign --sign "$CODESIGN_CERT_NAME" --deep "qView.app"
fi

if [[ -n "$1" ]]; then
    BUILD_NAME=qView-JDP-$1
    DMG_FILENAME=$BUILD_NAME-macOS$2.dmg
    hdiutil create -srcfolder "qView.app" -volname "$BUILD_NAME" -format UDSB "qView.sparsebundle"
    hdiutil convert "qView.sparsebundle" -format ULFO -o "$DMG_FILENAME"
    rm -r qView.sparsebundle
else
    DMG_FILENAME=qView-JDP-$VERSION.dmg
    brew install create-dmg
    create-dmg --volname "qView-JDP $VERSION" --filesystem APFS --format ULFO --window-size 660 400 --icon-size 160 --icon "qView.app" 180 170 --hide-extension qView.app --app-drop-link 480 170 "$DMG_FILENAME" "qView.app"
fi
if [[ "$APPLE_NOTARIZE_REQUESTED" == "true" ]]; then
    codesign --sign "$CODESIGN_CERT_NAME" --timestamp --identifier "$APP_IDENTIFIER.dmg" "$DMG_FILENAME"
    xcrun notarytool submit "$DMG_FILENAME" --apple-id "$APPLE_ID_USER" --password "$APPLE_ID_PASS" --team-id "${CODESIGN_CERT_NAME: -11:10}" --wait
    xcrun stapler staple "$DMG_FILENAME"
    xcrun stapler validate "$DMG_FILENAME"
fi

rm -r *.app
