#ifndef XicException_h_
#define XicException_h_

#include "xslib/XError.h"
#include "xslib/xstr.h"
#include "xslib/vbs.h"
#include "xslib/ostk.h"

namespace xic
{


class XicException: public ::XError {
public:
	XE_DEFAULT_METHODS_EX(::XError, XicException, "xic.XicException");
	virtual bool is_local() const throw()		{ return true; }
};


#define XIE_(BASE, DERIVED)	XE_X(BASE, DERIVED, "xic."XS_TOSTR(DERIVED))


XIE_(XicException, 	ProtocolException)
XIE_(ProtocolException,		MarshalException)
XIE_(ProtocolException,		MessageSizeException)


XIE_(XicException,		QuestFailedException)
XIE_(QuestFailedException, 	ServiceNotFoundException)
XIE_(QuestFailedException, 	ServiceEmptyException)
XIE_(QuestFailedException, 	MethodNotFoundException)
XIE_(QuestFailedException, 	MethodEmptyException)
XIE_(QuestFailedException, 	MethodOnewayException)
XIE_(QuestFailedException,	ParameterException)
XIE_(ParameterException,		ParameterLimitException)
XIE_(ParameterException,		ParameterNameException)
XIE_(ParameterException,		ParameterMissingException)
XIE_(ParameterException,		ParameterTypeException)
XIE_(ParameterException,		ParameterDataException)


XIE_(XicException, 	SocketException)
XIE_(SocketException,		ConnectFailedException)		// Failed to connect to server.
XIE_(SocketException,		ConnectionLostException)	// Connection is broken unexpectedly.


XIE_(XicException, 	TimeoutException)
XIE_(TimeoutException,		ConnectTimeoutException)
XIE_(TimeoutException,		AuthenticateTimeoutException)
XIE_(TimeoutException,		MessageTimeoutException)
XIE_(TimeoutException,		CloseTimeoutException)


XIE_(XicException, 	AuthenticationException)

XIE_(XicException, 	AdapterAbsentException)

XIE_(XicException,	ConnectionClosedException)		// Connection has been closed (gracefully or forcefully).

XIE_(XicException, 	EngineStoppedException)

XIE_(XicException, 	EndpointParseException)

XIE_(XicException, 	EndpointMissingException)

XIE_(XicException, 	ServiceParseException)

XIE_(XicException, 	ProxyFixedException)

XIE_(XicException,	ServantException)


XIE_(XicException,		InternalException)
XIE_(InternalException,		UnknownException)
XIE_(InternalException,		VWriterException)
XIE_(InternalException,		SThreadException)
XIE_(InternalException,		InterruptException)



class RemoteException: public XicException
{
public:
	XE_DEFAULT_METHODS_WITHOUT_EXNAME(XicException, RemoteException);

	/* _file, _line and _calltrace of XicException are local values,
	   _exname, _code, _tag and _message are remote values.
	 */
	RemoteException(const char *file, int line, int code, 
		const std::string& tag, const std::string& msg,
		const std::string& exname, const std::string& raiser, const vbs_dict_t* detail);

	virtual ~RemoteException() throw() {}

	virtual const char* what() const throw();

	virtual const char* exname() const throw() 	{ return _exname.c_str(); }
	virtual bool is_local() const throw()		{ return false; }

	const std::string& raiser() const throw()	{ return _raiser; }
	const vbs_dict_t* detail() const throw()	{ return _detail.dict(); }

private:
	class Detail
	{
	public:
		Detail();
		Detail(const vbs_dict_t* detail);
		Detail(const Detail& r);
		Detail& operator=(const Detail& r);
		~Detail();

		const vbs_dict_t *dict() const	{ return &_dat->_dict; }

	private:
		struct Data
		{
			OREF_DECLARE();
			ostk_t *_ostk;
			vbs_dict_t _dict;

			static Data* create();
			static void destroy(Data *d);
		} *_dat;
	};

	std::string _exname;
	std::string _raiser;
	Detail _detail;
};


};


#endif
