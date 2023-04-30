#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <event.h>

/* $ gcc udprx.c -levent -o udprx && ./udprx */

#if !defined(UDP_RX_PORT)
#define	UDP_RX_PORT	1987
#endif

static struct timespec clock_ts;
static struct event udp_event;

static void
udp_dump(char *buf, int len)
{

	for (int i = 0; len > 0;) {
		while (len && *buf == 0x00/*!isprint(*buf)*/) {
			buf++;
			len--;
		}
		if (!len)
			break;
		i = printf("%s", buf);
		buf += i + 1;
		len -= i + 1;
	}
}

static void
udp_cb(evutil_socket_t sock, short int why, void *arg)
{
	struct event_base *base = arg;
	struct sockaddr_in server_sin;
	socklen_t server_sz = sizeof(server_sin);
	char *rxbuf = malloc(4096);
	int bytes;

	if (!rxbuf)
		return;

	bytes = recvfrom(sock, rxbuf, 4096 - 1, MSG_PEEK,
			(struct sockaddr *)&server_sin, &server_sz);
	if (bytes == -1) {
		perror("recvfrom(MSG_PEEK)");
		event_base_loopbreak(base);
	}
	memset(rxbuf, 0, 4096);
	bytes = recvfrom(sock, rxbuf, bytes, 0,
			(struct sockaddr *)&server_sin, &server_sz);
	if (bytes == -1) {
		perror("recvfrom()");
		event_base_loopbreak(base);
	}
	udp_dump(rxbuf, bytes); /* XXX eww:D */
	free(rxbuf);
}

int
main(int argc, char **argv)
{
	struct event_base *base = event_base_new();
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in sin;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(UDP_RX_PORT);
	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin))) {
		perror("bind()");
		exit(EXIT_FAILURE);
	}

	event_assign(&udp_event, base, sock, EV_READ|EV_PERSIST, udp_cb, base);
	event_add(&udp_event, 0);

	event_base_dispatch(base);

	close(sock);
	return 0;
}
