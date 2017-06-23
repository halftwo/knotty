/* $Id: Setting.h,v 1.4 2011/10/11 08:23:29 jiagui Exp $ */
#ifndef Setting_h_
#define Setting_h_

#include "XRefCount.h"
#include "XError.h"
#include "xstr.h"
#include <stdint.h>
#include <string>
#include <vector>

XE_(XError, 	 SettingItemMissingError);
XE_(XError, 	 SettingItemEmptyError);
XE_(XError, 	 SettingItemSyntaxError);

class Setting;
typedef XPtr<Setting> SettingPtr;

SettingPtr newSetting();
SettingPtr loadSetting(const std::string& file);

class Setting: virtual public XRefCount
{
public:
	virtual xstr_t getXstr(const std::string& name, const xstr_t& dft = xstr_null) = 0;

	virtual std::string getString(const std::string& name, const std::string& dft = std::string()) = 0;
	virtual intmax_t getInt(const std::string& name, intmax_t dft = 0) = 0;
	virtual bool getBool(const std::string& name, bool dft = false) = 0;
	virtual double getReal(const std::string& name, double dft = 0.0) = 0;
	
	// the string value as pathname, will be resolved relative to the directory of myFileName()
	virtual std::string getPathname(const std::string& name, const std::string& dft = std::string()) = 0;
	virtual void getStringSeq(const std::string& name, std::vector<std::string>& value) = 0;

	virtual std::string wantString(const std::string& name) = 0;
	virtual intmax_t wantInt(const std::string& name) = 0;
	virtual bool wantBool(const std::string& name) = 0;
	virtual double wantReal(const std::string& name) = 0;

	virtual std::string wantPathname(const std::string& name) = 0;
	virtual void wantStringSeq(const std::string& name, std::vector<std::string>& value) = 0;

	virtual void set(const std::string& name, const std::string& value) = 0;
	virtual bool insert(const std::string& name, const std::string& value) = 0;
	virtual bool update(const std::string& name, const std::string& value) = 0;

	virtual void load(const std::string& file) = 0;
	virtual std::string myFileName() = 0;
	virtual SettingPtr clone() = 0;  

	xstr_t wantXstr(const std::string& name)
	{
		std::string s = wantString(name);
		xstr_t xs = XSTR_CXX(s);
		return xs;
	}
};

#endif
