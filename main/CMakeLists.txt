idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
										REQUIRES ii2c axp2101 esp_driver_gpio)
