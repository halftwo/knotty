#ifndef ostk_cxx_h_
#define ostk_cxx_h_

#include "ostk.h"


#define NEW_OBJ_WITH_OSTK(CLASS, OSTK)		do {			\
	ostk_t *_ostk__ = (OSTK);					\
	if (!_ostk__) {							\
		size_t chunk_size = 4096;				\
		if (chunk_size < sizeof(CLASS) + sizeof(ostk_t))	\
			chunk_size = sizeof(CLASS) + sizeof(ostk_t);	\
		_ostk__ = ostk_create(chunk_size);			\
	}								\
	void *p = ostk_hold(_ostk__, sizeof(CLASS));			\
	return new(p) CLASS(_ostk__);					\
} while (0)

#define NEW_OBJ_WITH_OSTK_ARGS(CLASS, OSTK, ...)	do {		\
	ostk_t *_ostk__ = (OSTK);					\
	if (!_ostk__) {							\
		size_t chunk_size = 4096;				\
		if (chunk_size < sizeof(CLASS) + sizeof(ostk_t))	\
			chunk_size = sizeof(CLASS) + sizeof(ostk_t);	\
		_ostk__ = ostk_create(chunk_size);			\
	}								\
	void *p = ostk_hold(_ostk__, sizeof(CLASS));			\
	return new(p) CLASS(_ostk__, __VA_ARGS__);			\
} while (0)

#define DESTROY_OBJ_WITH_OSTK(CLASS, OSTK) 		do {		\
	ostk_t *_ostk__ = (OSTK);					\
	this->~CLASS();							\
	ostk_destroy(_ostk__);						\
} while (0)



class ostk_xstr_maker
{
	ostk_t *_ostk;

public:
	ostk_xstr_maker(ostk_t *ostk)
		: _ostk(ostk)
	{
	}

	~ostk_xstr_maker()
	{
		ostk_object_cancel(_ostk);
	}

	xstr_t end()
	{
		return ostk_object_finish_xstr(_ostk);
	}

	ostk_xstr_maker& operator()(char ch)
	{
		ostk_object_putc(_ostk, ch);
		return *this;
	}

	ostk_xstr_maker& operator()(const char *str)
	{
		ostk_object_puts(_ostk, str);
		return *this;
	}

	ostk_xstr_maker& operator()(const std::string& str)
	{
		ostk_object_grow(_ostk, str.data(), str.length());
		return *this;
	}

	ostk_xstr_maker& operator()(const xstr_t& xs)
	{
		ostk_object_grow(_ostk, xs.data, xs.len);
		return *this;
	}

	ostk_xstr_maker& operator()(intmax_t v)
	{
		ostk_object_printf(_ostk, "%jd", v);
		return *this;
	}

	ostk_xstr_maker& operator()(uintmax_t v)
	{
		ostk_object_printf(_ostk, "%ju", v);
		return *this;
	}

	ostk_xstr_maker& printf(const char *fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		ostk_object_vprintf(_ostk, fmt, ap);
		va_end(ap);
		return *this;
	}

};


#endif 
