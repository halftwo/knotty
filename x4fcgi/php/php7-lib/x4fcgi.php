<?php

/**
* A framework to provide xic service with php in fcgi.
* @package x4fcgi 
* @author jiagui
* @version 170802.181018.15
* Following is an example program.
*
--------------- BEGIN OF EXAMPLE PROGRAM ------------

# NB: This require_once("x4fcgi.php") should be the first statement.
require_once("x4fcgi.php");

// You are not supposed to echo or print to the output directly.
// Direct outputs will be discarded by x4fcgi_serve().

function do_the_work(xic_Quest $quest)
{
	// do something on the $quest
	// and return result.
	return array('p1'=>$p1, 'p2'=>$p2);
}

x4fcgi_serve('do_the_work');

--------------- END OF EXAMPLE PROGRAM --------------
*
*/

function _discard_direct_output_($msg)
{
	if (strlen($msg) > 0)
	{
		error_log($msg, 4);
	}
	return "";
}

ob_start('_discard_direct_output_');


class xic_MarshalException extends XError
{
}

class xic_MethodNotFoundException extends XError
{
}

class xic_ServantException extends XError
{
}

class xic_Quest
{
	public $txid;		# integer
	public $service;	# string
	public $method;		# string
	public $context;	# associative array - dict
	public $args;		# associative array - dict

	public static function withMembers($method, $args, $context=NULL, $service="")
	{
		$q = new xic_Quest();
		$q->txid = -1;
		$q->service = $service;
		$q->method = $method;
		$q->context = is_array($context) ? $context : array();
		$q->args = $args;
		return $q;
	}

	public static function withInputBytes($input_bytes)
	{
		// input: %d %S %S {} {}
		$input = vbs_unpack($input_bytes, 0, 0, $used);
		if (!(is_array($input) && count($input) == 5))
		{
			throw new xic_MarshalException("vbs_unpack() failed", 400);
		}

		$q = new xic_Quest();
		$q->txid = $input[0];
		$q->service = $input[1];
		$q->method = $input[2];
		$q->context = $input[3];
		$q->args = $input[4];
		return $q;
	}
}

class xic_Servant
{
        // Return an associated array on success
        // or throw an exception on error.
        public function process($quest)
        {
                $method = "_xic_" . $quest->method;
                if (!in_array($method, get_class_methods($this), TRUE))
                {
                        throw new xic_MethodNotFoundException($quest->method, 404);
                }

                return call_user_func(array($this, $method), $quest);
        }
}

/* The $callback argument should be a function of the prototype:
 * 	array the_callback_function(xic_Quest $quest);
 */
function x4fcgi_serve($callback)
{
	$iscli = (php_sapi_name() == "cli");
	$txid = -1;

	try {
		if (!$iscli)
		{
			$input_fp = fopen("php://input", "rb");
			$input_bytes = stream_get_contents($input_fp);
			fclose($input_fp);
			$quest = xic_Quest::withInputBytes($input_bytes);
		}
		else
		{
			global $argc, $argv;
			if ($argc < 2)
			{
				printf("Usage: %s <method> [k1^v1] ... [--ctx1^v1] ...\n", $argv[0]);
				exit(1);
			}

			$program = $argv[0];
			$method = $argv[1];
			$service = basename(dirname(realpath($program)));

			$_SERVER['XIC_SERVICE'] = $service;
			$_SERVER['XIC_METHOD'] = $method;
			$_SERVER['XIC_ENDPOINT'] = 'loopback';

			$quest = xic_Quest::withMembers($method, array(), array(), $service);

			for ($i = 2; $i < $argc; ++$i)
			{
				$kv = explode('^', $argv[$i], 2);
				if (count($kv) < 2)
				{
					continue;
				}

				$k = $kv[0];
				$v = $kv[1];
				if ($v[0] == '~')
				{
					switch ($v[1])
					{
					case 'T':
						$v = TRUE;
						break;
					case 'F':
						$v = FALSE;
						break;
					case 'S':
						if ($v[strlen($v)-1] == '~')
						{
							$v = substr($v, 2, -1);
						}
						break;
					case 'B':
						if ($v[strlen($v)-1] == '~')
						{
							$v = vbs_blob(substr($v, 2, -1));
						}
						break;
					}
				}
				else if (strspn($v, "0123456789") > 0)
				{
					$v = (strcspn($v, ".Ee") == strlen($v)) ? intval($v) : floatval($v);
				}

				if ($k[0] == '-' && $k[1] == '-')
				{
					$quest->context[substr($k, 2)] = $v;
				}
				else
				{
					$quest->args[$k] = $v;
				}
			}
		}

		$txid = $quest->txid;
		if (substr($quest->method, 0, 1) == "\x00")
		{
			if ($quest->method == "\x00ping")
				$out_args = array();
			else
				throw new xic_MethodNotFoundException($quest->method, 404);
		}
		else
		{
			$out_args = call_user_func($callback, $quest);
		}

		$status = 0;
		if (!is_array($out_args))
		{
			$msg = "CALLBACK_RETURN_NON_ARRAY (".gettype($out_args)."): ".$out_args;
			error_log($msg, 4);
			throw new xic_ServantException($msg, 500);
		}
		else if (empty($out_args))
		{
			$out_args = vbs_dict();
		}

		$out_vbs = vbs_pack(array(intval($txid), intval($status), $out_args));
	}
	catch (Throwable $ex)
	{
		$status = -1;
		$raiser = $_SERVER['XIC_METHOD'].'*'.$_SERVER['XIC_SERVICE'].' @'.$_SERVER['XIC_ENDPOINT'];
		$exname = method_exists($ex, "getExname") ? $ex->getExname() : "";
		if ($exname == "")
		{
			$exname = get_class($ex);
		}

		if (strncmp($exname, "xic_", 4) == 0)
			$exname = "xic.".substr($exname, 4);
		else
			$exname = str_replace('\\', '.', $exname);

		$tag = method_exists($ex, "getTag") ? $ex->getTag() : "";
		$out_args = array(
			"raiser" => $raiser,
			"exname" => $exname,
			"code" => $ex->getCode(),
			"tag" => $tag,
			"message" => $ex->getMessage(),
			"detail" => array(
				"file" => $ex->getFile(),
				"line" => $ex->getLine(),
				"calltrace" => $ex->getTrace(),
			),
		);

		try {
			$out_vbs = vbs_pack(array(intval($txid), intval($status), $out_args));
		}
		catch (Throwable $ex)
		{
			$simple_out_args = array(
				"raiser" => $raiser,
				"exname" => $exname,
				"code" => $ex->getCode(),
				"tag" => $tag,
				"message" => $ex->getMessage(),
				"detail" => array(
					"file" => $ex->getFile(),
					"line" => $ex->getLine(),
				),
			);
			$out_vbs = vbs_pack(array(intval($txid), intval($status), $simple_out_args));
		}
	}

	// NB. Do NOT use ob_end_clean(). It will discard the log messages to
	// the stderr. I don't know why. Maybe it depends on some configurations
	// in php.ini.
	ob_end_flush();

	if (!$iscli)
	{
		$ver = intval($_SERVER['XIC4FCGI_VERSION']);
		$ver = ($ver < 1) ? 1 : ($ver > 2) ? 2 : $ver;
		header("XIC4FCGI_VERSION: ".$ver);
		header("Content-Type: application/octet-stream");

		// output: %d %d {...}
		$output_fp = fopen("php://output", "wb");
		if ($ver == 2)
			fwrite($output_fp, "\x00\x00\x00\x00XiC4fCgI\x00\x00\x00\x00", 16);
		fwrite($output_fp, $out_vbs);
		if ($ver == 2)
			fwrite($output_fp, "\x00\x00\x00\x00xIc4FcGi\x00\x00\x00\x00", 16);
		fclose($output_fp);
	}
	else
	{
		$x = vbs_unpack($out_vbs, 0, 0);
		$s = $x[1];
		$o = $x[2];
		printf("answer_status=%d\nanswer_args=", $s);
		var_export($o);
		echo "\n";
	}

	ob_start('_discard_direct_output_');
}


/* vim: set ts=8 sw=8 st=8 noet:
 */
