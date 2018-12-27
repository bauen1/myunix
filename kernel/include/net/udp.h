#ifndef UDP_H
#define UDP_H

#include <net/net.h>

/* UDP */
typedef struct {
	uint16_t srcport __attribute__((packed));
	uint16_t dstport __attribute__((packed));
	uint16_t length __attribute__((packed));
	uint16_t checksum __attribute__((packed));
	uint8_t data[] __attribute__((packed));
} __attribute__((packed)) udp_packet_t;

void net_handle_udp(netif_t *netif, const ethernet_packet_t *ethernet_packet, size_t ethernet_length, const ipv4_packet_t *ipv4_packet, size_t ipv4_data_length);

bool net_send_udp(netif_t *netif, const uint8_t dstip[4], uint16_t srcport, uint16_t dstport, uint16_t length, const uint8_t *data);

#endif
