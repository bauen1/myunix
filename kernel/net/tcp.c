#include <net/tcp.h>
#include <net/net.h>

#define DEBUG

#ifdef DEBUG
#define DEBUG_TCP
#endif

#ifdef DEBUG_TCP
#include <console.h>
#define debug_tcp(...) printf(__VA_ARGS__)
#else
#define debug_tcp(...)
#endif


void handle_tcp(netif_t *netif, ethernet_packet_t *ethernet_packet, size_t ethernet_length,
	ipv4_packet_t *ipv4_packet, size_t ipv4_data_length) {
	(void)netif;
	(void)ethernet_packet;
	(void)ethernet_length;
	if (ipv4_data_length < sizeof(tcp_packet_t)) {
		debug_tcp("    length too short!\n");
		return;
	}

	tcp_packet_t *tcp_packet = (tcp_packet_t *)(&ipv4_packet->data);
	debug_tcp("    srcport: %u\n", ntohs(tcp_packet->srcport));
	debug_tcp("    dstport: %u\n", ntohs(tcp_packet->dstport));
}
