<?php
/**
 * xic functions that implemented using PHP (instead of C language).
 * @package xic
 * @author jiagui
 * @version 201202.201202.15
 */
#
# xic.so extension includes following constants:
#	VBS_SPECIAL_DESCRIPTOR
#	VBS_DESCRIPTOR_MAX
#
# xic.so extension includes following functions:
#	xic_Engine xic_engine();
#
#	void xic_set_rid(string $rid);
#	string xic_rid();
#
#	string xic_self();
#
#	v_Blob vbs_blob(string $s);
#	v_Dict vbs_dict(array $a);
#	v_Decimal vbs_decimal(string $s);
#	v_Data vbs_data(mixed $d, int descriptor);
#
#	string vbs_encode(mixed $value);
#
#	// If the value of used == -1, decode blob as string
#	mixed vbs_decode(string $vbs [, int &$used]);
#	void vbs_encode_write(resource handle, mixed $value);
#
#	string vbs_pack(array $values);
#	array vbs_unpack(string $vbs, int $offset, int $num [, int &$used]);
#
#	void dlog(string $identity, string $tag, mixed $content);
#
#
# xic.so extension also includes an exception class XError with construction function
#	XError::__construct([string $message [, long $code [, string $tag [, Exception $previous = NULL]]]]);
#
#
# Object of class xic_Engine has following method:
#	xic_Proxy xic_Engine::stringToProxy(string $proxy);
#	void xic_Engine::setSecret(string $secret);
#	string xic_Engine::getSecret();
#
#
# Object of class xic_Proxy has folloing methods:
#	// If $ctx has key "BLOB2STR" with TRUE or non-zero integer value, the blobs in answer are decoded as string
#	array xic_Proxy::invoke(string $method, array $args [, array $ctx]);
#	void xic_Proxy::invokeOneway(string $method, array $args [, array $ctx]);
#	string xic_Proxy::service();
#

function xic_ctx($init_array=NULL)
{
	return is_array($init_array) ? $init_array : array();
}

function xic_ctx_cache($second)
{
	return array("CACHE" => intval($second));
}

function xic_setSecret($secret)
{
	return xic_engine()->setSecret($secret);
}

function xic_getSecret()
{
	return xic_engine()->getSecret();
}

function xic_createProxy($proxystr)
{
	if (strpos($proxystr, '@') === FALSE)
	{
		$proxystr .= '@tcp++9999 timeout=70000';
	}
	$prx = xic_engine()->stringToProxy($proxystr);
	return $prx;
}

?>
