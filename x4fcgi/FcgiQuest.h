#ifndef FcgiQuest_h_
#define FcgiQuest_h_

#include "xslib/XRefCount.h"
#include "xslib/XLock.h"
#include "xslib/XEvent.h"
#include "xslib/ostk.h"
#include "xslib/ostk_allocator.h"
#include "xic/XicMessage.h"
#include <string>
#include <vector>
#include <set>
#include <deque>


const xstr_t XIC4FCGI_VERSION_XS = XSTR_CONST("XIC4FCGI_VERSION");

class FcgiQuest;
class FcgiAnswer;
class FcgiCallback;
class FcgiClient;

typedef XPtr<FcgiQuest> FcgiQuestPtr;
typedef XPtr<FcgiAnswer> FcgiAnswerPtr;
typedef XPtr<FcgiCallback> FcgiCallbackPtr;
typedef XPtr<FcgiClient> FcgiClientPtr;


class FcgiCallback: public XRefCount
{
public:
	virtual void response(const FcgiAnswerPtr& fa) = 0;
	virtual void response(const std::exception& ex) = 0;
};


class FcgiQuest: public XRefCount
{
	ostk_t *_ostk;
	FcgiClientPtr _client;
	FcgiCallbackPtr _callback;
	xic::QuestPtr _kq;
	std::string _endpoint;
	xstr_t _script_filename;
	xstr_t _request_uri;
	int64_t _txid;
	uint32_t _rid;
	bool _retry;
	std::vector<struct iovec, ostk_allocator<struct iovec> > _iov;

	int _params_len;

	void _add_iov(const void *data, size_t len);
	void _write_params_data(const void *data, size_t len);
	void _params_begin();
	void _params_add(const xstr_t& key, const xstr_t& value);
	void _params_add(const xstr_t& key, const char *value, size_t len);
	void _params_end();

	FcgiQuest(ostk_t *ostk, const FcgiClientPtr& client, const FcgiCallbackPtr& callback, 
				const xic::QuestPtr& kq, const std::string& endpoints);
	virtual ~FcgiQuest();
	virtual void xref_destroy();
public:
	static FcgiQuest* create(const FcgiClientPtr& client, const FcgiCallbackPtr& callback,
				 const xic::QuestPtr& kq, const std::string& endpoints);

	void finish(const FcgiAnswerPtr& a)
	{
		_callback->response(a);
	}

	void finish(const std::exception& ex)
	{
		_callback->response(ex);
	}

	const xstr_t& script_filename() const	{ return _script_filename; } 
	const xstr_t& request_uri() const	{ return _request_uri; } 
	int64_t txid() const			{ return _txid; } 
	uint16_t requestId() const		{ return _rid; }
	void setRequestId(uint16_t rid)		{ _rid = rid; }
	bool isRetry() const			{ return _retry; }
	void setRetry()				{ _retry = true; }
	ostk_t *ostk()				{ return _ostk; }
	struct iovec* get_iovec(int* count);
};


class FcgiAnswer: public XRefCount
{
	ostk_t *_ostk;
	rope_t _header;
	rope_t _content;
	rope_t _stderr;
	int _xic4fcgi;
	int _status;
	const char *_script_filename;
	const char *_request_uri;
	ssize_t _answer_begin;
	ssize_t _answer_size;

	FcgiAnswer(ostk_t *ostk, const xstr_t& script_filename, const xstr_t& request_uri);
	virtual ~FcgiAnswer();
	virtual void xref_destroy();
public:
	static FcgiAnswer* create(const xstr_t& script_filename, const xstr_t& request_uri);

	void append_header(const void *data, size_t len);
	void append_content(const void *data, size_t len);
	void append_stderr(const void *data, size_t len);

	const rope_t* get_header();
	const rope_t* get_content();
	const rope_t* get_stderr();

	ssize_t xic_answer_size();
	ssize_t xic_answer_copy(uint8_t *buf);	// return _answer_size;

	const char* script_filename() const	{ return _script_filename; } 
	const char* request_uri() const		{ return _request_uri; } 

	int status() const			{ return _status; }
	void set_status(int status)		{ _status = status; }

	void set_xic4fcgi(int ver)		{ _xic4fcgi = ver; }

	ostk_t *ostk() const			{ return _ostk; }
};


#endif

