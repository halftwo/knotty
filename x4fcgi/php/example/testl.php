<?php

/**
* x4fcgi local test program
* @package x4fcgi 
* @author jiagui
*/

try {
	require_once("ExampleServant.php");
	$servant = new example\ExampleServant();
	$q = xic_Quest::withMembers("foo", array("arg1"=>1.2345, "arg2"=>"hello, world!"));
	$r = $servant->process($q);
	print_r($r);
}
catch (Exception $ex)
{
	print_r($ex);
}

