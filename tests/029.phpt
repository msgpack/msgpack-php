--TEST--
Msgpack module info
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) print "skip";
if (!extension_loaded("session")) {
   echo "skip needs session enabled";
}
?>
--FILE--
<?php
ob_start();
phpinfo(INFO_MODULES);
$str = ob_get_clean();

$array = explode("\n", $str);

$section = false;
$blank = 0;
foreach ($array as $key => $val)
{
    if (strcmp($val, 'msgpack') == 0 || $section)
    {
        $section = true;
    }
    else
    {
        continue;
    }

    if (empty($val))
    {
        $blank++;
        if ($blank == 3)
        {
            $section = false;
        }
    }

    echo $val, PHP_EOL;
}
--EXPECTF--
msgpack

MessagePack Support => enabled
Session Support => enabled
extension Version => %s
header Version => %s

Directive => Local Value => Master Value
msgpack.error_display => %s => %s
msgpack.illegal_key_insert => %s => %s
msgpack.php_only => %s => %s
msgpack.use_str8_serialization => %s => %s
