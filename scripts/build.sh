#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export IDF_TOOLS_PATH="${PROJECT_DIR}/.cache/espressif"
export IDF_PYTHON_ENV_PATH="${IDF_TOOLS_PATH}/python_env/idf5.4_py3.12_uv_env"
BUILD_JOBS="${IDF_BUILD_JOBS:-$(nproc)}"

cd "${PROJECT_DIR}"
source "${PROJECT_DIR}/.cache/esp-idf/v5.4.2/export.sh"
echo "使用 ${BUILD_JOBS} 个并行任务构建"

if [[ ! -f "${PROJECT_DIR}/build/Makefile" && ! -f "${PROJECT_DIR}/build/build.ninja" ]]; then
    idf.py reconfigure
fi

cmake --build "${PROJECT_DIR}/build" --parallel "${BUILD_JOBS}"
