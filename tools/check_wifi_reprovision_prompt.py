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

    require("#define WIFI_FORCE_PROV_BIT BIT3" in source,
            "缺少手动进入配网的事件位 WIFI_FORCE_PROV_BIT。")
    require("APP_VIEW_WIFI_CONFIRM" in source, "缺少 Wi-Fi 配网确认视图。")
    require("WIFI_STATUS_HIT_X" in source and "WIFI_STATUS_HIT_W" in source,
            "左上角 Wi-Fi 状态需要定义独立触摸命中区域。")
    require("WIFI_CONFIRM_YES_X" in source and "WIFI_CONFIRM_NO_X" in source,
            "确认框需要定义 YES/NO 按钮命中区域。")

    draw_confirm = extract_function(source, "draw_wifi_reprovision_confirm_screen")
    require("SETUP WIFI?" in draw_confirm and "OPEN AP MODE" in draw_confirm,
            "确认页需要使用完整 ASCII 文案，避免内置中文点阵缺字。")
    require("YES" in draw_confirm and "NO" in draw_confirm,
            "确认页需要绘制 YES/NO 按钮。")
    require("重新配网" not in draw_confirm and "打开配置热点" not in draw_confirm,
            "确认页不能使用当前字库不完整的中文文案。")

    request_func = extract_function(source, "request_wifi_reprovision")
    require("WIFI_FORCE_PROV_BIT" in request_func,
            "确认 YES 后需要设置 WIFI_FORCE_PROV_BIT 通知网络任务。")
    require("s_prov_active" in request_func,
            "设备已在配网模式时不应重复请求配网。")

    draw_current = extract_function(source, "draw_current_view")
    require("APP_VIEW_WIFI_CONFIRM" in draw_current and "draw_wifi_reprovision_confirm_screen" in draw_current,
            "draw_current_view() 需要能绘制 Wi-Fi 配网确认页。")

    touch = extract_function(source, "touch_poll_view_change")
    require("WIFI_STATUS_HIT_X" in touch and "APP_VIEW_WIFI_CONFIRM" in touch,
            "点击左上角 Wi-Fi 状态需要进入确认页。")
    require("WIFI_CONFIRM_YES_X" in touch and "request_wifi_reprovision" in touch,
            "确认页 YES 按钮需要触发手动配网请求。")
    require("WIFI_CONFIRM_NO_X" in touch and "APP_VIEW_DASHBOARD" in touch,
            "确认页 NO 按钮需要返回首页。")

    network_task = extract_function(source, "network_task")
    require("WIFI_FORCE_PROV_BIT" in network_task,
            "网络任务需要监听手动进入配网事件。")
    require("manual provisioning requested" in network_task,
            "网络任务需要记录手动进入配网日志，便于串口确认。")
    require("provisioning_start(saved_credentials_available)" in network_task,
            "手动请求后仍应复用现有配网入口，保留已保存 Wi-Fi 凭据。")

    print("通过：左上角 Wi-Fi 状态可确认后手动进入配网模式。")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as exc:
        print(f"失败：{exc}")
        sys.exit(1)
