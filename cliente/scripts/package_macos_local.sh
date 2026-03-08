#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CLIENT_DIR="$ROOT_DIR/cliente"
BUILD_DIR="${BUILD_DIR:-$CLIENT_DIR/build/Desktop_Qt_6_Homebrew-Debug}"
APP_NAME="appcliente.app"
SOURCE_APP="$BUILD_DIR/$APP_NAME"
DIST_DIR="${DIST_DIR:-$ROOT_DIR/dist/local-macos}"
PACKAGED_APP="$DIST_DIR/$APP_NAME"
ZIP_PATH="$DIST_DIR/appcliente-macos-local.zip"
MODEL_PACKAGE_SRC="$ROOT_DIR/../../ai-authenticity-mobile/model-package"
LEGAL_SRC="$ROOT_DIR/legal"
DISTRIBUTION_SRC="$ROOT_DIR/distribution"
MACDEPLOYQT="${MACDEPLOYQT:-/opt/homebrew/bin/macdeployqt}"
DEPLOY_QT="${DEPLOY_QT:-0}"

if [[ ! -d "$SOURCE_APP" ]]; then
  echo "Missing app bundle: $SOURCE_APP" >&2
  exit 1
fi

if [[ "$DEPLOY_QT" = "1" && ! -x "$MACDEPLOYQT" ]]; then
  echo "macdeployqt not found at $MACDEPLOYQT" >&2
  exit 1
fi

mkdir -p "$DIST_DIR"
rm -rf "$PACKAGED_APP" "$ZIP_PATH"
rsync -a "$SOURCE_APP" "$DIST_DIR/"

if [[ "$DEPLOY_QT" = "1" ]]; then
  env \
    DYLD_FRAMEWORK_PATH="/opt/homebrew/lib:/opt/homebrew/Frameworks" \
    DYLD_LIBRARY_PATH="/opt/homebrew/lib" \
    "$MACDEPLOYQT" "$PACKAGED_APP" -qmldir="$CLIENT_DIR" -libpath=/opt/homebrew/lib -libpath=/opt/homebrew/Frameworks -always-overwrite
else
  echo "Skipping macdeployqt (DEPLOY_QT=0). Bundle will reuse locally installed Qt/OpenCV/ONNX libraries."
fi

RESOURCES_DIR="$PACKAGED_APP/Contents/Resources"
FRAMEWORKS_DIR="$PACKAGED_APP/Contents/Frameworks"
MAIN_BINARY="$PACKAGED_APP/Contents/MacOS/appcliente"
mkdir -p "$RESOURCES_DIR" "$FRAMEWORKS_DIR"

rsync -a "$MODEL_PACKAGE_SRC/" "$RESOURCES_DIR/model-package/"
rsync -a "$LEGAL_SRC/" "$RESOURCES_DIR/legal/"
rsync -a "$DISTRIBUTION_SRC/" "$RESOURCES_DIR/distribution/"

resolve_dep() {
  local dep="$1"
  if [[ "$dep" = @rpath/* ]]; then
    local base
    base="$(basename "$dep")"
    local candidate
    for candidate in \
      "/opt/homebrew/lib/$base" \
      /opt/homebrew/opt/*/lib/"$base" \
      /opt/homebrew/Cellar/*/*/lib/"$base"
    do
      [[ -e "$candidate" ]] && { printf '%s\n' "$candidate"; return 0; }
    done
    return 1
  fi
  printf '%s\n' "$dep"
}

bundle_binary_deps() {
  local binary="$1"
  while IFS= read -r dep; do
    [[ -z "$dep" ]] && continue
    [[ "$dep" = /System/* || "$dep" = /usr/lib/* ]] && continue
    [[ "$dep" = @executable_path/* || "$dep" = @loader_path/* ]] && continue
    [[ "$dep" = "$PACKAGED_APP"* ]] && continue

    local resolved
    resolved="$(resolve_dep "$dep" || true)"
    [[ -z "$resolved" || ! -e "$resolved" ]] && continue

    local base
    base="$(basename "$resolved")"
    local target="$FRAMEWORKS_DIR/$base"

    if [[ ! -e "$target" ]]; then
      cp -f "$resolved" "$target"
      chmod u+w "$target"
      install_name_tool -id "@executable_path/../Frameworks/$base" "$target"
      bundle_binary_deps "$target"
    fi

    install_name_tool -change "$dep" "@executable_path/../Frameworks/$base" "$binary" 2>/dev/null || true
  done < <(otool -L "$binary" | tail -n +2 | awk '{print $1}')
}

if [[ "$DEPLOY_QT" = "1" ]]; then
  bundle_binary_deps "$MAIN_BINARY"

  while IFS= read -r bundled; do
    bundle_binary_deps "$bundled"
  done < <(find "$FRAMEWORKS_DIR" -type f \( -name "*.dylib" -o -perm -111 \))
fi

echo "Packaged app: $PACKAGED_APP"
echo "Bundle resources:"
find "$RESOURCES_DIR" -maxdepth 2 -type d | sort

ditto -c -k --keepParent "$PACKAGED_APP" "$ZIP_PATH"
echo "Zip archive: $ZIP_PATH"
