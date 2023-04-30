/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>

extern void	stdio_udp_init(void);

#define	WIFI_SSID	"dlink"
#define	WIFI_PASS	"yhlf3097"

const char *randomline =
	__FILE__ " " __TIME__ " " __DATE__ "\t";

const char *randomlongline =
	__FILE__ " " __TIME__ " " __DATE__ "\n"
	__FILE__ " " __TIME__ " " __DATE__ "\n"
	__FILE__ " " __TIME__ " " __DATE__ "\n"
	__FILE__ " " __TIME__ " " __DATE__ "\n"
	__FILE__ " " __TIME__ " " __DATE__ "\n"
	__FILE__ " " __TIME__ " " __DATE__ "\n"
	__FILE__ " " __TIME__ " " __DATE__ "\n"
	__FILE__ " " __TIME__ " " __DATE__ "\n"
	__FILE__ " " __TIME__ " " __DATE__ "\n"
	__FILE__ " " __TIME__ " " __DATE__ "\n"
	__FILE__ " " __TIME__ " " __DATE__ "\n"
	__FILE__ " " __TIME__ " " __DATE__ "\n"
	__FILE__ " " __TIME__ " " __DATE__ "\n"
	__FILE__ " " __TIME__ " " __DATE__ "\n"
	__FILE__ " " __TIME__ " " __DATE__ "\n"
	__FILE__ " " __TIME__ " " __DATE__ "\n"
	__FILE__ " " __TIME__ " " __DATE__ "\n"
	__FILE__ " " __TIME__ " " __DATE__ "\n";

int
main(void)
{
	absolute_time_t timo;
	int rv;

	stdio_init_all();

	if (cyw43_arch_init())
		panic("Wi-Fi init failed\n");
	cyw43_arch_enable_sta_mode();

	if ((rv = cyw43_arch_wifi_connect_blocking(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK)))
		panic("failed to connect %d\n", rv);

	stdio_udp_init();

	timo = make_timeout_time_ms(10 * 1000);
	rv = 0;
	while (true) {
		if (absolute_time_diff_us(get_absolute_time(), timo) < 0) {
			for (int i = 0; i < 10; i++)
				printf("%d%s\n", rv++, randomline);
			printf("%d%s", rv++, randomlongline);
			timo = make_timeout_time_ms(10 * 1000);
		}
		sleep_ms(50);
	}
}
