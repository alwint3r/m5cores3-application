idf_component_register(
	SRCS
		"main.c"
		"bmf_reader.c"
  INCLUDE_DIRS "."
	REQUIRES
	ii2c
	axp2101
	aw9523b
	ispi
	igpio
	ili9342
	esp_timer
)
