--TEST--
Issue #80 (Serialized failed on unseted value)
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) {
   echo "skip";
}
--FILE--
<?php

class MyClass
{
    protected $field;

    public function preSerialize()
    {
        unset($this->field);
    }
}

$t = new MyClass();
$t->preSerialize();
var_dump(msgpack_unserialize(msgpack_serialize($t)));
?>
--EXPECTF--
object(MyClass)#%d (1) {
  ["field":protected]=>
  NULL
}
