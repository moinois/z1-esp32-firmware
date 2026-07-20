# Composes ESP-IDF adapters around the portable firmware core.
idf_component_register(
    SRCS
        "can_twai_adapter.cpp"
        "controller_uart_adapter.cpp"
        "main.cpp"
    REQUIRES
        application
        core
        driver
        esp_driver_gpio
        esp_driver_uart
        nvs_flash
)
