--TEST--
Map with assoc option
--SKIPIF--
--FILE--
<?php

if (!extension_loaded('msgpack')) {
    dl('msgpack.' . PHP_SHLIB_SUFFIX);
}

function test(string $type, $data, bool $phpOnly, bool $assoc)
{
    $msgpack = new MessagePack();
    if (version_compare(PHP_VERSION, '5.1.0') <= 0) {
        $msgpack->setOption(MESSAGEPACK_OPT_PHPONLY, $phpOnly);
        $msgpack->setOption(MESSAGEPACK_OPT_ASSOC, $assoc);
    } else {
        $msgpack->setOption(MessagePack::OPT_PHPONLY, $phpOnly);
        $msgpack->setOption(MessagePack::OPT_ASSOC, $assoc);
    }
    if (is_string($data)) {
        $result = $msgpack->unpack(hex2bin($data));
    } else {
        $result = bin2hex($msgpack->pack($data));
    }

    printf("%s, %d, %d\n", $type, $phpOnly, $assoc);
    var_dump($result);
    return $result;
}

$emptyMapData = '80'; // {}
test("empty map unpack", $emptyMapData, true, true);
test("empty map unpack", $emptyMapData, false, true);
test("empty map unpack", $emptyMapData, false, false);

$mapData = '82a131a142a130a141'; // {"1":"B","0":"A"}
$map = test("map unpack", $mapData, true, true);
test("map pack", $map, true, true);
$map = test("map unpack", $mapData, true, false);
test("map pack", $map, true, false);

$obj = new MyObj();
$obj->member = 1;
test("obj pack", $obj, true, true);
test("obj pack", $obj, false, true);
test("obj pack", $obj, true, false);
test("obj pack", $obj, false, false);

class MyObj
{
    public $member;
}

--EXPECTF--
empty map unpack, 1, 1
object(stdClass)#%d (0) {
}
empty map unpack, 0, 1
array(0) {
}
empty map unpack, 0, 0
object(stdClass)#%d (0) {
}
map unpack, 1, 1
array(2) {
  [1]=>
  string(1) "B"
  [0]=>
  string(1) "A"
}
map pack, 1, 1
string(14) "8201a14200a141"
map unpack, 1, 0
object(stdClass)#%d (2) {
  ["1"]=>
  string(1) "B"
  ["0"]=>
  string(1) "A"
}
map pack, 1, 0
string(18) "82a131a142a130a141"
obj pack, 1, 1
string(32) "82c0a54d794f626aa66d656d62657201"
obj pack, 0, 1
string(4) "9101"
obj pack, 1, 0
string(32) "82c0a54d794f626aa66d656d62657201"
obj pack, 0, 0
string(18) "81a66d656d62657201"
