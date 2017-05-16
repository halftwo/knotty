#ifndef XicCheck_h_
#define XicCheck_h_

#include "XicMessage.h"

namespace xic
{

class Check;
typedef XPtr<Check> CheckPtr;

class CheckReader;
class CheckWriter;


class Check: public XicMessage
{
	Check(ostk_t *ostk);
	virtual ~Check();
	static Check* _create();
public:
	static CheckPtr create();
	static CheckPtr create(size_t bodySize);
	// If failed, cleanup will be called. 
	static CheckPtr create(void (*cleanup)(void *), void *cleanup_arg, const void *body, size_t bodySize);

	CheckPtr clone();

	virtual void unpack_body();
	virtual struct iovec* body_iovec(int* count);

	void setCommand(const xstr_t& command)	{ _command = ostk_xstr_dup(_ostk, &command); }
	void setCommand(const char* command)	{ _command = ostk_xstr_dup_cstr(_ostk, command); }

	const xstr_t& command() const 		{ return _command; }

	VDict args()				{ return VDict(args_dict()); }

	const xstr_t& args_xstr();
	const vbs_dict_t* args_dict();

	rope_t *args_rope();

private:
	xstr_t _command;
	xstr_t _p_xstr;

	vbs_dict_t _p_dict;

	rope_t *_p_rope;
};


class CheckWriter: public MsgWriter
{
	CheckPtr _check;
	vbs_packer_t _args_packer;
public:
	CheckWriter(const char* command)
		: _check(Check::create())
	{
		_check->setCommand(command);
		vbs_packer_init(&_args_packer, rope_xio.write, _check->args_rope(), -1);
		_dw.setPacker(&_args_packer);
	}

	CheckWriter(const xstr_t& command)
		: _check(Check::create())
	{
		_check->setCommand(command);
		vbs_packer_init(&_args_packer, rope_xio.write, _check->args_rope(), -1);
		_dw.setPacker(&_args_packer);
	}

	~CheckWriter()
	{
		// NB. Very Important, this must be called before _check destructor.
		_dw.close();
	}

	operator CheckPtr()
	{
		return this->take();
	}

	ostk_t *ostk()	const
	{
		return _check->ostk();
	}

	int cleanup_push(void (*cleanup)(void *), void *arg)
	{
		return _check->cleanup_push(cleanup, arg);
	}

	int cleanup_pop(bool execute = true)
	{
		return _check->cleanup_pop(execute);
	}

	template<typename Value>
	CheckWriter& operator()(const char *key, const Value& v)
	{
		this->param(key, v);
		return *this;
	}

	void suggest_block_size(size_t block_size)
	{
		rope_t *rope = _check->args_rope();
		if (rope->block_size < (int)block_size)
			rope->block_size = block_size;
	}

	unsigned char *buffer(size_t size)
	{
		return rope_reserve(_check->args_rope(), size, true);
	}

	CheckPtr take()
	{
		CheckPtr a;
		_dw.close();
		_check.swap(a);
		return a;
	}
};


class CheckReader: public VDict
{
	CheckPtr _check;
public:
	CheckReader(const CheckPtr& check)
		: VDict(check->args_dict()), _check(check)
	{
	}

	operator const CheckPtr&() const	{ return _check; }

	const xstr_t& command() const 		{ return _check->command(); }
};


} // namespace xic

#endif

