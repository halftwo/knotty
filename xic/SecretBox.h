#ifndef SecretBox_h_
#define SecretBox_h_

#include "xslib/XRefCount.h"
#include "xslib/xstr.h"
#include "xslib/xio.h"
#include "xslib/ostk.h"
#include <string>
#include <vector>

class SecretBox;
typedef XPtr<SecretBox> SecretBoxPtr;

class SecretBox: virtual public XRefCount
{
	struct Secret
	{
		xstr_t service;
		xstr_t proto;
		xstr_t host;
		uint8_t ip6[16];
		uint8_t prefix;
		uint16_t port;
		xstr_t identity;
		xstr_t password;

		bool match_host(const xstr_t& host, const uint8_t ip6[]) const;
	};
	typedef std::vector<Secret> SecretVector;

	ostk_t *_ostk;
	time_t _mtime;
	std::string _filename;
	SecretVector _sv;

	void _load();
	void _init(uint8_t *content, ssize_t len);
	SecretBox(const std::string& filename);
	SecretBox(const xstr_t& content);
public:
	static SecretBoxPtr createFromFile(const std::string& filename);
	static SecretBoxPtr createFromContent(const std::string& content);
	virtual ~SecretBox();

	SecretBoxPtr reload();

	std::string filename() const		{ return _filename; }

	void dump(xio_write_function write, void *cookie);

	std::string getContent();

	size_t count() const			{ return _sv.size(); }

	bool getItem(size_t index, xstr_t& service, xstr_t& proto, xstr_t& host, int& port, xstr_t& identity, xstr_t& password);

	bool find(const xstr_t& service, const xstr_t& proto, const xstr_t& host, int port, xstr_t& identity, xstr_t& password);
	bool find(const std::string& service, const std::string& proto, const std::string& host, int port, xstr_t& identity, xstr_t& password);
};

#endif
