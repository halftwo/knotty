#include "IpMatcher.h"
#include "xslib/bit.h"
#include "xslib/xnet.h"
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>


IpMatcher::IpMatcher(const std::string& ips)
{
	xstr_t xs = XSTR_CXX(ips);
	reset(xs);
}

IpMatcher::~IpMatcher()
{
}

static bool lessthan(const IpMatcher::Ip6Address& a, const IpMatcher::Ip6Address& b)
{
	int rc = memcmp(a.dat, b.dat, sizeof(a.dat));
	if (rc == 0)
		return a.prefix < b.prefix;
	return rc < 0;
}

static bool _aton(const xstr_t& xs, IpMatcher::Ip6Address& ip6)
{
	char buf[64];
	size_t len = xs.len;
	if (len >= sizeof(buf))
		len = sizeof(buf) - 1;
	memcpy(buf, xs.data, len);
	buf[len] = 0;

	int prefix = -1;
	char *p = (char *)memrchr(buf, '/', len);
	if (p)
	{
		*p = 0;
		char *end;
		prefix = strtol(p+1, &end, 10);
		if (prefix <= 0 || prefix > 128 || *end)
			return false;
	}

	if (!xnet_ip46_aton(buf, ip6.dat))
		return false;

	if (ip6.isIpv4())
	{
		if (prefix > 32)
			return false;
	
		if (prefix > 0)
			prefix += 96;
	}

	if (prefix < 0)
		prefix = 128;

	int zero_bits = 128 - prefix;
	if (zero_bits)
	{
		int bytes = zero_bits / 8;
		int bits = zero_bits % 8;
		if (bytes)
			memset(ip6.dat + 16 - bytes, 0, bytes);
		if (bits)
			ip6.dat[16 - 1 - bytes] &= (0xFF << bits);
	}

	ip6.prefix = prefix;
	return true;
}

static bool _add(IpMatcher::Ip6Vector& ips, const IpMatcher::Ip6Address& ip6)
{
	if (ips.empty())
	{
		ips.insert(ips.end(), ip6);
	}
	else
	{
		IpMatcher::Ip6Vector::iterator iter = std::upper_bound(ips.begin(), ips.end(), ip6, lessthan);
		if (iter != ips.begin())
		{
			if ((iter-1)->containOrEqual(ip6))
				return false;
		}

		iter = ips.insert(iter, ip6);
		++iter;
		IpMatcher::Ip6Vector::iterator start = iter;
		for (; iter != ips.end(); ++iter)
		{
			if (!ip6.containOrEqual(*iter))
				break;
		}

		if (iter != start)
		{
			ips.erase(start, iter);
		}
	}
	return true;
}

void IpMatcher::reset(const xstr_t& xs)
{
	xstr_t tmp = xs;
	xstr_t ip;

	Lock lock(*this);
	_ips.clear();
	while (xstr_token_cstr(&tmp, " \t,", &ip))
	{
		Ip6Address ip6;
		if (!_aton(ip, ip6))
			continue;
		_add(_ips, ip6);
	}
}

bool IpMatcher::Ip6Address::isIpv4() const
{
	return (memcmp(dat, "\0\0\0\0\0\0\0\0\0\0\xff\xff", 12) == 0);
}

bool IpMatcher::Ip6Address::containOrEqual(const IpMatcher::Ip6Address& b) const
{
	int prefix = this->prefix < b.prefix ? this->prefix : b.prefix;
	if (bitmap_msb_equal(this->dat, b.dat, prefix))
	{
		return (this->prefix <= b.prefix);
	}
	return false;
}

bool IpMatcher::Ip6Address::contain(const IpMatcher::Ip6Address& b) const
{
	int prefix = this->prefix < b.prefix ? this->prefix : b.prefix;
	if (bitmap_msb_equal(this->dat, b.dat, prefix))
	{
		return (this->prefix < b.prefix);
	}
	return false;
}

bool IpMatcher::empty() const
{
	Lock lock(*this);
	return _ips.empty();
}

bool IpMatcher::add(const xstr_t& xs)
{
	Ip6Address ip6;
	if (!_aton(xs, ip6))
		return false;

	Lock lock(*this);
	return _add(_ips, ip6);
}

bool IpMatcher::add(const char* str)
{
	xstr_t xs = XSTR_C(str);
	return add(xs);
}

bool IpMatcher::remove(const xstr_t& xs)
{
	Ip6Address ip6;
	if (!_aton(xs, ip6))
		return false;

	Lock lock(*this);
	if (_ips.empty())
		return false;

	Ip6Vector::iterator iter = std::lower_bound(_ips.begin(), _ips.end(), ip6, lessthan);
	if (iter != _ips.begin())
	{
		if ((iter-1)->contain(ip6))
			return false;
	}

	Ip6Vector::iterator start = iter;
	for (; iter != _ips.end(); ++iter)
	{
		if (!ip6.containOrEqual(*iter))
			break;
	}

	_ips.erase(start, iter);
	return true;
}

bool IpMatcher::remove(const char* str)
{
	xstr_t xs = XSTR_C(str);
	return remove(xs);
}

static bool _match(const IpMatcher::Ip6Vector& ips, const IpMatcher::Ip6Address& ip6)
{
	if (ips.empty())
		return false;

	IpMatcher::Ip6Vector::const_iterator iter = std::upper_bound(ips.begin(), ips.end(), ip6, lessthan);
	if (iter != ips.begin())
	{
		if ((iter-1)->containOrEqual(ip6))
			return true;
	}

	return false;
}

bool IpMatcher::match(const char* str)
{
	xstr_t xs = XSTR_C(str);
	return match(xs);
}

bool IpMatcher::match(const xstr_t& xs)
{
	Ip6Address ip6;
	if (!_aton(xs, ip6))
		return false;

	Lock lock(*this);
	return _match(_ips, ip6);
}

bool IpMatcher::emptyOrMatch(const struct sockaddr *addr)
{
	Ip6Address ip6;

	ip6.prefix = 128;
	if (addr->sa_family == AF_INET6)
	{
		struct sockaddr_in6 *a6 = (struct sockaddr_in6 *)addr;
		memcpy(ip6.dat, &a6->sin6_addr, sizeof(a6->sin6_addr));
	}
	else if (addr->sa_family == AF_INET)
	{
		struct sockaddr_in *a4 = (struct sockaddr_in *)addr;
		memset(ip6.dat, 0, 10);
		ip6.dat[10] = 0xFF;
		ip6.dat[11] = 0xFF;
		memcpy(ip6.dat + 12, &a4->sin_addr, sizeof(a4->sin_addr));
	}
	else
	{
		return false;
	}

	Lock lock(*this);
	return _ips.empty() || _match(_ips, ip6);
}

void IpMatcher::dump(xio_write_function write, void *cookie)
{
	bool first = true;

	Lock lock(*this);
	for (Ip6Vector::iterator iter = _ips.begin(); iter != _ips.end(); ++iter)
	{
		char buf[64];
		int len = 0;
		const Ip6Address& ip6 = *iter;
		if (ip6.isIpv4())
		{
			len = xnet_ipv4_ntoa(xnet_uint_from_msb(ip6.dat + 12, 4), buf);
			if (ip6.prefix != 128)
				len += sprintf(buf + len, "/%d", ip6.prefix - 96);
		}
		else
		{
			len = xnet_ipv6_ntoa(ip6.dat, buf);
			if (ip6.prefix != 128)
				len += sprintf(buf + len, "/%d", ip6.prefix);
		}

		if (first)
			first = false;
		else
			write(cookie, " ", 1);
		write(cookie, buf, len);
	}
}


#ifdef TEST_IPMATCHER

#include <stdio.h>

static void add(const IpMatcherPtr& matcher, const char *ip)
{
	bool ok = matcher->add(ip);
	printf("%c add %s\n", ok?'+':'!', ip);
	if (ok)
	{
		matcher->dump(stdio_xio.write, stdout);
		printf("\n");
	}
}

static void remove(const IpMatcherPtr& matcher, const char *ip)
{
	bool ok = matcher->remove(ip);
	printf("%c remove %s\n", ok?'+':'!', ip);
	if (ok)
	{
		matcher->dump(stdio_xio.write, stdout);
		printf("\n");
	}
}

static void match(const IpMatcherPtr& matcher, const char *ip)
{
	bool ok = matcher->match(ip);
	printf("%c match %s\n", ok?'+':'!', ip);
}


int main()
{
	const char *ips = "127.0.0.1 ::1 192.168.1.0/24 172.16.1.1/16";
	IpMatcherPtr matcher = new IpMatcher(ips);
	add(matcher, "192.168.1.0/28");
	add(matcher, "172.16.1.1/12");
	remove(matcher, "172.16.1.1/10");
	match(matcher, "192.168.1.23");
	match(matcher, "192.168.2.23");
	return 0;
}

#endif
