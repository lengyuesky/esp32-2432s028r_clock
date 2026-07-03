#!/usr/bin/env python3
from pathlib import Path
import re
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MAIN_C = PROJECT_ROOT / "main" / "main.c"
README = PROJECT_ROOT / "README.md"


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
    readme = README.read_text(encoding="utf-8")

    require(
        "WEATHER_FORECAST_HTTP_BUFFER_SIZE" in source,
        "需要为备用预报响应使用独立缓冲，避免把较大 JSON 挤进普通天气文本缓冲。",
    )
    require(
        "www.7timer.info/bin/api.pl" in source and "product=civillight" in source,
        "缺少 7Timer civillight HTTPS 备用预报 URL。",
    )

    build_url = extract_function(source, "weather_build_7timer_forecast_url")
    require(
        "https://www.7timer.info/bin/api.pl" in build_url
        and "product=civillight" in build_url
        and "output=json" in build_url,
        "7Timer URL 必须使用 HTTPS civillight JSON 接口。",
    )

    parser = extract_function(source, "weather_parse_7timer_forecast_response")
    for token in ("dataseries", "date", "weather", "temp2m", "max", "min"):
        require(token in parser, f"7Timer 解析函数缺少字段 {token}。")
    require(
        "WEATHER_FORECAST_DAYS" in parser and "forecast_count" in parser,
        "7Timer 解析函数需要填充现有 7 日预报数组。",
    )

    fetch_7timer = extract_function(source, "weather_fetch_7timer_forecast")
    require(
        "weather_parse_7timer_forecast_response" in fetch_7timer,
        "7Timer 拉取函数需要调用 7Timer 解析函数。",
    )
    require(
        "WEATHER_FORECAST_HTTP_BUFFER_SIZE" in fetch_7timer,
        "7Timer 拉取函数需要使用备用预报响应缓冲。",
    )
    require(
        not re.search(r"\bchar\s+\w+\s*\[\s*WEATHER_FORECAST_HTTP_BUFFER_SIZE\s*\]", fetch_7timer),
        "7Timer 响应缓冲不能放在天气任务栈上，实机验证栈余量会过低。",
    )

    require(
        "weather_copy_forecast" in source,
        "需要有明确的预报合并函数，把 7Timer 预报合并到 wttr 当前天气结果里。",
    )

    fetch_cn = extract_function(source, "weather_fetch_preferred_cn")
    geo_pos = fetch_cn.find("weather_fetch_open_meteo_location")
    open_meteo_pos = fetch_cn.find("weather_fetch_open_meteo_forecast")
    seven_timer_pos = fetch_cn.find("weather_fetch_7timer_forecast")
    wttr_pos = fetch_cn.find("weather_fetch_url")
    require(
        geo_pos != -1 and open_meteo_pos != -1 and seven_timer_pos != -1 and wttr_pos != -1,
        "中国优先天气路径需要包含 Open-Meteo 定位、Open-Meteo 预报、7Timer 和 wttr 回退。",
    )
    require(
        geo_pos < open_meteo_pos < seven_timer_pos < wttr_pos,
        "回退顺序必须是 Open-Meteo 定位 -> Open-Meteo 预报 -> 7Timer 预报 -> wttr 当前天气。",
    )
    require(
        "weather_copy_forecast" in fetch_cn,
        "wttr 当前天气成功后需要合并 7Timer 备用预报。",
    )

    require(
        "7Timer" in readme and "civillight" in readme,
        "README 需要说明 7Timer civillight 备用预报源。",
    )

    print("通过：Open-Meteo 失败后会使用 7Timer civillight 作为 7 日预报备用源。")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as exc:
        print(f"失败：{exc}")
        sys.exit(1)
