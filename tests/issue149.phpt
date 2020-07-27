--TEST--
Issue #149 (msgpack (un)pack error with serialize())
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) {
   echo "skip";
}
--INI--
msgpack.php_only=1
--FILE--
Test
<?php
$data = unserialize(file_get_contents(__DIR__."/issue149.ser.txt"));
$check = msgpack_unpack(msgpack_pack($data));
var_dump($check == $data);
?>
OK
--EXPECT--
Test
bool(true)
OK
