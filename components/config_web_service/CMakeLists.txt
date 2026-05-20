idf_component_register(SRCS "config_web_service.c"
                    INCLUDE_DIRS "include"
                    REQUIRES app_config_service esp_http_client esp_http_server esp_system esp_timer lwip mbedtls network_service provider_service)
