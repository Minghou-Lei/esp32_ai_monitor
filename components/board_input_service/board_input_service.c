/**
 * @file    board_input_service.c
 * @brief   Physical BOOT button long-press handling for config AP toggling.
 */

#include "board_input_service.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "network_service.h"

static const char *TAG = "board_input_service";

static const gpio_num_t BOARD_INPUT_SERVICE_BOOT_BUTTON_GPIO = GPIO_NUM_35;
static const int BOARD_INPUT_SERVICE_BUTTON_ACTIVE_LEVEL = 0;
static const uint32_t BOARD_INPUT_SERVICE_POLL_INTERVAL_US = 50000U;
static const uint64_t BOARD_INPUT_SERVICE_LONG_PRESS_US = 2000000ULL;

static esp_timer_handle_t s_button_timer;
static bool s_button_was_pressed;
static bool s_long_press_reported;
static int64_t s_pressed_at_us;

static void board_input_service_button_poll(void *arg)
{
    (void)arg;

    int level = gpio_get_level(BOARD_INPUT_SERVICE_BOOT_BUTTON_GPIO);
    bool pressed = (level == BOARD_INPUT_SERVICE_BUTTON_ACTIVE_LEVEL);
    int64_t now_us = esp_timer_get_time();

    if (pressed && !s_button_was_pressed) {
        s_pressed_at_us = now_us;
        s_long_press_reported = false;
    } else if (!pressed) {
        s_pressed_at_us = 0;
        s_long_press_reported = false;
    }

    s_button_was_pressed = pressed;

    if (!pressed || s_long_press_reported || (s_pressed_at_us == 0)) {
        return;
    }

    if ((uint64_t)(now_us - s_pressed_at_us) < BOARD_INPUT_SERVICE_LONG_PRESS_US) {
        return;
    }

    s_long_press_reported = true;
    esp_err_t err = network_service_toggle_config_ap();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Upper BOOT button long press toggled config AP mode");
    } else {
        ESP_LOGE(TAG, "Failed to request config AP mode: %s", esp_err_to_name(err));
    }
}

esp_err_t board_input_service_start(void)
{
    if (s_button_timer != NULL) {
        return ESP_OK;
    }

    gpio_config_t config = {
        .pin_bit_mask = BIT64(BOARD_INPUT_SERVICE_BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&config), TAG, "boot button gpio config failed");

    const esp_timer_create_args_t timer_args = {
        .callback = board_input_service_button_poll,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "boot_btn_poll",
        .skip_unhandled_events = true,
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&timer_args, &s_button_timer), TAG, "button timer create failed");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(s_button_timer, BOARD_INPUT_SERVICE_POLL_INTERVAL_US),
                        TAG,
                        "button timer start failed");

    ESP_LOGI(TAG,
             "Monitoring upper BOOT button on GPIO %d for config AP long press",
             (int)BOARD_INPUT_SERVICE_BOOT_BUTTON_GPIO);
    return ESP_OK;
}
