--TEST--
Check for null serialisation
--SKIPIF--
--FILE--
<?php

function test($type, $variable) {
    $serialized = msgpack_serialize($variable);
    $unserialized = msgpack_unserialize($serialized);

    echo $type, PHP_EOL;
    echo bin2hex($serialized), PHP_EOL;
    var_dump($unserialized);
    echo $unserialized === $variable ? 'OK' : 'ERROR', PHP_EOL;
}

test('null', null);
?>
--EXPECT--
null
c0
NULL
OK
