# Composes ESP-IDF adapters around the portable firmware core.
idf_component_register(SRCS "main.cpp" REQUIRES core nvs_flash esp_driver_gpio)
