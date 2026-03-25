idf_component_register(
	SRCS
		"main.c"
		"graphics/bmf_reader.c"
		"power_mgmt.c"
  INCLUDE_DIRS "."
	REQUIRES
	ii2c
	axp2101
	aw9523b
	ft6x36
	ispi
	igpio
	ili9342
	esp_timer
	esp_driver_gpio
)
