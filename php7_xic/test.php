<?php
/*
 * Simple test for XIC
 * @author: jiagui
 */

require_once("xic.php");

$br = (php_sapi_name() == "cli")? "":"<br>";

if(!extension_loaded('xic')) {
	dl('xic.' . PHP_SHLIB_SUFFIX);
}
$module = 'xic';
$functions = get_extension_funcs($module);
echo "Functions available in the xic extension:$br\n";
foreach($functions as $func) {
    echo $func."$br\n";
}
echo "$br\n";


if (extension_loaded($module)) {
	print xic_build_info() . "\n";

	$bb = vbs_blob("world");
	$dd = vbs_dict(array("hello"=>vbs_blob($bb), "ni"=>vbs_data("hao", 1), "float"=>0.0, 'dec'=>vbs_decimal('1.2340000E373')));
	$x = vbs_dict($dd);
	$y = vbs_encode($x);
	$z = vbs_decode($y, $used);
	assert(strlen($y) == $used);

	$a = serialize($x);
	$b = unserialize($a);

	var_dump($z);
	var_dump($b);

	print strlen($y)." ".rawurlencode($y)."\n";
	print strlen($a)." ".$a."\n";

	xic_engine()->setSecret("@++=hello:world");
	$prx = xic_createProxy("Demo @ tcp+localhost+5555 timeout=60000 @ tcp+127.0.0.1+55555 timeout=50000");
	$ctx = array("CALLER"=>"haha", "CACHE"=>60, 'dodo'=>2);
	$prx->setContext($ctx);

	try {
		$r = $prx->invoke("time", array());
		print_r($r);
		print $prx . "\n";
	}
	catch (Exception $ex)
	{
		print_r($ex);
	}
} else {
	$str = "Module $module is not compiled into PHP";
	echo "$str\n";
}
?>
