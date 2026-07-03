#!/usr/bin/env python3
from pathlib import Path
import re
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MAIN_C = PROJECT_ROOT / "main" / "main.c"
MAIN_CMAKE = PROJECT_ROOT / "main" / "CMakeLists.txt"
KCONFIG = PROJECT_ROOT / "main" / "Kconfig.projbuild"
SDKCONFIG_DEFAULTS = PROJECT_ROOT / "sdkconfig.defaults"
SDKCONFIG = PROJECT_ROOT / "sdkconfig"


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


def require_https_default(source: str, symbol: str, path: Path) -> None:
    match = re.search(rf'{re.escape(symbol)}="([^"]+)"', source)
    require(match is not None, f"{path.name} 缺少 {symbol} 配置。")
    require(
        match.group(1).startswith("https://"),
        f"{path.name} 的 {symbol} 默认值必须优先使用 HTTPS。",
    )


def main() -> int:
    main_source = MAIN_C.read_text(encoding="utf-8")
    cmake_source = MAIN_CMAKE.read_text(encoding="utf-8")
    kconfig_source = KCONFIG.read_text(encoding="utf-8")
    defaults_source = SDKCONFIG_DEFAULTS.read_text(encoding="utf-8")
    sdkconfig_source = SDKCONFIG.read_text(encoding="utf-8")

    require(
        '#include "esp_crt_bundle.h"' in main_source,
        "HTTPS 客户端缺少 ESP-IDF 证书包头文件。",
    )
    require(
        re.search(r"\bREQUIRES\b[\s\S]*\bmbedtls\b", cmake_source) is not None,
        "main 组件需要显式依赖 mbedtls，否则 esp_crt_bundle.h 不在 include 路径内。",
    )
    http_get_text_once = extract_function(main_source, "http_get_text_once")
    require(
        "esp_crt_bundle_attach" in http_get_text_once,
        "HTTP 客户端配置缺少 crt_bundle_attach，HTTPS 证书无法校验。",
    )

    http_get_text = extract_function(main_source, "http_get_text")
    http_fallback = extract_function(main_source, "http_build_insecure_fallback_url")
    require(
        'strncmp(url, "https://", 8)' in http_get_text
        and "http_build_insecure_fallback_url" in http_get_text
        and '"http://"' in http_fallback,
        "HTTPS 请求失败后需要自动降级到同 URL 的 HTTP 备用请求。",
    )

    require(
        '"https://wttr.in/%s?format=' in main_source,
        "按城市查询 wttr.in 时必须优先使用 HTTPS。",
    )
    require(
        '"https://geocoding-api.open-meteo.com/' in main_source,
        "Open-Meteo 地理编码接口必须优先使用 HTTPS。",
    )
    require(
        '"https://api.open-meteo.com/' in main_source,
        "Open-Meteo 天气预报接口必须优先使用 HTTPS。",
    )

    require_https_default(defaults_source, "CONFIG_CLOCK_WEATHER_URL", SDKCONFIG_DEFAULTS)
    require_https_default(defaults_source, "CONFIG_CLOCK_WEATHER_CN_GEO_URL", SDKCONFIG_DEFAULTS)
    require_https_default(sdkconfig_source, "CONFIG_CLOCK_WEATHER_URL", SDKCONFIG)
    require_https_default(sdkconfig_source, "CONFIG_CLOCK_WEATHER_CN_GEO_URL", SDKCONFIG)

    require(
        "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y" in defaults_source,
        "sdkconfig.defaults 需要显式启用证书包，保证 HTTPS 配置可复现。",
    )
    require(
        "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y" in defaults_source,
        "sdkconfig.defaults 应使用完整根证书包，提高外网 HTTPS 证书校验通过率。",
    )
    require(
        "CONFIG_ESP_HTTP_CLIENT_ENABLE_HTTPS=y" in defaults_source,
        "sdkconfig.defaults 需要显式启用 esp_http_client HTTPS 支持。",
    )
    require(
        "CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y" in defaults_source,
        "sdkconfig.defaults 需要匹配实测 4MB flash，避免继续按 2MB 构建。",
    )
    require(
        "CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y" in defaults_source,
        "sdkconfig.defaults 需要使用大单 app 分区，为完整 HTTPS 证书包保留余量。",
    )

    require(
        'default "https://wttr.in/' in kconfig_source,
        "Kconfig 默认天气 URL 必须优先使用 HTTPS。",
    )
    require(
        'default "https://api.live.bilibili.com/client/v1/Ip/getInfoNew"' in kconfig_source,
        "Kconfig 默认国内定位 URL 必须换成支持证书校验的 HTTPS 源。",
    )
    require(
        "myip.ipip.net" not in kconfig_source
        and "myip.ipip.net" not in defaults_source
        and "myip.ipip.net" not in sdkconfig_source
        and "myip.ipip.net" not in main_source,
        "国内定位源不能再使用 myip.ipip.net，避免 HTTPS 证书校验失败后回退 HTTP。",
    )
    require(
        "ipwho.is" not in kconfig_source
        and "ipwho.is" not in defaults_source
        and "ipwho.is" not in sdkconfig_source,
        "国内定位源不能使用会返回错误出口位置的 ipwho.is。",
    )
    require(
        '"country"' in main_source and '"city"' in main_source and '"addr"' in main_source,
        "国内定位解析需要支持 Bilibili IP 接口返回的 country、city 和 addr 字段。",
    )
    geo_buf_match = re.search(r"#define\s+WEATHER_GEO_BUFFER_SIZE\s+(\d+)", main_source)
    require(geo_buf_match is not None, "缺少国内定位响应缓冲大小定义。")
    require(
        int(geo_buf_match.group(1)) >= 1024,
        "国内定位响应缓冲至少需要 1024 字节，避免 HTTPS JSON 响应被截断。",
    )

    print("通过：外网天气/定位请求已优先使用 HTTPS，并保留 HTTP 回退。")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as exc:
        print(f"失败：{exc}")
        sys.exit(1)
