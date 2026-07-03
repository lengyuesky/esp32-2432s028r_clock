#!/usr/bin/env bash
set -euo pipefail

workflow_file="${1:-.github/workflows/build-ota.yml}"

if [[ ! -f "${workflow_file}" ]]; then
    echo "missing workflow: ${workflow_file}" >&2
    exit 1
fi

require_text() {
    local needle="$1"
    if ! grep -Fq -- "${needle}" "${workflow_file}"; then
        echo "missing required workflow text: ${needle}" >&2
        exit 1
    fi
}

require_text "name: Publish OTA image to rolling release"
require_text "if: github.event_name == 'push' && github.ref == 'refs/heads/main'"
require_text "gh release create"
require_text "--latest"
require_text "ota/cyd_clock.bin"
require_text "ota/cyd_clock.bin.sha256"
require_text "ota/partition-table.bin"
require_text "ota/ota_data_initial.bin"

echo "OTA release workflow check passed"
