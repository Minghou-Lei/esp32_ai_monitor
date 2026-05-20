idf_component_register(SRCS "provider_service.c"
                    INCLUDE_DIRS "include"
                    REQUIRES app_config_service esp_http_client esp_event esp_netif esp_timer mbedtls network_service)
