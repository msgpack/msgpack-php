--TEST--
Test for GH issue #23
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.2.0') < 0) {
    echo "skip tests in PHP 5.2 or newer";
}
--FILE--
<?php
if(!extension_loaded('msgpack'))
{
    dl('msgpack.' . PHP_SHLIB_SUFFIX);
}

class test {
} 

var_dump (msgpack_unserialize (msgpack_serialize (new test())));

echo "OK" . PHP_EOL;

--EXPECTF--
object(test)#%d (0) {
}
OK
