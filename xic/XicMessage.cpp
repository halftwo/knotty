#include "XicMessage.h"
#include "XicCheck.h"
#include "XicException.h"
#include "xslib/xlog.h"
#include "xslib/xnet.h"
#include "xslib/xbuf.h"
#include "xslib/xformat.h"
#include "xslib/XError.h"
#include <limits.h>
#include <typeinfo>
#include <sstream>

#define CHUNK_SIZE		4096
#define ROPE_BLOCK_SIZE		1008
#define MAX_PARAM_DEPTH		16
#define MAX_PARAM_NUM		256

namespace xic
{

static XicMessage::Header _default_header = { 'X', 'I', 0, 0, (uint32_t)-1 };


static int _do_unpack_args(vbs_unpacker_t *job, vbs_dict_t *dict, const xmem_t *xm, void *xm_cookie, bool ctx)
{
	ssize_t len;
	unsigned char *begin, *end;

	vbs_dict_init(dict);
	if (vbs_unpack_head_of_dict_with_length(job, &len) < 0)
		return -1;

	begin = job->cur - 1;
	end = job->cur + len;

	while (true)
	{
		if (vbs_unpack_if_tail(job))
		{
			if (len > 0)
			{
				if (job->cur != end)
					return -1;
			}
			dict->_raw.data = begin;
			dict->_raw.len = job->cur - begin;
			return 0;
		}

		if (dict->count >= MAX_PARAM_NUM)
		{
			throw XERROR_FMT(xic::ParameterLimitException, "Number of %s exceeds limit(%d)",
					ctx ? "context" : "parameter", MAX_PARAM_NUM);
		}

		vbs_ditem_t *ent = (vbs_ditem_t *)xm->alloc(xm_cookie, sizeof(*ent));

		if (vbs_unpack_data(job, &ent->key, xm, xm_cookie) < 0)
			return -1;

		if (ent->key.type != VBS_STRING)
		{
			throw XERROR_FMT(xic::ParameterNameException, "Name of %s must be STRING", ctx ? "context" : "parameter");
		}

		if (vbs_unpack_data(job, &ent->value, xm, xm_cookie) < 0)
			return -1;

		if (ctx && ent->value.type != VBS_INTEGER
			&& ent->value.type != VBS_STRING
			&& ent->value.type != VBS_BOOL
			&& ent->value.type != VBS_FLOATING
			&& ent->value.type != VBS_DECIMAL)
			throw XERROR_MSG(xic::ParameterTypeException, "Type of Context value should be INTEGER, STRING, BOOL, FLOATING or DECIMAL");

		vbs_dict_push_back(dict, ent);
	}
	return -1;
}

void unpack_args(ostk_t *ostk, xstr_t* xs, vbs_dict_t *dict, bool ctx)
{
	short max_depth = ctx ? 1 : MAX_PARAM_DEPTH;
	vbs_unpacker_t uk = VBS_UNPACKER_INIT(xs->data, xs->len, max_depth);
	if (_do_unpack_args(&uk, dict, &ostk_xmem, ostk, ctx) < 0 || uk.cur < uk.end)
	{
		throw XERROR_FMT(xic::MarshalException, "args: error=%d depth=%d/%d consumed=%zd/%zd",
			uk.error, uk.depth, uk.max_depth, uk.cur - uk.buf, uk.end - uk.buf);
	}
}


XicMessage::XicMessage()
	: _ostk(NULL), _txid(0), _fixed(false), _msgType(0), 
	_cleanup_num(0), _cleanup_stack(NULL), _body(xstr_null), 
	_iov_count(0), _body_iov_count(0), _iov(NULL), _body_iov(NULL), _body_size(0)
{
}

XicMessage::~XicMessage()
{
	if (_cleanup_stack)
	{
		do
		{
			struct cleanup_t *cu = _cleanup_stack;
			_cleanup_stack = cu->next;
			if (cu->cleanup)
			{
				try
				{
					cu->cleanup(cu->arg);
				}
				catch (std::exception& ex)
				{
					XError* x = dynamic_cast<XError*>(&ex);
					xlog(XLOG_CRIT, "EXCEPTION: %s\n%s", ex.what(), x ? x->calltrace().c_str() : "");
				}
			}
		} while (_cleanup_stack);
	}
}

void XicMessage::xref_destroy()
{
	ostk_t *ostk = _ostk;
	this->~XicMessage();
	ostk_destroy(ostk);
}

int XicMessage::cleanup_push(void (*cleanup)(void *), void *arg)
{
	struct cleanup_t *cu = (struct cleanup_t *)ostk_alloc(_ostk, sizeof(*cu));
	cu->cleanup = cleanup;
	cu->arg = arg;
	cu->next = _cleanup_stack;
	_cleanup_stack = cu;
	int rc = _cleanup_num++;
	return rc;
}

int XicMessage::cleanup_pop(bool execute)
{
	struct cleanup_t *cu = _cleanup_stack;
	if (cu)
	{
		int rc = --_cleanup_num;
		_cleanup_stack = cu->next;
		if (execute && cu->cleanup)
		{
			cu->cleanup(cu->arg);
		}
		return rc;
	}
	return -1;
}

uint32_t XicMessage::body_size()
{
	if (!_iov)
	{
		int dummy;
		get_iovec(&dummy);
	}
	return _body_size;
}

struct iovec* XicMessage::body_iovec(int* count)
{
	if (!_body_iov)
	{
		if (!_iov)
		{
			int dummy;
			get_iovec(&dummy);
		}

		_body_iov = (struct iovec *)ostk_alloc(_ostk, _iov_count * sizeof(_body_iov[0]));
		memcpy(_body_iov, _iov, _iov_count * sizeof(_body_iov[0]));
		_body_iov_count = xnet_adjust_iovec(&_body_iov, _iov_count, sizeof(XicMessage::Header));
	}

	*count = _body_iov_count;
	return _body_iov;
}

XicMessagePtr XicMessage::create(uint8_t msgType, size_t bodySize)
{
	XicMessagePtr kmsg;

	if (msgType == 'Q')
	{
		kmsg = Quest::create(bodySize);
	}
	else if (msgType == 'A')
	{
		kmsg = Answer::create(bodySize);
	}
	else if (msgType == 'C')
	{
		kmsg = Check::create(bodySize);
	}
	else
	{
		throw XERROR_FMT(XError, "Unknown msgType '%c' (%#x)", msgType, msgType);
	}

	return kmsg;
}


Quest::Quest(ostk_t *ostk)
{
	_ostk = ostk;
	_msgType = 'Q';
	_txid = 0;
	_service = xstr_null;
	_method = xstr_null;
	_c_xstr = xstr_null;
	_p_xstr = xstr_null;
	_c_dict.count = -1;	// NB. unpack lazily 
	_p_dict.count = -1;	// NB. unpack lazily 
	_c_rope = NULL;
	_p_rope = NULL;
}

Quest::~Quest()
{
	if (_c_rope)
		rope_finish(_c_rope);

	if (_p_rope)
		rope_finish(_p_rope);
}

Quest* Quest::_create()
{
	ostk_t *ostk = ostk_create(CHUNK_SIZE);
	void *p = ostk_alloc(ostk, sizeof(Quest));
	return new(p) Quest(ostk);
}

QuestPtr Quest::create()
{
	return QuestPtr(_create());
}

QuestPtr Quest::create(size_t bodySize)
{
	Quest* p = _create();
	if (p)
	{
		p->_body.len = bodySize;
		p->_body.data = (unsigned char *)ostk_alloc(p->_ostk, bodySize);
	}
	return QuestPtr(p);
}

QuestPtr Quest::create(void (*cleanup)(void *), void *arg, const void *body, size_t bodySize)
{
	Quest* p = _create();
	if (p)
	{
		p->cleanup_push(cleanup, arg);
		p->_body.data = (unsigned char *)body;
		p->_body.len = bodySize;
		p->unpack_body();
	}
	else if (cleanup)
	{
		cleanup(arg);
	}
	return QuestPtr(p);
}

QuestPtr Quest::clone()
{
	QuestPtr q;
	int count = 0;
	struct iovec *iov = this->body_iovec(&count);
	size_t len = 0;
	for (int i = 0; i < count; ++i)
	{
		len += iov[i].iov_len;
	}

	q = create(len);

	xstr_t body = q->body();
	len = 0;
	for (int i = 0; i < count; ++i)
	{
		size_t n = iov[i].iov_len;
		memcpy(body.data + len, iov[i].iov_base, n);
		len += n;
	}

	q->unpack_body();
	return q;
}

void prepare_xstr(ostk_t *ostk, rope_t *rope, xstr_t* xs)
{
	if (!rope)
		throw XERROR_MSG(XError, "No rope data");

	if (rope->block_count > 1)
	{
		xs->data = (unsigned char *)ostk_alloc(ostk, rope->length);
		xs->len = rope->length;
		rope_copy_to(rope, xs->data);
	}
	else if (rope->block_count == 1)
	{
		rope_block_t *rb = NULL;
		rope_next_block(rope, &rb, &xs->data, &xs->len);
	}
}

bool Quest::hasContext() const
{
	return (_c_rope && _c_rope->length) || _c_xstr.len;
}

void Quest::unsetContext()
{
	if (_c_rope)
		rope_clear(_c_rope);

	_c_xstr = xstr_null;
	if (_c_dict.count != (size_t)-1)
	{
		_c_dict.count = -1;
	}
}

void Quest::setContext(const xic::ContextPtr& ctx)
{
	if (!ctx || ctx->empty())
	{
		unsetContext();
		return;
	}

	if (!_c_rope)
	{
		_c_rope = (rope_t *)ostk_alloc(_ostk, sizeof(rope_t));
		rope_init(_c_rope, ROPE_BLOCK_SIZE, &ostk_xmem, _ostk);
	}
	else
	{
		rope_clear(_c_rope);
	}

	_c_xstr = xstr_null;
	if (_c_dict.count != (size_t)-1)
	{
		_c_dict.count = -1;
	}

	const vbs_dict_t *dict = ctx->dict();
	if (dict)
	{
		vbs_packer_t pk = VBS_PACKER_INIT(rope_xio.write, _c_rope, 1);
		vbs_pack_dict(&pk, dict);
	}
	else
	{
		rope_putc(_c_rope, VBS_DICT);
		rope_putc(_c_rope, VBS_TAIL);
	}
}

const xstr_t& Quest::context_xstr()
{
	if (!_c_xstr.data && _c_rope)
	{
		prepare_xstr(_ostk, _c_rope, &_c_xstr);
	}
	return _c_xstr.data ? _c_xstr : vbs_packed_empty_dict;
}

const vbs_dict_t* Quest::context_dict()
{
	if (!_c_xstr.data && _c_rope)
	{
		prepare_xstr(_ostk, _c_rope, &_c_xstr);
	}
	if (_c_dict.count == (size_t)-1)
	{
		if (_c_xstr.data && _c_xstr.len)
		{
			unpack_args(_ostk, &_c_xstr, &_c_dict, true);
		}
		else
		{
			vbs_dict_init(&_c_dict);
		}
	}
	return &_c_dict;
}

const xstr_t& Quest::args_xstr()
{
	if (!_p_xstr.data)
	{
		prepare_xstr(_ostk, _p_rope, &_p_xstr);
	}
	return _p_xstr;
}

const vbs_dict_t* Quest::args_dict()
{
	if (!_p_xstr.data)
	{
		prepare_xstr(_ostk, _p_rope, &_p_xstr);
	}
	if (_p_dict.count == (size_t)-1)
	{
		unpack_args(_ostk, &_p_xstr, &_p_dict, false);
	}
	return &_p_dict;
}

rope_t *Quest::context_rope()
{
	if (!_c_rope)
	{
		_c_rope = (rope_t *)ostk_alloc(_ostk, sizeof(rope_t));
		rope_init(_c_rope, ROPE_BLOCK_SIZE, &ostk_xmem, _ostk);
		_c_xstr = xstr_null;
		if (_c_dict.count != (size_t)-1)
		{
			_c_dict.count = -1;
		}
	}
	return _c_rope;
}

rope_t *Quest::args_rope()
{
	if (!_p_rope)
	{
		_p_rope = (rope_t *)ostk_alloc(_ostk, sizeof(rope_t));
		rope_init(_p_rope, ROPE_BLOCK_SIZE, &ostk_xmem, _ostk);
		_p_xstr = xstr_null;
		if (_p_dict.count != (size_t)-1)
		{
			_p_dict.count = -1;
		}
	}
	return _p_rope;
}

void Quest::unpack_body()
{
	vbs_unpacker_t uk;
	vbs_unpacker_init(&uk, _body.data, _body.len, -1);

	if (vbs_unpack_int64(&uk, &_txid) < 0)
		throw XERROR_MSG(xic::MarshalException, "txid");
	if (vbs_unpack_xstr(&uk, &_service) < 0)
		throw XERROR_MSG(xic::MarshalException, "service");
	if (vbs_unpack_xstr(&uk, &_method) < 0)
		throw XERROR_MSG(xic::MarshalException, "method");

	unsigned char *p = uk.cur;
	if (_do_unpack_args(&uk, &_c_dict, &ostk_xmem, _ostk, true) < 0)
		throw XERROR_MSG(xic::MarshalException, "ctx");
	xstr_init(&_c_xstr, p, uk.cur - p);

	xstr_init(&_p_xstr, uk.cur, uk.end - uk.cur);
}

struct iovec* Quest::get_iovec(int* count)
{
	if (!_iov)
	{
		xbuf_t xb;
		xb.capacity = sizeof(XicMessage::Header) + 10 + 10 + 10 + _service.len + _method.len;
		xb.data = (unsigned char *)ostk_alloc(_ostk, xb.capacity);
		xb.len = sizeof(XicMessage::Header);
		XicMessage::Header *hdr = (XicMessage::Header *)xb.data;
		*hdr = _default_header;
		vbs_packer_t pk = VBS_PACKER_INIT(xbuf_xio.write, &xb, 0);

		vbs_pack_integer(&pk, _txid);
		vbs_pack_xstr(&pk, &_service);
		vbs_pack_xstr(&pk, &_method);

		_iov_count = 1;
		if (_c_xstr.len)
		{
			_iov_count += 1;
		}
		else if (_c_rope && _c_rope->length)
		{
			_iov_count += _c_rope->block_count;
		}
		else
		{
			_iov_count += 1;
		}

		if (_p_xstr.len)
			_iov_count += 1;
		else if (_p_rope && _p_rope->length)
			_iov_count += _p_rope->block_count;
		else
			throw XERROR_MSG(XError, "No data");

		_iov = (struct iovec *)ostk_alloc(_ostk, _iov_count * sizeof(_iov[0]));

		hdr->bodySize = xb.len - sizeof(XicMessage::Header);
		_iov[0].iov_base = xb.data;
		_iov[0].iov_len = xb.len;

		int idx = 1;
		if (_c_xstr.len)
		{
			hdr->bodySize += _c_xstr.len;
			_iov[idx].iov_base = _c_xstr.data;
			_iov[idx].iov_len = _c_xstr.len;
			++idx;
		}
		else if (_c_rope && _c_rope->length)
		{
			hdr->bodySize += _c_rope->length;
			rope_iovec(_c_rope, &_iov[idx]);
			idx += _c_rope->block_count;
		}
		else
		{
			hdr->bodySize += vbs_packed_empty_dict.len;
			_iov[idx].iov_base = vbs_packed_empty_dict.data;
			_iov[idx].iov_len = vbs_packed_empty_dict.len;
			++idx;
		}

		if (_p_xstr.len)
		{
			hdr->bodySize += _p_xstr.len;
			_iov[idx].iov_base = _p_xstr.data;
			_iov[idx].iov_len = _p_xstr.len;
		}
		else if (_p_rope && _p_rope->length)
		{
			hdr->bodySize += _p_rope->length;
			rope_iovec(_p_rope, &_iov[idx]);
		}
		_body_size = hdr->bodySize;

		hdr->msgType = _msgType;
		xnet_msb32(&hdr->bodySize);
	}

	*count = _iov_count;
	return _iov;
}

void Quest::reset_iovec()
{
	_iov = NULL;
	_iov_count = 0;
}


Answer::Answer(ostk_t *ostk)
{
	_ostk = ostk;
	_msgType = 'A';
	_txid = 0;
	_status = 0;
	_p_xstr = xstr_null;
	_p_dict.count = -1;	// NB. unpack lazily 
	_p_rope = NULL;
}

Answer::~Answer()
{
	if (_p_rope)
		rope_finish(_p_rope);
}

Answer* Answer::_create()
{
	ostk_t *ostk = ostk_create(CHUNK_SIZE);
	void *p = ostk_alloc(ostk, sizeof(Answer));
	return new(p) Answer(ostk);
}

AnswerPtr Answer::create()
{
	return AnswerPtr(_create());
}

AnswerPtr Answer::create(size_t bodySize)
{
	Answer* p = _create();
	if (p)
	{
		p->_body.len = bodySize;
		p->_body.data = (unsigned char *)ostk_alloc(p->_ostk, bodySize);
	}
	return AnswerPtr(p);
}

AnswerPtr Answer::create(void (*cleanup)(void *), void *arg, const void *body, size_t bodySize)
{
	Answer* p = _create();
	if (p)
	{
		p->cleanup_push(cleanup, arg);
		p->_body.data = (unsigned char *)body;
		p->_body.len = bodySize;
		p->unpack_body();
	}
	else if (cleanup)
	{
		cleanup(arg);
	}
	return AnswerPtr(p);
}

AnswerPtr Answer::clone()
{
	AnswerPtr a;
	int count = 0;
	struct iovec *iov = this->body_iovec(&count);
	size_t len = 0;
	for (int i = 0; i < count; ++i)
	{
		len += iov[i].iov_len;
	}

	a = create(len);

	xstr_t body = a->body();
	len = 0;
	for (int i = 0; i < count; ++i)
	{
		size_t n = iov[i].iov_len;
		memcpy(body.data + len, iov[i].iov_base, n);
		len += n;
	}

	a->unpack_body();
	return a;
}

const xstr_t& Answer::args_xstr()
{
	if (!_p_xstr.data)
	{
		prepare_xstr(_ostk, _p_rope, &_p_xstr);
	}
	return _p_xstr;
}

const vbs_dict_t* Answer::args_dict()
{
	if (!_p_xstr.data)
	{
		prepare_xstr(_ostk, _p_rope, &_p_xstr);
	}
	if (_p_dict.count == (size_t)-1)
	{
		unpack_args(_ostk, &_p_xstr, &_p_dict, false);
	}
	return &_p_dict;
}

rope_t *Answer::args_rope()
{
	if (!_p_rope)
	{
		_p_rope = (rope_t *)ostk_alloc(_ostk, sizeof(rope_t));
		rope_init(_p_rope, ROPE_BLOCK_SIZE, &ostk_xmem, _ostk);
		_p_xstr = xstr_null;
		if (_p_dict.count != (size_t)-1)
		{
			_p_dict.count = -1;
		}
	}
	return _p_rope;
}

void Answer::unpack_body()
{
	vbs_unpacker_t uk;
	vbs_unpacker_init(&uk, _body.data, _body.len, -1);

	intmax_t x;
	if (vbs_unpack_int64(&uk, &_txid) < 0)
		throw XERROR_MSG(xic::MarshalException, "txid");
	if (vbs_unpack_integer(&uk, &x) < 0 || x < INT_MIN || x > INT_MAX)
		throw XERROR_MSG(xic::MarshalException, "status");
	_status = x;

	xstr_init(&_p_xstr, uk.cur, uk.end - uk.cur);
}

struct iovec* Answer::get_iovec(int* count)
{
	if (!_iov)
	{
		xbuf_t xb;
		xb.capacity = sizeof(XicMessage::Header) + 10 + 10;
		xb.data = (unsigned char *)ostk_alloc(_ostk, xb.capacity);
		xb.len = sizeof(XicMessage::Header);
		XicMessage::Header *hdr = (XicMessage::Header *)xb.data;
		*hdr = _default_header;
		vbs_packer_t pk = VBS_PACKER_INIT(xbuf_xio.write, &xb, 0);
		vbs_pack_integer(&pk, _txid);
		vbs_pack_integer(&pk, _status);

		_iov_count = 1;
		if (_p_xstr.len)
			_iov_count += 1;
		else if (_p_rope && _p_rope->length)
			_iov_count += _p_rope->block_count;
		else
			throw XERROR_MSG(XError, "No data");

		_iov = (struct iovec *)ostk_alloc(_ostk, _iov_count * sizeof(_iov[0]));

		hdr->bodySize = xb.len - sizeof(XicMessage::Header);
		_iov[0].iov_base = xb.data;
		_iov[0].iov_len = xb.len;

		if (_p_xstr.len)
		{
			hdr->bodySize += _p_xstr.len;
			_iov[1].iov_base = _p_xstr.data;
			_iov[1].iov_len = _p_xstr.len;
		}
		else if (_p_rope && _p_rope->length)
		{
			hdr->bodySize += _p_rope->length;
			rope_iovec(_p_rope, &_iov[1]);
		}
		_body_size = hdr->bodySize;

		hdr->msgType = _msgType;
		xnet_msb32(&hdr->bodySize);
	}

	*count = _iov_count;
	return _iov;
}

std::string exanswer2string(const AnswerPtr& answer)
{
	std::string s;
	AnswerReader ar(answer);
	if (ar.status())
	{
		xstr_t exname = ar.getXstr("exception");
		int code = ar.getInt("code");
		xstr_t tag = ar.getXstr("tag");
		xstr_t msg = ar.getXstr("message");
		xstr_t raiser = ar.getXstr("raiser");
		const vbs_dict_t* detail = ar.get_dict("detail");

		std::ostringstream os;

		os << '!' << exname << '(' << code;
		if (tag.len)
			os << ',' << tag;
		os << ')';

		if (raiser.len)
			os << " on " << raiser;

		if (msg.len)
			os << " --- " << msg;

		if (detail && detail->count > 0)
		{
			os << " --- ";
			iobuf_t ob = make_iobuf(os, NULL, 0);
			vbs_print_dict(detail, &ob);
		}

		s = os.str();
	}
	return s;
}


} // namespace xic

