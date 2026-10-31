#!/usr/bin/env bash
#
#  Publish a ChurchCam firmware release to GitHub so devices can self-update.
#
#  Usage:  scripts/release.sh vX.Y.Z ["release notes"]
#
#  Tags the current commit, builds the firmware from that tag, and creates a GitHub
#  Release carrying firmware.bin as an asset named "firmware.bin" -- exactly what the
#  device's "Check GitHub for updates" button looks for. Because the build is stamped
#  from `git describe` (scripts/gen_version.py), the released image reports its own
#  tag as its version, and the firmware.bin left in .pio/build/ is byte-identical to
#  the published asset: flash THAT file over USB and the local unit reports the same
#  version as GitHub.
#
#  Requires the GitHub CLI (gh): https://cli.github.com  (brew install gh)
#
set -euo pipefail

TAG="${1:-}"
NOTES="${2:-Release ${TAG}}"
ENV="esp32-poe"
PIO="${PIO:-$HOME/.platformio/penv/bin/platformio}"

if [[ -z "$TAG" ]]; then
  echo "usage: scripts/release.sh vX.Y.Z [\"release notes\"]" >&2
  exit 1
fi
# The device compares versions as semver, so the tag must be vMAJOR.MINOR.PATCH.
if [[ ! "$TAG" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "tag must look like v1.2.3 (the device compares semver)" >&2
  exit 1
fi

cd "$(git rev-parse --show-toplevel)"
BIN=".pio/build/${ENV}/firmware.bin"

command -v gh >/dev/null || { echo "the GitHub CLI (gh) is required: https://cli.github.com" >&2; exit 1; }

# A clean tree guarantees the built image is exactly this commit -- no "-dirty" in
# the stamped version, and the local flash matches the published asset.
if [[ -n "$(git status --porcelain)" ]]; then
  echo "working tree not clean -- commit or stash first (the build stamps the version from git):" >&2
  git status --short >&2
  exit 1
fi

if git rev-parse "$TAG" >/dev/null 2>&1; then
  echo "tag $TAG already exists" >&2
  exit 1
fi

# Tag first so the build's git-describe stamp is exactly $TAG. If the build fails,
# drop the local tag again (it hasn't been pushed yet).
echo ">> tagging $TAG"
git tag -a "$TAG" -m "$NOTES"
trap 'git tag -d "$TAG" >/dev/null 2>&1 || true' ERR

echo ">> building firmware ($ENV)"
"$PIO" run -e "$ENV"
[[ -f "$BIN" ]] || { echo "build produced no $BIN" >&2; exit 1; }

echo ">> pushing tag"
git push origin "$TAG"
trap - ERR   # tag is published now; keep it even if the release step hiccups

echo ">> creating GitHub release $TAG"
gh release create "$TAG" "$BIN" --title "$TAG" --notes "$NOTES"

echo
echo ">> done -- $TAG is published."
echo "   Devices will see it via 'Check GitHub for updates'."
echo "   To flash the identical build over USB: $PIO run -e $ENV -t upload"
