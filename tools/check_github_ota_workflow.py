#!/usr/bin/env python3
from pathlib import Path
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = PROJECT_ROOT / ".github" / "workflows" / "build-ota.yml"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    require(WORKFLOW.exists(), "缺少 GitHub Actions OTA 构建工作流 .github/workflows/build-ota.yml。")
    source = WORKFLOW.read_text(encoding="utf-8")

    for token in (
        "workflow_dispatch:",
        "release:",
        "types: [published]",
        "permissions:",
        "contents: write",
        "espressif/esp-idf-ci-action@v1",
        "esp_idf_version: v5.4.2",
        "target: esp32",
        "build/cyd_clock.bin",
        "ota/cyd_clock.bin",
        "sha256sum",
        "actions/upload-artifact@v4",
        "gh release upload",
        "--clobber",
        "github.event_name == 'release'",
        "github.event.release.tag_name",
    ):
        require(token in source, f"OTA 工作流缺少必要配置：{token}")

    require(
        "cyd_clock.bin" in source and "cyd_clock.bin.sha256" in source,
        "OTA 工作流需要产出固定文件名 cyd_clock.bin 及校验文件。",
    )
    require(
        "if-no-files-found: error" in source,
        "artifact 上传需要在固件缺失时直接失败。",
    )

    print("通过：GitHub Actions 会构建 OTA 固件并在发布 Release 时上传 cyd_clock.bin。")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as exc:
        print(f"失败：{exc}")
        sys.exit(1)
