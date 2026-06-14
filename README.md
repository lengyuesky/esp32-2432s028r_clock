# ESP32-2432S028R 时钟显示器

这是一个面向 ESP32-2432S028R/CYD 2.8 寸屏的 ESP-IDF 小项目。当前版本使用 ESP-IDF 自带 `esp_lcd` 驱动 ILI9341 SPI 屏，不依赖联网组件下载；没有配置 Wi-Fi 时会用固件编译时间启动一个本地时钟，配置 Wi-Fi 后会通过 SNTP 校时。

## 当前硬件假设

- 目标芯片：ESP32
- 屏幕控制器：ILI9341
- 分辨率：320x240 横屏
- SPI 引脚默认值：
  - MOSI: GPIO13
  - MISO: GPIO12
  - SCLK: GPIO14
  - CS: GPIO15
  - DC: GPIO2
  - RST: 未连接（-1）
  - 背光：GPIO21，高电平点亮
- 触摸：暂未启用，后续可接入 XPT2046
- OTA/自定义分区：暂未启用，先使用 ESP-IDF 默认分区

如果你的板子批次引脚不同，可以用 `idf.py menuconfig` 修改 `CYD Clock` 菜单里的 GPIO、颜色字节序、Wi-Fi 和时区配置。

## 构建

在 ESP-IDF PowerShell/Command Prompt 中：

```powershell
idf.py set-target esp32
idf.py build
```

也可以在普通 PowerShell 中尝试：

```powershell
.\scripts\build.ps1
```

## 配置 Wi-Fi 校时

```powershell
idf.py menuconfig
```

进入 `CYD Clock`：

- `Wi-Fi SSID`
- `Wi-Fi password`
- `Timezone string`，中国时区默认是 `CST-8`
- `NTP server`，默认是 `pool.ntp.org`

不配置 Wi-Fi 时，屏幕仍会显示基于编译时间的时钟，状态栏显示 `LOCAL`.

## 烧录和监视

```powershell
idf.py -p COMx flash monitor
```

把 `COMx` 换成你的串口号。

