--TEST--
Issue #67 (uint64_t above ZEND_LONG_MAX up to 2^52 unpacks as float on 32-bit)
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) {
    die("skip");
}
if (PHP_INT_SIZE !== 4)
    die("skip this test is for 32-bit platforms only");
?>
--FILE--
<?php
var_dump(msgpack_unpack("\xcf"."\x00\x00\x00\x00"."\x80\x00\x00\x00")); // 2^31
var_dump(msgpack_unpack("\xcf"."\x00\x00\x00\x01"."\x00\x00\x00\x00")); // 2^32
var_dump(msgpack_unpack("\xcf"."\x00\x10\x00\x00"."\x00\x00\x00\x00")); // 2^52
var_dump(msgpack_unpack("\xcf"."\x00\x10\x00\x00"."\x00\x00\x00\x01")); // 2^52 + 1
?>
OK
--EXPECT--
float(2147483648)
float(4294967296)
float(4503599627370496)
string(16) "4503599627370497"
OK
