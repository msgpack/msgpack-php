--TEST--
Bug #13 (ensure that __get/__set aren't called when packing/unpacking)
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) {
   echo "skip";
}
--FILE--
<?php

class magicClass {
    public function __set($name, $value) {
        echo 'Called __set' . PHP_EOL;
        $this->$name = $value;
    }
    public function __get($name) {
        echo 'Called __get' . PHP_EOL;
        return $this->$name;
    }
}

$magicInstance = new \magicClass;
$magicInstance->val = 5;
var_dump($magicInstance);

$packed = msgpack_pack($magicInstance);
var_dump(bin2hex($packed));
$unpacked = msgpack_unpack($packed);
var_dump($unpacked);

?>
--EXPECTF--
Called __set
object(magicClass)#%d (1) {
  ["val"]=>
  int(5)
}
string(36) "82c0aa6d61676963436c617373a376616c05"
object(magicClass)#%d (1) {
  ["val"]=>
  int(5)
}
