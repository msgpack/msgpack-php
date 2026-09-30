--TEST--
MessagePack and MessagePackUnpacker cannot be serialized
--FILE--
<?php
if(!extension_loaded('msgpack')) {
    dl('msgpack.' . PHP_SHLIB_SUFFIX);
}

foreach ([new MessagePack(), new MessagePackUnpacker()] as $object) {
    try {
        serialize($object);
    } catch (Exception $e) {
        echo $e->getMessage(), PHP_EOL;
    }
}

// Before PHP 8.1, only the C: format reaches the handler that throws
$format = PHP_VERSION_ID < 80100 ? 'C' : 'O';

foreach (['MessagePack', 'MessagePackUnpacker'] as $class) {
    try {
        unserialize($format . ':' . strlen($class) . ':"' . $class . '":0:{}');
    } catch (Exception $e) {
        echo $e->getMessage(), PHP_EOL;
    }
}

try {
    msgpack_pack(new MessagePack());
} catch (Exception $e) {
    echo $e->getMessage(), PHP_EOL;
}
?>
--EXPECT--
Serialization of 'MessagePack' is not allowed
Serialization of 'MessagePackUnpacker' is not allowed
Unserialization of 'MessagePack' is not allowed
Unserialization of 'MessagePackUnpacker' is not allowed
Serialization of 'MessagePack' is not allowed
