#!/usr/bin/env bash
#
#  Cut a ChurchCam firmware release.
#
#  Usage:  scripts/release.sh vX.Y.Z ["release notes"]
#
#  Tags the current commit and pushes the tag. GitHub Actions (.github/workflows/
#  release.yml) then checks out that tag, builds the firmware, and publishes a
#  release carrying firmware.bin -- exactly what the device's "Check GitHub for
#  updates" button looks for. Because CI builds on the tag, gen_version.py stamps
#  FW_VERSION = the tag, so the published image reports its own version.
#
#  No local build and no `gh` needed -- the build happens in CI. To flash the
#  identical image over USB locally, check out the tag and run:
#      platformio run -e esp32-poe -t upload
#
set -euo pipefail

TAG="${1:-}"
NOTES="${2:-Release ${TAG}}"

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

# A clean tree means the tag points at exactly what's committed -- CI builds from
# the tag, so whatever is uncommitted here would NOT be in the release.
if [[ -n "$(git status --porcelain)" ]]; then
  echo "working tree not clean -- commit or stash first (CI builds from the tagged commit):" >&2
  git status --short >&2
  exit 1
fi

if git rev-parse "$TAG" >/dev/null 2>&1; then
  echo "tag $TAG already exists" >&2
  exit 1
fi

# The release workflow must exist at the tagged commit, or GitHub won't run it.
if [[ ! -f .github/workflows/release.yml ]]; then
  echo "warning: .github/workflows/release.yml not found at this commit --" >&2
  echo "         CI won't build this tag. Merge the release workflow first." >&2
fi

echo ">> tagging $TAG"
git tag -a "$TAG" -m "$NOTES"

echo ">> pushing tag (this triggers the CI build + release)"
git push origin "$TAG"

REMOTE_URL="$(git remote get-url origin 2>/dev/null || true)"
SLUG="$(sed -E 's#(git@github.com:|https://github.com/)([^/]+/[^/.]+)(\.git)?#\2#' <<<"$REMOTE_URL")"

echo
echo ">> done -- $TAG pushed."
echo "   GitHub Actions is now building and publishing the release."
if [[ -n "$SLUG" ]]; then
  echo "   Watch it:   https://github.com/$SLUG/actions"
  echo "   Release:    https://github.com/$SLUG/releases/tag/$TAG"
fi
echo "   When it finishes, devices see it via 'Check GitHub for updates'."
