idf_component_register(SRCS "config_web_service.c"
                    INCLUDE_DIRS "include"
                    REQUIRES app_config_service esp_http_server esp_system esp_timer network_service provider_service)
