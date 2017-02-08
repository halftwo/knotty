<?php
/*
 * Example file demonstrate the usage of XIC RPC
 * @author: jiagui
 */

require_once("xic.php");

xic_engine()->setSecret("@++=hello:world");

# Direct connect to the Tester service
$prx = xic_createProxy("Tester @tcp+localhost+5555");

# Through KProxy to the Tester service
# KProxy listen on port 9999
#$prx = xic_createProxy("Tester @tcp+localhost+9999");

$ctx = xic_ctx_cache(60); 
$ctx["MASTER"] = true;

$r = $prx->invoke("echo", array("i" => 1, "S"=>"hello", "b"=>TRUE, "r"=>0.0, "B"=>vbs_Blob("BLOBLOB")), $ctx);

var_dump($r);

?>
