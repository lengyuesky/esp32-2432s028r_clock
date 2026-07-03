#!/usr/bin/env python3
from pathlib import Path
import re
import sys


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MAIN_C = PROJECT_ROOT / "main" / "main.c"
CMAKE = PROJECT_ROOT / "main" / "CMakeLists.txt"


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
    cmake = CMAKE.read_text(encoding="utf-8")

    require("#define KOMARI_URL_LEN" in source, "缺少 Komari 地址长度限制。")
    require("komari_info_t" in source, "缺少 Komari 状态快照结构。")
    require("s_komari_base_url" in source, "缺少 Komari 地址全局配置。")
    require("s_komari_lock" in source, "缺少 Komari 状态互斥锁。")
    require("s_network_fetch_lock" in source, "天气和 Komari 网络请求需要共用互斥锁，避免 TLS/WSS 堆峰值叠加。")

    for func_name in (
        "komari_state_init",
        "komari_config_load",
        "komari_config_save",
        "komari_get_snapshot",
        "komari_fetch_once",
        "komari_task",
        "start_komari_task_once",
    ):
        extract_function(source, func_name)

    root_handler = extract_function(source, "provisioning_root_handler")
    require("komari_url" in root_handler, "配网页面缺少 Komari 地址输入框。")
    require("maxlength='191'" in root_handler, "Komari 地址输入框需要限制最大长度。")

    save_handler = extract_function(source, "provisioning_save_handler")
    require("komari_url" in save_handler, "保存配网表单时没有读取 Komari 地址。")
    require("komari_config_save" in save_handler, "保存配网表单时没有持久化 Komari 地址。")

    config_load = extract_function(source, "komari_config_load")
    config_save = extract_function(source, "komari_config_save")
    require('"komari_cfg"' in config_load and '"url"' in config_load,
            "Komari 地址需要从 komari_cfg/url 读取。")
    require('"komari_cfg"' in config_save and '"url"' in config_save,
            "Komari 地址需要保存到 komari_cfg/url。")

    require('"/api/public"' in source, "缺少 Komari /api/public 公开属性接口。")
    require('"/api/nodes"' in source, "缺少 Komari /api/nodes 节点列表接口。")
    require('"/api/clients"' in source, "缺少 Komari /api/clients 在线信息 WebSocket 接口。")
    require('"get"' in source, "Komari WebSocket 需要发送 get 请求。")
    require("KOMARI_WS_BUFFER_SIZE 4096" in source,
            "Komari WebSocket 缓冲应控制在 4KB，降低 ESP32 TLS 峰值堆压力。")

    ws_func = extract_function(source, "komari_fetch_online_ws")
    require("esp_transport_ws_init" not in ws_func and
            "esp_transport_ws_send_raw" not in ws_func and
            "esp_transport_ws_set_path" not in ws_func,
            "Komari 在线数不能再使用 ESP-IDF WebSocket transport；其握手头缓冲过小会连接失败。")
    require("Upgrade: websocket" in ws_func and
            "Sec-WebSocket-Key" in ws_func and
            "Sec-WebSocket-Version: 13" in ws_func and
            "Origin:" in ws_func,
            "Komari 在线数需要手写 WebSocket 握手，带同源 Origin 并使用项目自己的 4KB 缓冲读取响应头。")
    require("komari_ws_build_origin_header" in source,
            "Komari /api/clients 会校验 Origin，握手需要按配置地址生成同源 Origin。")
    clients_parser = extract_function(source, "komari_parse_clients_response")
    require("cJSON_Parse" not in clients_parser,
            "Komari 在线数解析不能用 cJSON 构建完整实时数据树，会放大 TLS 阶段堆峰值。")
    require("komari_json_count_array_items" in source and
            "komari_json_count_object_members" in source,
            "Komari 在线数应轻量扫描 online 数组，并在需要时统计 data 对象成员数。")
    require("komari_ws_send_text_get" in source and "komari_ws_read_text_frame" in source,
            "Komari 在线数需要显式发送 masked get 帧并读取文本帧。")
    require("esp_fill_random" in source and "0x80 |" in source,
            "客户端 WebSocket 文本帧必须带随机 mask。")

    nav = extract_function(source, "draw_nav_cards")
    require("komari_configured" in nav, "状态栏需要按是否配置 Komari 动态切换内容。")
    require("nav_card_count(show_komari)" in nav and "show_komari ? 4 : 3" not in nav,
            "配置 Komari 后状态栏仍应保持 3 张卡，不能再压缩成 4 张。")
    require("if (!show_komari)" in nav and '"日期"' in nav and "APP_VIEW_CALENDAR" in nav,
            "只有未配置 Komari 时才显示日期状态栏。")
    require("KOMARI" in nav, "状态栏缺少 Komari 卡。")
    require("APP_VIEW_KOMARI" in nav, "Komari 状态栏需要有激活视图。")

    target_view = extract_function(source, "nav_card_target_view")
    require("APP_VIEW_KOMARI" in target_view and "APP_VIEW_CALENDAR" in target_view,
            "状态栏触摸映射需要覆盖 Komari 和日期视图。")
    require("show_komari ? APP_VIEW_KOMARI : APP_VIEW_WEATHER" in target_view,
            "配置 Komari 后第 3 张状态栏需要进入 Komari，未配置时仍进入天气。")

    touch = extract_function(source, "touch_poll_view_change")
    require("nav_card_count(show_komari)" in touch and "show_komari ? 4 : 3" not in touch,
            "触摸命中区域需要与 3 张状态栏布局保持一致。")
    require("nav_card_target_view" in touch,
            "触摸命中需要通过统一映射决定当前卡片进入哪个视图。")
    require("komari_configured" in touch, "未配置 Komari 时不应命中 Komari 卡。")

    draw_current = extract_function(source, "draw_current_view")
    require("APP_VIEW_KOMARI" in source and "draw_komari_screen" in draw_current,
            "缺少 Komari 详情视图绘制入口。")

    network_task = extract_function(source, "network_task")
    require("start_komari_task_once" in network_task, "联网后需要启动 Komari 后台任务。")

    weather_task = extract_function(source, "weather_task")
    komari_task = extract_function(source, "komari_task")
    require("network_fetch_lock_take" in weather_task and "network_fetch_lock_give" in weather_task,
            "天气任务需要通过网络抓取互斥锁串行化 HTTPS 请求。")
    require("network_fetch_lock_take" in komari_task and "network_fetch_lock_give" in komari_task,
            "Komari 任务需要通过网络抓取互斥锁串行化 HTTPS/WSS 请求。")

    require("tcp_transport" in cmake, "main 组件需要依赖 tcp_transport 以使用 TCP/TLS transport。")
    require("esp-tls" in cmake, "main 组件需要依赖 esp-tls 以生成 WebSocket 握手 key。")

    print("通过：Komari 配置、状态栏、详情页和在线信息后台任务已实现。")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as exc:
        print(f"失败：{exc}")
        sys.exit(1)
