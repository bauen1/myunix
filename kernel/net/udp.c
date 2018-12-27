#include <net/udp.h>
#include <heap.h>
#include <string.h>

#define DEBUG_UDP

#ifdef DEBUG_UDP
#include <console.h>
#define debug_udp(...) printf(__VA_ARGS__)
#else
#define debug_udp(...)
#endif

void net_handle_udp(netif_t *netif, const ethernet_packet_t *ethernet_packet, size_t ethernet_length, const ipv4_packet_t *ipv4_packet, size_t ipv4_data_length) {
	(void)ethernet_length;
	(void)ethernet_packet;
	(void)netif;
	if (ipv4_data_length < sizeof(udp_packet_t)) {
		debug_udp("    length too short!\n");
		return;
	}

	udp_packet_t *udp_packet = (udp_packet_t *)(&ipv4_packet->data);
	debug_udp("    srcport : %u\n", ntohs(udp_packet->srcport));
	debug_udp("    dstport : %u\n", ntohs(udp_packet->dstport));
	debug_udp("    length  : 0x%4x\n", ntohs(udp_packet->length));
	debug_udp("    checksum: 0x%4x\n", ntohs(udp_packet->checksum));

/*
	// TODO: check checksum
	if (ntohs(udp_packet->checksum) == 0) {
		debug_udp("checksum unused\n");
	} else {
		uint16_t old_checksum = ntohs(udp_packet->checksum);
		udp_packet->checksum = 0;
		if (old_checksum != net_calc_checksum((uint8_t *)udp_packet, ipv4_data_length)) {
			debug_udp("checksum invalid!\n");
			return;
		}
		udp_packet->checksum = htons(old_checksum);
	}
*/
	debug_udp("   data:\n");
	for (unsigned int i = 0; i < ntohs(udp_packet->length) - sizeof(udp_packet_t); i++) {
		debug_udp("0x%4x: 0x%2x\n", i, udp_packet->data[i]);
	}
	// ECHO
	if (!net_send_udp(netif, ipv4_packet->srcip, ntohs(udp_packet->dstport), ntohs(udp_packet->srcport), ntohs(udp_packet->length), udp_packet->data)) {
		debug_udp("failure sending echo!\n");
	}
}

bool net_send_udp(netif_t *netif, const uint8_t dstip[4], uint16_t srcport, uint16_t dstport, uint16_t length, const uint8_t *data) {
	size_t size = sizeof(udp_packet_t) + length;
	udp_packet_t *packet = kcalloc(1, size);
	if (packet == NULL) {
		printf("out of memory!\n");
		return false;
	}

	packet->srcport = htons(srcport);
	packet->dstport = htons(dstport);
	packet->length = htons(size);
	packet->checksum = 0;
	memcpy(packet->data, data, length);
	// FIXME: pretty sure identification = 0 isn't valid (more than once atleast)
	bool success = net_send_ipv4(netif, NULL, 0, IPV4_TYPE_UDP, netif->ip, dstip, (uint8_t *)packet, size);
	kfree(packet);
	return success;
}
