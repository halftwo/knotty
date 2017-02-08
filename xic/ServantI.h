#ifndef ServantI_h_
#define ServantI_h_

#include "Engine.h"
#include "xslib/xstr.h"
#include "xslib/ostk.h"
#include "xslib/xatomic.h"


namespace xic
{


struct MethodTab
{
	struct PairType
	{
		const char *name;
		Servant::MethodFunction func;
	};

	class NodeType
	{
		friend class MethodTab;
		NodeType(const char *name, size_t nlen, const Servant::MethodFunction& func);
		NodeType* hash_next;
	public:
		mutable xatomic64_t ncall;
		Servant::MethodFunction func;
		uint32_t hash;
		uint32_t nlen;
		mutable bool mark;
		char name[];
	};

	MethodTab(const PairType* pairs, size_t size);
	~MethodTab();

	NodeType* insert(const char *name, size_t nlen, const Servant::MethodFunction& func);
	NodeType* find(const xstr_t& name) const;
	NodeType* next(const NodeType *node) const;

	void mark(const xstr_t& method, bool on=true) const;
	void markMany(const xstr_t& methods, bool on=true) const;

	mutable xatomic64_t notFound;
private:
	ostk_t *_ostk;
	NodeType **_tab;
	unsigned int _mask;
	unsigned int _total;
};


AnswerPtr process_servant_method(Servant* srv, const MethodTab* mtab, 
				const QuestPtr& quest, const Current& current);


class ServantI: public Servant
{
public:
	typedef xic::MethodTab MethodTab;

	ServantI(const MethodTab* mtab)
		: _mtab(mtab)
	{
	}

	virtual AnswerPtr process(const QuestPtr& quest, const Current& current)
	{
		return process_servant_method(this, _mtab, quest, current);
	}

private:
	const MethodTab* _mtab;
};


}; // namespace xic

#endif

