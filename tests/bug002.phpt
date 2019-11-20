--TEST--
Bug #2 (Deserializing a large array of nested objects gives "zend_mm_heap corrupted")
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) {
   echo "skip";
}
?>
--FILE--
<?php

$data = array();

$tmp = &$data;
for ($i = 0; $i < 1023; $i++) {
    $tmp[] = array();
    $tmp = &$tmp[0];
}

$newdata = msgpack_unserialize(msgpack_serialize($data));
var_dump($newdata == $data);
?>
--EXPECTF--
bool(true)
