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

void handle_udp(netif_t *netif, ethernet_packet_t *ethernet_packet, size_t ethernet_length,
	ipv4_packet_t *ipv4_packet, size_t ipv4_data_length);

bool net_send_udp(netif_t *netif, uint8_t *destip, uint16_t srcport, uint16_t dstport, uint16_t length, uint8_t *data);

#endif
