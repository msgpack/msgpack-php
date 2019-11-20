--TEST--
Issue #137 (DateTime(Immutable) serialization doesn't work with php 7.4)
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) {
   echo "skip";
}
--FILE--
<?php
var_dump(msgpack_unpack(msgpack_pack(new DatetimeImmutable)));
?>
OK
--EXPECTF--
object(DateTimeImmutable)#%d (3) {
  ["date"]=>
  string(26) "%d-%d-%d %d:%d:%f"
  ["timezone_type"]=>
  int(%d)
  ["timezone"]=>
  string(%d) "%s"
}
OK
