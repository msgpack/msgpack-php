--TEST--
Issue #23 (Empty objects get serialized with incorrect type)
--SKIPIF--
--FILE--
<?php
class test {} 
print_r(msgpack_unserialize (msgpack_serialize (new test())));
?>
--EXPECTF--
test Object
(
)
