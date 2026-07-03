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


def main() -> int:
    source = MAIN_C.read_text(encoding="utf-8")
    failures: list[str] = []
    parse_body = extract_function(source, "weather_parse_response")

    large_stack_buffer = re.search(
        r"\bchar\s+\w+\s*\[\s*WEATHER_HTTP_BUFFER_SIZE\s*\]", parse_body
    )
    if large_stack_buffer is not None:
        failures.append(
            "weather_parse_response() 仍在栈上复制 WEATHER_HTTP_BUFFER_SIZE 响应缓冲。"
        )

    fetch_text_body = extract_function(source, "weather_fetch_url")
    if "WEATHER_TEXT_HTTP_BUFFER_SIZE" not in source:
        failures.append("wttr 文本天气需要使用独立的小响应缓冲。")
    if re.search(r"\bchar\s+\w+\s*\[\s*WEATHER_HTTP_BUFFER_SIZE\s*\]", fetch_text_body):
        failures.append("weather_fetch_url() 不应为 wttr 文本响应使用 4KB 栈缓冲。")

    open_meteo_location_body = extract_function(source, "weather_fetch_open_meteo_location")
    if re.search(
        r"\bchar\s+\w+\s*\[\s*WEATHER_HTTP_BUFFER_SIZE\s*\]", open_meteo_location_body
    ):
        failures.append("Open-Meteo geocode 响应不能放在天气任务栈上。")

    open_meteo_body = extract_function(source, "weather_fetch_open_meteo_forecast")
    if re.search(r"\bchar\s+\w+\s*\[\s*WEATHER_HTTP_BUFFER_SIZE\s*\]", open_meteo_body):
        failures.append("Open-Meteo forecast 响应不能放在天气任务栈上。")

    if failures:
        for failure in failures:
            print(f"失败：{failure}")
        return 1

    print("通过：天气解析和天气请求未再创建高风险响应栈缓冲。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
