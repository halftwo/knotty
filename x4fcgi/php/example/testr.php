<?php

/**
* x4fcgi remote test client program
* @package x4fcgi 
* @author jiagui
*/

try {
	require_once("xic.php");
	xic_engine()->setSecret("@++=hello:world");
	$prx = xic_createProxy("example @ tcp++3030");
	$r = $prx->invoke("foo", array("arg1"=>1.2345, "arg2"=>"hello, world!"));
	print_r($r);
	$r = $prx->invoke("bar", array("arg1"=>1.2345, "arg2"=>"hello, world!"));
	print_r($r);
	#$r = $prx->invoke("oops", array("arg1"=>1.2345, "arg2"=>"hello, world!"));
}
catch (Exception $ex)
{
	print_r($ex);
}

