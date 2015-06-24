--TEST--
unpack bin format family
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

	$bin8_serialized = pack('H*', 'c40962696e382074657374');
	$bin8_unserialized = msgpack_unpack($bin8_serialized);
	var_dump($bin8_unserialized);
	echo ($bin8_unserialized == "bin8 test") ?  "OK" : "FAILED", PHP_EOL;

	$bin16_serialized = pack('H*', 'c5000a62696e31362074657374');
	$bin16_unserialized = msgpack_unpack($bin16_serialized);
	var_dump($bin16_unserialized);
	echo ($bin16_unserialized == "bin16 test") ?  "OK" : "FAILED", PHP_EOL;


	$bin32_serialized = pack('H*', 'c60000000a62696e33322074657374');
	$bin32_unserialized = msgpack_unpack($bin32_serialized);
	var_dump($bin32_unserialized);
	echo ($bin32_unserialized == "bin32 test") ?  "OK" : "FAILED", PHP_EOL;
?>
--EXPECTF--
string(9) "bin8 test"
OK
string(10) "bin16 test"
OK
string(10) "bin32 test"
OK
