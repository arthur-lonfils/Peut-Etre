#!/usr/bin/env bash
# Determine the next semantic version based on conventional commits since the last tag.
# Usage: ./scripts/version-bump.sh
# Output: "major", "minor", "patch", or "none" on stdout.
# Exit 0 if a bump is needed, exit 1 if no release-worthy commits found.

set -euo pipefail

# Get the latest tag, or default to v0.0.0 if no tags exist
LAST_TAG=$(git describe --tags --abbrev=0 2>/dev/null || echo "v0.0.0")

# Get commits since last tag (first line of each commit = subject)
COMMITS=$(git log "${LAST_TAG}..HEAD" --pretty=format:"%s%n%b" 2>/dev/null || true)

if [ -z "$COMMITS" ]; then
    echo "none"
    exit 1
fi

BUMP="none"

while IFS= read -r line; do
    # Skip empty lines
    [ -z "$line" ] && continue

    # Breaking change (feat!:, fix!:, or BREAKING CHANGE in body)
    if echo "$line" | grep -qE '^[a-z]+(\(.+\))?!:' || echo "$line" | grep -q 'BREAKING CHANGE'; then
        BUMP="major"
        break
    fi

    # Feature
    if echo "$line" | grep -qE '^feat(\(.+\))?:'; then
        [ "$BUMP" != "major" ] && BUMP="minor"
    fi

    # Patch-level changes
    if echo "$line" | grep -qE '^(fix|refactor|perf|security)(\(.+\))?:'; then
        [ "$BUMP" = "none" ] && BUMP="patch"
    fi
done <<< "$COMMITS"

echo "$BUMP"
[ "$BUMP" != "none" ]
