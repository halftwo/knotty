#ifndef ETH_SPEED_H_
#define ETH_SPEED_H_

#ifdef __cplusplus
extern "C" {
#endif


int eth_speed(const char *ethname, int *speed, int *duplex_full);


#ifdef __cplusplus
}
#endif

#endif
