--TEST--
Profiling perf test.
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) {
    die("skip");
}
if (defined("PHP_DEBUG") && PHP_DEBUG) {
    die("skip debug build");
}
?>
--FILE--
<?php
$data_array = array();
for ($i = 0; $i < 5000; $i++) {
    $data_array[random_bytes(10)] = random_bytes(10);
}

$time_start = microtime(true);
for ($i = 0; $i < 4000; $i++) {
    $s = msgpack_serialize($data_array);
    $array = msgpack_unserialize($s);
    unset($array);
    unset($s);
}
$time_end = microtime(true);

if ($time_end <= $time_start) {
    echo "Strange, $i iterations ran in infinite speed: $time_end <= $time_start", PHP_EOL;
} else {
    $speed = $i / ($time_end - $time_start);
    printf("%d iterations took %f seconds: %d/s (%s)\n",
        $i, $time_end - $time_start, $speed, ($speed > 400 ? "GOOD" : "BAD"));
}
?>
--EXPECTF--
%d iterations took %f seconds: %d/s (GOOD)
