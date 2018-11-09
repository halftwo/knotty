/*
   Author: XIONG Jiagui
   Date: 2005-06-20
 */
#include "xnet.h"
#include "msec.h"
#include "xlog.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <ifaddrs.h>
#include <endian.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: xnet.c,v 1.1 2015/04/08 02:44:33 gremlin Exp $";
#endif

#if __BYTE_ORDER != __BIG_ENDIAN && __BYTE_ORDER != __LITTLE_ENDIAN
#error Not supported __BYTE_ORDER
#endif


int xnet_tcp_listen(const char *ip, uint16_t port, int queue_size)
{
	int sock = xnet_tcp_bind(ip, port);
	if (sock >= 0)
	{
		if (listen(sock, queue_size) == -1)
		{
			close(sock);
			return -1;
		}
	}
	return sock;
}

int xnet_unix_listen(const char *pathname, int queue_size)
{
	int sock = xnet_unix_bind(pathname);
	if (sock >= 0)
	{
		if (listen(sock, queue_size) == -1)
		{
			close(sock);
			return -1;
		}
	}
	return sock;
}

static bool _ip2addr(const char *ip, uint16_t port, bool ipv6, xnet_inet_sockaddr_t *addr)
{
	bool ok = false;
	if (ip == NULL || ip[0] == 0)
	{
		ip = ipv6 ? "::" : "0.0.0.0";
	}

	memset(addr, 0, sizeof(*addr));
	if (strchr(ip, ':'))
	{
		addr->family = AF_INET6;
		ok = inet_pton(AF_INET6, ip, &addr->a6.sin6_addr) > 0;
		addr->a6.sin6_port = htons(port);
	}
	else
	{
		addr->family = AF_INET;
		ok = inet_pton(AF_INET, ip, &addr->a4.sin_addr) > 0;
		addr->a4.sin_port = htons(port);
	}
	return ok;
}

int xnet_tcp_bind(const char *ip, uint16_t port)
{
	xnet_inet_sockaddr_t addr;
	int sock = -1;
	int on = 1;
	bool ipv6 = true;
again:
	if (!_ip2addr(ip, port, ipv6, &addr))
		goto error;

	sock = socket((addr.family == AF_INET6) ? PF_INET6 : PF_INET, SOCK_STREAM, 0);
	if (sock == -1)
	{
		if (errno == EAFNOSUPPORT && (ip == NULL || ip[0] == 0) && ipv6)
		{
			ipv6 = false;
			goto again;
		}
			
		goto error;
	}

	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		goto error;

	return sock;
error:
	if (sock != -1)
		close(sock);
	return -1;
}

int xnet_udp_bind(const char *ip, uint16_t port)
{
	xnet_inet_sockaddr_t addr;
	int sock = -1;
	bool ipv6 = true;
again:
	if (!_ip2addr(ip, port, ipv6, &addr))
		goto error;

	sock = socket((addr.family == AF_INET6) ? PF_INET6 : PF_INET, SOCK_DGRAM, 0);
	if (sock == -1)
	{
		if (errno == EAFNOSUPPORT && (ip == NULL || ip[0] == 0) && ipv6)
		{
			ipv6 = false;
			goto again;
		}
			
		goto error;
	}

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		goto error;

	return sock;
error:
	if (sock != -1)
		close(sock);
	return -1;
}

int xnet_tcp_accept(int sock, char *ip/*NULL*/, uint16_t *port/*NULL*/)
{
	xnet_inet_sockaddr_t addr;
	socklen_t addrlen = sizeof(addr);
	int fd = accept(sock, (struct sockaddr *)&addr, &addrlen);
	if (fd < 0)
		return fd;
	
	if (ip || port)
	{
		char buf[XNET_IP6STR_SIZE];
		int n = xnet_sockaddr_to_ip((const struct sockaddr *)&addr, buf, sizeof(buf));
		if (ip)
			strcpy(ip, buf);

		if (port)
			*port = n;
	}

	return fd;
}

int xnet_udp_connect_sockaddr(const struct sockaddr *addr, socklen_t addrlen)
{
	int domain;
	int sock;

	if (addr->sa_family == AF_INET6)
		domain = PF_INET6;
	else if (addr->sa_family == AF_INET)
		domain = PF_INET;
	else
		return -1;

	sock = socket(domain, SOCK_DGRAM, 0);
	if (sock == -1)
		goto error;

	if (connect(sock, addr, addrlen) == -1)
	{
		goto error;
	}

	return sock;
error:
	if (sock != -1)
		close(sock);
	return -1;
}

int xnet_udp_connect(const char *host, uint16_t port)
{
	xnet_inet_sockaddr_t addr;

	if (xnet_ip46_sockaddr(host, port, (struct sockaddr *)&addr) < 0)
		return -1;

	return xnet_udp_connect_sockaddr((struct sockaddr *)&addr, sizeof(addr));
}

int xnet_socketpair(int sv[2])
{
	return socketpair(AF_LOCAL, SOCK_STREAM, 0, sv);
}

int xnet_sockaddr_ip(const struct sockaddr_in *addr, char ip[16])
{
	inet_ntop(AF_INET, &addr->sin_addr, ip, 16);
	return ntohs(addr->sin_port);
}

int xnet_sockaddr_to_ip(const struct sockaddr *addr, char ip[], size_t len)
{
	if (addr->sa_family == AF_INET6)
	{
		struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)addr;
		if (inet_ntop(AF_INET6, &a6->sin6_addr, ip, len))
			return ntohs(a6->sin6_port);
	}
	else if (addr->sa_family == AF_INET)
	{
		struct sockaddr_in *a4 = (struct sockaddr_in *)addr;
		if (inet_ntop(AF_INET, &a4->sin_addr, ip, len))
			return ntohs(a4->sin_port);
	}

	if (len > 0)
		ip[0] = 0;
	return 0;
}

int xnet_ip_sockaddr(const char *host, uint16_t port, struct sockaddr_in *addr)
{
	int errcode = 0;

	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = htonl(INADDR_ANY);

	if (host && host[0])
	{
		struct in_addr ipaddr;

		if (inet_aton(host, &ipaddr))
		{
			addr->sin_addr = ipaddr;
		}
		else if (strcasecmp(host, "localhost") == 0)
		{
			addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		}
		else
		{
			struct addrinfo hints = {0}, *res = NULL;

			/* Don't use gethostbyname(), it's not thread-safe. 
			   Use getaddrinfo() instead.
			 */
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;
			errcode = getaddrinfo(host, NULL, &hints, &res);
			if (errcode != 0)
			{
				xlog(XLOG_WARN, "getaddrinfo() failed: %s\n", gai_strerror(errcode));
			}
			else
			{
				assert(res->ai_family == AF_INET && res->ai_socktype == SOCK_STREAM);
				assert(res->ai_addrlen == sizeof(*addr));

				memcpy(addr, (struct sockaddr_in *)res->ai_addr, sizeof(*addr));
			}

			if (res)
				freeaddrinfo(res);
		}
	}

	addr->sin_port = htons(port);
	return errcode ? -1 : 0;
}

static int _ip4or6_sockaddr(const char *host, uint16_t port, bool ipv6first, struct sockaddr *addr)
{
	struct sockaddr_in *a4 = (struct sockaddr_in *)addr;
	struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)addr;
	int errcode = 0;

	addr->sa_family = AF_UNSPEC;
	if (host && host[0])
	{
		struct addrinfo hints = {0}, *res = NULL;

		/* Don't use gethostbyname(), it's not thread-safe. 
		   Use getaddrinfo() instead.
		 */
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		errcode = getaddrinfo(host, NULL, &hints, &res);
		if (errcode != 0)
		{
			xlog(XLOG_WARN, "getaddrinfo() failed: %s\n", gai_strerror(errcode));
		}
		else
		{
			struct addrinfo *info, *in4 = NULL, *in6 = NULL;
			for (info = res; info; info = info->ai_next)
			{
				if (info->ai_family == AF_INET6)
				{
					if (!in6)
					{
						in6 = info;
						if (ipv6first)
							break;
					}
				}
				else if (info->ai_family == AF_INET)
				{
					if (!in4)
					{
						in4 = info;
						if (!ipv6first)
							break;
					}
				}
			}

			if (in6 && (ipv6first || !in4))
			{
				memcpy(a6, in6->ai_addr, sizeof(*a6));
			}
			else if (in4)
			{
				memcpy(a4, in4->ai_addr, sizeof(*a4));
			}
			else
			{
				errcode = -1;
			}
		}

		if (res)
			freeaddrinfo(res);
	}
	else
	{
		if (ipv6first)
		{
			a6->sin6_family = AF_INET6;
			a6->sin6_addr = in6addr_any;
		}
		else
		{
			a4->sin_family = AF_INET;
			a4->sin_addr.s_addr = htonl(INADDR_ANY);
		}
	}

	if (addr->sa_family == AF_INET6)
		a6->sin6_port = htons(port);
	else if (addr->sa_family == AF_INET)
		a4->sin_port = htons(port);

	return errcode ? -1 : 0;
}

int xnet_ip46_sockaddr(const char *host, uint16_t port, struct sockaddr *addr)
{
	return _ip4or6_sockaddr(host, port, false, addr);
}

int xnet_ip64_sockaddr(const char *host, uint16_t port, struct sockaddr *addr)
{
	return _ip4or6_sockaddr(host, port, true, addr);
}

static int _tcp_connect_sockaddr(const struct sockaddr *addr, socklen_t addrlen, int nonblock)
{
	int domain;
	int sock;

	if (addr->sa_family == AF_INET6)
		domain = PF_INET6;
	else if (addr->sa_family == AF_INET)
		domain = PF_INET;
	else
		return -1;

	sock = socket(domain, SOCK_STREAM, 0);
	if (sock == -1)
		goto error;

	if (nonblock)
	{
		int flags = fcntl(sock, F_GETFL, 0);
		if (flags == -1 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1)
			goto error;
	}

	if (connect(sock, addr, addrlen) == -1)
	{
		if (!nonblock || errno != EINPROGRESS)
			goto error;
	}

	return sock;
error:
	if (sock != -1)
		close(sock);
	return -1;
}

int xnet_tcp_connect_sockaddr(const struct sockaddr *addr, socklen_t addrlen)
{
	return _tcp_connect_sockaddr(addr, addrlen, 0);
}

int xnet_tcp_connect_sockaddr_nonblock(const struct sockaddr *addr, socklen_t addrlen)
{
	return _tcp_connect_sockaddr(addr, addrlen, 1);
}

int xnet_tcp_connect(const char *host, uint16_t port)
{
	xnet_inet_sockaddr_t addr;

	if (xnet_ip46_sockaddr(host, port, (struct sockaddr *)&addr) < 0)
		return -1;

	return _tcp_connect_sockaddr((struct sockaddr *)&addr, sizeof(addr), 0);
}

int xnet_tcp_connect_nonblock(const char *host, uint16_t port)
{
	xnet_inet_sockaddr_t addr;

	if (xnet_ip46_sockaddr(host, port, (struct sockaddr *)&addr) < 0)
		return -1;

	return _tcp_connect_sockaddr((struct sockaddr *)&addr, sizeof(addr), 1);
}

int xnet_unix_bind(const char *pathname)
{
	struct sockaddr_un addr;
	int sock = -1;
	mode_t mode = 0;

	sock = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (sock == -1)
		goto error;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_LOCAL;
	strncpy(addr.sun_path, pathname, sizeof(addr.sun_path) - 1);

	unlink(pathname);
	mode = umask(0);

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		goto error;

	umask(mode);
	return sock;
error:
	if (sock != -1)
		close(sock);
	if (mode)
		umask(mode);
	return -1;
}

static int _unix_connect_nonblock(const char *pathname, bool nonblock)
{
	struct sockaddr_un addr;
	int sock = -1;

	sock = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (sock == -1)
		goto error;

	if (nonblock)
	{
		int flags = fcntl(sock, F_GETFL, 0);
		if (flags == -1 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1)
			goto error;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_LOCAL;
	strncpy(addr.sun_path, pathname, sizeof(addr.sun_path) - 1);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		goto error;

	return sock;
error:
	if (sock != -1)
		close(sock);
	return -1;
}

int xnet_unix_connect(const char *pathname)
{
	return _unix_connect_nonblock(pathname, false);
}

int xnet_unix_connect_nonblock(const char *pathname)
{
	return _unix_connect_nonblock(pathname, true);
}

ssize_t xnet_read_n(int fd, void *buf, size_t len)
{
	ssize_t cur = 0;

	if ((ssize_t)len <= 0)
		return len;

	do
	{
		ssize_t need = len - cur;
		ssize_t n = read(fd, (char *)buf + cur, need);
		if (n == 0)
			break;
		if (n == -1)
		{
			if (errno == EINTR)
				n = 0;
			else
				return -1;
		}
		cur += n;
	} while (cur < (ssize_t)len);
	return cur;
}

ssize_t xnet_write_n(int fd, const void *buf, size_t len)
{
	ssize_t cur = 0;

	if ((ssize_t)len <= 0)
		return len;

	do
	{
		ssize_t need = len - cur;
		ssize_t n = write(fd, (char *)buf + cur, need);
		if (n == -1)
		{
			if (errno == EINTR)
				n = 0;
			else
				return -1;
		}
		cur += n;
	} while (cur < (ssize_t)len);
	return cur;
}


int xnet_read_resid(int fd, void *buf, size_t *resid, int *timeout_ms)
{
	int ret = 0;
	ssize_t n;

	if ((ssize_t)*resid <= 0)
		return ret;

	n = xnet_read_nonblock(fd, buf, *resid);
	if (n < 0)
		return n;

	*resid -= n;
	if (*resid > 0)
	{
		uint64_t start;
		int timeout = timeout_ms ? *timeout_ms : -1;
		if (timeout == 0)
			return -3;

		start = timeout > 0 ? exact_mono_msec() : 0;
		buf = (char *)buf + n;
		while (1)
		{
			int rc;
			struct pollfd fds[1];
			fds[0].fd = fd;
			fds[0].events = POLLIN | POLLPRI;

			rc = poll(fds, 1, timeout);
			if (rc < 0 && errno != EINTR)
			{
				ret = -1;
				goto done;
			}

			if (rc > 0)
			{
				n = xnet_read_nonblock(fd, buf, *resid);
				if (n < 0)
				{
					ret = n;
					goto done;
				}

				if (n > 0)
				{
					*resid -= n;
					if (*resid == 0)
						goto done;
					buf = (char *)buf + n;
				}
			}

			if (timeout > 0)
			{
				uint64_t now = exact_mono_msec();
				timeout -= (now - start);
				if (timeout <= 0)
				{
					*timeout_ms = 0;
					return -3;
				}
				start = now;
			}
		}
	done:
		if (timeout > 0)
		{
			uint64_t now = exact_mono_msec();
			timeout -= (now - start);
			*timeout_ms = timeout > 0 ? timeout : 0;
		}
	}

	return ret;
}

int xnet_write_resid(int fd, const void *buf, size_t *resid, int *timeout_ms)
{
	int ret = 0;
	ssize_t n;

	if ((ssize_t)*resid <= 0)
		return ret;

	n = xnet_write_nonblock(fd, buf, *resid);
	if (n < 0)
		return n;

	*resid -= n;
	if (*resid > 0)
	{
		uint64_t start;
		int timeout = timeout_ms ? *timeout_ms : -1;
		if (timeout == 0)
			return -3;

		buf = (char *)buf + n;
		start = timeout > 0 ? exact_mono_msec() : 0;
		while (1)
		{
			int rc;
			struct pollfd fds[1];
			fds[0].fd = fd;
			fds[0].events = POLLOUT | POLLHUP;

			rc = poll(fds, 1, timeout);
			if (rc < 0 && errno != EINTR)
			{
				ret = -1;
				goto done;
			}

			if (rc > 0)
			{
				n = xnet_write_nonblock(fd, buf, *resid);
				if (n < 0)
				{
					ret = n;
					goto done;
				}

				if (n > 0)
				{
					*resid -= n;
					if (*resid == 0)
						goto done;
					buf = (char *)buf + n;
				}
			}

			if (timeout > 0)
			{
				uint64_t now = exact_mono_msec();
				timeout -= (now - start);
				if (timeout <= 0)
				{
					*timeout_ms = 0;
					return -3;
				}
				start = now;
			}
		}
	done:
		if (timeout > 0)
		{
			uint64_t now = exact_mono_msec();
			timeout -= (now - start);
			*timeout_ms = timeout > 0 ? timeout : 0;
		}
	}

	return ret;
}

int xnet_readv_resid(int fd, struct iovec **iov, int *count, int *timeout_ms)
{
	int ret = 0;
	ssize_t n;

	if (*count <= 0)
		return ret;

	n = xnet_readv_nonblock(fd, *iov, *count);
	if (n < 0)
		return n;

	if (n > 0)
		*count = xnet_adjust_iovec(iov, *count, n);

	if (*count > 0)
	{
		uint64_t start;
		int timeout = timeout_ms ? *timeout_ms : -1;
		if (timeout == 0)
			return -3;

		start = timeout > 0 ? exact_mono_msec() : 0;
		while (1)
		{
			int rc;
			struct pollfd fds[1];
			fds[0].fd = fd;
			fds[0].events = POLLIN | POLLPRI;

			rc = poll(fds, 1, timeout);
			if (rc < 0 && errno != EINTR)
			{
				ret = -1;
				goto done;
			}

			if (rc > 0)
			{
				n = xnet_readv_nonblock(fd, *iov, *count);
				if (n < 0)
				{
					ret = n;
					goto done;
				}

				if (n > 0)
				{
					*count = xnet_adjust_iovec(iov, *count, n);
					if (*count == 0)
						goto done;
				}
			}

			if (timeout > 0)
			{
				uint64_t now = exact_mono_msec();
				timeout -= (now - start);
				if (timeout <= 0)
				{
					*timeout_ms = 0;
					return -3;
				}
				start = now;
			}
		}
	done:
		if (timeout > 0)
		{
			uint64_t now = exact_mono_msec();
			timeout -= (now - start);
			*timeout_ms = timeout > 0 ? timeout : 0;
		}
	}

	return ret;
}

int xnet_writev_resid(int fd, struct iovec **iov, int *count, int *timeout_ms)
{
	int ret = 0;
	ssize_t n;

	if (*count <= 0)
		return ret;

	n = xnet_writev_nonblock(fd, *iov, *count);
	if (n < 0)
		return n;

	if (n > 0)
		*count = xnet_adjust_iovec(iov, *count, n);

	if (*count > 0)
	{
		uint64_t start;
		int timeout = timeout_ms ? *timeout_ms : -1;
		if (timeout == 0)
			return -3;

		start = timeout > 0 ? exact_mono_msec() : 0;
		while (1)
		{
			int rc;
			struct pollfd fds[1];
			fds[0].fd = fd;
			fds[0].events = POLLOUT | POLLHUP;

			rc = poll(fds, 1, timeout);
			if (rc < 0 && errno != EINTR)
			{
				ret = -1;
				goto done;
			}

			if (rc > 0)
			{
				n = xnet_writev_nonblock(fd, *iov, *count);
				if (n < 0)
				{
					ret = n;
					goto done;
				}

				if (n > 0)
				{
					*count = xnet_adjust_iovec(iov, *count, n);
					if (*count == 0)
						goto done;
				}
			}

			if (timeout > 0)
			{
				uint64_t now = exact_mono_msec();
				timeout -= (now - start);
				if (timeout <= 0)
				{
					*timeout_ms = 0;
					return -3;
				}
				start = now;
			}
		}
	done:
		if (timeout > 0)
		{
			uint64_t now = exact_mono_msec();
			timeout -= (now - start);
			*timeout_ms = timeout > 0 ? timeout : 0;
		}
	}

	return ret;
}

ssize_t xnet_recv_nonblock(int fd, void *buf, size_t len, int flags)
{
	ssize_t n;
again:
	n = recv(fd, buf, len, flags);
	if (n == -1)
	{
		switch (errno)
		{
		case EINTR: goto again; break;
		case EAGAIN: n = 0; break;
		}
	}
	else if (n == 0)
		return -2;
	return n;
}

ssize_t xnet_recvfrom_nonblock(int fd, void *buf, size_t len, int flags, struct sockaddr *addr, socklen_t *addrlen)
{
	ssize_t n;
again:
	n = recvfrom(fd, buf, len, flags, addr, addrlen);
	if (n == -1)
	{
		switch (errno)
		{
		case EINTR: goto again; break;
		case EAGAIN: n = 0; break;
		}
	}
	else if (n == 0)
		return -2;
	return n;
}

ssize_t xnet_read_nonblock(int fd, void *buf, size_t len)
{
	ssize_t n;
again:
	n = read(fd, buf, len);
	if (n == -1)
	{
		switch (errno)
		{
		case EINTR: goto again; break;
		case EAGAIN: n = 0; break;
		}
	}
	else if (n == 0)
		return -2;
	return n;
}

ssize_t xnet_readv_nonblock(int fd, const struct iovec *vec, int count)
{
	ssize_t n;
again:
	n = readv(fd, vec, count);
	if (n == -1)
	{
		switch (errno)
		{
		case EINTR: goto again; break;
		case EAGAIN: n = 0; break;
		case EPIPE: return -2; break;
		}
	}
	return n;
}

ssize_t xnet_peek_nonblock(int fd, void *buf, size_t len)
{
	ssize_t n;
again:
	n = recv(fd, buf, len, MSG_PEEK | MSG_DONTWAIT); 
	if (n == -1)
	{
		switch (errno)
		{
		case EINTR: goto again; break;
		case EAGAIN: n = 0; break;
		}
	}
	else if (n == 0)
		return -2;
	return n;
}

ssize_t xnet_send_nonblock(int fd, const void *buf, size_t len, int flags)
{
	ssize_t n;
again:
	n = send(fd, buf, len, flags);
	if (n == -1)
	{
		switch (errno)
		{
		case EINTR: goto again; break;
		case EAGAIN: n = 0; break;
		case EPIPE: return -2; break;
		}
	}
	return n;
}

ssize_t xnet_sendto_nonblock(int fd, const void *buf, size_t len, int flags, const struct sockaddr *addr, socklen_t addrlen)
{
	ssize_t n;
again:
	n = sendto(fd, buf, len, flags, addr, addrlen);
	if (n == -1)
	{
		switch (errno)
		{
		case EINTR: goto again; break;
		case EAGAIN: n = 0; break;
		case EPIPE: return -2; break;
		}
	}
	return n;
}

ssize_t xnet_write_nonblock(int fd, const void *buf, size_t len)
{
	ssize_t n;
again:
	n = write(fd, buf, len);
	if (n == -1)
	{
		switch (errno)
		{
		case EINTR: goto again; break;
		case EAGAIN: n = 0; break;
		case EPIPE: return -2; break;
		}
	}
	return n;
}


ssize_t xnet_writev_nonblock(int fd, const struct iovec *vec, int count)
{
	ssize_t n;
again:
	n = writev(fd, vec, count);
	if (n == -1)
	{
		switch (errno)
		{
		case EINTR: goto again; break;
		case EAGAIN: n = 0; break;
		case EPIPE: return -2; break;
		}
	}
	return n;
}

int xnet_adjust_iovec(struct iovec **piov, int iov_count, size_t writen)
{
	if ((ssize_t)writen > 0)
	{
		ssize_t n = 0;
		int i;

		for (i = 0; i < iov_count; ++i)
		{
			ssize_t k = n + (*piov)[i].iov_len;
			if (k > (ssize_t)writen)
				break;
			n = k;
		}

		*piov += i;
		iov_count -= i;
		if (iov_count)
		{
			ssize_t k = writen - n;
			(*piov)->iov_base = (char *)(*piov)->iov_base + k;
			(*piov)->iov_len -= k;
		}
	}
	return iov_count;
}

int xnet_get_so_error(int fd)
{
	int so_error = 0;
	socklen_t len = sizeof(so_error);
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len) == -1)
		so_error = errno;
	return so_error;
}

int xnet_set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int xnet_set_block(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		return -1;
	return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
}

int xnet_set_tcp_nodelay(int fd)
{
	int flag = 1;
	return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
}

int xnet_set_keepalive(int fd)
{
	int flag = 1;
	return setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
}

int xnet_set_send_buffer_size(int fd, int size)
{
	return setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
}

int xnet_get_send_buffer_size(int fd)
{
	int size;
	socklen_t len = sizeof(size);
	if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, &len) == 0 && (size_t)len == sizeof(size))
		return size;
	return -1;
}

int xnet_set_recv_buffer_size(int fd, int size)
{
	return setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
}

int xnet_get_recv_buffer_size(int fd)
{
	int size;
	socklen_t len = sizeof(size);
	if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, &len) == 0 && (size_t)len == sizeof(size))
		return size;
	return -1;
}

int xnet_set_reuse_address(int fd)
{
	int flag = 1;
	return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
}

int xnet_clear_reuse_address(int fd)
{
	int flag = 0;
	return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
}

int xnet_set_linger_on(int fd, int seconds)
{
	struct linger lv;
	lv.l_onoff = 1;
	lv.l_linger = seconds;
	return setsockopt(fd, SOL_SOCKET, SO_LINGER, &lv, sizeof(lv));
}

int xnet_set_linger_off(int fd)
{
	struct linger lv;
	lv.l_onoff = 0;
	lv.l_linger = 0;
	return setsockopt(fd, SOL_SOCKET, SO_LINGER, &lv, sizeof(lv));
}

int xnet_set_close_on_exec(int fd)
{
	int flags = fcntl(fd, F_GETFD, 0);
	if (flags == -1)
		return -1;
	return fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

int xnet_clear_close_on_exec(int fd)
{
	int flags = fcntl(fd, F_GETFD, 0);
	if (flags == -1)
		return -1;
	return fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC);
}

uint16_t xnet_get_sock_ipv4(int sock, uint32_t *ip)
{
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);

	if (getsockname(sock, (struct sockaddr *)&addr, &addr_len) == 0 && addr.sin_family == AF_INET)
	{
		*ip = ntohl(addr.sin_addr.s_addr);
		return ntohs(addr.sin_port);
	}
	*ip = htonl(INADDR_ANY);
	return 0;
}

uint16_t xnet_get_peer_ipv4(int sock, uint32_t *ip)
{
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);

	if (getpeername(sock, (struct sockaddr *)&addr, &addr_len) == 0 && addr.sin_family == AF_INET)
	{
		*ip = ntohl(addr.sin_addr.s_addr);
		return ntohs(addr.sin_port);
	}
	*ip = htonl(INADDR_ANY);
	return 0;
}

uint16_t xnet_get_sock_ipv6(int sock, uint8_t ip[16])
{
	struct sockaddr_in6 addr;
	socklen_t addr_len = sizeof(addr);

	if (getsockname(sock, (struct sockaddr *)&addr, &addr_len) == 0 && addr.sin6_family == AF_INET6)
	{
		memcpy(ip, addr.sin6_addr.s6_addr, 16);
		return ntohs(addr.sin6_port);
	}
	memcpy(ip, in6addr_any.s6_addr, 16);
	return 0;
}

uint16_t xnet_get_peer_ipv6(int sock, uint8_t ip[16])
{
	struct sockaddr_in6 addr;
	socklen_t addr_len = sizeof(addr);

	if (getpeername(sock, (struct sockaddr *)&addr, &addr_len) == 0 && addr.sin6_family == AF_INET6)
	{
		memcpy(ip, addr.sin6_addr.s6_addr, 16);
		return ntohs(addr.sin6_port);
	}
	memcpy(ip, in6addr_any.s6_addr, 16);
	return 0;
}

uint16_t xnet_get_sock_ip_port(int sock, char ip[XNET_IP6STR_SIZE])
{
	xnet_inet_sockaddr_t addr;
	socklen_t addr_len = sizeof(addr);

	if (getsockname(sock, (struct sockaddr *)&addr, &addr_len) == 0)
	{
		return xnet_sockaddr_to_ip((struct sockaddr *)&addr, ip, XNET_IP6STR_SIZE);
	}
	ip[0] = 0;
	return 0;
}

uint16_t xnet_get_peer_ip_port(int sock, char ip[XNET_IP6STR_SIZE])
{
	xnet_inet_sockaddr_t addr;
	socklen_t addr_len = sizeof(addr);

	if (getpeername(sock, (struct sockaddr *)&addr, &addr_len) == 0)
	{
		return xnet_sockaddr_to_ip((struct sockaddr *)&addr, ip, XNET_IP6STR_SIZE);
	}
	ip[0] = 0;
	return 0;
}


int xnet_get_endian()
{
	static int i = 0x01020304;
	static char *p = (char *)&i;
	if (p[0] == 0x01)
		return 1;
	return 0;
}

/* XXX: this has only tested on Linux */
int xnet_get_mac(unsigned char macs[][6], int num)
{
	struct ifconf ifc;
	struct ifreq *ifr, ifrcopy;
	int sock = -1;
	int len = 32 * sizeof(struct ifreq);
	int lastlen = 0;
	char *buf = NULL, *ptr;
	char lastname[IFNAMSIZ];
	bool error = true;
	int k = 0;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		goto done;

	buf = (char *)malloc(len);
	while (1)
	{
		ifc.ifc_len = len;
		ifc.ifc_buf = buf;
		if (ioctl(sock, SIOCGIFCONF, &ifc) < 0)
		{
			if (errno != EINVAL || lastlen != 0)
			{
				goto done;
			}
		}
		else
		{
			if (ifc.ifc_len == lastlen)
				break;
			lastlen = ifc.ifc_len;

			if (len == ifc.ifc_len)
			{
				len += (len < 4096) ? len : 32 * sizeof(struct ifreq);
				buf = (char *)realloc(buf, len);
			}
		}
	}

	lastname[0] = 0;
	for (ptr = buf; ptr < buf + ifc.ifc_len; ptr += sizeof(struct ifreq))
	{
		char *cptr;

		ifr = (struct ifreq *)ptr;

		if ((cptr = strchr(ifr->ifr_name, ':')) != NULL)
			*cptr = '\0';
		if (strncmp(lastname, ifr->ifr_name, IFNAMSIZ) == 0)
			continue;
		memcpy(lastname, ifr->ifr_name, IFNAMSIZ);

		ifrcopy = *ifr;
		if (ioctl(sock, SIOCGIFHWADDR, &ifrcopy) != 0)
			continue;
		if (ifrcopy.ifr_hwaddr.sa_family != ARPHRD_ETHER)
			continue;

		if (k < num)
			memcpy(macs[k], ifrcopy.ifr_hwaddr.sa_data, 6);
		++k;
	}
	error = false;
done:
	free(buf);
	if (sock >= 0)
		close(sock);
	return error ? -1 : k;
}

int xnet_ipv4_get_all(uint32_t ips[], int num)
{
	return xnet_get_all_ips(NULL, NULL, ips, &num);
}

/* XXX: This may only work on Linux */
int xnet_get_all_ips(uint8_t ipv6s[][16], int *v6num/*NULL*/, uint32_t ipv4s[], int *v4num/*NULL*/)
{
	struct ifaddrs *ifaddr, *ifa;
	int k4 = 0, k6 = 0;

	if (getifaddrs(&ifaddr) == -1)
		return -1;

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
	{
		struct sockaddr *addr = ifa->ifa_addr;
		if (addr == NULL)
			continue;
		if ((ifa->ifa_flags & IFF_UP) == 0)
			continue;

		if (v6num && addr->sa_family == AF_INET6)
		{
			struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)addr;
			uint8_t *bs = in6->sin6_addr.s6_addr;
			if (xnet_ipv6_is_loopback(bs) || xnet_ipv6_is_linklocal(bs) || xnet_ipv6_is_sitelocal(bs))
				continue;

			if (k6 < *v6num)
			{
				memcpy(ipv6s[k6], &in6->sin6_addr, 16);
			}
			++k6;
		}
		else if (v4num && addr->sa_family == AF_INET)
		{
			struct sockaddr_in *in4 = (struct sockaddr_in *)addr;
			uint8_t *bs = (uint8_t *)&in4->sin_addr.s_addr;
			if (bs[0] == 127)
				continue;

			if (k4 < *v4num)
			{
				ipv4s[k4] = ntohl(in4->sin_addr.s_addr);
			}
			++k4;
		}
	}
	freeifaddrs(ifaddr);

	if (v6num && *v6num > k6)
		*v6num = k6;
	if (v4num && *v4num > k4)
		*v4num = k4;

	return k6 + k4;
}

int xnet_ipv4_ntoa(uint32_t ip, char str[])
{
	int b0 = (ip>>24);
	int b1 = (ip>>16) & 0xFF;
	int b2 = (ip>>8) & 0xFF;
	int b3 = ip & 0xFF;
	return sprintf(str, "%d.%d.%d.%d", b0, b1, b2, b3);
}

bool xnet_ipv4_aton(const char *str, uint32_t *ip)
{
	struct in_addr addr;

	if (inet_aton(str, &addr))
	{
		*ip = ntohl(addr.s_addr);
		return true;
	}

	*ip = 0;
	return false;
}

int xnet_ipv6_ntoa(const uint8_t ip[16], char str[])
{
	if (inet_ntop(AF_INET6, ip, str, XNET_IP6STR_SIZE))
	{
		return strlen(str);
	}
	return 0;
}

bool xnet_ipv6_aton(const char *str, uint8_t ip[XNET_IP6BIN_SIZE])
{
	struct in6_addr addr;

	if (inet_pton(AF_INET6, str, &addr) > 0)
	{
		memcpy(ip, addr.s6_addr, sizeof(addr));
		return true;
	}

	memset(ip, 0, XNET_IP6BIN_SIZE);
	return false;
}

bool xnet_ip46_aton(const char *str, uint8_t ip[XNET_IP6BIN_SIZE])
{
	struct in6_addr a6;
	struct in_addr a4;

	if (inet_pton(AF_INET6, str, &a6) > 0)
	{
		memcpy(ip, a6.s6_addr, sizeof(a6));
		return true;
	}
	else if (inet_pton(AF_INET, str, &a4) > 0)
	{
		memset(ip, 0, 10);
		ip[10] = 0xFF;
		ip[11] = 0xFF;
		memcpy(&ip[12], &a4.s_addr, sizeof(a4));
		return true;
	}

	memset(ip, 0, XNET_IP6BIN_SIZE);
	return false;
}

inline bool xnet_ipv4_is_loopback(uint32_t ip)
{
	return ((ip >> 24) == 127);
}

inline bool xnet_ipv4_is_internal(uint32_t ip)
{
	int b0, b1;

	b0 = (ip >> 24);
	if (b0 == 10)
		return true;

	b1 = (ip >> 16) & 0xFF;
	if (b0 == 172)
	{
		if (b1 >= 16 && b1 < 32)
			return true;
	}
	else if (b0 == 192)
	{
		if (b1 == 168)
			return true;
	}
	return false;
}

bool xnet_ipv4_is_external(uint32_t ip)
{
	return !xnet_ipv4_is_loopback(ip) && !xnet_ipv4_is_internal(ip);
}

bool xnet_ipv6_is_loopback(const uint8_t ip[XNET_IP6BIN_SIZE])
{
	static uint8_t ipv4_loopback[] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0x7F, 
	};

	return (memcmp(ip, &in6addr_loopback, XNET_IP6BIN_SIZE) == 0)
		|| (memcmp(ip, ipv4_loopback, sizeof(ipv4_loopback)) == 0);
}

bool xnet_ipv6_is_linklocal(const uint8_t ip[XNET_IP6BIN_SIZE])
{
	return (ip[0] == 0xfe && (ip[1] & 0xc0) == 0x80);
}

bool xnet_ipv6_is_sitelocal(const uint8_t ip[XNET_IP6BIN_SIZE])
{
	return (ip[0] == 0xfe && (ip[1] & 0xc0) == 0xc0);
}

bool xnet_ipv6_is_uniquelocal(const uint8_t ip[XNET_IP6BIN_SIZE])
{
	return ((ip[0] & 0xfe) == 0xfc);
}

bool xnet_ipv6_is_ipv4(const uint8_t ip[XNET_IP6BIN_SIZE])
{
	if (memcmp(ip, "\0\0\0\0\0\0\0\0\0\0", 10) == 0)
	{
		if (ip[10] == 0xff && ip[11] == 0xff)
			return true;

		if (ip[10] == 0 && ip[11] == 0)
		{
			if (ip[15] != 1)
				return true;
		}
	}
	return false;
}

bool xnet_is_loopback_sockaddr(const struct sockaddr *addr, socklen_t addrlen)
{
	xnet_inet_sockaddr_t *a = (xnet_inet_sockaddr_t *)addr;
	if (a->family == AF_INET6)
		return xnet_ipv6_is_loopback(a->a6.sin6_addr.s6_addr);
	else if (a->family == AF_INET)
		return xnet_ipv4_is_loopback(ntohl(a->a4.sin_addr.s_addr));
	return false;
}

bool xnet_is_loopback_ip(const char *ip)
{
	if (strchr(ip, ':'))
	{
		uint8_t ipv6[XNET_IP6BIN_SIZE];
		return (xnet_ipv6_aton(ip, ipv6) && xnet_ipv6_is_loopback(ipv6));
	}
	else
	{
		uint32_t ipv4;
		return (xnet_ipv4_aton(ip, &ipv4) && xnet_ipv4_is_loopback(ipv4));
	}
}

bool xnet_is_internal_ip(const char *ip)
{
	uint32_t ipv4;
	return (xnet_ipv4_aton(ip, &ipv4) && xnet_ipv4_is_internal(ipv4));
}

bool xnet_is_external_ip(const char *ip)
{
	uint32_t ipv4;
	return (xnet_ipv4_aton(ip, &ipv4) && xnet_ipv4_is_external(ipv4));
}

int xnet_get_internal_ip(char ip[])
{
	uint32_t ips[256];
	int n = xnet_ipv4_get_all(ips, 256);
	if (n > 0)
	{
		int i;
		int num = (n < 256) ? n : 256;
		for (i = 0; i < num; ++i)
		{
			if (xnet_ipv4_is_internal(ips[i]))
				return xnet_ipv4_ntoa(ips[i], ip);
		}
	}

	ip[0] = 0;
	return 0;
}

int xnet_get_external_ip(char ip[])
{
	uint32_t ips[256];
	int n = xnet_ipv4_get_all(ips, 256);
	if (n > 0)
	{
		int i;
		int num = (n < 256) ? n : 256;
		for (i = 0; i < num; ++i)
		{
			if (!xnet_ipv4_is_internal(ips[i]))
				return xnet_ipv4_ntoa(ips[i], ip);
		}
	}

	ip[0] = 0;
	return 0;
}

bool xnet_ip_equal(const char *ip1, const char *ip2)
{
	char *p1 = strchr(ip1, ':');
	char *p2 = strchr(ip2, ':');
	if (!p1 && !p2)
	{
		return (strcmp(ip1, ip2) == 0);
	}
	else
	{
		uint8_t a[16], b[16];
		xnet_ip46_aton(ip1, a);
		xnet_ip46_aton(ip2, b);
		return (memcmp(a, b, sizeof(a)) == 0);
	}
}


inline void xnet_swap(void *p, size_t n)
{
	unsigned int c;
	unsigned char *s = (unsigned char *)p;
	unsigned char *d = (unsigned char *)p + n;

	n /= 2;
	if (n <= 16)
	{
		switch (n)
		{
		case 16: c = *--d; *d = *s; *s++ = c;
		case 15: c = *--d; *d = *s; *s++ = c;
		case 14: c = *--d; *d = *s; *s++ = c;
		case 13: c = *--d; *d = *s; *s++ = c;
		case 12: c = *--d; *d = *s; *s++ = c;
		case 11: c = *--d; *d = *s; *s++ = c;
		case 10: c = *--d; *d = *s; *s++ = c;
		case 9: c = *--d; *d = *s; *s++ = c;
		case 8: c = *--d; *d = *s; *s++ = c;
		case 7: c = *--d; *d = *s; *s++ = c;
		case 6: c = *--d; *d = *s; *s++ = c;
		case 5: c = *--d; *d = *s; *s++ = c;
		case 4: c = *--d; *d = *s; *s++ = c;
		case 3: c = *--d; *d = *s; *s++ = c;
		case 2: c = *--d; *d = *s; *s++ = c;
		case 1: c = *--d; *d = *s; *s++ = c;
		}
	}
	else
	{
		while (n--)
		{
			c = *--d; *d = *s; *s++ = c;
		}
	}
}

void (xnet_lsb)(void *p, size_t n)
{
#if __BYTE_ORDER == __BIG_ENDIAN
	xnet_swap(p, n);
#endif
}

void (xnet_msb)(void *p, size_t n)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	xnet_swap(p, n);
#endif
}


#define SWAP2(p)	do {					\
	unsigned int _c___;					\
	char *_s___ = (char *)(p), *_d___ = (char*)(p) + 2;	\
	_c___ = *--_d___; *_d___ = *_s___; *_s___++ = _c___;	\
} while (0)

#define SWAP4(p)	do {					\
	unsigned int _c___;					\
	char *_s___ = (char *)(p), *_d___ = (char*)(p) + 4;	\
	_c___ = *--_d___; *_d___ = *_s___; *_s___++ = _c___;	\
	_c___ = *--_d___; *_d___ = *_s___; *_s___++ = _c___;	\
} while (0)

#define SWAP8(p)	do {					\
	unsigned int _c___;					\
	char *_s___ = (char *)(p), *_d___ = (char*)(p) + 8;	\
	_c___ = *--_d___; *_d___ = *_s___; *_s___++ = _c___;	\
	_c___ = *--_d___; *_d___ = *_s___; *_s___++ = _c___;	\
	_c___ = *--_d___; *_d___ = *_s___; *_s___++ = _c___;	\
	_c___ = *--_d___; *_d___ = *_s___; *_s___++ = _c___;	\
} while (0)


void (xnet_lsb16)(uint16_t *p)
{
#if __BYTE_ORDER == __BIG_ENDIAN
	SWAP2(p);
#endif
}

void (xnet_lsb32)(uint32_t *p)
{
#if __BYTE_ORDER == __BIG_ENDIAN
	SWAP4(p);
#endif
}

void (xnet_lsb64)(uint64_t *p)
{
#if __BYTE_ORDER == __BIG_ENDIAN
	SWAP8(p);
#endif
}

void (xnet_msb16)(uint16_t *p)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	SWAP2(p);
#endif
}

void (xnet_msb32)(uint32_t *p)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	SWAP4(p);
#endif
}

void (xnet_msb64)(uint64_t *p)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	SWAP8(p);
#endif
}

uint16_t (xnet_l16)(uint16_t v)
{
#if __BYTE_ORDER == __BIG_ENDIAN
	SWAP2(&v);
#endif
	return v;
}

uint32_t (xnet_l32)(uint32_t v)
{
#if __BYTE_ORDER == __BIG_ENDIAN
	SWAP4(&v);
#endif
	return v;
}

uint64_t (xnet_l64)(uint64_t v)
{
#if __BYTE_ORDER == __BIG_ENDIAN
	SWAP8(&v);
#endif
	return v;
}

uint16_t (xnet_m16)(uint16_t v)
{
#if __BYTE_ORDER == __LITTILE_ENDIAN
	SWAP2(&v);
#endif
	return v;
}

uint32_t (xnet_m32)(uint32_t v)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	SWAP4(&v);
#endif
	return v;
}

uint64_t (xnet_m64)(uint64_t v)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	SWAP8(&v);
#endif
	return v;
}

void *xnet_copy_lsb(void *dst, const void *src, size_t n)
{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint8_t *d = dst;
	const uint8_t *s = dst + n;
	while (n > 0)
		*d++ = *--s;
	return d;
#else
	memcpy(dst, src, n);
	return dst + n;
#endif
}

void *xnet_copy_msb(void *dst, const void *src, size_t n)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	uint8_t *d = dst;
	const uint8_t *s = dst + n;
	while (n > 0)
		*d++ = *--s;
	return d;
#else
	memcpy(dst, src, n);
	return dst + n;
#endif
}

static inline uint8_t *swap_2(uint8_t *dst, const uint8_t *src)
{
	src += 2;
	*dst++ = *--src;
	*dst++ = *--src;
	return dst;
}

static inline uint8_t *swap_4(uint8_t *dst, const uint8_t *src)
{
	src += 4;
	*dst++ = *--src;
	*dst++ = *--src;
	*dst++ = *--src;
	*dst++ = *--src;
	return dst;
}

static inline uint8_t *swap_8(uint8_t *dst, const uint8_t *src)
{
	src += 8;
	*dst++ = *--src;
	*dst++ = *--src;
	*dst++ = *--src;
	*dst++ = *--src;
	*dst++ = *--src;
	*dst++ = *--src;
	*dst++ = *--src;
	*dst++ = *--src;
	return dst;
}

static inline uint8_t *copy_2(uint8_t *dst, const uint8_t *src)
{
	*dst++ = *src++;
	*dst++ = *src++;
	return dst;
}

static inline uint8_t *copy_4(uint8_t *dst, const uint8_t *src)
{
	*dst++ = *src++;
	*dst++ = *src++;
	*dst++ = *src++;
	*dst++ = *src++;
	return dst;
}

static inline uint8_t *copy_8(uint8_t *dst, const uint8_t *src)
{
	*dst++ = *src++;
	*dst++ = *src++;
	*dst++ = *src++;
	*dst++ = *src++;
	*dst++ = *src++;
	*dst++ = *src++;
	*dst++ = *src++;
	*dst++ = *src++;
	return dst;
}

void *xnet_copy_2lsb(void *dst, const void *src)
{
#if __BYTE_ORDER == __BIG_ENDIAN
	return swap_2(dst, src);
#else
	return copy_2(dst, src);
#endif
}

void *xnet_copy_4lsb(void *dst, const void *src)
{
#if __BYTE_ORDER == __BIG_ENDIAN
	return swap_4(dst, src);
#else
	return copy_4(dst, src);
#endif
}

void *xnet_copy_8lsb(void *dst, const void *src)
{
#if __BYTE_ORDER == __BIG_ENDIAN
	return swap_8(dst, src);
#else
	return copy_8(dst, src);
#endif
}

void *xnet_copy_2msb(void *dst, const void *src)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return swap_2(dst, src);
#else
	return copy_2(dst, src);
#endif
}

void *xnet_copy_4msb(void *dst, const void *src)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return swap_4(dst, src);
#else
	return copy_4(dst, src);
#endif
}

void *xnet_copy_8msb(void *dst, const void *src)
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	return swap_8(dst, src);
#else
	return copy_8(dst, src);
#endif
}

uintmax_t xnet_uint_from_msb(const void *data, size_t len)
{
	size_t i;
	uintmax_t n;
	assert(len <= sizeof(uintmax_t));

	n = ((uint8_t*)data)[0];
	for (i = 1; i < len; ++i)
	{
		n <<= 8;
		n += ((uint8_t*)data)[i];
	}
	return n;
}

uintmax_t xnet_uint_from_lsb(const void *data, size_t len)
{
	size_t i;
	uintmax_t n;
	assert(len <= sizeof(uintmax_t));

	n = ((uint8_t*)data)[len-1];
	for (i = len - 1; i > 0; )
	{
		--i;
		n <<= 8;
		n += ((uint8_t*)data)[i];
	}
	return n;
}

ssize_t xnet_msb_from_uint(void *data, size_t size, uintmax_t n)
{
	uint8_t *end = (uint8_t *)data + size;
	uint8_t *ptr = end;
	ssize_t len;

	while (n)
	{
		--ptr;
		if (ptr >= (uint8_t *)data)
			*ptr = n;
		n >>= 8;
	}

	len = end - ptr;

	while (ptr > (uint8_t*)data)
		*--ptr = 0;

	return (len <= size) ? len : -len;
}

ssize_t xnet_lsb_from_uint(void *data, size_t size, uintmax_t n)
{
	uint8_t *ptr = (uint8_t *)data;
	uint8_t *end = ptr + size;
	ssize_t len;

	while (n)
	{
		if (ptr < end)
			*ptr = n;
		n >>= 8;
		++ptr;
	}

	len = ptr - (uint8_t*)data;

	while (ptr < end)
		*ptr++ = 0;

	return (len <= size) ? len : -len;
}

