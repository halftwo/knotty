#include "netaddr.h"
#include "eth_speed.h"
#include "xslib/hex.h"
#include "xslib/iobuf.h"
#include "xslib/xnet.h"
#include "xslib/xstr.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

int get_netaddr(std::ostream& oss)
{
	struct ifconf ifc;
	int sock;
	int len, lastlen;
	int k = 0;
	char *buf, *ptr;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		return -1;

	lastlen = 0;
	len = 100 * sizeof(struct ifreq);
	buf = (char *)malloc(len);
	while (true)
	{
		ifc.ifc_len = len;
		ifc.ifc_buf = buf;
		if (ioctl(sock, SIOCGIFCONF, &ifc) < 0)
		{
			if (errno != EINVAL || lastlen != 0)
			{
				free(buf);
				close(sock);
				return -1;
			}
		}
		else
		{
			if (ifc.ifc_len == lastlen)
				break;
			lastlen = ifc.ifc_len;
		}
		len += 10 * sizeof(struct ifreq);
		buf = (char *)realloc(buf, len);
	}

	iobuf_t ob = make_iobuf(oss, NULL, 0);
	bool first = true;
	bool ip_first = true;
	xstr_t lastname = xstr_null;
	for (ptr = buf; ptr < buf + ifc.ifc_len; ptr += sizeof(struct ifreq))
	{
		struct ifreq *ifr = (struct ifreq *)ptr;
		char *name = ifr->ifr_name;
		char *p = strchr(name, ':');
		xstr_t thisname = XSTR_INIT((unsigned char *)name, p ? p - name : (ssize_t)strlen(name));

		if (xstr_equal_cstr(&thisname, "lo"))
			continue;
	
		struct ifreq ifrcopy;
		if (!xstr_equal(&lastname, &thisname))
		{
			++k;
			lastname = thisname;
			ip_first = true;

			if (first)
				first = !first;
			else
				iobuf_putc(&ob, ',');

			char hwaddr[33] = { '-', 0 };
			ifrcopy = *ifr;
			if (ioctl(sock, SIOCGIFHWADDR, &ifrcopy) == 0)
			{
				unsigned char *u = (unsigned char *)ifrcopy.ifr_hwaddr.sa_data;
				switch (ifrcopy.ifr_hwaddr.sa_family)
				{
				case ARPHRD_ETHER:
				case ARPHRD_IEEE802:
				case ARPHRD_HIPPI:
				case ARPHRD_IEEE802_TR:
					hexlify_upper(hwaddr, u, 6);
					break;
				default:
					break;
				}
			}
			iobuf_printf(&ob, "%s:%s", ifr->ifr_name, hwaddr);

			int speed = 0;
			int duplex_full = 0;
			if (eth_speed(ifr->ifr_name, &speed, &duplex_full) == 0)
				iobuf_printf(&ob, "~%d%c", speed, duplex_full ? 'F' : 'H');
			else
				iobuf_printf(&ob, "~-");
		}

		char ipaddr[32];
		ifrcopy = *ifr;
		if (ifr->ifr_addr.sa_family == AF_INET && ioctl(sock, SIOCGIFFLAGS, &ifrcopy) == 0 && (ifrcopy.ifr_flags & IFF_UP))
		{
			struct sockaddr_in *sin = (struct sockaddr_in *)&ifr->ifr_addr;
			uint32_t ip = ntohl(sin->sin_addr.s_addr);
			xnet_ipv4_ntoa(ip, ipaddr);
		}
		else
		{
			ipaddr[0] = '-';
			ipaddr[1] = 0;
		}
		iobuf_printf(&ob, "%c%s", (ip_first ? '~' : '+'), ipaddr);

		ip_first = false;
	}

	free(buf);
	close(sock);

	return k;
}


#ifdef TEST_NETADDR

#include <stdio.h>
#include <sstream>

int main()
{
	std::ostringstream oss;
	int r = get_netaddr(oss);
	printf("%d %s\n", r, oss.str().c_str());
	return 0;
}

#endif
