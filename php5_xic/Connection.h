#ifndef Connection_h_
#define Connection_h_

#include "xslib/XRefCount.h"
#include "xslib/rope.h"
#include "xslib/xstr.h"
#include "xslib/ostk.h"
#include "xslib/vbs_pack.h"
#include "xslib/Srp6a.h"
#include "xic/XicMessage.h"
#include "xic/XicCheck.h"
#include "xic/MyCipher.h"
#include <stdint.h>
#include <string>


namespace xic
{

class Connection;
typedef XPtr<Connection> ConnectionPtr;

class Engine;
typedef XPtr<Engine> EnginePtr;


class Connection: public XRefCount
{
	EnginePtr _engine;
	std::string _endpoint;
	int64_t _last_id;
	std::string _service;
	std::string _proto;
	std::string _host;
	int _port;
	int _timeout;
	int _priority;
	int _fd;
	char _peer_ip[40];
	enum {
		ST_INIT,
		ST_WAITING_HELLO,
		ST_ACTIVE,
		ST_CLOSE,
		ST_CLOSING,
		ST_CLOSED,
	} _state;

	MyCipherPtr _cipher;

	bool _read_hello_or_check(int *timeout, ostk_t *ostk, xstr_t *command, vbs_dict_t *dict);
	void _send_check_message(int *timeout, const xic::CheckPtr& check);
	void _check(int *timeout);
	int _connect(int *timeout);
	void _close();
	void _throw_IOError(const char *rdwr, int rc);
	void _sendMessage(int64_t id, const xstr_t& service, const xstr_t& method,
			const rope_t& args, const std::string& ctx, int *timeout);
public:
	Connection(const EnginePtr& engine, const std::string& endpoint);
	virtual ~Connection();

	int priority() const			{ return _priority; }
	const std::string& endpoint() const 	{ return _endpoint; }

	void setService(const std::string& service) 	{ _service = service; }

	void close();

	xstr_t invoke(const xstr_t& service, const xstr_t& method, const rope_t& args, const std::string& ctx);
	void invoke_oneway(const xstr_t& service, const xstr_t& method, const rope_t& args, const std::string& ctx);
};


};

#endif
