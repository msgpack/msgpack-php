--TEST--
Null byte key position while unpacking objects
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.2.0') < 0) {
    echo "skip tests in PHP 5.2 or newer";
}
--FILE--
<?php
if(!extension_loaded('msgpack'))
{
    dl('msgpack.' . PHP_SHLIB_SUFFIX);
}

function test($type, $array)
{
    $stdClass = hex2bin('c0a8737464436c617373'); // "\0" => 'stdClass'
    $placeholder = hex2bin('a178a178'); // 'x' => 'x'

    $serialized = msgpack_pack($array);
    $serialized = str_replace($placeholder, $stdClass, $serialized);
    $unserialized = msgpack_unpack($serialized);

    var_dump($unserialized);
    unset($array['x']);

    echo $unserialized == (object) $array ? 'OK' : 'ERROR', PHP_EOL;
}

$array = array('x' => 'x', 'foo' => 1);
test('single property, key at the beginning', $array);

$array = array('foo' => 1, 'x' => 'x');
test('single property, key at the end', $array);

$array = array('x' => 'x', 'foo' => 1, 'bar' => 2);
test('multiple properties, key at the beginning', $array);

$array = array('foo' => 1, 'x' => 'x', 'bar' => 2);
test('multiple properties, key in the middle', $array);

$array = array('foo' => 1, 'bar' => 2, 'x' => 'x');
test('multiple properties, key at the end', $array);

$array = array('null' => null, 'x' => 'x');
test('null, key at the end', $array);

$array = array('int' => 1, 'x' => 'x');
test('int, key at the end', $array);

$array = array('float' => 4.2, 'x' => 'x');
test('float, key at the end', $array);

$array = array('string' => 'str', 'x' => 'x');
test('string, key at the end', $array);

$array = array('array' => array(42), 'x' => 'x');
test('array, key at the end', $array);

class Foo { public $a = null; }
$obj = new Foo();
$array = array('object' => $obj, 'x' => 'x');
test('object, key at the end', $array);

--EXPECTF--
object(stdClass)#%d (1) {
  ["foo"]=>
  int(1)
}
OK
object(stdClass)#%d (1) {
  ["foo"]=>
  int(1)
}
OK
object(stdClass)#%d (2) {
  ["foo"]=>
  int(1)
  ["bar"]=>
  int(2)
}
OK
object(stdClass)#%d (2) {
  ["foo"]=>
  int(1)
  ["bar"]=>
  int(2)
}
OK
object(stdClass)#%d (2) {
  ["foo"]=>
  int(1)
  ["bar"]=>
  int(2)
}
OK
object(stdClass)#%d (1) {
  ["null"]=>
  NULL
}
OK
object(stdClass)#%d (1) {
  ["int"]=>
  int(1)
}
OK
object(stdClass)#%d (1) {
  ["float"]=>
  float(4.2)
}
OK
object(stdClass)#%d (1) {
  ["string"]=>
  string(3) "str"
}
OK
object(stdClass)#%d (1) {
  ["array"]=>
  array(1) {
    [0]=>
    int(42)
  }
}
OK
object(stdClass)#%d (1) {
  ["object"]=>
  object(Foo)#%d (1) {
    ["a"]=>
    NULL
  }
}
OK
