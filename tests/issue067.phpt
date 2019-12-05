--TEST--
Issue #67 (uint64_t)
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) {
    die("skip");
}
?>
--FILE--
<?php
echo msgpack_unpack("\xcf"."\xff\xff\xff\xff"."\xff\xff\xff\xff"), "\n";
?>
OK
--EXPECT--
18446744073709551615
OK
