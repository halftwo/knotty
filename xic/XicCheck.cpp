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

namespace xic
{

static XicMessage::Header _default_header = { 'X', 'I', 0, 0, (uint32_t)-1 };


Check::Check(ostk_t *ostk)
{
	_ostk = ostk;
	_msgType = 'C';
	_command = xstr_null;
	_p_xstr = xstr_null;
	_p_dict.count = -1;	// NB. unpack lazily 
	_p_rope = NULL;
}

Check::~Check()
{
	if (_p_rope)
		rope_finish(_p_rope);
}

Check* Check::_create()
{
	ostk_t *ostk = ostk_create(CHUNK_SIZE);
	void *p = ostk_alloc(ostk, sizeof(Check));
	return new(p) Check(ostk);
}

CheckPtr Check::create()
{
	return CheckPtr(_create());
}

CheckPtr Check::create(size_t bodySize)
{
	Check* p = _create();
	if (p)
	{
		p->_body.len = bodySize;
		p->_body.data = (unsigned char *)ostk_alloc(p->_ostk, bodySize);
	}
	return CheckPtr(p);
}

CheckPtr Check::create(void (*cleanup)(void *), void *arg, const void *body, size_t bodySize)
{
	Check* p = _create();
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
	return CheckPtr(p);
}

CheckPtr Check::clone()
{
	CheckPtr a;
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

const xstr_t& Check::args_xstr()
{
	if (!_p_xstr.data)
	{
		prepare_xstr(_ostk, _p_rope, &_p_xstr);
	}
	return _p_xstr;
}

const vbs_dict_t* Check::args_dict()
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

rope_t *Check::args_rope()
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

void Check::unpack_body()
{
	vbs_unpacker_t uk;
	vbs_unpacker_init(&uk, _body.data, _body.len, -1);

	if (vbs_unpack_xstr(&uk, &_command) < 0)
		throw XERROR_MSG(xic::MarshalException, "command");

	xstr_init(&_p_xstr, uk.cur, uk.end - uk.cur);
}

struct iovec* Check::get_iovec(int* count)
{
	if (!_iov)
	{
		xbuf_t xb;
		xb.capacity = sizeof(XicMessage::Header) + 10 + _command.len;
		xb.data = (unsigned char *)ostk_alloc(_ostk, xb.capacity);
		xb.len = sizeof(XicMessage::Header);
		XicMessage::Header *hdr = (XicMessage::Header *)xb.data;
		*hdr = _default_header;
		vbs_packer_t pk = VBS_PACKER_INIT(xbuf_xio.write, &xb, 0);
		vbs_pack_xstr(&pk, &_command);

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


} // namespace xic


