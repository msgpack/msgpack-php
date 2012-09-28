--TEST--
Bug #2 (Deserializing a large array of nested objects gives "zend_mm_heap corrupted")
--XFAIL--
Bug is not fixed yet
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

$data = array();

$tmp = &$data;
for ($i = 0; $i < 1024; $i++) {
    $tmp[] = array();
    $tmp = &$tmp[0];
}

$newdata = msgpack_unserialize(msgpack_serialize($data));
var_dump($newdata == $data);
?>
--EXPECTF--
bool(true)
