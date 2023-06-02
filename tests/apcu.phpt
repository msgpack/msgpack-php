--TEST--
APCu serialization
--INI--
apc.enabled=1
apc.enable_cli=1
apc.serializer=msgpack
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) print "skip";
if (!extension_loaded("apcu")) {
   echo "skip needs APCu enabled";
}
?>
--FILE--
<?php
echo ini_get('apc.serializer'), "\n";

apcu_store('foo', 100);
var_dump(apcu_fetch('foo'));

$foo = 'hello world';

apcu_store('foo', $foo);
var_dump(apcu_fetch('foo'));

apcu_store('foo\x00bar', $foo);
var_dump(apcu_fetch('foo\x00bar'));

apcu_store('foo', ['foo' => $foo]);
var_dump(apcu_fetch('foo'));

class Foo {
    public $int = 10;
    protected $array = [];
    private $string = 'foo';
}

$a = new Foo;
apcu_store('foo', $a);
unset($a);
var_dump(apcu_fetch('foo'));

?>
===DONE===
--EXPECT--
msgpack
int(100)
string(11) "hello world"
string(11) "hello world"
array(1) {
  ["foo"]=>
  string(11) "hello world"
}
object(Foo)#1 (3) {
  ["int"]=>
  int(10)
  ["array":protected]=>
  array(0) {
  }
  ["string":"Foo":private]=>
  string(3) "foo"
}
===DONE===
