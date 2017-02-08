#include "xptr.h"

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: xptr.cpp,v 1.5 2011/10/17 11:16:33 jiagui Exp $";
#endif

bool xptr_throw_on_null;


void *xptr_refcount::operator new(size_t size)
{
	return malloc(size);
}

void xptr_refcount::operator delete(void *ptr)
{
	return free(ptr);
}


#ifdef TEST_XPTR

#include <iostream>

using namespace std;

class B
{
public:
	B()		{ cerr << "B::B()" << endl; }
	virtual ~B() 	{ cerr << "B::~B()" << endl; }
	int n;
};

class D: public B
{
public:
	D()		{ cerr << "D::D()" << endl; }
	virtual ~D() 	{ cerr << "D::~D()" << endl; }
};

int main()
{
	xptr<B> x, b = new D;
	xptr<D> y, d = new D;
	x = new D;
	x = d;
	y = xptr<D>::cast(b);
	cerr << "x.use_count()=" << x.use_count() << endl;
	return 0;
}

#endif

