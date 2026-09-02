#!/bin/bash
set -euo pipefail

# LuaJIT requires this when compiling host-side tools on macOS.
export MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-12.0}"

if [ -z "${JAVA_HOME:-}" ]; then
  if [ -d "/Applications/Android Studio.app/Contents/jbr/Contents/Home" ]; then
    export JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home"
  elif [ -d "/Library/Java/JavaVirtualMachines/jdk-21.jdk/Contents/Home" ]; then
    export JAVA_HOME="/Library/Java/JavaVirtualMachines/jdk-21.jdk/Contents/Home"
  fi
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Building Android APK using the Android Studio project..."
cd "${SCRIPT_DIR}/android"
./gradlew :app:assembleRelease

echo "Done. APK output is under android/app/build/outputs/apk/release/."
