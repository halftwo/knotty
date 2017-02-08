#include "XicException.h"
#include "VData.h"
#include "xslib/XLock.h"
#include "xslib/cxxstr.h"
#include <sstream>
#include <typeinfo>

using namespace xic;


RemoteException::RemoteException(const char *file, int line, int code,
			const std::string& tag, const std::string& msg,
			const std::string& exname, const std::string& raiser, const vbs_dict_t* detail)
	: XicException(file, line, code, tag, msg), _exname(exname), _raiser(raiser), _detail(detail)
{
}

const char* RemoteException::what() const throw()
{
	XRecMutex::Lock lock(_mutex());
	if (_what.empty())
	{
		std::ostringstream os;
		os << '!' << _exname << '(' << _code << ") on " << _raiser;
		if (!_message.empty())
			os << " --- " << _message;
		os << " --- ";
		iobuf_t ob = make_iobuf(os, NULL, 0);
		vbs_print_dict(_detail.dict(), &ob);
		_what = os.str();
	}
	return _what.c_str();
}

RemoteException::Detail::Detail()
{
	_dat = RemoteException::Detail::Data::create();
}

RemoteException::Detail::Detail(const vbs_dict_t* detail)
{
	_dat = RemoteException::Detail::Data::create();
	if (detail)
		vbs_copy_dict(&_dat->_dict, detail, &ostk_xmem, _dat->_ostk);
}

RemoteException::Detail::Detail(const RemoteException::Detail& r)
	: _dat(r._dat)
{
	if (_dat)
		OREF_INC(_dat);
}

RemoteException::Detail& RemoteException::Detail::operator=(const RemoteException::Detail& r)
{
	if (_dat != r._dat)
	{               
		if (r._dat) OREF_INC(r._dat);
		if (_dat) OREF_DEC(_dat, Data::destroy);
		_dat = r._dat;  
	}               
	return *this;
}

RemoteException::Detail::~Detail()
{
	if (_dat)
	{
		OREF_DEC(_dat, Data::destroy);
	}
}

RemoteException::Detail::Data *RemoteException::Detail::Data::create()
{
	ostk_t *ostk = ostk_create(1024);
	RemoteException::Detail::Data *d = (RemoteException::Detail::Data*)ostk_hold(ostk, sizeof(*d));
	OREF_INIT(d);
	d->_ostk = ostk;
	vbs_dict_init(&d->_dict);
	return d;
}

void RemoteException::Detail::Data::destroy(RemoteException::Detail::Data *d)
{
	if (d)
	{
		ostk_destroy(d->_ostk);
	}
}

