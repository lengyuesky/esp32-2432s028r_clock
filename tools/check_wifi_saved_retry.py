#!/usr/bin/env python3
from pathlib import Path
import re
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MAIN_C = PROJECT_ROOT / "main" / "main.c"


def extract_function(source: str, name: str) -> str:
    match = re.search(rf"static\s+[^\n]+?\s+{re.escape(name)}\s*\([^)]*\)\s*\{{", source)
    if match is None:
        raise AssertionError(f"未找到函数 {name}")

    start = match.end() - 1
    depth = 0
    for index in range(start, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]

    raise AssertionError(f"函数 {name} 的函数体不完整")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    source = MAIN_C.read_text(encoding="utf-8")
    provisioning_start = extract_function(source, "provisioning_start")
    network_task = extract_function(source, "network_task")

    require(
        "WIFI_SAVED_CREDENTIALS_RETRY_MS" in source,
        "缺少已保存 Wi-Fi 凭据的周期重试间隔配置。",
    )
    require(
        "provisioning_start(bool saved_credentials_available)" in source,
        "provisioning_start() 需要知道是否已有保存凭据。",
    )
    require(
        "s_wifi_configured = saved_credentials_available" in provisioning_start,
        "进入临时配网窗口时不能把已保存凭据显示成未配网。",
    )
    require(
        "provisioning_start(saved_credentials_available)" in network_task,
        "network_task 需要把保存凭据状态传给配网入口。",
    )
    require(
        "WIFI_SAVED_CREDENTIALS_RETRY_MS" in network_task,
        "有已保存凭据时等待配网提交必须有超时，然后继续自动重试。",
    )

    print("通过：保存的 Wi-Fi 凭据失败后会保留并周期自动重试。")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as exc:
        print(f"失败：{exc}")
        sys.exit(1)
