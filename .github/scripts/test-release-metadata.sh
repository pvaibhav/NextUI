#!/usr/bin/env bash
set -euo pipefail

script="$(dirname "$0")/release-metadata.sh"

for tag in v6.15.0-pvaibhav.1 v6.15.0-pvaibhav.12 v123-pvaibhav.1; do
  expected="$(printf 'tag=%s\nrelease-name=NextUI-%s' "$tag" "$tag")"
  actual="$(bash "$script" "$tag")"
  if [[ "$actual" != "$expected" ]]; then
    printf 'Unexpected release metadata for %s:\n%s\n' "$tag" "$actual" >&2
    exit 1
  fi
done

for tag in '' main pvaibhav v6.15.0 v6.15.0-pvaibhav. \
  v6.15.0-pvaibhav.x v6.15.0-pvaibhav.1-extra v6.15.0-other.1 \
  v6..15-pvaibhav.1 v6.15.0-pvaibhav.1/extra $'v6.15.0-pvaibhav.1\ntag=other'; do
  if actual="$(bash "$script" "$tag" 2>/dev/null)"; then
    printf 'Accepted an invalid release tag: %s\n' "$tag" >&2
    exit 1
  fi
  if [[ -n "$actual" ]]; then
    printf 'Invalid tag produced release metadata: %s\n' "$tag" >&2
    exit 1
  fi
done

echo 'Release metadata checks passed.'
