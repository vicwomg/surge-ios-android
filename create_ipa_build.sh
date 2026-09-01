#!/bin/bash
set -e

SURGE_IOS_BUNDLE_FACTORY_DATA="${SURGE_IOS_BUNDLE_FACTORY_DATA:-ON}"
case "${SURGE_IOS_BUNDLE_FACTORY_DATA}" in
  OFF|off|FALSE|false|NO|no|0)
    SURGE_IOS_BUNDLE_FACTORY_DATA=OFF
    ;;
  *)
    SURGE_IOS_BUNDLE_FACTORY_DATA=ON
    ;;
esac
APP_BUNDLE="build_ios/src/surge-xt/surge-xt_artefacts/Release/Standalone/Surge XT.app"
PAYLOAD_APP="build_ios/Payload/Surge XT.app"
IPA_PATH="build_ios/Surge XT.ipa"

# Ensure Payload directory exists and is clean
rm -rf build_ios/Payload
mkdir -p build_ios/Payload

cmake -Bbuild_ios -GXcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 -DSURGE_SKIP_DISTRIBUTION=TRUE -DSURGE_IOS_BUNDLE_FACTORY_DATA="${SURGE_IOS_BUNDLE_FACTORY_DATA}"
xcodebuild -project build_ios/Surge.xcodeproj -scheme surge-xt_Standalone -configuration Release -sdk iphoneos -allowProvisioningUpdates build

if [ "${SURGE_IOS_BUNDLE_FACTORY_DATA}" = "OFF" ]; then
  cmake -E remove_directory "${APP_BUNDLE}/SurgeXTData"
fi

cp -R "${APP_BUNDLE}" build_ios/Payload/

if [ "${SURGE_IOS_BUNDLE_FACTORY_DATA}" = "OFF" ]; then
  cmake -E remove_directory "${PAYLOAD_APP}/SurgeXTData"
fi

rm -f "${IPA_PATH}"
cd build_ios && zip -qr "Surge XT.ipa" Payload && cd ..                                             
echo "Done. IPA is in ./build_ios/Surge XT.ipa"
