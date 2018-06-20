#ifndef NET_H
#define NET_H 1

#include <stdint.h>

typedef struct {
	size_t length;
	uint8_t *data;
} ethernet_packet_t;

typedef void (* send_packet_t)(void *extra, uint8_t *data, size_t length);
typedef ethernet_packet_t *(* receive_packet_t)(void *extra);

typedef struct {
	send_packet_t send_packet;
	receive_packet_t receive_packet;
	uint8_t mac[6];
	void *extra;
} netif_t;

void net_register_netif(send_packet_t send, receive_packet_t receive, uint8_t *mac, void *extra);

void net_init(void);

#endif
