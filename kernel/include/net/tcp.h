#ifndef TCP_H
#define TCP_H 1

#include <stddef.h>
#include <net/net.h>

/* TCP */
typedef struct {
	uint16_t srcport __attribute__((packed));
	uint16_t dstport __attribute__((packed));
	uint32_t seq_num __attribute__((packed));
	uint32_t ack_num __attribute__((packed));
	/* TODO: complete */
} __attribute__((packed)) tcp_packet_t;

void handle_tcp(netif_t *netif, ethernet_packet_t *ethernet_packet, size_t ethernet_length,
	ipv4_packet_t *ipv4_packet, size_t ipv4_data_length);


#endif
