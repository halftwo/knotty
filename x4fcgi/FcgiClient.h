#ifndef FcgiClient_h_
#define FcgiClient_h_

#include "FcgiQuest.h"
#include "xslib/XRefCount.h"
#include "xslib/XLock.h"
#include "xslib/XEvent.h"
#include "xslib/ostk.h"
#include "xslib/ostk_allocator.h"
#include "xic/XicMessage.h"
#include <limits.h>
#include <string>
#include <vector>
#include <set>
#include <deque>

class FcgiConnection;
typedef XPtr<FcgiConnection> FcgiConnectionPtr;


struct FcgiConfig
{
	std::string location;
	std::string rootdir;
	std::string entryfile;
	int conmax;
	int timeout;
	int reqmax;
	bool autoretry;

	FcgiConfig()
		: conmax(2), timeout(INT_MAX), reqmax(1), autoretry(false)
	{
	}
};


class FcgiClient: virtual public XRefCount, private XRecMutex
{
	XEvent::DispatcherPtr _dispatcher;
	FcgiConfig _conf;

	int _idle;
	bool _shutdown;
	std::deque<FcgiQuestPtr> _queue;
	std::vector<FcgiConnectionPtr> _idlestack;
	std::set<FcgiConnectionPtr> _cons;

	int _err_count;	// number of connection errors
	int _con_count;	// number of connections when every thing is ok

	int on_reap_timer();
	void _new_connection();

public:
	FcgiClient(const XEvent::DispatcherPtr& dispatcher, const FcgiConfig& config);
	virtual ~FcgiClient();

	XEvent::DispatcherPtr dispatcher() const 	{ return _dispatcher; }
	const std::string& server() const 		{ return _conf.location; }
	const FcgiConfig& conf() const			{ return _conf; }

	void process(const FcgiQuestPtr& q);

	void start();
	void shutdown();

	FcgiQuestPtr getQuestOrPutback(FcgiConnection* con);
	void releaseClosed(FcgiConnection* con, const FcgiQuestPtr& quest);

	std::string getScriptFilename(const xstr_t& service) const
	{
		return _conf.rootdir + service + '/' + _conf.entryfile;
	}
};


#endif
