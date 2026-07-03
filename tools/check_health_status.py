#!/usr/bin/env python3
from pathlib import Path
import re
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MAIN_C = PROJECT_ROOT / "main" / "main.c"


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

    require("APP_VIEW_HEALTH" in source, "缺少健康状态页视图枚举。")
    require("APP_VIEW_OTA_CONFIRM" in source, "缺少 OTA 确认页视图枚举。")
    require("HEALTH_STATUS_HIT_X" in source and "HEALTH_STATUS_HIT_Y" in source,
            "首页需要定义健康页不可见触摸入口。")
    require("OTA_CONFIRM_YES_X" in source and "OTA_CONFIRM_NO_X" in source,
            "OTA 确认页需要定义确认/取消触摸区域。")

    for function_name in (
        "draw_health_screen",
        "draw_health_row",
        "draw_ota_confirm_screen",
        "format_uptime",
        "format_time_age",
        "ota_status_text",
    ):
        require(function_name in source, f"缺少健康页函数 {function_name}。")

    draw_health = extract_function(source, "draw_health_screen")
    for token in (
        "UPTIME",
        "IP",
        "RSSI",
        "HEAP",
        "MIN",
        "RESET",
        "RUN",
        "NEXT",
        "WEATHER",
        "KOMARI",
        "VER",
        "OTA",
    ):
        require(token in draw_health, f"健康页缺少字段 {token}。")
    require("heap_caps_get_free_size" in draw_health and
            "heap_caps_get_minimum_free_size" in draw_health and
            "heap_caps_get_largest_free_block" in draw_health,
            "健康页需要显示当前/最低/最大 heap 信息。")
    require("esp_ota_get_running_partition" in draw_health and
            "esp_ota_get_next_update_partition" in draw_health,
            "健康页需要显示当前分区和可更新分区。")
    require("esp_app_get_description" in draw_health,
            "健康页需要显示固件版本。")
    require("esp_wifi_sta_get_ap_info" in draw_health,
            "健康页需要显示 Wi-Fi RSSI。")

    draw_confirm = extract_function(source, "draw_ota_confirm_screen")
    require("OTA UPDATE" in draw_confirm and "YES" in draw_confirm and "NO" in draw_confirm,
            "OTA 确认页需要显示升级确认和 YES/NO 按钮。")

    draw_current = extract_function(source, "draw_current_view")
    require("APP_VIEW_HEALTH" in draw_current and "draw_health_screen" in draw_current,
            "draw_current_view 需要绘制健康页。")
    require("APP_VIEW_OTA_CONFIRM" in draw_current and "draw_ota_confirm_screen" in draw_current,
            "draw_current_view 需要绘制 OTA 确认页。")

    view_name = extract_function(source, "view_name")
    require("health" in view_name and "ota-confirm" in view_name,
            "触摸日志需要能输出健康页和 OTA 确认页名称。")

    touch = extract_function(source, "touch_poll_view_change")
    require("APP_VIEW_HEALTH" in touch and "HEALTH_STATUS_HIT_X" in touch,
            "首页触摸处理需要能进入健康页。")
    require("APP_VIEW_OTA_CONFIRM" in touch and "OTA_CONFIRM_YES_X" in touch and
            "ota_start_update" in touch,
            "触摸处理需要能确认并启动 OTA。")
    require("APP_VIEW_HEALTH" in touch and "APP_VIEW_DASHBOARD" in touch,
            "健康页需要支持返回首页。")

    print("通过：健康状态页、入口、字段和 OTA 确认流程已实现。")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as exc:
        print(f"失败：{exc}")
        sys.exit(1)
