--TEST--
Check for f32 serialisation
--SKIPIF--
--INI--
msgpack.force_f32 = 1
--FILE--
<?php
if(!extension_loaded('msgpack')) {
    dl('msgpack.' . PHP_SHLIB_SUFFIX);
}

function test($type, $variable) {
    $packer = new \MessagePack(false);

    $serialized = $packer->pack($variable);

    echo $type, PHP_EOL;
    echo bin2hex($serialized), PHP_EOL;
}

test('double: 123.456', 123.456);
?>
--EXPECT--
double: 123.456
ca42f6e979
