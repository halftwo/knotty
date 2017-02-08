<?php

/**
* An example php xic Servant
* @package example 
* @author jiagui 
*/

require_once("x4fcgi.php");

class ExampleServant extends xic_Servant
{
	// Methods should be protected.
	// Return an associated array on success
	// or throw an exception on error.

	protected function _xic_foo($quest)
	{
		return array("callee" => xic_self(), "quest" => $quest);
	}

	protected function _xic_bar($quest)
	{
		return array("_GET"=>$_GET,		/* empty */
				"_POST"=>$_POST,	/* empty */
				"_COOKIE"=>$_COOKIE,	/* empty */
				"_SERVER"=>$_SERVER,
				"php"=>phpversion()." ".php_sapi_name(),
			);
	}

	protected function _xic_oops($quest)
	{
		throw new XError("", 999, "TEST_ERROR");
	}
}

