/* $Id: WithRoot.h,v 1.1 2012/07/11 04:26:22 jiagui Exp $ */
#ifndef WithRoot_h_
#define WithRoot_h_

#include "xsdef.h"
#include <unistd.h>
#include <sys/types.h>

class WithRoot
{
	uid_t _uid;
	mutable bool _root;
public:
	WithRoot()
	{
		_uid = geteuid();
		if (_uid != 0)
			_root = (seteuid(0) == 0);
		else
			_root = true;
	}

	~WithRoot()
	{
		if (_uid != 0 && _root)
		{
			seteuid(_uid);
		}
	}

	bool isRoot() const throw()
	{
		return _root;
	}

	void abdicate() const throw() 
	{
		if (_uid != 0 && _root)
		{
			seteuid(_uid);
			_root = false;
		}
	}
};


#define WITH_ROOT 	WithRoot XS_ANONYMOUS_VARIABLE(_withRoot__)


#endif
