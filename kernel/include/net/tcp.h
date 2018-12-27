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
	uint16_t flags __attribute__((packed));
	uint16_t window_size __attribute__((packed));
	uint16_t checksum __attribute__((packed));
	uint16_t urgent_ptr __attribute__((packed));

	uint8_t data[] __attribute__((packed));
} __attribute__((packed)) tcp_header_t;

typedef struct {
	uint8_t srcip[4] __attribute__((packed));
	uint8_t dstip[4] __attribute__((packed));
	uint8_t zero __attribute__((packed));
	uint8_t protocol __attribute__((packed));
	uint16_t length __attribute__((packed));
} __attribute__((packed)) tcp_check_header_t;

enum tcp_flag {
	TCP_FLAG_FIN = (1<<0),
	TCP_FLAG_SYN = (1<<1),
	TCP_FLAG_RST = (1<<2),
	TCP_FLAG_PSH = (1<<3),
	TCP_FLAG_ACK = (1<<4),
	TCP_FLAG_URG = (1<<5),
	TCP_FLAG_ECE = (1<<6),
	TCP_FLAG_CWR = (1<<7),
	TCP_FLAG_NS  = (1<<8),
};

void net_handle_tcp(netif_t *netif, const ethernet_packet_t *ethernet_packet, size_t ethernet_length, const ipv4_packet_t *ipv4_packet, size_t ipv4_data_length);


#endif
