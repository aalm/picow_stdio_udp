/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <pico/stdio/driver.h>
#include <pico/stdio.h>
#include <pico/mutex.h>
#include <pico/cyw43_arch.h>

#include <lwip/dns.h>
#include <lwip/pbuf.h>
#include <lwip/udp.h>

#if !defined(UDP_STDIO_DESTINATION)
#define	UDP_STDIO_DESTINATION	"192.168.9.1"
#endif
#if !defined(UDP_STDIO_DST_PORT)
#define	UDP_STDIO_DST_PORT	1987
#endif

#ifndef PICO_STDIO_UDP_DEFAULT_CRLF
#define PICO_STDIO_UDP_DEFAULT_CRLF PICO_STDIO_DEFAULT_CRLF
#endif

#define	UDP_TXBUFSZ	4096
static uint8_t		 tx_buffer[UDP_TXBUFSZ];
static const char	*txbuf = (const char *)&tx_buffer[0];
static int		 tx_bytes;
static mutex_t		 tx_mutex;
static alarm_id_t	 txbuf_alarm;

absolute_time_t		 _tx_buf_filled;
absolute_time_t		 _tx_buf_flushed;

static bool	 _udp_stdio_init(void);
static int	 _udp_stdio_send(const char *, int);

static void	 stdio_udp_alarm(int);

stdio_driver_t stdio_udp;

void
stdio_udp_init(void)
{

	mutex_init(&tx_mutex);
	tx_bytes = 0;
	if (_udp_stdio_init())
		stdio_set_driver_enabled(&stdio_udp, true);
	/* XXX initialize _tx_buf_{filled,flushed} here ? */
}

static int64_t
stdio_udp_txbuf_check(alarm_id_t id, void *user_data)
{
	absolute_time_t _now;
	bool resetalarm = true;

	mutex_enter_blocking(&tx_mutex);
	if (!tx_bytes)
		resetalarm = false;
	else {
		_now = get_absolute_time();
		/* XXX i think these timeouts need some work. first would be
		 * to figure out which one is being hit more often. i guess
		 */
		if ((absolute_time_diff_us(_now, _tx_buf_filled) < 0 ||
		    absolute_time_diff_us(_now, _tx_buf_flushed) < 0)) {
			if (_udp_stdio_send(txbuf, tx_bytes) == 0) {
				tx_bytes = 0;
				stdio_udp_alarm(0);
				resetalarm = false;
			}
		}
	}
	mutex_exit(&tx_mutex);

	if (resetalarm)
		stdio_udp_alarm(1);
	return 0;
}

static void
stdio_udp_alarm(int arm)
{

	if (!arm && txbuf_alarm > 0) {
		cancel_alarm(txbuf_alarm);
		txbuf_alarm = 0;
	} else
	if (arm && txbuf_alarm == 0)
		txbuf_alarm = add_alarm_in_ms(arm * 1000, stdio_udp_txbuf_check, NULL, true);
}

static bool
stdio_udp_out_chars_queue(const char *buf, int length)
{
	int bytes_free = UDP_TXBUFSZ - tx_bytes;
	int bytes = length;
	
	for (; bytes_free; bytes_free--) {
		tx_buffer[tx_bytes++] = *buf++;
		if (!--bytes)
			break;
	}
	/* update timeout XXX */
	_tx_buf_filled = make_timeout_time_ms(5 * 1000);
	return true;
}

static bool
stdio_udp_out_chars_real(const char *buf, int length)
{
	const char *pb = tx_bytes ? txbuf : buf;
	int bytes = tx_bytes ? tx_bytes : length;

	if (_udp_stdio_send(pb, bytes) != 0)
		return false;
	tx_bytes = 0;
	/* update timeout for next flushing XXX  */
	_tx_buf_flushed = make_timeout_time_ms(10 * 1000);
	return true;
}

static void
stdio_udp_out_chars(const char *buf, int length)
{
	bool inbuffer_or_sent = false;
	bool resetalarm = false;

	mutex_enter_blocking(&tx_mutex);

	if (tx_bytes) /* bytes already in queue, so we can't just send */
		inbuffer_or_sent = stdio_udp_out_chars_queue(buf, length);

	if (absolute_time_diff_us(get_absolute_time(), _tx_buf_flushed) < 0 ||
	    tx_bytes >= 1024) { /* XXX 1024 is just a guess */
		inbuffer_or_sent = stdio_udp_out_chars_real(buf, length);
	}

	if (inbuffer_or_sent)
		stdio_udp_alarm(0);
	else
		stdio_udp_out_chars_queue(buf, length);

	if (tx_bytes)
		resetalarm = true;

	mutex_exit(&tx_mutex);

	if (resetalarm)
		stdio_udp_alarm(5);
}

stdio_driver_t stdio_udp = {
	.out_chars = stdio_udp_out_chars,
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
	.crlf_enabled = PICO_STDIO_UDP_DEFAULT_CRLF
#endif
};

ip_addr_t	 _udp_stdio_server_address;
struct udp_pcb	*_udp_stdio_pcb;
static bool	 _udp_stdio_gotaddr = false;

static void
_udp_stdio_dns_cb(const char *hostname, const ip_addr_t *ipaddr, void *arg)
{

	if (ipaddr) {
		_udp_stdio_server_address = *ipaddr;
		/*printf("%s: address %s \"%s\"\n", __func__, ipaddr_ntoa(ipaddr), hostname);*/
		_udp_stdio_gotaddr = true;
	} else {
		printf("%s: dns request \"%s\"failed\n", __func__, hostname);
		_udp_stdio_gotaddr = false;
	}
}

static int
_udp_stdio_send(const char *buf, int length)
{
	struct pbuf *p;
	err_t rv;

	if (!_udp_stdio_pcb || !_udp_stdio_gotaddr)
		return ERR_MEM;	

	cyw43_arch_lwip_begin();
	p = pbuf_alloc(PBUF_TRANSPORT, length, PBUF_RAM);
	if (!p) {
		cyw43_arch_lwip_end();
		return ERR_MEM;
	}

	memcpy(p->payload, buf, length);
	rv = udp_sendto(_udp_stdio_pcb, p, &_udp_stdio_server_address, UDP_STDIO_DST_PORT);

	pbuf_free(p);
	cyw43_arch_lwip_end();
	return rv;
}

static bool
_udp_stdio_init(void)
{
	const char *destination = UDP_STDIO_DESTINATION;
	err_t rv;

	if (_udp_stdio_pcb) {
		udp_remove(_udp_stdio_pcb);
		_udp_stdio_pcb = NULL;
	}
	_udp_stdio_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
	if (!_udp_stdio_pcb) {
		printf("%s: failed to create udp pcb\n", __func__);
		return false;
	}

	cyw43_arch_lwip_begin();
	_udp_stdio_gotaddr = false;
	rv = dns_gethostbyname(destination, &_udp_stdio_server_address, _udp_stdio_dns_cb, NULL);
	cyw43_arch_lwip_end();
	if (rv == ERR_OK)
		_udp_stdio_gotaddr = true;
	else
	if (rv != ERR_INPROGRESS) {
		printf("%s: dns request \"%s\" failed\n", __func__, destination);
		return false;
	}

	return true;
}
