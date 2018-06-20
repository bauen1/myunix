#include <assert.h>

#include <process.h>

#include <console.h>
#include <module.h>
#include <net/net.h>
#include <net/e1000.h>
#include <string.h>
#include <list.h>
#include <heap.h>

//list_t *netif_list;
netif_t *_netif;

void net_register_netif(send_packet_t send, receive_packet_t receive, uint8_t *mac, void *extra) {
	assert(send != NULL);
	assert(receive != NULL);
	assert(extra != NULL); // technically valid
	netif_t *netif = kcalloc(1, sizeof(netif_t));
	assert(netif != NULL);
	netif->send_packet = send;
	netif->receive_packet = receive;
	netif->extra = extra;
	memcpy(&netif->mac[0], mac, 6);
//	list_insert(netif_list, netif);
	_netif = netif;
}

__attribute__((unused))
static void dump_arp(uint8_t *data) {
	printf(" ARP!\n");
	printf("   hardware type: 0x%2x%2x\n", data[14], data[15]);
	printf("   protocol type: 0x%2x%2x\n", data[16], data[17]);
	printf("   hardware size: 0x%x\n", data[18]);
	printf("   protocol size: 0x%x\n", data[19]);
	printf("   opcode: 0x%2x%2x\n", data[20], data[21]);
}

static int ktask_net(void *extra, char *name) {
	netif_t *netif = _netif;
	if (netif == NULL) { // should not happen
		return -1;
	}

	uint8_t buf[60] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // destination
		0, 0, 0, 0, 0, 0, // source
		0xDD, 0xDD, // protocol
	};
	// replace source with our mac address
	buf[ 6] = netif->mac[0];
	buf[ 7] = netif->mac[1];
	buf[ 8] = netif->mac[2];
	buf[ 9] = netif->mac[3];
	buf[10] = netif->mac[4];
	buf[11] = netif->mac[5];

	netif->send_packet(netif->extra, buf, 60);

	while (1) {
		ethernet_packet_t *packet = netif->receive_packet(netif->extra);
		printf("net received packet length: 0x%x\n", (uintptr_t)packet->length);
		printf(" dst: %2x:%2x:%2x:%2x:%2x:%2x\n",
			packet->data[0], packet->data[1], packet->data[2], packet->data[3], packet->data[4], packet->data[5]);
		printf(" src: %2x:%2x:%2x:%2x:%2x:%2x\n",
			packet->data[6], packet->data[7], packet->data[8], packet->data[9], packet->data[10], packet->data[11]);
		printf(" protocol: 0x%2x%2x\n", packet->data[12], packet->data[13]);
		if ((packet->data[12] == 0x08) && (packet->data[13] == 0x06)) {
			dump_arp(packet->data);
		}
		kfree(packet);
	}
}

void net_init(void) {
//	netif_list = list_init();
//	assert(netif_list != NULL);
	e1000_init();
	if (_netif != NULL) {
		create_ktask(ktask_net, "[net]", NULL);
	}
}

MODULE_INFO(net, net_init);
