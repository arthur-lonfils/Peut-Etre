#!/usr/bin/env bash
# Compute the next version number from the current tag + bump type.
# Usage: ./scripts/next-version.sh [major|minor|patch]
# Output: the new version string (without "v" prefix), e.g. "0.2.0"

set -euo pipefail

BUMP="${1:-patch}"

# Get current version from latest tag
LAST_TAG=$(git describe --tags --abbrev=0 2>/dev/null || echo "v0.0.0")
VERSION="${LAST_TAG#v}"

IFS='.' read -r MAJOR MINOR PATCH <<< "$VERSION"

case "$BUMP" in
    major)
        MAJOR=$((MAJOR + 1))
        MINOR=0
        PATCH=0
        ;;
    minor)
        MINOR=$((MINOR + 1))
        PATCH=0
        ;;
    patch)
        PATCH=$((PATCH + 1))
        ;;
    *)
        echo "Unknown bump type: $BUMP" >&2
        exit 1
        ;;
esac

echo "${MAJOR}.${MINOR}.${PATCH}"
