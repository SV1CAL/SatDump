#!/bin/bash

echo "Making app shell..." 
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd $SCRIPT_DIR/../build
mkdir -p MacApp/SatDump.app/Contents/MacOS
mkdir -p MacApp/SatDump.app/Contents/Resources/plugins
cp -r ../resources MacApp/SatDump.app/Contents/Resources/resources
cp -r ../pipelines MacApp/SatDump.app/Contents/Resources/pipelines
cp ../satdump_cfg.json MacApp/SatDump.app/Contents/Resources
cp $SCRIPT_DIR/Info.plist MacApp/SatDump.app/Contents

echo "Creating app icon..." 
mkdir macOSIcon.iconset
sips -z 16 16     ../icon.png --out macOSIcon.iconset/icon_16x16.png
sips -z 32 32     ../icon.png --out macOSIcon.iconset/icon_16x16@2x.png
sips -z 32 32     ../icon.png --out macOSIcon.iconset/icon_32x32.png
sips -z 64 64     ../icon.png --out macOSIcon.iconset/icon_32x32@2x.png
sips -z 128 128   ../icon.png --out macOSIcon.iconset/icon_128x128.png
sips -z 256 256   ../icon.png --out macOSIcon.iconset/icon_128x128@2x.png
sips -z 256 256   ../icon.png --out macOSIcon.iconset/icon_256x256.png
sips -z 512 512   ../icon.png --out macOSIcon.iconset/icon_256x256@2x.png
sips -z 512 512   ../icon.png --out macOSIcon.iconset/icon_512x512.png
sips -z 1024 1024 ../icon.png --out macOSIcon.iconset/icon_512x512@2x.png
iconutil -c icns -o MacApp/SatDump.app/Contents/Resources/icon.icns macOSIcon.iconset
rm -rf macOSIcon.iconset

echo "Copying binaries..."
cp libsatdump_core.dylib MacApp/SatDump.app/Contents/MacOS
cp satdump MacApp/SatDump.app/Contents/MacOS
cp satdump-ui MacApp/SatDump.app/Contents/MacOS
cp plugins/*.dylib MacApp/SatDump.app/Contents/Resources/plugins

echo "Re-linking binaries"
plugin_args=$(ls MacApp/SatDump.app/Contents/Resources/plugins | xargs printf -- '-x MacApp/SatDump.app/Contents/Resources/plugins/%s ')
dylibbundler -cd -s /usr/local/lib -d MacApp/SatDump.app/Contents/libs -b -x MacApp/SatDump.app/Contents/MacOS/satdump -x MacApp/SatDump.app/Contents/MacOS/satdump-ui -x MacApp/SatDump.app/Contents/MacOS/libsatdump_core.dylib $plugin_args

echo "Creating SatDump.dmg..."
hdiutil create -srcfolder MacApp/ -volname SatDump SatDump-macOS.dmg

echo "Done!"