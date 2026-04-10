
file(
  GLOB
  SRC_FILES
  *.c
  ../main/*.c
  ../components/*.c
  ../components/dap/*.c
  ../components/dap/io/*.c
)

idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS
        ../main
        ../components
        ../components/dap
        ../components/dap/io
    PRIV_REQUIRES
        nvs_flash
        esp_timer
        esp_driver_uart
        esp_driver_gpio
        spi_flash
)
