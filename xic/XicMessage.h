#ifndef XicMessage_h_
#define XicMessage_h_

#include "VWriter.h"
#include "VData.h"
#include "Context.h"
#include "xslib/xsdef.h"
#include "xslib/iobuf.h"
#include "xslib/ostk.h"
#include "xslib/xstr.h"
#include "xslib/vbs.h"
#include "xslib/rope.h"
#include "xslib/XRefCount.h"
#include <stdint.h>
#include <assert.h>

#define XIC_FLAG_CIPHER_MODE0	(0x01 << 0)
#define XIC_FLAG_CIPHER_MODE1	(0x01 << 1)
#define XIC_FLAG_MASK		0x03

namespace xic
{

class XicMessage;
class Quest;
class Answer;

typedef XPtr<XicMessage> XicMessagePtr;
typedef XPtr<Quest> QuestPtr;
typedef XPtr<Answer> AnswerPtr;

class QuestReader;
class AnswerReader;

class QuestWriter;
class AnswerWriter;


extern const xstr_t x00ping;
extern const xstr_t x00stat;
extern const xstr_t x00mark;

std::string exanswer2string(const AnswerPtr& answer);
void unpack_args(ostk_t *ostk, xstr_t* xs, vbs_dict_t *dict, bool ctx);
void prepare_xstr(ostk_t *ostk, rope_t *rope, xstr_t* xs);

class XicMessage: public XRefCount
{
public:
	struct Header
	{
		uint8_t magic;
		uint8_t version;
		uint8_t msgType;
		uint8_t flags;
		uint32_t bodySize;
	};

	virtual void xref_destroy();

	ostk_t* ostk() const				{ return _ostk; }

	uint8_t msgType() const 			{ return _msgType; }
	int64_t txid() const				{ return _txid; }
	const xstr_t& body() const			{ return _body; }

	bool isQuest() const				{ return _msgType == 'Q'; }
	bool isAnswer() const				{ return _msgType == 'A'; }

	uint32_t bodySize();
	virtual void unpack_body() 			= 0;
	virtual struct iovec* body_iovec(int* count)	= 0;

	int cleanup_push(void (*cleanup)(void *), void *arg);
	int cleanup_pop(bool execute = true);

	static XicMessagePtr create(uint8_t msgType, size_t bodySize);

protected:
	XicMessage();
	virtual ~XicMessage();

protected:
	struct cleanup_t
	{
		struct cleanup_t *next;
		void (*cleanup)(void *);
		void *arg;
	};

	ostk_t* _ostk;
	int64_t _txid;
	uint8_t _msgType;
	int _cleanup_num;
	struct cleanup_t *_cleanup_stack;
	xstr_t _body;
	struct iovec* _iov;	// not include header
	int _iov_count;		// not include header
	uint32_t _body_size;	// when send out
};


class Quest: public XicMessage
{
	Quest(ostk_t *ostk);
	virtual ~Quest();
	static Quest* _create();
public:
	static QuestPtr create();
	static QuestPtr create(size_t bodySize);

	// If failed, cleanup will be called.
	static QuestPtr create(void (*cleanup)(void *), void *cleanup_arg,
				const void *body, size_t bodySize);

	QuestPtr clone();

	virtual void unpack_body();
	virtual struct iovec* body_iovec(int* count);
	void reset_iovec();

	void setTxid(int64_t txid)		{ _txid = txid; }
	void setService(const xstr_t& service)	{ _service = ostk_xstr_dup(_ostk, &service); }
	void setService(const char* service)	{ _service = ostk_xstr_dup_cstr(_ostk, service); }
	void setMethod(const xstr_t& method)	{ _method = ostk_xstr_dup(_ostk, &method); }
	void setMethod(const char* method)	{ _method = ostk_xstr_dup_cstr(_ostk, method); }

	int64_t txid() const 			{ return _txid; }
	const xstr_t& service() const 		{ return _service; }
	const xstr_t& method() const 		{ return _method; }

	bool hasContext() const;
	void setContext(const xic::ContextPtr& ctx);
	void unsetContext();

	VDict context()				{ return VDict(context_dict()); }
	VDict args()				{ return VDict(args_dict()); }

	const xstr_t& context_xstr();
	const vbs_dict_t* context_dict();

	const xstr_t& args_xstr();
	const vbs_dict_t* args_dict();

	rope_t* context_rope();

	rope_t* args_rope();

private:
	xstr_t _service;
	xstr_t _method;
	xstr_t _c_xstr;
	xstr_t _p_xstr;

	vbs_dict_t _c_dict;
	vbs_dict_t _p_dict;

	rope_t *_c_rope;
	rope_t *_p_rope;
};


class Answer: public XicMessage
{
	Answer(ostk_t *ostk);
	virtual ~Answer();
	static Answer* _create();
public:
	static AnswerPtr create();
	static AnswerPtr create(size_t bodySize);
	// If failed, cleanup will be called. 
	static AnswerPtr create(void (*cleanup)(void *), void *cleanup_arg, const void *body, size_t bodySize);

	AnswerPtr clone();

	virtual void unpack_body();
	virtual struct iovec* body_iovec(int* count);

	void setTxid(int64_t txid)		{ _txid = txid; }
	void setStatus(int status)		{ _status = status; }

	int64_t txid() const 			{ return _txid; }
	int status() const 			{ return _status; }

	VDict args()				{ return VDict(args_dict()); }

	const xstr_t& args_xstr();
	const vbs_dict_t* args_dict();

	rope_t *args_rope();

private:
	int _status;
	xstr_t _p_xstr;

	vbs_dict_t _p_dict;

	rope_t *_p_rope;
};


class MsgWriter
{
public:
	template<typename Value>
	void param(const char *key, const Value& v, int descriptor=0)
	{
		_dw.kv(key, v, descriptor);
	}

	template<typename Value>
	void param(const xstr_t& key, const Value& v, int descriptor=0)
	{
		_dw.kv(key, v, descriptor);
	}

	template<typename Value>
	void param(const std::string& key, const Value& v, int descriptor=0)
	{
		_dw.kv(key, v, descriptor);
	}

	void paramFormat(const char *key, xfmt_callback_function cb/*NULL*/, const char *fmt, ...) XS_C_PRINTF(4, 5)
	{
		va_list ap;
		va_start(ap, fmt);
		_dw.kvfmt(key, cb, fmt, ap);
		va_end(ap);
	}

	void paramString(const char *key, const xstr_t& xstr, int descriptor=0)
	{
		_dw.kvstring(key, xstr, descriptor);
	}

	void paramString(const char *key, const char *str, size_t len, int descriptor=0)
	{
		_dw.kvstring(key, str, len, descriptor);
	}

	void paramString(const char *key, const rope_t *rope, int descriptor=0)
	{
		_dw.kvstring(key, rope, descriptor);
	}

	void paramString(const char *key, const struct iovec *iov, int count, int descriptor=0)
	{
		_dw.kvstring(key, iov, count, descriptor);
	}

	void paramBlob(const char *key, const void *data, size_t len, int descriptor=0)
	{
		_dw.kvblob(key, data, len, descriptor);
	}

	void paramBlob(const char *key, const rope_t *rope, int descriptor=0)
	{
		_dw.kvblob(key, rope, descriptor);
	}

	void paramBlob(const char *key, const struct iovec *iov, int count, int descriptor=0)
	{
		_dw.kvblob(key, iov, count, descriptor);
	}

	void paramBlob(const char *key, const xstr_t& blob, int descriptor=0)
	{
		_dw.kvblob(key, blob.data, blob.len, descriptor);
	}

	void paramStanza(const char *key, const unsigned char *buf, size_t len, int descriptor=0)
	{
		_dw.kvstanza(key, buf, len, descriptor);
	}

	void paramNull(const char *key)
	{
		_dw.kvnull(key);
	}

	VListWriter paramVList(const char *key, int descriptor=0)
	{
		return _dw.kvlist(key, descriptor);
	}

	template <typename ListType>
	VListWriter paramVList(const char *key, const ListType& values, int descriptor=0)
	{
		return _dw.kvlist(key, values, descriptor);
	}

	VListWriter paramVListKind(const char *key, int kind, int descriptor=0)
	{
		return _dw.kvlistKind(key, kind, descriptor);
	}

	template <typename ListType>
	VListWriter paramVListKind(const char *key, const ListType& values, int kind, int descriptor=0)
	{
		return _dw.kvlistKind(key, values, kind, descriptor);
	}

	VDictWriter paramVDict(const char *key, int descriptor=0)
	{
		return _dw.kvdict(key, descriptor);
	}

	template <typename DictType>
	VDictWriter paramVDict(const char *key, const DictType& dict, int descriptor=0)
	{
		return _dw.kvdict(key, dict, descriptor);
	}

	VDictWriter paramVDictKind(const char *key, int kind, int descriptor=0)
	{
		return _dw.kvdictKind(key, kind, descriptor);
	}

	template <typename DictType>
	VDictWriter paramVDictKind(const char *key, const DictType& dict, int kind, int descriptor=0)
	{
		return _dw.kvdictKind(key, dict, kind, descriptor);
	}

	void paramStrHead(const char *key, size_t len, int descriptor=0)
	{
		_dw.kvstrhead(key, len, descriptor);
	}

	void paramBlobHead(const char *key, size_t len, int descriptor=0)
	{
		_dw.kvblobhead(key, len, descriptor);
	}

	void raw(const void *buf, size_t len)
	{
		_dw.raw(buf, len);
	}

protected:
	MsgWriter()
	{
	}

	~MsgWriter()
	{
	}

	VDictWriter _dw;
};

class QuestWriter: public MsgWriter
{
	QuestPtr _quest;
	xic::ContextPacker _ctx_packer;
	vbs_packer_t _args_packer;
public:
	QuestWriter(const char *method, bool twoway = true)
		: _quest(Quest::create()), _ctx_packer(_quest.get())
	{
		_quest->setMethod(method);
		_quest->setTxid(twoway ? -1 : 0);
		vbs_packer_init(&_args_packer, rope_xio.write, _quest->args_rope(), -1);
		_dw.setPacker(&_args_packer, 0);
	}

	QuestWriter(const xstr_t& method, bool twoway = true)
		: _quest(Quest::create()), _ctx_packer(_quest.get())
	{
		_quest->setMethod(method);
		_quest->setTxid(twoway ? -1 : 0);
		vbs_packer_init(&_args_packer, rope_xio.write, _quest->args_rope(), -1);
		_dw.setPacker(&_args_packer, 0);
	}

	~QuestWriter()
	{
		// NB. Very Important, this must be called before _quest destructor.
		_dw.close();
		_ctx_packer.finish();
	}

	operator QuestPtr()
	{
		return this->take();
	}

	ostk_t *ostk()	const
	{
		return _quest->ostk();
	}

	int cleanup_push(void (*cleanup)(void *), void *arg)
	{
		return _quest->cleanup_push(cleanup, arg);
	}

	int cleanup_pop(bool execute = true)
	{
		return _quest->cleanup_pop(execute);
	}

	template<typename VALUE>
	QuestWriter& operator()(const char *key, const VALUE& v)
	{
		this->param(key, v);
		return *this;
	}

	template<typename VALUE>
	QuestWriter& ctx(const char *key, const VALUE& v)
	{
		_ctx_packer.pack(key, v);
		return *this;
	}

	void setContext(const xic::ContextPtr& ctx)
	{
		_quest->setContext(ctx);
	}

	void suggest_block_size(size_t block_size)
	{
		rope_t *rope = _quest->args_rope();
		if (rope->block_size < (int)block_size)
			rope->block_size = block_size;
	}

	unsigned char *buffer(size_t size)
	{
		return rope_reserve(_quest->args_rope(), size, true);
	}

	QuestPtr take()
	{
		_dw.close();
		_ctx_packer.finish();

		QuestPtr q;
		_quest.swap(q);
		return q;
	}
};

class AnswerWriter: public MsgWriter
{
	AnswerPtr _answer;
	vbs_packer_t _args_packer;
public:
	enum Status
	{ 
		NORMAL = 0, 
		EXCEPTION = -1,
	};

	AnswerWriter()
		: _answer(Answer::create())
	{
		vbs_packer_init(&_args_packer, rope_xio.write, _answer->args_rope(), -1);
		_dw.setPacker(&_args_packer, 0);
	}

	AnswerWriter(AnswerWriter::Status status)
		: _answer(Answer::create())
	{
		if (status)
			_answer->setStatus(status);
		vbs_packer_init(&_args_packer, rope_xio.write, _answer->args_rope(), -1);
		_dw.setPacker(&_args_packer, 0);
	}

	~AnswerWriter()
	{
		// NB. Very Important, this must be called before _answer destructor.
		_dw.close();
	}

	operator AnswerPtr()
	{
		return this->take();
	}

	ostk_t *ostk()	const
	{
		return _answer->ostk();
	}

	int cleanup_push(void (*cleanup)(void *), void *arg)
	{
		return _answer->cleanup_push(cleanup, arg);
	}

	int cleanup_pop(bool execute = true)
	{
		return _answer->cleanup_pop(execute);
	}

	template<typename Value>
	AnswerWriter& operator()(const char *key, const Value& v, int descriptor=0)
	{
		this->param(key, v, descriptor);
		return *this;
	}

	void suggest_block_size(size_t block_size)
	{
		rope_t *rope = _answer->args_rope();
		if (rope->block_size < (int)block_size)
			rope->block_size = block_size;
	}

	unsigned char *buffer(size_t size)
	{
		return rope_reserve(_answer->args_rope(), size, true);
	}

	AnswerPtr take()
	{
		AnswerPtr a;
		_dw.close();
		_answer.swap(a);
		return a;
	}
};


class QuestReader: public VDict
{
	QuestPtr _quest;
public:
	QuestReader(const QuestPtr& quest)
		: VDict(quest->args_dict()), _quest(quest)
	{
	}

	operator const QuestPtr&() const	{ return _quest; }

	int64_t txid() const 		{ return _quest->txid(); }
	const xstr_t& service() const 	{ return _quest->service(); }
	const xstr_t& method() const 	{ return _quest->method(); }

	VDict context() const		{ return VDict(_quest->context_dict()); }
};

class AnswerReader: public VDict
{
	AnswerPtr _answer;
public:
	AnswerReader(const AnswerPtr& answer)
		: VDict(answer->args_dict()), _answer(answer)
	{
	}

	operator const AnswerPtr&() const	{ return _answer; }

	int64_t txid() const 		{ return _answer->txid(); }
	int status() const 		{ return _answer->status(); }
};


} // namespace xic

#endif
