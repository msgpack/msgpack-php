--TEST--
Check for double NaN, Inf, -Inf, 0, and -0
--FILE--
<?php
function test($type, $variable) {
    $serialized = msgpack_serialize($variable);
    $unserialized = msgpack_unserialize($serialized);

    echo $type, PHP_EOL;
    var_dump($variable);
    var_dump($unserialized);

    echo PHP_EOL;
}

test('double NaN:', NAN);
test('double Inf:', INF);
test('double -Inf:', -INF);
test('double 0.0:', 0.0);
test('double -0.0:', -0.0);

--EXPECTF--
double NaN:
float(NAN)
float(NAN)

double Inf:
float(INF)
float(INF)

double -Inf:
float(-INF)
float(-INF)

double 0.0:
float(0)
float(0)

double -0.0:
float(-0)
float(-0)
