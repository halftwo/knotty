/* $Id: xnet.h,v 1.1 2015/04/08 02:44:33 gremlin Exp $ */
/*
   Author: XIONG Jiagui
   Date: 2005-06-20
 */
#ifndef XNET_H_
#define XNET_H_ 1

#include <endian.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XNET_IP4STR_SIZE		16

#define XNET_IP6STR_SIZE		40
#define XNET_IP6BIN_SIZE		16


typedef union {
	sa_family_t family;
	struct sockaddr_in a4;
	struct sockaddr_in6 a6;
} xnet_inet_sockaddr_t;


int xnet_tcp_listen(const char *ip/*NULL*/, uint16_t port, int queue_size);

int xnet_tcp_bind(const char *ip/*NULL*/, uint16_t port);

int xnet_tcp_accept(int sock, char *ip/*NULL*/, uint16_t *port/*NULL*/);

int xnet_tcp_connect(const char *host, uint16_t port);

int xnet_tcp_connect_nonblock(const char *host, uint16_t port);

int xnet_tcp_connect_sockaddr(const struct sockaddr *addr, socklen_t addrlen);

int xnet_tcp_connect_sockaddr_nonblock(const struct sockaddr *addr, socklen_t addrlen);


int xnet_unix_listen(const char *pathname, int queue_size);

int xnet_unix_bind(const char *pathname);

int xnet_unix_connect(const char *pathname);

int xnet_unix_connect_nonblock(const char *pathname);


int xnet_udp_bind(const char *ip/*NULL*/, uint16_t port);

int xnet_udp_connect(const char *host, uint16_t port);

int xnet_udp_connect_sockaddr(const struct sockaddr *addr, socklen_t addrlen);


int xnet_socketpair(int sv[2]);


int xnet_ip_sockaddr(const char *host, uint16_t port, struct sockaddr_in *addr);
int xnet_ip46_sockaddr(const char *host, uint16_t port, struct sockaddr *addr);
int xnet_ip64_sockaddr(const char *host, uint16_t port, struct sockaddr *addr);

/* Return port number or -1 on error.
 */
int xnet_sockaddr_ip(const struct sockaddr_in *addr, char ip4str[XNET_IP4STR_SIZE]);
int xnet_sockaddr_to_ip(const struct sockaddr *addr, char ipstr[], size_t len);


/* Same as read(), except that it will restart read() when EINTR.
 */
ssize_t xnet_read_n(int fd, void *buf, size_t len);


/* Same as write(), except that it will restart write() when EINTR.
 */
ssize_t xnet_write_n(int fd, const void *buf, size_t len);



/*
 * 0 for success, -1 for error, -2 for end-of-file, -3 for timeout.
 * NB: the fd should be in NONBLOCK mode
 */
int xnet_read_resid(int fd, void *buf, size_t *resid, int *timeout_ms/*NULL*/);

int xnet_readv_resid(int fd, struct iovec **iov, int *count, int *timeout_ms/*NULL*/);


int xnet_write_resid(int fd, const void *buf, size_t *resid, int *timeout_ms/*NULL*/);

int xnet_writev_resid(int fd, struct iovec **iov, int *count, int *timeout_ms/*NULL*/);



/* Non-block read. 
 * The return value has a different meanings than read() function.
 * 0 for would-block, -1 for error, -2 for end-of-file (read() returning 0).
 */ 
ssize_t xnet_recv_nonblock(int fd, void *buf, size_t len, int flags);
ssize_t xnet_recvfrom_nonblock(int fd, void *buf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen);
ssize_t xnet_read_nonblock(int fd, void *buf, size_t len);

ssize_t xnet_readv_nonblock(int fd, const struct iovec *iov, int count);


/* Non-block peek.
 * The return value: 
 * 0 for would-block, -1 for error, -2 for end-of-file (read() returning 0).
 */
ssize_t xnet_peek_nonblock(int fd, void *buf, size_t len);


/* Non-block write.
   The return value has a different meaning than write() function.
   0 for would-block, -1 for error, -2 for end-of-file (EPIPE).
 */
ssize_t xnet_send_nonblock(int fd, const void *buf, size_t len, int flags);
ssize_t xnet_sendto_nonblock(int fd, const void *buf, size_t len, int flags, const struct sockaddr *addr, socklen_t addrlen);
ssize_t xnet_write_nonblock(int fd, const void *buf, size_t len);

ssize_t xnet_writev_nonblock(int fd, const struct iovec *iov, int count);


int xnet_adjust_iovec(struct iovec **piov, int iov_count, size_t writen);



int xnet_get_so_error(int fd);


int xnet_set_tcp_nodelay(int fd);
int xnet_set_keepalive(int fd);

int xnet_set_send_buffer_size(int fd, int size);
int xnet_get_send_buffer_size(int fd);

int xnet_set_recv_buffer_size(int fd, int size);
int xnet_get_recv_buffer_size(int fd);

int xnet_set_reuse_address(int fd);
int xnet_clear_reuse_address(int fd);

int xnet_set_linger_on(int fd, int seconds);
int xnet_set_linger_off(int fd);

int xnet_set_nonblock(int fd);
int xnet_set_block(int fd);


int xnet_set_close_on_exec(int fd);
int xnet_clear_close_on_exec(int fd);


uint16_t xnet_get_sock_ip_port(int sock, char ipstr[XNET_IP6STR_SIZE]);
uint16_t xnet_get_peer_ip_port(int sock, char ipstr[XNET_IP6STR_SIZE]);

uint16_t xnet_get_sock_ipv4(int sock, uint32_t *ipv4);
uint16_t xnet_get_peer_ipv4(int sock, uint32_t *ipv4);
uint16_t xnet_get_sock_ipv6(int sock, uint8_t ipv6[XNET_IP6BIN_SIZE]);
uint16_t xnet_get_peer_ipv6(int sock, uint8_t ipv6[XNET_IP6BIN_SIZE]);

/* Return 1 for big-endian, 0 for little-endian. 
 */
int xnet_get_endian();


int xnet_get_mac(uint8_t macs[][6], int num);


/* Return the number of IPs that the current machine has, not including 
   the local loop ip (127.x.x.x).  
   The returned number may be greater than the 2nd argument "num".
   A negative number is returned if some error occured.
   The IPs get are returned in the 1st argument.
   The returned ips are in host byte order.
 */
int xnet_ipv4_get_all(uint32_t ips[], int num);
int xnet_get_all_ips(uint8_t ipv6s[][XNET_IP6BIN_SIZE], int *v6num/*NULL*/, uint32_t ipv4s[], int *v4num/*NULL*/);

/* ipv4 is in host byte order */
int xnet_ipv4_ntoa(uint32_t ip, char str[XNET_IP4STR_SIZE]);
int xnet_ipv6_ntoa(const uint8_t ip[XNET_IP6BIN_SIZE], char str[XNET_IP6STR_SIZE]);

bool xnet_ipv4_aton(const char *str, uint32_t *ip);
bool xnet_ipv6_aton(const char *str, uint8_t ip[XNET_IP6BIN_SIZE]);
bool xnet_ip46_aton(const char *str, uint8_t ip[XNET_IP6BIN_SIZE]);

bool xnet_is_loopback_sockaddr(const struct sockaddr *addr, socklen_t addrlen);
bool xnet_is_loopback_ip(const char *ip);

bool xnet_is_internal_ip(const char *ip);
bool xnet_is_external_ip(const char *ip);

bool xnet_ipv4_is_loopback(uint32_t ip);
bool xnet_ipv4_is_internal(uint32_t ip);
bool xnet_ipv4_is_external(uint32_t ip);

bool xnet_ipv6_is_loopback(const uint8_t ip[XNET_IP6BIN_SIZE]);
bool xnet_ipv6_is_linklocal(const uint8_t ip[XNET_IP6BIN_SIZE]);
bool xnet_ipv6_is_sitelocal(const uint8_t ip[XNET_IP6BIN_SIZE]);
bool xnet_ipv6_is_uniquelocal(const uint8_t ip[XNET_IP6BIN_SIZE]);
bool xnet_ipv6_is_ipv4(const uint8_t ip[XNET_IP6BIN_SIZE]);


/* Compare ::ffff:12.34.56.78 and 12.34.56.78 
 */
bool xnet_ip_equal(const char *ip1, const char *ip2);


/* Return the strlen() of the ip, or 0 if no internal ip.
   If 0 is returned, the ip[0] is set to '\0'.
 */
int xnet_get_internal_ip(char ip[]);
int xnet_get_external_ip(char ip[]);


/* Return the buffer length used to hold the integer value.
 * Unused buffer are filled with '\0'.
 * Return a negative if buffer not large enough to hold the integer value.
 * The bytes of msb is right aligned and lsb is left aligned.
 */
ssize_t xnet_msb_from_uint(void *buf, size_t size, uintmax_t n);
ssize_t xnet_lsb_from_uint(void *buf, size_t size, uintmax_t n);

uintmax_t xnet_uint_from_msb(const void *data, size_t len);
uintmax_t xnet_uint_from_lsb(const void *data, size_t len);


void xnet_swap(void *p, size_t n);

void (xnet_lsb)(void *p, size_t n);
void (xnet_msb)(void *p, size_t n);

void (xnet_lsb16)(uint16_t *p);
void (xnet_lsb32)(uint32_t *p);
void (xnet_lsb64)(uint64_t *p);

void (xnet_msb16)(uint16_t *p);
void (xnet_msb32)(uint32_t *p);
void (xnet_msb64)(uint64_t *p);

uint16_t (xnet_l16)(uint16_t v);
uint32_t (xnet_l32)(uint32_t v);
uint64_t (xnet_l64)(uint64_t v);

uint16_t (xnet_m16)(uint16_t v);
uint32_t (xnet_m32)(uint32_t v);
uint64_t (xnet_m64)(uint64_t v);


void *xnet_copy_lsb(void *dst, const void *src, size_t n);
void *xnet_copy_msb(void *dst, const void *src, size_t n);

void *xnet_copy_2lsb(void *dst, const void *src);
void *xnet_copy_4lsb(void *dst, const void *src);
void *xnet_copy_8lsb(void *dst, const void *src);

void *xnet_copy_2msb(void *dst, const void *src);
void *xnet_copy_4msb(void *dst, const void *src);
void *xnet_copy_8msb(void *dst, const void *src);


#if __BYTE_ORDER == __LITTLE_ENDIAN

# define xnet_lsb(p, n)	((void)0)
# define xnet_msb(p, n)	((xnet_msb)((p), (n)))

# define xnet_lsb16(p)	((void)0)
# define xnet_lsb32(p)	((void)0)
# define xnet_lsb64(p)	((void)0)

# define xnet_msb16(p)	((xnet_msb16)((p)))
# define xnet_msb32(p)	((xnet_msb32)((p)))
# define xnet_msb64(p)	((xnet_msb64)((p)))

# define xnet_l16(v)	((uint16_t)(v))
# define xnet_l32(v)	((uint32_t)(v))
# define xnet_l64(v)	((uint64_t)(v))

# define xnet_m16(v)	((xnet_m16)((v)))
# define xnet_m32(v)	((xnet_m32)((v)))
# define xnet_m64(v)	((xnet_m64)((v)))

#elif __BYTE_ORDER == __BIG_ENDIAN

# define xnet_lsb(p, n)	((xnet_lsb)((p), (n)))
# define xnet_msb(p, n)	((void)0)

# define xnet_lsb16(p)	((xnet_lsb16)((p)))
# define xnet_lsb32(p)	((xnet_lsb32)((p)))
# define xnet_lsb64(p)	((xnet_lsb64)((p)))

# define xnet_msb16(p)	((void)0)
# define xnet_msb32(p)	((void)0)
# define xnet_msb64(p)	((void)0)

# define xnet_l16(v)	((xnet_l16)((v)))
# define xnet_l32(v)	((xnet_l32)((v)))
# define xnet_l64(v)	((xnet_l64)((v)))

# define xnet_m16(v)	((uint16_t)(v))
# define xnet_m32(v)	((uint32_t)(v))
# define xnet_m64(v)	((uint64_t)(v))

#else
# error Not supported __BYTE_ORDER
#endif


#ifdef __cplusplus
}
#endif

#endif

