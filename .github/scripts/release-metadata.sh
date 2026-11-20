#!/usr/bin/env bash
set -euo pipefail

tag="${1:-}"
pattern='^v[0-9]+(\.[0-9]+)*-pvaibhav\.[0-9]+$'
if [[ ! "$tag" =~ $pattern ]]; then
  echo "Expected a tag such as v6.15.0-pvaibhav.1, got: $tag" >&2
  exit 1
fi

printf 'tag=%s\nrelease-name=NextUI-%s\n' "$tag" "$tag"
