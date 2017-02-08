#include "XRefCount.h"

#ifdef XSLIB_RCSID
static const char rcsid[] = "$Id: XRefCount.cpp,v 1.5 2012/09/20 03:21:47 jiagui Exp $";
#endif


bool XPtr_throw_on_null;

const XPtr_Ready_t XPTR_READY = XPtr_Ready_t();


#ifdef TEST_XREFCOUNT

#include <iostream>

using namespace std;

class B: virtual public XRefCount
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
	B *t = NULL;
	XPtr<B> x, b = new D;
	XPtr<D> y, d = new D;
	x = new D;
	t = x.reset_and_return_zombie(d.get());
	if (t)
		t->xref_destroy();
	y = XPtr<D>::cast(b);
	cerr << "x->xref_count()=" << x->xref_count() << endl;
	return 0;
}

#endif

