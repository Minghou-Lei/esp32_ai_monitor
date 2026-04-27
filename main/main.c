#include "esp_err.h"
#include "esp_log.h"
#include "network_service.h"
#include "wifi_info_screen.h"

static const char *TAG = "app_main";

void app_main(void)
{
    esp_err_t err = wifi_info_screen_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start display service: %s", esp_err_to_name(err));
    }

    err = network_service_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Wi-Fi service: %s", esp_err_to_name(err));
    }
}
