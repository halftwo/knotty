<?php

/**
* An example entry file for php xic service
* @package example 
* @author jiagui 
*/

# NB: This require_once("x4fcgi.php") should be the first statement.
require_once("x4fcgi.php");

require_once("ExampleServant.php");

// You are not supposed to echo or print to the output directly.
// Direct outputs will be discarded by x4fcgi_serve().

$servant = new ExampleServant();

x4fcgi_serve(array($servant, 'process'));

