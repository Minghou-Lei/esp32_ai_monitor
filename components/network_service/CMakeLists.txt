idf_component_register(SRCS "network_service.c"
                    INCLUDE_DIRS "include"
                    REQUIRES app_config_service esp_event esp_netif esp_wifi lwip nvs_flash provider_service wpa_supplicant)
