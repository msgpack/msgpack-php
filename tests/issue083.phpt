--TEST--
Issue #83 (Arrays and negative index)
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) {
    die("skip");
}
?>
--FILE--
<?php

var_dump(msgpack_unpack(msgpack_pack([-1=>-1,1=>1])));

$a = [
    -1 => -1,
    0,
    1 => 1,
    2
];
// next free element == 3 and count() == 3, but it's still not an MP array
unset($a[1]);
var_dump(msgpack_unpack(msgpack_pack($a)));

?>
OK
--EXPECT--
array(2) {
  [-1]=>
  int(-1)
  [1]=>
  int(1)
}
array(3) {
  [-1]=>
  int(-1)
  [0]=>
  int(0)
  [2]=>
  int(2)
}
OK
