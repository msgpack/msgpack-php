--TEST--
Issue #91 (private property in base class)
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) {
    die("skip");
}
?>
--FILE--
<?php
class TestBase
{
  private $name = 'default';

  public function getName()
  {
    return $this->name;
  }

  public function setName($name)
  {
    $this->name = $name;
  }
}

class Test extends TestBase
{

}

$test = new Test();

$test->setName('new-name');
var_dump($test, $test->getName());

$new_test = msgpack_unpack(msgpack_pack($test));
var_dump($new_test, $new_test->getName());
?>
OK
--EXPECTF--
object(Test)#%d (1) {
  ["name":"TestBase":private]=>
  string(8) "new-name"
}
string(8) "new-name"
object(Test)#%d (1) {
  ["name":"TestBase":private]=>
  string(8) "new-name"
}
string(8) "new-name"
OK
