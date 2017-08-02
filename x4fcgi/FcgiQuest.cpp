#include "FcgiQuest.h"
#include "FcgiClient.h"
#include "fastcgi.h"
#include "dlog/dlog.h"
#include "xslib/XEvent.h"
#include "xslib/loc.h"
#include "xslib/iobuf.h"
#include "xslib/ostk.h"
#include "xslib/ostk_cxx.h"
#include <assert.h>
#include <errno.h>
#include <unistd.h>


static xatomic_t _last_request_id;

FcgiQuest::FcgiQuest(ostk_t *ostk, const FcgiClientPtr& client, const FcgiCallbackPtr& callback,
			const xic::QuestPtr& kq, const std::string& endpoint)
	: _ostk(ostk), _client(client), _callback(callback),
	 _kq(kq), _txid(kq->txid()), _retry(false), _iov(ostk_allocator<struct iovec>(_ostk))
{
	_rid = xatomic_add_return(&_last_request_id, 1);
	_params_len = 0;
	_endpoint = endpoint;

	const FcgiConfig& conf = _client->conf();
	_request_uri = ostk_xstr_maker(_ostk)('/')(_kq->service())("::")(_kq->method())
				.printf("/%04x-%08x", _rid, random()).end();
	_script_filename = ostk_xstr_maker(_ostk)(conf.rootdir)('/')(_kq->service())('/')(conf.entryfile).end();
}

FcgiQuest::~FcgiQuest()
{
}

void FcgiQuest::xref_destroy()
{
	DESTROY_OBJ_WITH_OSTK(FcgiQuest, _ostk);
}

FcgiQuest* FcgiQuest::create(const FcgiClientPtr& client, const FcgiCallbackPtr& callback,
				const xic::QuestPtr& kq, const std::string& endpoints)
{
	ostk_t *ostk = ostk_create(1024*16);
	NEW_OBJ_WITH_OSTK_ARGS(FcgiQuest, ostk, client, callback, kq, endpoints);
}

static FCGI_BeginRequestRecord MY_FCGI_BEGIN = 
{
	{ FCGI_VERSION_1, FCGI_BEGIN_REQUEST, 0, 0, 0, sizeof(FCGI_BeginRequestBody), },
	{ 0, FCGI_RESPONDER, FCGI_KEEP_CONN, },
};

static void fill_begin_record(FCGI_BeginRequestRecord *record, uint16_t rid)
{
	*record = MY_FCGI_BEGIN;
	record->header.requestIdB1 = (rid >> 8) & 0xff;
	record->header.requestIdB0 = rid & 0xff;
}

static void fill_header(FCGI_Header *header, int type, uint16_t rid, uint16_t length)
{
	header->version = FCGI_VERSION_1;
	header->type = type;
	header->requestIdB1 = (rid >> 8) & 0xff;
	header->requestIdB0 = rid & 0xff;
	header->contentLengthB1 = (length >> 8) & 0xff;
	header->contentLengthB0 = length & 0xff;
	header->paddingLength = 0;
	header->reserved = 0;
}

void FcgiQuest::_add_iov(const void *data, size_t len)
{
	struct iovec iov = { (void *)data, len };
	_iov.push_back(iov);
}

void FcgiQuest::_write_params_data(const void *data, size_t len)
{
	if (len > FCGI_MAX_LENGTH)
	{
		throw XERROR_MSG(XError, "The length of key or value in the env is too large");
	}

	if (len > ostk_room(_ostk) && _params_len > 0)
	{
		size_t n;
		FCGI_Header *params_header = (FCGI_Header *)ostk_object_finish(_ostk, &n);
		fill_header(params_header, FCGI_PARAMS, _rid, _params_len);
		_add_iov(params_header, n);

		ostk_object_blank(_ostk, sizeof(FCGI_Header));
		_params_len = 0;
	}

	ostk_object_grow(_ostk, data, len);
	_params_len += len;
}

static size_t name_header(unsigned char *buf, size_t nlen)
{
	unsigned char *p = buf;
	if (nlen >= 128)
	{
		*p++ = (nlen >> 24) | 0x80;
		*p++ = (nlen >> 16);
		*p++ = (nlen >> 8);
	}
	*p++ = nlen;
	return p - buf;
}

void FcgiQuest::_params_begin()
{
	ostk_object_blank(_ostk, sizeof(FCGI_Header));
}

void FcgiQuest::_params_add(const xstr_t& key, const char *value, size_t len)
{
	xstr_t xs = XSTR_INIT((uint8_t*)value, len);
	_params_add(key, xs);
}

void FcgiQuest::_params_add(const xstr_t& key, const xstr_t& value)
{
	unsigned char buf[8];
	size_t len = name_header(buf, key.len);
	len += name_header(buf + len, value.len);

	_write_params_data(buf, len);
	_write_params_data(key.data, key.len);
	_write_params_data(value.data, value.len);
}

void FcgiQuest::_params_end()
{
	size_t n;
	if (_params_len > 0)
	{
		FCGI_Header *params_header = (FCGI_Header *)ostk_object_finish(_ostk, &n);
		fill_header(params_header, FCGI_PARAMS, _rid, _params_len);
		_add_iov(params_header, n);

		ostk_object_blank(_ostk, sizeof(FCGI_Header));
		_params_len = 0;
	}

	FCGI_Header *params_header = (FCGI_Header *)ostk_object_finish(_ostk, &n);
	fill_header(params_header, FCGI_PARAMS, _rid, 0);
	_add_iov(params_header, n);
}

struct iovec* FcgiQuest::get_iovec(int* count)
{
	static const xstr_t REQUEST_METHOD_XS = XSTR_CONST("REQUEST_METHOD");
	static const xstr_t REQUEST_URI_XS = XSTR_CONST("REQUEST_URI");
	static const xstr_t SERVER_NAME_XS = XSTR_CONST("SERVER_NAME");
	static const xstr_t DOCUMENT_ROOT_XS = XSTR_CONST("DOCUMENT_ROOT");
	static const xstr_t SCRIPT_FILENAME_XS = XSTR_CONST("SCRIPT_FILENAME");
	static const xstr_t CONTENT_LENGTH_XS = XSTR_CONST("CONTENT_LENGTH");
	static const xstr_t XIC_ENDPOINT_XS = XSTR_CONST("XIC_ENDPOINT");
	static const xstr_t XIC_SERVICE_XS = XSTR_CONST("XIC_SERVICE");
	static const xstr_t XIC_METHOD_XS = XSTR_CONST("XIC_METHOD");
	static const xstr_t POST_XS = XSTR_CONST("POST");

	if (!_iov.empty())
	{
		*count = (int)_iov.size();
		return &_iov[0];
	}

	_iov.reserve(16);

	/*
	   FCGI_BEGIN_REQUEST
	   FCGI_PARAMS 	0 or more non empty PARAMS
	   FCGI_PARAMS 	1 empty PARAMS
	   FCGI_STDIN  	0 or more non empty STDIN
	   FCGI_STDIN  	1 empty STDIN,
	 */

	FCGI_Header *header = NULL;
	FCGI_BeginRequestRecord *begin_record = OSTK_ALLOC_ONE(_ostk, FCGI_BeginRequestRecord);
	fill_begin_record(begin_record, _rid);
	_add_iov(begin_record, sizeof(FCGI_BeginRequestRecord));

	// params records
	// Don't call ostk_xxx() functions until _params_end().
	_params_begin();

	const FcgiConfig& conf = _client->conf();

	_params_add(REQUEST_METHOD_XS, POST_XS);
	_params_add(SERVER_NAME_XS, XS_SNL("-x4fcgi-"));
	_params_add(REQUEST_URI_XS, _request_uri);
	_params_add(SCRIPT_FILENAME_XS, _script_filename);
	_params_add(DOCUMENT_ROOT_XS, conf.rootdir.data(), conf.rootdir.length());

	_params_add(XIC4FCGI_VERSION_XS, XS_SNL("2"));
	_params_add(XIC_SERVICE_XS, _kq->service());
	_params_add(XIC_METHOD_XS, _kq->method());
	_params_add(XIC_ENDPOINT_XS, _endpoint.data(), _endpoint.length());

	char buf[32];
	xstr_t body = _kq->body();
	xstr_t value = make_xstr(buf, snprintf(buf, sizeof(buf), "%zd", body.len));
	_params_add(CONTENT_LENGTH_XS, value);

	_params_end();

	// stdin records
	for (ssize_t pos = 0; pos < body.len; pos += FCGI_MAX_LENGTH)
	{
		ssize_t len = body.len - pos;
		if (len >= FCGI_MAX_LENGTH)
			len = FCGI_MAX_LENGTH;

		header = OSTK_ALLOC_ONE(_ostk, FCGI_Header);
		fill_header(header, FCGI_STDIN, _rid, len);
		_add_iov(header, sizeof(FCGI_Header));
		_add_iov(body.data + pos, len);
	}

	// an empty stdin header
	header = OSTK_ALLOC_ONE(_ostk, FCGI_Header);
	fill_header(header, FCGI_STDIN, _rid, 0);
	_add_iov(header, sizeof(FCGI_Header));

	*count = (int)_iov.size();
	return &_iov[0];
}

FcgiAnswer::FcgiAnswer(ostk_t *ostk, const xstr_t& request_uri)
	: _ostk(ostk)
{
	rope_init(&_header, FCGI_MAX_LENGTH + ROPE_BLOCK_HEAD_SIZE, &ostk_xmem, _ostk);
	rope_init(&_content, FCGI_MAX_LENGTH + ROPE_BLOCK_HEAD_SIZE, &ostk_xmem, _ostk);
	rope_init(&_stderr, FCGI_MAX_LENGTH + ROPE_BLOCK_HEAD_SIZE, &ostk_xmem, _ostk);
	_xic4fcgi = 0;
	_status = 200;
	_request_uri = ostk_strdup_xstr(_ostk, &request_uri);
	_answer_begin = -1;
	_answer_size = 0;
}

FcgiAnswer::~FcgiAnswer()
{
}

void FcgiAnswer::xref_destroy()
{
	DESTROY_OBJ_WITH_OSTK(FcgiAnswer, _ostk);
}

FcgiAnswer* FcgiAnswer::create(const xstr_t& request_uri)
{
	NEW_OBJ_WITH_OSTK_ARGS(FcgiAnswer, NULL, request_uri);
}

ssize_t FcgiAnswer::xic_answer_size()
{
	static const xstr_t BEGIN = XSTR_CONST("\x00\x00\x00\x00XiC4fCgI\x00\x00\x00\x00");
	static const xstr_t END = XSTR_CONST("\x00\x00\x00\x00xIc4FcGi\x00\x00\x00\x00");

	if (_answer_size == 0)
	{
		if (_xic4fcgi != 2)
			goto error;

		if (_content.length < 36)
			goto error;

		ssize_t start = rope_find(&_content, 0, BEGIN.data, BEGIN.len);
		if (start < 0)
			goto error;

		_answer_begin = start + BEGIN.len;

		ssize_t end = rope_find(&_content, _answer_begin, END.data, END.len);
		if (end < 0)
			goto error;

		_answer_size = end - _answer_begin;

		if (_answer_size + 32 != (ssize_t)_content.length)
		{
			rope_block_t *block = NULL;
			xstr_t body = xstr_null;
			rope_next_block(&_content, &block, &body.data, &body.len);
			const char *more_body = (_content.block_count > 1) ? " ... " : ""; 

			xdlog(NULL, NULL, "STDOUT", _request_uri, "%.*s%s", XSTR_P(&body), more_body);
			xdlog(NULL, NULL, "WARNING", _request_uri, "require_once(\"x4fcgi.php\") should be the first statement in the entry file run.php");
		}
	}
	return _answer_size;

error:
	_answer_size = -1;
	return -1;
}

void FcgiAnswer::xic_answer_copy(uint8_t *buf)
{
	rope_substr_copy(&_content, _answer_begin, buf, _answer_size);
}

void FcgiAnswer::append_header(const void *data, size_t len)
{
	rope_append_external(&_header, data, len, NULL, NULL);
}

void FcgiAnswer::append_content(const void *data, size_t len)
{
	rope_append_external(&_content, data, len, NULL, NULL);
}

void FcgiAnswer::append_stderr(const void *data, size_t len)
{
	rope_append_external(&_stderr, data, len, NULL, NULL);
}

const rope_t* FcgiAnswer::get_header()
{
	return &_header;
}

const rope_t* FcgiAnswer::get_content()
{
	return &_content;
}

const rope_t* FcgiAnswer::get_stderr()
{
	return &_stderr;
}

