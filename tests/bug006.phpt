--TEST--
Bug #6 (bug with incorrect packing of mixed arrays)
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.2.0') < 0) {
    echo "skip tests in PHP 5.2 or newer";
}
if (!extension_loaded("msgpack")) {
   echo "skip"; 
}
--FILE--
<?php
$data = array('key' => 2, 1 => 3);

print_r(msgpack_unpack(msgpack_pack($data)));

$var = array( 1=> "foo", 2 => "bar");

$var[0] = "dummy";

print_r(msgpack_unpack(msgpack_pack($var)));

while ($v = current($var)) {
   var_dump($v);
   next($var);
}
?>
--EXPECTF--
Array
(
    [key] => 2
    [1] => 3
)
Array
(
    [0] => dummy
    [1] => foo
    [2] => bar
)
string(3) "foo"
string(3) "bar"
string(5) "dummy"
