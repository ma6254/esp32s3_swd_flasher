
file(
  GLOB
  SRC_FILES
  *.c
  ../main/*.c
  ../components/*.c
  ../components/dap/*.c
  ../components/dap/io/*.c
  ../components/key/*.c
  ../components/utils/*.c
)



idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS
        ../main
        ../components
        ../components/utils
        ../components/dap
        ../components/dap/io
        ../components/key
    PRIV_REQUIRES
        nvs_flash
        esp_timer
        esp_driver_uart
        esp_driver_gpio
        spi_flash
)
