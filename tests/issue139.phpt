--TEST--
Issue #139 (Segmentation fault when using cloned unpacker)
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) {
   echo "skip";
}
--FILE--
<?php
try {
    $unpacker = new \MessagePackUnpacker(true);
    $unpacker = clone $unpacker; // <-- this line is causing the segmentation fault error
    $unpacker->feed("\xc3");
    $unpacker->execute();
    $data = $unpacker->data();
} catch (\Throwable $e) {
    echo $e->getMessage(),"\n";
}
?>
OK
--EXPECT--
Trying to clone an uncloneable object of class MessagePackUnpacker
OK