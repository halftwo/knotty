/* $Id: httpcode.h,v 1.2 2012/09/20 03:21:47 jiagui Exp $ */
#ifndef HTTPCODE_H_
#define HTTPCODE_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#define HTTPCODES								\
	HC(100, CONTINUE, 			"Continue")			\
	HC(101, SWITCHING_PROTOCOLS, 		"Switching Protocols")		\
										\
	HC(200, OK, 				"OK")				\
	HC(201, CREATED,			"Created")			\
	HC(202, ACCEPTED,			"Accepted")			\
	HC(203, NON_AUTHORITATIVE_INFORMATION,	"Non-Authoritative Information")\
	HC(204, NO_CONTENT,			"No Content")			\
	HC(205, RESET_CONTENT,			"Reset Content")		\
	HC(206, PARTIAL_CONTENT,		"Partial Content")		\
										\
	HC(300, MULTIPLE_CHOICES,		"Multiple Choices")		\
	HC(301, MOVED_PERMANENTLY,		"Moved Permanently")		\
	HC(302, FOUND,				"Found")			\
	HC(303, SEE_OTHER,			"See Other")			\
	HC(304, NOT_MODIFIED,			"Not Modified")			\
	HC(305, USER_PROXY,			"Use Proxy")			\
	HC(307, TEMPORARY_REDIRECT,		"Temporary Redirect")		\
										\
	HC(400, BAD_REQUEST,			"Bad Request")			\
	HC(401, UNAUTHORIZED,			"Unauthorized")			\
	HC(402, PAYMENT_REQUIRED,		"Payment Required")		\
	HC(403, FORBIDDEN,			"Forbidden")			\
	HC(404, NOT_FOUND,			"Not Found")			\
	HC(405, METHOD_NOT_ALLOWED,		"Method Not Allowed")		\
	HC(406, NOT_ACCEPTABLE,			"Not Acceptable")		\
	HC(407, PROXY_AUTHENTICATION_REQUIRED,	"Proxy Authentication Required")\
	HC(408, REQUEST_TIMEOUT,		"Request Timeout")		\
	HC(409, CONFLICT,			"Conflict")			\
	HC(410, GONE,				"Gone")				\
	HC(411, LENGTH_REQUIRED,		"Length Required")		\
	HC(412, PRECONDITION_FAILED,		"Precondition Failed")		\
	HC(413, REQUEST_ENTITY_TOO_LARGE,	"Request Entity Too Large")	\
	HC(414, REQUEST_URI_TOO_LONG,		"Request-URI Too Long")		\
	HC(415, UNSUPPORTED_MEDIA_TYPE,		"Unsupported Media Type")	\
	HC(416, REQUESTED_RANGE_NOT_SATISFIABLE, "Requested Range Not Satisfiable")\
	HC(417, EXPECTATION_FAILED,		"Expectation Failed")		\
										\
	HC(500, INTERNAL_SERVER_ERROR,		"Internal Server Error")	\
	HC(501, NOT_IMPLEMENTED,		"Not Implemented")		\
	HC(502, BAD_GATEWAY,			"Bad Gateway")			\
	HC(503, SERVICE_UNAVAILABLE,		"Service Unavailable")		\
	HC(504, GATEWAY_TIMEOUT,		"Gateway Timeout")		\
	HC(505, VERSION_NOT_SUPPPORTED,		"HTTP Version Not Supported")	\
										\
	/* END OF HTTPCODES */


enum
{
#define HC(code, name, desc)	HTTPCODE_##name = code,
	HTTPCODES
#undef HC
};


const char *httpcode_description(int code);


#ifdef __cplusplus
}
#endif

#endif

