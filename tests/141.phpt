--TEST--
Check for f32 serialisation
--SKIPIF--
--FILE--
<?php
if(!extension_loaded('msgpack')) {
    dl('msgpack.' . PHP_SHLIB_SUFFIX);
}

function test($type, $variable) {
    $packer = new \MessagePack(false);

    $packer->setOption(\MessagePack::OPT_FORCE_F32, true);

    $serialized = $packer->pack($variable);

    echo $type, PHP_EOL;
    echo bin2hex($serialized), PHP_EOL;
}

test('double: 123.456', 123.456);
?>
--EXPECT--
double: 123.456
ca42f6e979
