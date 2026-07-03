#!/usr/bin/env python3
from pathlib import Path
import re
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MAIN_C = PROJECT_ROOT / "main" / "main.c"
SDKCONFIG_DEFAULTS = PROJECT_ROOT / "sdkconfig.defaults"


def extract_function(source: str, name: str) -> str:
    match = re.search(
        rf"(?m)^(?:static[ \t]+)?[A-Za-z_][A-Za-z0-9_ \t\*]*[ \t]+\*?{re.escape(name)}[ \t]*\([^{{}};]*\)[ \t\r\n]*\{{",
        source,
    )
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
    defaults = SDKCONFIG_DEFAULTS.read_text(encoding="utf-8")

    require(
        "CONFIG_MBEDTLS_DYNAMIC_BUFFER=y" in defaults,
        "sdkconfig.defaults 需要启用 mbedTLS 动态收发缓冲，降低 HTTPS 峰值内存压力。",
    )

    ota_task = extract_function(source, "ota_task")
    require(
        ".partial_http_download = true" in ota_task,
        "HTTPS OTA 需要启用分段下载，避免一次 OTA 请求占用过多内存。",
    )
    require(
        ".max_http_request_size = OTA_HTTP_REQUEST_SIZE" in ota_task,
        "HTTPS OTA 需要限制每次 Range 请求大小。",
    )
    require(
        ".buffer_size = OTA_HTTP_BUFFER_SIZE" in ota_task
        and ".buffer_size_tx = OTA_HTTP_TX_BUFFER_SIZE" in ota_task,
        "HTTPS OTA 需要显式设置 HTTP 收发缓冲大小。",
    )

    print("通过：HTTPS/OTA 峰值内存配置已收紧。")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as exc:
        print(f"失败：{exc}")
        sys.exit(1)
