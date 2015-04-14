--TEST--
unpack/pack str8
--SKIPIF--
<?php
	if (version_compare(PHP_VERSION, '5.2.0') < 0) {
		echo "skip tests in PHP 5.2 or newer";
	}
--FILE--
<?php
	function test() {
		if(!extension_loaded('msgpack'))
		{
			dl('msgpack.' . PHP_SHLIB_SUFFIX);
		}

		$str = "Simple test for short string - type str8";
		$str8 = chr(0xD9) . chr(strlen($str)) . $str;
		echo msgpack_unpack($str8) . "\n";
		$data = msgpack_pack($str);
		echo ($data[0] == chr(0xD9) && $data[1] == chr(strlen($str)) ?  "OK" : "FAILED"), PHP_EOL;
	}

	test();
?>
--EXPECTF--
Simple test for short string - type str8
OK
