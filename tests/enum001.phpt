--TEST--
Enums
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
<?php
echo "Test\n";
enum Enum {
    case A;
    case B;
    case C;
}
$a = [Enum::A,Enum::B,Enum::C];
$b = msgpack_unpack(msgpack_pack([Enum::A,Enum::B,Enum::C]));
var_dump($a == $b);
?>
DONE
--EXPECT--
Test
bool(true)
DONE
