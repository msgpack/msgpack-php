--TEST--
Issue #171 (Serializing & Unserializing Enum)
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) {
    exit('skip because msgpack extension is missing');
}
if (version_compare(PHP_VERSION, '8.1.0', '<')) {
    exit('skip Enum tests in PHP older than 8.1.0');
}
?>
--FILE--
Test
<?php
enum TestEnum {
    case EITHER;
    case OTHER;
}

$packed = msgpack_pack(TestEnum::OTHER);
var_dump(bin2hex($packed));

$unpacked = msgpack_unpack($packed);
var_dump($unpacked);
?>
OK
--EXPECT--
Test
string(36) "82c006a854657374456e756da54f54484552"
enum(TestEnum::OTHER)
OK
