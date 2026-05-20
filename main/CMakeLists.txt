idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES app_config_service config_web_service network_service provider_service ui_service)
