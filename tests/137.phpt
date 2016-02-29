--TEST--
unpack/pack str8
--INI--
msgpack.use_str8_serialization = 1
--SKIPIF--
<?php
	if (version_compare(PHP_VERSION, '5.2.0') < 0) {
		echo "skip tests in PHP 5.2 or newer";
	}
--FILE--
<?php
	if(!extension_loaded('msgpack')) {
		dl('msgpack.' . PHP_SHLIB_SUFFIX);
	}

	$str = "Simple test for short string - type str8";
	$str8 = chr(0xD9) . chr(strlen($str)) . $str;
	echo msgpack_unpack($str8) . "\n";

	//assert str8 serialization works, and default for use use_str8_serialization is 1
	$data = msgpack_pack($str);
	var_dump(bin2hex($data));
	echo ($data[0] == chr(0xD9) && $data[1] == chr(strlen($str)) ?  "OK" : "FAILED"), PHP_EOL;

	ini_set('msgpack.use_str8_serialization', 0);
	$data = msgpack_pack($str);
	var_dump(bin2hex($data));
	echo ($data[0] == chr(0xDA) && $data[2] == chr(strlen($str)) ?  "OK" : "FAILED"), PHP_EOL;

?>
--EXPECTF--
Simple test for short string - type str8
string(84) "d92853696d706c65207465737420666f722073686f727420737472696e67202d20747970652073747238"
OK
string(86) "da002853696d706c65207465737420666f722073686f727420737472696e67202d20747970652073747238"
OK
