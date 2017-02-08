#ifndef IpMatcher_h_
#define IpMatcher_h_

#include "xslib/XRefCount.h"
#include "xslib/xstr.h"
#include "xslib/XLock.h"
#include "xslib/xio.h"
#include <stdint.h>
#include <vector>

class IpMatcher;
typedef XPtr<IpMatcher> IpMatcherPtr;


class IpMatcher: virtual public XRefCount, private XMutex
{
public:
	struct Ip6Address {
		uint8_t dat[16];
		uint8_t prefix;

		bool isIpv4() const;
		bool contain(const Ip6Address& ip) const;
		bool containOrEqual(const Ip6Address& ip) const;
	};

	typedef std::vector<Ip6Address> Ip6Vector;

public:
	IpMatcher(const std::string& ips);
	virtual ~IpMatcher();

	void reset(const xstr_t& ips);
	void dump(xio_write_function write, void *cookie);

	bool add(const char* ip);
	bool add(const xstr_t& ip);

	bool remove(const char* ip);
	bool remove(const xstr_t& ip);

	bool match(const char* ip);
	bool match(const xstr_t& ip);
	bool emptyOrMatch(const struct sockaddr *addr);

	bool empty() const;

private:
	Ip6Vector _ips;
};


#endif
