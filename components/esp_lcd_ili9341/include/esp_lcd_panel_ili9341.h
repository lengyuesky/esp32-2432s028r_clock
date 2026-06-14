/*
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_dev.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建 ILI9341 LCD panel
 *
 * @param[in] io LCD panel IO 句柄
 * @param[in] panel_dev_config 通用 panel 设备配置
 * @param[out] ret_panel 返回的 panel 句柄
 * @return
 *      - ESP_OK 成功
 *      - ESP_ERR_INVALID_ARG 参数无效
 *      - ESP_ERR_NO_MEM 内存不足
 *      - ESP_ERR_NOT_SUPPORTED 不支持的颜色或像素格式
 */
esp_err_t esp_lcd_new_panel_ili9341(const esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel);

#ifdef __cplusplus
}
#endif
