#include "eth_speed.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/types.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>

int eth_speed(const char *ethname, int *speed, int *duplex_full)
{
	int fd = -1;
	struct ifreq ifr;
	struct ethtool_cmd ecmd;

	*speed = 0;
	*duplex_full = 0;

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, ethname);

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		goto error;

	ecmd.cmd = ETHTOOL_GSET;
	ifr.ifr_data = (caddr_t)&ecmd;
	if (ioctl(fd, SIOCETHTOOL, &ifr) != 0)
		goto error;

	switch (ecmd.speed)
	{
	case SPEED_10:
		*speed = 10;
		break;
	case SPEED_100:
		*speed = 100;
		break;
	case SPEED_1000:
		*speed = 1000;
		break;
	case SPEED_2500:
		*speed = 2500;
		break;
	case SPEED_10000:
		*speed = 10000;
		break;
	}

	if (ecmd.duplex == DUPLEX_FULL)
		*duplex_full = 1;

	close(fd);
	return 0;
error:
	if (fd >= 0)
		close(fd);
	return -1;
}

