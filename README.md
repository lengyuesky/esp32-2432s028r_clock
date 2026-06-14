# ESP32-2432S028R Clock

这是一个面向 ESP32-2432S028R / CYD 2.8 寸触摸屏的 ESP-IDF 时钟固件。它使用 ILI9341 横屏显示、XPT2046 触摸、手机热点配网、SNTP 网络对时、IP 定位天气和 7 日天气预报。

## 功能

- 320x240 横屏时钟首页，显示 Wi-Fi 状态、城市、日期、时间、温度和湿度。
- 手机连接设备热点后打开配网页面，写入家庭 Wi-Fi SSID 和密码。
- 联网后使用 SNTP 自动对时，默认优先使用国内 NTP 服务器。
- 优先通过国内 IP 定位接口获取中国大陆城市，再用 Open-Meteo 获取当前天气和 7 日预报。
- 触摸交互：
  - 点时间区域或底部“时间”进入圆盘时钟。
  - 点日期区域或底部“日期”进入日历。
  - 点底部“湿度”进入天气详情。
  - 在任意子页面点击任意位置返回首页。
- 内置轻量 5x7 ASCII 字体和少量 16x16 中文字模，不依赖外部字体文件。

## 硬件

默认适配常见 ESP32-2432S028R：

| 模块 | 信号 | GPIO |
| --- | --- | --- |
| LCD ILI9341 | MOSI | 13 |
| LCD ILI9341 | MISO | 12 |
| LCD ILI9341 | SCLK | 14 |
| LCD ILI9341 | CS | 15 |
| LCD ILI9341 | DC | 2 |
| LCD ILI9341 | RST | -1 |
| LCD 背光 | BL | 21 |
| XPT2046 触摸 | T_CLK | 25 |
| XPT2046 触摸 | T_DIN / MOSI | 32 |
| XPT2046 触摸 | T_DO / MISO | 39 |
| XPT2046 触摸 | T_CS | 33 |
| XPT2046 触摸 | T_IRQ | 36 |

如果你的板子批次不同，可以在 `idf.py menuconfig` 的 `CYD Clock` 菜单中调整 LCD、触摸、颜色字节序、镜像和校准参数。

## 环境

- 目标芯片：ESP32
- 推荐 ESP-IDF：5.4.x
- 串口工具：`idf.py flash monitor`
- Linux / WSL / Windows ESP-IDF PowerShell 均可构建

WSL 下建议把项目放在 Linux 文件系统中，例如 `~/esp32/esp32-2432s028r_clock`。如果放在 `/mnt/c` 或 `/mnt/e` 这类 Windows 盘，ESP-IDF 编译会明显变慢。

## 构建

先进入 ESP-IDF 环境：

```bash
source ~/esp/esp-idf/export.sh
```

设置目标并构建：

```bash
idf.py set-target esp32
idf.py build
```

本仓库也提供了便捷脚本：

```bash
./scripts/build.sh
```

该脚本默认用 `nproc` 个并行任务构建，可用环境变量覆盖：

```bash
IDF_BUILD_JOBS=16 ./scripts/build.sh
```

Windows PowerShell：

```powershell
.\scripts\build.ps1
```

## 烧录和监视

Linux / WSL：

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Windows：

```powershell
idf.py -p COM3 flash monitor
```

把串口号换成你自己的设备端口。

## Wi-Fi 配网

首次启动或没有可用 Wi-Fi 凭据时，设备会开启配网热点：

- 默认热点前缀：`CYD-CLOCK`
- 默认热点密码：空
- 默认配网页面：`http://192.168.4.1/`

手机连接该热点后，系统通常会自动弹出配网页面。如果没有弹出，手动打开浏览器访问 `http://192.168.4.1/`。

配网成功后，SSID 和密码保存到 NVS。下次启动会优先读取 NVS 中的配置。

## 天气和定位

天气逻辑：

1. 请求 `CONFIG_CLOCK_WEATHER_CN_GEO_URL`，默认 `http://myip.ipip.net/json`。
2. 如果识别到中国大陆 IP 和城市，优先使用该城市。
3. 通过 Open-Meteo 地理编码获取经纬度。
4. 获取当前天气和 7 日预报。
5. 如果 Open-Meteo 失败，回退到 `CONFIG_CLOCK_WEATHER_URL` 的 wttr.in 文本接口。

由于固件只内置少量中文字模，城市名显示使用拼音，例如 `Suzhou`，避免未收录汉字显示为方框。天气状态保留中文：`晴`、`多云`、`雨`、`雪`、`雾`、`雷雨`。

## 主要配置

运行：

```bash
idf.py menuconfig
```

进入 `CYD Clock` 菜单可配置：

- `CLOCK_WIFI_SSID` / `CLOCK_WIFI_PASSWORD`：编译期 Wi-Fi 备用配置。
- `CLOCK_NTP_SERVER`：主 NTP 服务器，默认 `ntp.aliyun.com`。
- `CLOCK_PROV_ENABLE`：是否启用手机配网热点。
- `CLOCK_PROV_AP_SSID_PREFIX`：配网热点前缀。
- `CLOCK_WEATHER_ENABLE`：是否启用天气。
- `CLOCK_WEATHER_REFRESH_MINUTES`：天气刷新间隔。
- `CLOCK_TOUCH_*`：XPT2046 引脚、压力阈值和坐标映射。
- `CLOCK_LCD_*`：LCD 引脚、颜色顺序、字节序和镜像。

默认配置保存在 `sdkconfig.defaults`。实际生成的 `sdkconfig` 是本机配置文件，不提交到仓库。

## 目录结构

```text
.
├── components/esp_lcd_ili9341/   # ILI9341 面板驱动组件
├── main/                         # 主程序、Kconfig 和 UI/网络/触摸逻辑
├── scripts/                      # 构建脚本
├── sdkconfig.defaults            # 可复现默认配置
└── README.md
```

## 常见问题

### 触摸没反应

确认 XPT2046 使用的是独立 SPI 引脚：

```text
T_CLK=25, T_DIN=32, T_DO=39, T_CS=33, T_IRQ=36
```

串口中应能看到类似日志：

```text
touch initialized: XPT2046 SCLK=25 MOSI=32 MISO=39 CS=33 IRQ=36
touch raw=(...) pressure=... screen=(...) view=...
```

如果有触摸日志但点击位置不对，调整 `CLOCK_TOUCH_SWAP_XY`、`CLOCK_TOUCH_MIRROR_X/Y` 和 `CLOCK_TOUCH_RAW_*`。

### 汉字显示成方框

当前固件不是完整中文字库，只内置了 UI 需要的一小部分字模。未收录汉字会显示方框。城市名因此使用拼音显示。

### 构建很慢

ESP-IDF 已经会并行编译。WSL 下最大的影响通常是项目位于 Windows 盘。把项目移动到 WSL Linux 文件系统能明显改善编译速度。

### Flash size 警告

如果串口日志提示实际 Flash 大于镜像头中的大小，说明当前配置按 2MB 镜像头构建，但板子可能是 4MB Flash。现有默认分区足够当前固件使用；如需使用更多 Flash，可在 `menuconfig` 中调整 Flash size 和分区表。
