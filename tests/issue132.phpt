--TEST--
Issue #132 (Segmentation fault when using cloned unpacker)
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) {
   echo "skip";
}
if (!extension_loaded("session")) {
    echo "skip - need ext/session";
}
?>
--FILE--
<?php
session_start();
$_SESSION['a'] = 1;
session_write_close();

ini_set('session.serialize_handler', 'msgpack');
session_start();
?>
OK
--EXPECT--
OK
