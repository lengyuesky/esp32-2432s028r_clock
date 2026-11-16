#!/usr/bin/env python3
from pathlib import Path
import re
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MAIN_C = PROJECT_ROOT / "main" / "main.c"
MAIN_CMAKE = PROJECT_ROOT / "main" / "CMakeLists.txt"


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
    cmake = MAIN_CMAKE.read_text(encoding="utf-8")

    for view in ("APP_VIEW_MORE", "APP_VIEW_POMODORO", "APP_VIEW_COUNTDOWN", "APP_VIEW_NIGHT"):
        require(view in source, f"缺少视图枚举 {view}。")

    nav = extract_function(source, "draw_nav_cards")
    require('"更多"' in nav and '"时间"' not in nav, "底部第一张状态栏应改为“更多”，不再显示“时间”。")
    require("APP_VIEW_MORE" in nav, "更多卡片需要有激活视图高亮。")
    target_view = extract_function(source, "nav_card_target_view")
    require("return APP_VIEW_MORE;" in target_view, "底部第一张状态栏应进入更多页。")

    draw_current = extract_function(source, "draw_current_view")
    for fn in ("draw_more_screen", "draw_pomodoro_screen", "draw_countdown_screen", "draw_night_screen"):
        require(fn in draw_current, f"draw_current_view 缺少 {fn}。")

    more = extract_function(source, "draw_more_screen")
    for label in ('"番茄钟"', '"倒计时"', '"夜间模式"'):
        require(label in more, f"更多页缺少 {label} 入口。")

    require("draw_progress_ring" in extract_function(source, "draw_timer_dial"),
            "番茄钟/倒计时需要环形进度条。")
    presets = re.search(r"s_pomodoro_presets\[\] = \{(.*?)\};", source, re.S)
    require(presets is not None and '{"专注", 25,' in presets.group(1), "番茄钟需要 25 分钟专注预设。")
    require("s_countdown_presets[]" in source, "倒计时需要常用时长预设。")

    touch = extract_function(source, "touch_poll_view_change")
    require("backlight_handle_touch_wake" in touch, "夜间调暗时点击需要先点亮背光。")
    for fn in ("pomodoro_handle_touch", "countdown_handle_touch", "night_handle_touch"):
        require(fn in touch, f"触摸处理缺少 {fn}。")

    require("ledc_channel_config" in extract_function(source, "backlight_init"), "背光需要使用 LEDC PWM 调光。")
    require("night_mode_active" in extract_function(source, "backlight_update"), "背光需要按夜间时段自动调暗。")
    require("nvs_set_u8" in extract_function(source, "night_config_save"), "夜间模式设置需要保存到 NVS。")
    require("esp_driver_ledc" in cmake and "esp_timer" in cmake, "main 组件需要依赖 esp_driver_ledc 和 esp_timer。")

    app_main = extract_function(source, "app_main")
    require("backlight_update" in app_main and "focus_timer_poll_finished" in app_main,
            "主循环需要轮询计时结束并更新背光。")

    glyphs = set(re.findall(r'\{"(.)", \{0x', source))
    for text in ("更多", "番茄钟", "倒计时", "夜间模式", "专注", "短休", "长休", "暂停", "继续",
                 "重置", "自动调暗", "生效中", "结束", "亮度", "关屏", "夜间轻触屏幕亮起"):
        missing = [ch for ch in text if ch not in glyphs]
        require(not missing, f"中文字模缺少：{''.join(missing)}（{text}）")

    print("通过：更多页、番茄钟、倒计时和夜间模式检查通过。")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as exc:
        print(f"失败：{exc}")
        sys.exit(1)
