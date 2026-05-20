idf_component_register(SRCS "board_input_service.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_gpio esp_timer network_service)
