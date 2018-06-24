#ifndef NET_H
#define NET_H 1

#include <assert.h>
#include <stdint.h>

/* big endian to little endian helpers */

// host to network long
uint32_t htonl(uint32_t hostlong);

// host to network short
uint16_t htons(uint16_t hostshort);

// network to host long
uint32_t ntohl(uint32_t netlong);

// network to host short
uint16_t ntohs(uint16_t netshort);

/* Ethernet */
enum ethernet_type {
	ETHERNET_TYPE_ARP  = 0x0806,
	ETHERNET_TYPE_IPV4 = 0x0800,
};

typedef struct {
	uint8_t dest[6] __attribute__((packed));
	uint8_t src[6] __attribute__((packed));
	uint16_t type __attribute__((packed));

	uint8_t data[] __attribute__((packed));
} __attribute__((packed)) ethernet_packet_t;

/* ARP */
#define ARP_HWTYPE_ETHERNET 0x01
enum arp_opcode {
	ARP_OPCODE_REQUEST = 0x0001,
	ARP_OPCODE_REPLY   = 0x0002,
};

typedef struct {
	uint16_t hwtype __attribute__((packed)); // hardware type
	uint16_t ptype __attribute__((packed));  // protocol type
	uint8_t hlen __attribute__((packed));    // hardware address length (ethernet = 6)
	uint8_t plen __attribute__((packed));    // protocol address length (ipv4 = 6)
	uint16_t opcode __attribute__((packed)); // arp opcode

	// assuming hlen = 6 plen = 4
	uint8_t srchw[6] __attribute__((packed));
	uint8_t srcpr[4] __attribute__((packed));
	uint8_t dsthw[6] __attribute__((packed));
	uint8_t dstpr[4] __attribute__((packed));
} __attribute__((packed)) arp_packet_t;

/* IPv4 */

enum ipv4_type {
	IPV4_TYPE_ICMP = 0x01,
	IPV4_TYPE_TCP  = 0x06,
	IPV4_TYPE_UDP  = 0x11,
};

typedef struct {
	uint8_t version_ihl __attribute__((packed));
	uint8_t dscp_ecn __attribute__((packed));
	uint16_t length __attribute__((packed));
	uint16_t identification __attribute__((packed));
	uint16_t flags_fragment_offset __attribute__((packed));
	uint8_t ttl __attribute__((packed));
	uint8_t protocol __attribute__((packed));
	uint16_t checksum __attribute__((packed));
	uint8_t srcip[4] __attribute__((packed));
	uint8_t dstip[4] __attribute__((packed));
	uint8_t data[] __attribute__((packed));
} __attribute__((packed)) ipv4_packet_t;

/* ICMP */
enum icmp_type {
	ICMP_TYPE_ECHO_REPLY = 0,
	ICMP_TYPE_DEST_UNREACHABLE = 3,
	ICMP_TYPE_ECHO_REQUEST = 8,
	ICMP_TYPE_ROUTER_ADVERTISEMENT = 9,
	ICMP_TYPE_ROUTER_SOLICITATION = 10,
	ICMP_TYPE_TIME_EXCEEDED = 11,
};

typedef struct {
	uint8_t type __attribute__((packed));
	uint8_t code __attribute__((packed));
	uint16_t checksum __attribute__((packed));
	uint8_t data[] __attribute__((packed));
} __attribute__((packed)) icmp_packet_t;

typedef struct {
	uint16_t identifier __attribute__((packed));
	uint16_t sequence __attribute__((packed));
	uint8_t data[] __attribute__((packed));
} __attribute__((packed)) icmp_echo_packet_t;

/* UDP */
typedef struct {
	uint16_t srcport __attribute__((packed));
	uint16_t dstport __attribute__((packed));
	uint16_t length __attribute__((packed));
	uint16_t checksum __attribute__((packed));
	uint8_t data[] __attribute__((packed));
} __attribute__((packed)) udp_packet_t;

/* other stuff */

typedef struct {
	size_t length;
	void *data;
} packet_t;

typedef void (* send_packet_t)(void *extra, uint8_t *data, size_t length);
typedef packet_t *(* receive_packet_t)(void *extra);

typedef struct {
	send_packet_t send_packet;
	receive_packet_t receive_packet;
	uint8_t mac[6];
	void *extra;
	uint8_t ip[4];
} netif_t;

void net_register_netif(send_packet_t send, receive_packet_t receive, uint8_t *mac, void *extra);

void net_init(void);

#endif
