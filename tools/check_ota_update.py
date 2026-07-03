#!/usr/bin/env python3
from pathlib import Path
import csv
import re
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MAIN_C = PROJECT_ROOT / "main" / "main.c"
MAIN_CMAKE = PROJECT_ROOT / "main" / "CMakeLists.txt"
KCONFIG = PROJECT_ROOT / "main" / "Kconfig.projbuild"
SDKCONFIG = PROJECT_ROOT / "sdkconfig"
SDKCONFIG_DEFAULTS = PROJECT_ROOT / "sdkconfig.defaults"
PARTITIONS = PROJECT_ROOT / "partitions.csv"


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


def partition_rows() -> dict[str, dict[str, str]]:
    require(PARTITIONS.exists(), "缺少 OTA 分区表 partitions.csv。")
    rows: dict[str, dict[str, str]] = {}
    with PARTITIONS.open(newline="", encoding="utf-8") as handle:
        for raw in handle:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parsed = next(csv.reader([line], skipinitialspace=True))
            require(len(parsed) >= 5, f"分区行字段不足：{line}")
            rows[parsed[0]] = {
                "type": parsed[1],
                "subtype": parsed[2],
                "offset": parsed[3],
                "size": parsed[4],
            }
    return rows


def main() -> int:
    source = MAIN_C.read_text(encoding="utf-8")
    cmake = MAIN_CMAKE.read_text(encoding="utf-8")
    kconfig = KCONFIG.read_text(encoding="utf-8")
    sdkconfig = SDKCONFIG.read_text(encoding="utf-8")
    defaults = SDKCONFIG_DEFAULTS.read_text(encoding="utf-8")

    rows = partition_rows()
    require(rows.get("otadata", {}).get("type") == "data" and rows["otadata"]["subtype"] == "ota",
            "OTA 分区表必须包含 otadata。")
    require(rows.get("ota_0", {}).get("type") == "app" and rows["ota_0"]["subtype"] == "ota_0",
            "OTA 分区表必须包含 ota_0 app 分区。")
    require(rows.get("ota_1", {}).get("type") == "app" and rows["ota_1"]["subtype"] == "ota_1",
            "OTA 分区表必须包含 ota_1 app 分区。")
    require(rows["ota_0"]["size"].lower() == rows["ota_1"]["size"].lower() == "0x180000",
            "4MB flash 上 ota_0/ota_1 应为相同的 0x180000 大小。")

    for config_text, name in ((sdkconfig, "sdkconfig"), (defaults, "sdkconfig.defaults")):
        require("CONFIG_PARTITION_TABLE_CUSTOM=y" in config_text,
                f"{name} 必须启用自定义 OTA 分区表。")
        require('CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"' in config_text,
                f"{name} 必须指向 partitions.csv。")
        require("CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y" in config_text,
                f"{name} 必须启用 OTA 回滚保护。")

    require("esp_https_ota" in cmake and "app_update" in cmake,
            "main 组件需要依赖 esp_https_ota 和 app_update。")
    require('#include "esp_https_ota.h"' in source, "OTA 代码需要包含 esp_https_ota.h。")
    require('#include "esp_ota_ops.h"' in source, "OTA 代码需要包含 esp_ota_ops.h。")
    require('#include "esp_app_desc.h"' in source, "健康页需要读取固件版本描述。")

    require(("config CLOCK_OTA_DEFAULT_URL" in kconfig or "CONFIG_CLOCK_OTA_DEFAULT_URL" in kconfig) and
            "https://github.com/lengyuesky/esp32-2432s028r_clock/releases/latest/download/cyd_clock.bin" in kconfig,
            "Kconfig 必须提供默认 GitHub Releases OTA URL。")
    require("#define OTA_URL_LEN" in source and "ota_info_t" in source and "s_ota" in source,
            "OTA 需要有固定长度 URL、状态结构和全局状态。")

    for function_name in (
        "ota_state_init",
        "ota_config_load",
        "ota_config_save",
        "ota_get_effective_url",
        "ota_start_update",
        "ota_task",
        "ota_mark_running_app_valid",
    ):
        require(function_name in source, f"缺少 OTA 函数 {function_name}。")

    ota_task = extract_function(source, "ota_task")
    require("esp_https_ota_begin" in ota_task and "esp_https_ota_perform" in ota_task,
            "OTA 任务需要使用流式 esp_https_ota 以便更新进度。")
    require("esp_https_ota_get_image_len_read" in ota_task and "esp_restart" in ota_task,
            "OTA 任务需要记录进度并在成功后重启。")
    require("crt_bundle_attach" in ota_task,
            "HTTPS OTA 需要使用证书包校验服务器。")

    save_handler = extract_function(source, "provisioning_save_handler")
    root_handler = extract_function(source, "provisioning_root_handler")
    require("ota_url" in root_handler and "OTA 地址" in root_handler,
            "配网页面需要提供 OTA URL 输入框。")
    require("ota_url" in save_handler and "ota_config_save" in save_handler,
            "保存配置时需要保存 OTA URL 覆盖值。")

    app_main = extract_function(source, "app_main")
    require("ota_state_init" in app_main and "ota_config_load" in app_main and
            "ota_mark_running_app_valid" in app_main,
            "app_main 需要初始化 OTA 状态、读取配置并确认 OTA 回滚状态。")

    print("通过：OTA 分区、默认 GitHub URL、配置覆盖和 HTTPS OTA 任务已实现。")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as exc:
        print(f"失败：{exc}")
        sys.exit(1)
