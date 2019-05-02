--TEST--
Issue #81 (Serialize optimization)
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) {
   echo "skip"; 
}
--FILE--
<?php
class MyClass
{
  private $first_field;
  private $second_field;

  public function __construct()
  {
    $this->first_field = 'first_field';
    $this->second_field = 'second_field';
  }

  public function preSerialize()
  {
    unset($this->first_field);
  }
}

$t = new MyClass();
var_dump($t);
var_dump((msgpack_pack($t)));
var_dump(msgpack_unpack(msgpack_pack($t)));

$t = new MyClass();
$t->preSerialize();
var_dump($t);
var_dump((msgpack_pack($t)));
var_dump(msgpack_unpack(msgpack_pack($t)));
?>
--EXPECTF--
object(MyClass)#1 (2) {
  ["first_field":"MyClass":private]=>
  string(11) "first_field"
  ["second_field":"MyClass":private]=>
  string(12) "second_field"
}
string(78) "���MyClass� MyClass first_field�first_field� MyClass second_field�second_field"
object(MyClass)#2 (2) {
  ["first_field":"MyClass":private]=>
  string(11) "first_field"
  ["second_field":"MyClass":private]=>
  string(12) "second_field"
}
object(MyClass)#2 (1) {
  ["second_field":"MyClass":private]=>
  string(12) "second_field"
}
string(45) "���MyClass� MyClass second_field�second_field"
object(MyClass)#1 (2) {
  ["first_field":"MyClass":private]=>
  NULL
  ["second_field":"MyClass":private]=>
  string(12) "second_field"
}
