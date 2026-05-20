/**
 * @file    board_input_service.h
 * @brief   Waveshare board physical input actions.
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start physical button monitoring.
 *
 * The upper BOOT button toggles the Wi-Fi configuration portal when held long
 * enough. Short presses are ignored so normal dashboard operation is not
 * interrupted.
 */
esp_err_t board_input_service_start(void);

#ifdef __cplusplus
}
#endif
