# ESP32-2432S028R Clock

这是一个面向 ESP32-2432S028R / CYD 2.8 寸触摸屏的 ESP-IDF 时钟固件。它使用 ILI9341 横屏显示、XPT2046 触摸、手机热点配网、SNTP 网络对时、IP 定位天气、7 日天气预报、Komari 在线状态和 HTTPS OTA 更新。

## 功能

- 320x240 横屏时钟首页，显示 Wi-Fi 状态、城市、日期、时间、天气、温度、湿度和健康状态入口。
- 手机连接设备热点后打开配网页面，写入家庭 Wi-Fi SSID、密码、Komari 地址和 OTA 地址。
- 联网后使用 SNTP 自动对时，默认优先使用国内 NTP 服务器。
- 优先通过国内 IP 定位接口获取中国大陆城市，再用 Open-Meteo 获取当前天气、未来 24 小时逐小时预报和 7 日预报。
- Open-Meteo 预报失败时使用 7Timer HTTPS 接口作为 7 日预报备用源。
- 可选接入 Komari 服务端，首页显示在线节点数/总节点数，并提供 Komari 详情页。
- 内置健康状态页，显示运行时间、IP、RSSI、堆内存、重启原因、OTA 分区、天气和 Komari 状态。
- 支持通过 GitHub Releases 或自定义 HTTPS 地址进行 OTA 更新，默认启用 OTA 回滚保护。
- 底部 `更多` 入口集合小工具：番茄钟、倒计时（环形进度条）、夜间模式（按时段自动调暗背光）和圆盘时钟。
- 触摸交互：
  - 点左上角 Wi-Fi 状态，确认后重新进入配网模式。
  - 点时间区域进入圆盘时钟。
  - 点底部 `更多` 进入小工具页，可进入番茄钟、倒计时、夜间模式和圆盘时钟；计时运行时 `更多` 卡片直接显示剩余时间。
  - 点日期区域或底部“日期”进入日历；配置 Komari 后底部会隐藏日期状态栏以避免拥挤。
  - 点右上角天气进入未来 24 小时逐小时天气页，点右半屏/左半屏翻页。
  - 点底部“湿度”进入天气详情。
  - 点健康状态区域进入健康状态页，在健康状态页点击 `更新` 可确认并启动 OTA。
  - 配置 Komari 后点底部 `KOMARI` 进入 Komari 详情页。
  - 多数子页面点击空白区域返回首页。
- 内置轻量 5x7 ASCII 字体和少量 16x16 中文字模，不依赖外部字体文件。

## 更多页小工具

- **番茄钟**：预设 `专注 25`、`专注 50`、`短休 5`、`长休 15` 分钟，点击预设立即开始，环形进度条显示剩余时间。专注结束后自动准备休息（每 4 个番茄一次长休），休息结束后准备下一轮 25 分钟专注；圆环下方的圆点显示本组已完成的番茄数。
- **倒计时**：预设 1/3/5/10/30/60 分钟，点击即开始。
- 两个计时器可以同时运行，点 `暂停`/`继续` 或点圆环切换暂停，点 `重置` 回到满时长。计时结束时会自动跳到对应页面、圆环闪烁并点亮背光 60 秒。
- **夜间模式**：开关和起止整点（默认 22:00-07:00）、夜间亮度（关屏/2%/5%/10%/20%/40%）都保存在 NVS。夜间时段内背光渐暗；点击屏幕亮起 15 秒，此时第一次点击只负责点亮，不会误触进入其它页面。背光使用 LEDC PWM 驱动。
- 小工具页点击空白区域返回 `更多` 页，`更多` 页点击空白区域返回首页。

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

## OTA 更新

固件支持 HTTPS OTA。默认 OTA 地址为：

```text
https://github.com/lengyuesky/esp32-2432s028r_clock/releases/download/ota-latest/cyd_clock.bin
```

进入健康状态页后点击 `更新`，确认后设备会从当前 OTA 地址下载 `cyd_clock.bin`，写入备用 OTA 分区并重启。默认分区表包含两个 0x180000 大小的 OTA 应用分区：

```text
ota_0: 0x20000  0x180000
ota_1: 0x1a0000 0x180000
```

配网页面里的 OTA 地址可以覆盖默认地址；留空则继续使用 GitHub Releases 的默认地址。OTA 地址只支持 HTTPS。

也可以在串口监视器里输入维护命令：

- `ota`：启动 OTA 更新。
- `status`：打印 Wi-Fi、OTA、进度和堆内存状态。

## GitHub Actions 构建

仓库包含 `.github/workflows/build-ota.yml`，会使用 ESP-IDF v5.4.2 构建 OTA 镜像。

- `push` 到 `main` 会构建固件，上传 Actions artifact，并创建新的 `ota-时间-提交号` Release；该 Release 会被标记为 GitHub Latest。
- 工作流会同步更新 `ota-latest` 兼容 Release，供已经使用默认固定 OTA 地址的设备继续更新。
- 自动 OTA Release 最多保留 3 个：`ota-latest` 加最近 2 个归档 Release；手工创建的普通版本 Release 不会被清理。
- `pull_request` 和手动 `workflow_dispatch` 会构建固件，并把 `cyd_clock.bin`、`cyd_clock.bin.sha256`、`partition-table.bin`、`ota_data_initial.bin` 上传为 Actions artifact。
- 发布普通 GitHub Release 时，会再次构建并把 `cyd_clock.bin` 和 `cyd_clock.bin.sha256` 上传到该 Release 的资产中。
- 固件默认 OTA 地址使用 `releases/download/ota-latest/cyd_clock.bin`，因此 `main` 每次推送成功后，设备默认会拉取兼容 Release 的最新资产。

当前 Release 页面：

```text
https://github.com/lengyuesky/esp32-2432s028r_clock/releases/latest
```

## Wi-Fi 配网

首次启动或没有可用 Wi-Fi 凭据时，设备会开启配网热点：

- 默认热点前缀：`CYD-CLOCK`
- 默认热点密码：空
- 默认配网页面：`http://192.168.4.1/`

手机连接该热点后，系统通常会自动弹出配网页面。如果没有弹出，手动打开浏览器访问 `http://192.168.4.1/`。

配网页面还可以填写两个可选地址：

- `Komari 地址`：填写后首页底部显示 `KOMARI` 状态栏，内容为在线节点数/总节点数；留空则关闭 Komari 状态栏。
- `OTA 地址`：填写后覆盖默认 OTA 地址；留空则使用 GitHub Releases 的默认 HTTPS OTA 地址。

配网成功后，Wi-Fi、Komari 和 OTA 配置都会保存到 NVS。下次启动会优先读取 NVS 中的配置。

设备正常运行时也可以点击首页左上角 Wi-Fi 状态，确认后重新进入配网模式，方便更换 Wi-Fi、Komari 地址或 OTA 地址。

## 天气和定位

天气逻辑：

1. 请求 `CONFIG_CLOCK_WEATHER_CN_GEO_URL`，默认 `https://api.live.bilibili.com/client/v1/Ip/getInfoNew`。
2. 如果识别到中国大陆 IP 和城市，优先使用该城市。
3. 通过 Open-Meteo 地理编码获取经纬度。
4. 获取当前天气和 7 日预报。
5. Open-Meteo 成功时额外获取未来 24 小时逐小时预报，用于首页右上角天气点击后的逐小时天气页面。
6. 如果 Open-Meteo 预报接口失败，使用 7Timer `civillight` HTTPS 接口作为 7 日预报备用源。
7. 再使用 wttr.in 文本接口补齐当前天气、温度和湿度。

外网天气和定位请求优先使用 HTTPS，并使用 ESP-IDF 证书包校验证书。如果某个外部服务临时无法完成 TLS 连接，固件会对同一 URL 降级重试一次 HTTP，避免天气功能完全不可用。本地配网页面仍是设备热点内的 `http://192.168.4.1/`。

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
- `CLOCK_OTA_DEFAULT_URL`：默认 HTTPS OTA 固件地址。
- `CLOCK_TOUCH_*`：XPT2046 引脚、压力阈值和坐标映射。
- `CLOCK_LCD_*`：LCD 引脚、颜色顺序、字节序和镜像。

默认配置保存在 `sdkconfig.defaults`。实际生成的 `sdkconfig` 是本机配置文件，不提交到仓库。

## 目录结构

```text
.
├── .github/workflows/            # GitHub Actions OTA 构建工作流
├── components/esp_lcd_ili9341/   # ILI9341 面板驱动组件
├── main/                         # 主程序、Kconfig 和 UI/网络/触摸逻辑
├── partitions.csv                # NVS、otadata 和双 OTA 分区表
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

### Release 页面没有 OTA 镜像或不是最新

`main` 分支 push 会构建镜像，创建新的 GitHub Latest Release，并同步更新 `ota-latest` 兼容 Release。如果 Release 页面没有新镜像，先检查最近一次 `Build OTA firmware` 工作流是否成功。

如果设备使用默认 OTA 地址，请确认最新 Release 中存在 `cyd_clock.bin`：

```text
https://github.com/lengyuesky/esp32-2432s028r_clock/releases/download/ota-latest/cyd_clock.bin
```
