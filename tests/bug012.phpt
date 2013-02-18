--TEST--
Bug #12 (msgpack_seriallize interfere with php serialize)
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) {
   echo "skip"; 
}
if (version_compare(PHP_VERSION, '5.4.0') < 0) {
    echo "skip tests before PHP 5.4";
}
--FILE--
<?php

class Demo extends ArrayObject {

}

$obj = new StdClass();

$demo = new Demo;

$demo[] = $obj;
$demo[] = $obj;

$data = array(
    $demo,
    $obj,
    $obj,
);

print_r(msgpack_unserialize(msgpack_serialize($data)));
?>
--EXPECTF--
Array
(
    [0] => Demo Object
        (
            [storage:ArrayObject:private] => Array
                (
                    [0] => stdClass Object
                        (
                        )

                    [1] => stdClass Object
                        (
                        )

                )

        )

    [1] => stdClass Object
        (
        )

    [2] => stdClass Object
        (
        )

)
