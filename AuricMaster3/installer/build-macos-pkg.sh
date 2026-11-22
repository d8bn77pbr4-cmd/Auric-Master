#!/bin/sh
# Builds "Auric Master 3 Installer.pkg" from the signed bundles in ./dist (run from the repo root on macOS).
# Installs VST3 + AU into /Library/Audio/Plug-Ins (all users) and refreshes the Audio Unit registry.
set -e
VERSION="${1:-3.0.0}"
ROOT=pkgroot
rm -rf "$ROOT" && mkdir -p "$ROOT/Library/Audio/Plug-Ins/VST3" "$ROOT/Library/Audio/Plug-Ins/Components"
cp -R "dist/Auric Master 3.vst3" "$ROOT/Library/Audio/Plug-Ins/VST3/"
cp -R "dist/Auric Master 3.component" "$ROOT/Library/Audio/Plug-Ins/Components/"
# bundles must not be "relocated" to an older copy somewhere else on the disk
pkgbuild --analyze --root "$ROOT" components.plist
python3 - <<'PY'
import plistlib
with open("components.plist", "rb") as f: items = plistlib.load(f)
for it in items: it["BundleIsRelocatable"] = False
with open("components.plist", "wb") as f: plistlib.dump(items, f)
PY
pkgbuild --root "$ROOT" --component-plist components.plist --identifier com.auricaudio.auricmaster3 --version "$VERSION" \
         --scripts AuricMaster3/installer/macos-scripts --install-location / AuricMaster3-component.pkg
productbuild --package AuricMaster3-component.pkg "Auric Master 3 Installer.pkg"
rm -rf "$ROOT" components.plist AuricMaster3-component.pkg
echo "built: Auric Master 3 Installer.pkg"
