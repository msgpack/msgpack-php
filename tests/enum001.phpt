--TEST--
Enums
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
