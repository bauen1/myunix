#include <net/checksum.h>
#include <net/net.h>
#include <net/tcp.h>

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

static uint16_t tcp_calc_checksum(const tcp_header_t *packet, size_t length, const uint8_t srcip[4], const uint8_t dstip[4]) {
	assert(packet != NULL);
	assert(srcip != NULL);
	assert(dstip != NULL);
	tcp_check_header_t tcp_check_header = {
		.srcip = { srcip[0], srcip[1], srcip[2], srcip[3] },
		.dstip = { dstip[0], dstip[1], dstip[2], dstip[3] },
		.zero = 0,
		.protocol = IPV4_TYPE_TCP,
		.length = htons(length),
	};

	return net_calc_checksum2(&tcp_check_header, sizeof(tcp_check_header), packet, length, ntohs(packet->checksum));
}

void net_handle_tcp(netif_t *netif, const ethernet_packet_t *ethernet_packet, size_t ethernet_length, const ipv4_packet_t *ipv4_packet, size_t ipv4_data_length) {
	(void)netif;
	(void)ethernet_packet;
	(void)ethernet_length;
	if (ipv4_data_length < sizeof(tcp_header_t)) {
		debug_tcp("    length too short!\n\n");
		return;
	}

	tcp_header_t *tcp_header = (tcp_header_t *)&ipv4_packet->data;
	debug_tcp("    srcport: %u\n", ntohs(tcp_header->srcport));
	debug_tcp("    dstport: %u\n", ntohs(tcp_header->dstport));
	debug_tcp("    seq_num: %u\n", ntohs(tcp_header->seq_num));
	debug_tcp("    ack_num: %u\n", ntohs(tcp_header->ack_num));
	const uint16_t flags = ntohs(tcp_header->flags);
	const size_t data_offset = ((flags >> 12) & 0xF) * 4;
	const size_t data_length = ipv4_data_length - data_offset;
	debug_tcp("    data offset: %u, data_length: %u\n", (unsigned int)data_offset, (unsigned int)data_length);
	if (data_offset < sizeof(tcp_header_t)) {
		debug_tcp("    data offset too small\n\n");
		return;
	} else if (data_offset > 60) {
		debug_tcp("    data offset > 60!\n\n");
		return;
	}
	const size_t size = data_offset + data_length;
	if (size > ipv4_data_length) {
		// shouldn't happen
		debug_tcp("    packet longer than expected\n\n");
		return;
	}
	debug_tcp("    flags: 0x%x ", ntohs(tcp_header->flags));
	if (flags & TCP_FLAG_FIN) {
		debug_tcp("FIN ");
	}
	if (flags & TCP_FLAG_SYN) {
		debug_tcp("SYN ");
	}
	if (flags & TCP_FLAG_RST) {
		debug_tcp("RST ");
	}
	if (flags & TCP_FLAG_PSH) {
		debug_tcp("PSH ");
	}
	if (flags & TCP_FLAG_ACK) {
		debug_tcp("ACK ");
	}
	if (flags & TCP_FLAG_URG) {
		debug_tcp("URG ");
	}
	if (flags & TCP_FLAG_ECE) {
		debug_tcp("ECE ");
	}
	if (flags & TCP_FLAG_CWR) {
		debug_tcp("CWR ");
	}
	if (flags & TCP_FLAG_NS) {
		debug_tcp("NS  ");
	}
	if ((flags & 0xE00) != 0) {
		debug_tcp("\nreserved not set to 0 zero, dropping\n\n");
		return;
	}
	debug_tcp("\n");
	debug_tcp("    window_size: %u\n", ntohs(tcp_header->window_size));

	uint16_t checksum_received = ntohs(tcp_header->checksum);
	assert(tcp_header != NULL);
	uint16_t checksum_calculated = tcp_calc_checksum(tcp_header, size, ipv4_packet->srcip, ipv4_packet->dstip);
	if (checksum_received != checksum_calculated) {
		debug_tcp("    INVALID: checksum: 0x%x != calculated 0x%x\n\n", checksum_received, checksum_calculated);
		return;
	}

	debug_tcp("    urgent_ptr: %u\n", ntohs(tcp_header->urgent_ptr));
	debug_tcp("\n");

}
