#include "Context.h"
#include "XicMessage.h"

namespace xic
{

Context::Context(ostk_t *ostk, vbs_dict_t *dict)
	: _ostk(ostk), _dict(dict)
{
}

Context::~Context()
{
}

void Context::xref_destroy()
{
	ostk_t *ostk = _ostk;
	this->~Context();
	ostk_destroy(ostk);
}

vbs_data_t *Context::_find(const char *key) const
{
	vbs_ditem_t *ent;
	for (ent = _dict->first; ent; ent = ent->next)
	{
		if (xstr_equal_cstr(&ent->key.d_xstr, key))
			return &ent->value;
	}
	return NULL;
}

intmax_t Context::getInt(const char* name, intmax_t dft) const
{
	vbs_data_t *v = _find(name);
	if (v && v->type == VBS_INTEGER)
		return v->d_int;
	return dft;
}

std::string Context::getString(const char *name, const std::string& dft) const
{
	vbs_data_t *v = _find(name);
	if (v && v->type == VBS_STRING)
		return make_string(v->d_xstr);
	return dft;
}

xstr_t Context::getXstr(const char *name, const xstr_t& dft) const
{
	vbs_data_t *v = _find(name);
	if (v && v->type == VBS_STRING)
		return v->d_xstr;
	return dft;
}

bool Context::getBool(const char *name, bool dft) const
{
	vbs_data_t *v = _find(name);
	if (v && v->type == VBS_BOOL)
		return v->d_bool;
	return dft;
}

double Context::getFloating(const char *name, double dft) const
{
	vbs_data_t *v = _find(name);
	if (v && v->type == VBS_FLOATING)
		return v->d_floating;
	return dft;
}

decimal64_t Context::getDecimal64(const char *name, decimal64_t dft) const
{
	vbs_data_t *v = _find(name);
	if (v && v->type == VBS_DECIMAL)
		return v->d_decimal64;
	return dft;
}

ContextBuilder::ContextBuilder()
{
	_init();
}

void ContextBuilder::_init()
{
	_ostk = ostk_create(512);
	_dict = (vbs_dict_t *)ostk_alloc(_ostk, sizeof(*_dict));
	vbs_dict_init(_dict);
}

ContextBuilder::~ContextBuilder()
{
	if (_ostk)
	{
		ostk_destroy(_ostk);
	}
}

ContextBuilder::ContextBuilder(const VDict& d)
{
	_init();

	for (vbs_ditem_t *re = d.dict()->first; re; re = re->next)
	{
		if (re->key.type != VBS_STRING)
			continue;

		if (re->value.type != VBS_STRING
			&& re->value.type != VBS_INTEGER
			&& re->value.type != VBS_BOOL
			&& re->value.type != VBS_FLOATING
			&& re->value.type != VBS_DECIMAL
			&& re->value.type != VBS_BLOB)
		{
			continue;
		}

		vbs_ditem_t *ent = (vbs_ditem_t *)ostk_alloc(_ostk, sizeof(*ent));

		ent->key.type = VBS_STRING;
		ent->key.d_xstr = ostk_xstr_dup(_ostk, &re->key.d_xstr);
		if (re->value.type == VBS_STRING || re->value.type == VBS_BLOB)
		{
			ent->value.type = re->value.type;
			ent->value.d_xstr = ostk_xstr_dup(_ostk, &re->value.d_xstr);
		}
		else
		{
			ent->value = re->value;
		}

		vbs_dict_push_back(_dict, ent);
	}
}
ContextPtr ContextBuilder::build()
{
	void *p = ostk_alloc(_ostk, sizeof(Context));
	ContextPtr ctx(new(p) Context(_ostk, _dict));
	_ostk = NULL;
	_dict = NULL;
	return ctx;
}

vbs_ditem_t* ContextBuilder::_put_item(const char *name)
{
	size_t len = strlen(name);
	vbs_ditem_t *ent;
	for (ent = _dict->first; ent; ent = ent->next)
	{
		if (xstr_equal_mem(&ent->key.d_xstr, name, len))
		{
			return ent;
		}
	}

	ent = (vbs_ditem_t *)ostk_alloc(_ostk, sizeof(*ent));
	ent->key.type = VBS_STRING;
	ent->key.d_xstr = ostk_xstr_dup_mem(_ostk, name, len);
	vbs_dict_push_back(_dict, ent);
	return ent;
}

void ContextBuilder::_seti(const char *name, intmax_t v)
{
	vbs_ditem_t *ent = _put_item(name);
	ent->value.type = VBS_INTEGER;
	ent->value.d_int = v;
}

void ContextBuilder::set(const char *name, const xstr_t& v)
{
	vbs_ditem_t *ent = _put_item(name);
	ent->value.type = VBS_STRING;
	ent->value.d_xstr = ostk_xstr_dup(_ostk, &v);
}

void ContextBuilder::set(const char *name, const std::string& v)
{
	vbs_ditem_t *ent = _put_item(name);
	ent->value.type = VBS_STRING;
	ent->value.d_xstr = ostk_xstr_dup_mem(_ostk, v.data(), v.length());
}

void ContextBuilder::set(const char *name, const char *v)
{
	vbs_ditem_t *ent = _put_item(name);
	ent->value.type = VBS_STRING;
	ent->value.d_xstr = ostk_xstr_dup_cstr(_ostk, v);
}

void ContextBuilder::set(const char *name, const char *data, size_t size)
{
	vbs_ditem_t *ent = _put_item(name);
	ent->value.type = VBS_STRING;
	ent->value.d_xstr = ostk_xstr_dup_mem(_ostk, data, size);
}

void ContextBuilder::set(const char *name, bool v)
{
	vbs_ditem_t *ent = _put_item(name);
	ent->value.type = VBS_BOOL;
	ent->value.d_bool = v;
}

void ContextBuilder::set(const char *name, double v)
{
	vbs_ditem_t *ent = _put_item(name);
	ent->value.type = VBS_FLOATING;
	ent->value.d_floating = v;
}

void ContextBuilder::set(const char *name, decimal64_t v)
{
	vbs_ditem_t *ent = _put_item(name);
	ent->value.type = VBS_DECIMAL;
	ent->value.d_decimal64 = v;
}

ContextPacker::ContextPacker(Quest* q)
	: _q(q)
{
	XASSERT(_q);
	_pk.write = NULL;
}

ContextPacker::~ContextPacker()
{
}

void ContextPacker::init()
{
	if (!_pk.write)
	{
		vbs_packer_init(&_pk, rope_xio.write, _q->context_rope(), 1);
		vbs_pack_head_of_dict(&_pk);
	}
}

void ContextPacker::finish()
{
	if (_pk.write)
	{
		vbs_pack_tail(&_pk);
		_pk.write = NULL;
	}
}

void ContextPacker::_packi(const char *name, intmax_t v)
{
	if (!_pk.write)
		init();
	vbs_pack_cstr(&_pk, name);
	vbs_pack_integer(&_pk, v);
}

void ContextPacker::_packu(const char *name, uintmax_t v)
{
	if (!_pk.write)
		init();
	vbs_pack_cstr(&_pk, name);
	vbs_pack_uinteger(&_pk, v);
}

void ContextPacker::pack(const char *name, const xstr_t& v)
{
	if (!_pk.write)
		init();
	vbs_pack_cstr(&_pk, name);
	vbs_pack_xstr(&_pk, &v);
}

void ContextPacker::pack(const char *name, const std::string& v)
{
	if (!_pk.write)
		init();
	vbs_pack_cstr(&_pk, name);
	vbs_pack_lstr(&_pk, v.data(), v.length());
}

void ContextPacker::pack(const char *name, const char *v)
{
	if (!_pk.write)
		init();
	vbs_pack_cstr(&_pk, name);
	vbs_pack_cstr(&_pk, v);
}

void ContextPacker::pack(const char *name, const char *data, size_t size)
{
	if (!_pk.write)
		init();
	vbs_pack_cstr(&_pk, name);
	vbs_pack_lstr(&_pk, data, size);
}

void ContextPacker::pack(const char *name, bool v)
{
	if (!_pk.write)
		init();
	vbs_pack_cstr(&_pk, name);
	vbs_pack_bool(&_pk, v);
}

void ContextPacker::pack(const char *name, double v)
{
	if (!_pk.write)
		init();
	vbs_pack_cstr(&_pk, name);
	vbs_pack_floating(&_pk, v);
}


};

