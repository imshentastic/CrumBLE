#!/usr/bin/env bash
# Build the CrumBLE release binaries and name them the way OtaUpdater expects.
#
# Why this exists: CrumBLE has no release automation. scripts/release.sh and
# .github/workflows/release.yml are both inherited from upstream CrossInk --
# they bump crossink_version, tag "v<ver>", and emit firmware-tiny-v<ver>.bin.
# CrumBLE tags "crumble-v<ver>" and its OTA matcher looks for entirely
# different asset names (src/network/OtaUpdater.cpp firmwareAssetStem):
#
#   crumble-firmware-<ver>.bin           main build        (stem crumble-firmware)
#   crumble-firmware-chinese-<ver>.bin   CJK variant       (stem crumble-firmware-chinese)
#
# So releases were cut by hand, which is how v4.7.4's notes came to announce a
# Chinese variant that was never built. The variants are NOT interchangeable:
# a CJK device refuses a main image and vice versa, so shipping one without the
# other strands every device on the missing side with nothing to update to.
#
# Usage: ./scripts/crumble-release.sh [outdir]      (default: ~/Downloads)
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."
OUTDIR="${1:-$HOME/Downloads}"
VERSION="$(sed -nE 's/^crumble_version = (.+)$/\1/p' platformio.ini)"
[[ -n "$VERSION" ]] || { echo "could not read crumble_version from platformio.ini" >&2; exit 1; }

# env -> release asset stem. Keep in sync with OtaUpdater.cpp.
ENVS=(
  "tiny-bitter:crumble-firmware"
  "tiny-bitter-chinese-fallback:crumble-firmware-chinese"
)

echo "=== CrumBLE $VERSION -> $OUTDIR"
mkdir -p "$OUTDIR"

for pair in "${ENVS[@]}"; do
  env_name="${pair%%:*}"
  stem="${pair##*:}"
  echo
  echo "--- building $env_name"
  pio run -e "$env_name"

  src=".pio/build/$env_name/firmware.bin"
  [[ -f "$src" ]] || { echo "missing $src" >&2; exit 1; }

  # The 6.25 MB stock slot is the SD-card-flash promise. check_firmware_size.py
  # already enforces each env's own ceiling during the build; re-assert the
  # hard partition limit here so a renamed asset can never exceed it.
  size=$(stat -f%z "$src" 2>/dev/null || stat -c%s "$src")
  if (( size > 6553600 )); then
    echo "FATAL: $env_name is $size bytes, over the 6,553,600 slot" >&2
    exit 1
  fi

  dest="$OUTDIR/$stem-$VERSION.bin"
  cp "$src" "$dest"
  printf '    %-34s %10d bytes  (%d under cap)\n' "$(basename "$dest")" "$size" "$((6553600 - size))"
done

echo
echo "Assets ready in $OUTDIR. Attach BOTH to the crumble-v$VERSION release --"
echo "OTA will not offer an update to devices on the variant you leave out."
