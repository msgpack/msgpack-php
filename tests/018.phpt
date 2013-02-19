--TEST--
Object test, __sleep error cases
--SKIPIF--
--FILE--
<?php
if(!extension_loaded('msgpack')) {
    dl('msgpack.' . PHP_SHLIB_SUFFIX);
}

function test($type, $variable, $test) {
    $serialized = msgpack_serialize($variable);
    $unserialized = msgpack_unserialize($serialized);

    echo $type, PHP_EOL;
    echo bin2hex($serialized), PHP_EOL;
    var_dump($unserialized);
    echo $test || $unserialized == $variable ? 'OK' : 'ERROR', PHP_EOL;
}

class Obj {
    var $a;
    var $b;

    function __construct($a, $b) {
        $this->a = $a;
        $this->b = $b;
    }

    function __sleep() {
        return array('c');
    }

#   function __wakeup() {
#       $this->b = $this->a * 3;
#   }
}

class Opj {
    var $a;
    var $b;

    function __construct($a, $b) {
        $this->a = $a;
        $this->b = $b;
    }

    function __sleep() {
        return array(1);
    }

#   function __wakeup() {
#
#   }
}

$o = new Obj(1, 2);
$p = new Opj(1, 2);

test('nonexisting', $o, true);
test('wrong', $p, true);
?>
--EXPECTF--
Notice: [msgpack] (msgpack_serialize_class) "c" returned as member variable from __sleep() but does not exist in %s on line %d
nonexisting
82c0a34f626aa163c0
object(Obj)#%d (3) {
  ["a"]=>
  NULL
  ["b"]=>
  NULL
  ["c"]=>
  NULL
}
OK

Notice: [msgpack] (msgpack_serialize_class) __sleep should return an array only containing the names of instance-variables to serialize in %s018.php on line %d

Warning: [msgpack] (php_msgpack_unserialize) Insufficient data for unserializing in %s018.php on line %d
wrong
82c0a34f706a
bool(false)
OK
