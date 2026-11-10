# Edit following two lines to set component requirements (see docs)
idf_component_register(
    SRCS "app_main.c" "uart_read.c" "http_server.c"
    INCLUDE_DIRS "include"
)
