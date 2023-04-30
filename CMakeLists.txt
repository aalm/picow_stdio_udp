cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(picow_stdio_udp_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(picow_stdio_udp
	picow_stdio_udp.c
	stdio_udp.c
	)

pico_enable_stdio_usb(picow_stdio_udp 0)
pico_enable_stdio_uart(picow_stdio_udp 1)

pico_add_extra_outputs(picow_stdio_udp)
target_include_directories(picow_stdio_udp PRIVATE
	${CMAKE_CURRENT_LIST_DIR}
	)
target_link_libraries(picow_stdio_udp
	pico_stdlib
	pico_multicore
	pico_sync
	pico_cyw43_arch_lwip_threadsafe_background
	)
