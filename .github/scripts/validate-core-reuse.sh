#!/usr/bin/env bash
set -euo pipefail

[[ "$CORE_RUN_ID" =~ ^[0-9]+$ ]]
run="$(gh api "repos/$GITHUB_REPOSITORY/actions/runs/$CORE_RUN_ID")"
jq -e '.conclusion == "success" and .path == ".github/workflows/release.yaml"' <<< "$run" >/dev/null
source_sha="$(jq -r '.head_sha' <<< "$run")"
git diff --exit-code "$source_sha" HEAD -- workspace toolchains makefile makefile.toolchain .gitmodules
echo "Reusing unchanged cores from successful workflow run $CORE_RUN_ID ($source_sha)."
