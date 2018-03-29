--TEST--
Bug #2 (Deserializing a large array of nested objects used to give "zend_mm_heap corrupted", now gives parse error)
--SKIPIF--
--FILE--
<?php
if (!extension_loaded("msgpack")) {
   dl('msgpack.' . PHP_SHLIB_SUFFIX);
}

$data = array();
$tmp = &$data;
for ($i = 0; $i < 1024; $i++) {
    $tmp[] = array();
    $tmp = &$tmp[0];
}

// Count the number of first-array-elements to confirm the large data structure
var_dump(substr_count(print_r($data, true), "[0]"));

$newdata = msgpack_unserialize(msgpack_serialize($data));
var_dump($newdata == $data);
?>
--EXPECTF--
int(1024)

Warning: [msgpack] (php_msgpack_unserialize) Parse error in %s on line %d
bool(false)
