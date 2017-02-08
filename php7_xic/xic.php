<?php
/**
 * xic functions that implemented using PHP (instead of C language).
 * @package xic
 * @author jiagui
 */
#
#
# xic.so extension includes following functions:
#	xic_Engine xic_engine();
#	string xic_self_id();
#	string xic_self();
#
#	vbs_Blob vbs_blob(string $s);
#	vbs_Dict vbs_dict(array $a);
#	vbs_Decimal vbs_decimal(string $s);
#
#	string vbs_encode(mixed $value);
#	mixed vbs_decode(string $vbs [, int &$used]);
#	void vbs_encode_write(resource handle, mixed $value);
#
#	string vbs_pack(array $values);
#	array vbs_unpack(string $vbs, int $offset, int $num [, int &$used]);
#
#	void dlog(string $identity, string $tag, string $content);
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
#	array xic_Proxy::invoke(string $method, array $args [, array $ctx]);
#	void xic_Proxy::invokeOneway(string $method, array $args [, array $ctx]);
#	void xic_Proxy::setContext(array $ctx);
#	array xic_Proxy::getContext();
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
