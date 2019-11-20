--TEST--
Issue #23 (Empty objects get serialized with incorrect type)
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) {
   echo "skip";
}
--FILE--
<?php
class test {}
print_r(msgpack_unserialize (msgpack_serialize (new test())));
?>
--EXPECTF--
test Object
(
)
