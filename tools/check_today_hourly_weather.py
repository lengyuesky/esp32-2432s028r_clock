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

    require("APP_VIEW_TODAY_WEATHER" in source, "缺少今日时段天气视图。")
    require("WEATHER_HOURLY_SLOTS" in source, "缺少小时预报槽位上限。")
    require("#define WEATHER_HOURLY_WINDOW_HOURS 24" in source,
            "小时预报需要固定显示未来 24 小时。")
    require("#define WEATHER_HOURLY_STEP_HOURS 1" in source,
            "未来 24 小时天气必须按每小时显示，不能再按多小时间隔筛选。")
    require("#define TODAY_HOURLY_PAGE_COUNT 2" in source,
            "今日小时天气需要拆成 2 页。")
    require("#define TODAY_HOURLY_ITEMS_PER_PAGE 12" in source,
            "今日小时天气每页需要显示 12 个小时。")
    require("s_today_hourly_page" in source,
            "今日小时天气需要保存当前页状态。")
    require("hourly_count" in source and "hourly[WEATHER_HOURLY_SLOTS]" in source,
            "weather_info_t 需要保存今日小时预报。")

    build_hourly_url = extract_function(source, "weather_build_open_meteo_hourly_url")
    require(
        "https://api.open-meteo.com/v1/forecast" in build_hourly_url
        and "hourly=temperature_2m,relative_humidity_2m,weather_code" in build_hourly_url
        and "forecast_days=2" in build_hourly_url
        and "forecast_days=1" not in build_hourly_url,
        "Open-Meteo 小时天气 URL 必须请求 2 天数据，才能覆盖从当前小时起的未来 24 小时。",
    )

    hourly_parser = extract_function(source, "weather_parse_open_meteo_hourly_response")
    for token in ("hourly", "time", "temperature_2m", "relative_humidity_2m", "weather_code"):
        require(token in hourly_parser, f"小时预报解析函数缺少字段 {token}。")
    require(
        "WEATHER_HOURLY_SLOTS" in hourly_parser and "hourly_count" in hourly_parser,
        "小时预报解析函数需要填充 bounded hourly 数组。",
    )
    require(
        "weather_hourly_time_matches_today" not in hourly_parser
        and "WEATHER_HOURLY_WINDOW_HOURS" in hourly_parser
        and "start_time" in hourly_parser
        and "end_time" in hourly_parser
        and "mktime" in hourly_parser,
        "小时预报解析必须按当前小时起的未来 24 小时筛选，不能只匹配今天。",
    )
    require(
        "WEATHER_HOURLY_STEP_HOURS" not in hourly_parser
        or "#define WEATHER_HOURLY_STEP_HOURS 1" in source,
        "小时预报解析不能再跳过中间小时。",
    )

    hourly_fetch = extract_function(source, "weather_fetch_open_meteo_hourly")
    require(
        "weather_parse_open_meteo_hourly_response" in hourly_fetch,
        "小时预报拉取函数需要调用小时解析函数。",
    )
    require(
        "heap_caps_calloc" in hourly_fetch and "WEATHER_HOURLY_HTTP_BUFFER_SIZE" in hourly_fetch,
        "小时预报响应需要使用堆分配的独立缓冲。",
    )
    require(
        not re.search(r"\bchar\s+\w+\s*\[\s*WEATHER_HOURLY_HTTP_BUFFER_SIZE\s*\]", hourly_fetch),
        "小时预报响应缓冲不能放在天气任务栈上。",
    )

    fetch_cn = extract_function(source, "weather_fetch_preferred_cn")
    require(
        "weather_fetch_open_meteo_hourly" in fetch_cn,
        "中国优先天气路径需要在 Open-Meteo 定位成功后补充今日小时预报。",
    )

    draw_current = extract_function(source, "draw_current_view")
    require(
        "APP_VIEW_TODAY_WEATHER" in draw_current and "draw_today_weather_screen" in draw_current,
        "draw_current_view() 需要能绘制今日时段天气页。",
    )

    draw_clock = extract_function(source, "draw_clock_screen")
    require(
        "DASHBOARD_WEATHER_HIT_X" not in draw_clock,
        "首页右上角天气入口不能画可见框，只保留不可见触摸命中区域。",
    )

    draw_rows = extract_function(source, "draw_today_hourly_rows")
    require(
        "col =" in draw_rows and "row =" in draw_rows,
        "未来 24 小时天气需要使用双列紧凑布局，容纳每小时条目。",
    )
    require(
        "page_offset" in draw_rows and "TODAY_HOURLY_ITEMS_PER_PAGE" in draw_rows,
        "未来 24 小时天气需要按当前页偏移显示每页 12 个小时。",
    )
    require(
        re.search(r"rows_per_col\s*=\s*6", draw_rows) is not None,
        "今日逐小时天气两页布局每列应为 6 行，让小时卡片更高更清晰。",
    )
    row_y_match = re.search(r"row_y0\s*=\s*CLOCK_PANEL_Y\s*\+\s*(\d+)", draw_rows)
    require(row_y_match is not None and int(row_y_match.group(1)) <= 50,
            "今日逐小时天气需要去掉顶部状态栏，把小时列表上移到屏幕主体顶部。")
    row_h_match = re.search(r"row_h\s*=\s*(\d+)", draw_rows)
    require(row_h_match is not None and int(row_h_match.group(1)) >= 24,
            "今日逐小时天气行高需要至少 24 像素，避免两页小时天气仍然拥挤。")
    card_h_match = re.search(r"card_h\s*=\s*(\d+)", draw_rows)
    require(card_h_match is not None and int(card_h_match.group(1)) >= 22,
            "今日逐小时天气小时卡片高度需要至少 22 像素，避免文字太小。")
    require(
        "draw_today_hourly_card" in source and "today_hour_is_current" in source,
        "今日逐小时天气需要使用小时卡片并高亮当前小时。",
    )
    hourly_card = extract_function(source, "draw_today_hourly_card")
    require(
        "lcd_fill_round_rect" in hourly_card and "draw_weather_mini_symbol" in hourly_card,
        "小时预报卡片需要有背景层次和天气图标。",
    )
    require(
        "RH" in hourly_card and "temp_buf" in hourly_card,
        "小时预报卡片需要突出温度并弱化显示湿度。",
    )

    draw_today = extract_function(source, "draw_today_weather_screen")
    require(
        "draw_view_header" not in draw_today and "draw_today_summary_band" not in draw_today,
        "今日天气页不能再绘制顶部天气状态栏，只保留每小时段天气。",
    )
    require(
        "24H" not in draw_rows and "lcd_draw_hline" not in draw_rows,
        "今日天气页不应再绘制上方标题线，应把空间留给每小时天气。",
    )
    require(
        "draw_today_page_indicator" in source and "draw_today_page_indicator" in draw_today,
        "今日天气页需要显示 1/2、2/2 页码提示。",
    )

    touch = extract_function(source, "touch_poll_view_change")
    require(
        "DASHBOARD_WEATHER_HIT_X" in touch and "APP_VIEW_TODAY_WEATHER" in touch,
        "触摸处理需要把首页右上角天气区域切到今日时段天气页。",
    )
    require(
        "s_today_hourly_page = 0" in touch and
        "TODAY_HOURLY_PAGE_COUNT" in touch and
        "APP_VIEW_TODAY_WEATHER" in touch,
        "进入 TODAY 页需要从第 1 页开始，TODAY 页左右半屏需要翻页或返回。",
    )

    print("通过：首页右上角天气可无框点击进入 TODAY，TODAY 显示从当前小时起的未来 24 小时预报。")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as exc:
        print(f"失败：{exc}")
        sys.exit(1)
