--TEST--
Check for double NaN, Inf, -Inf, 0, and -0
--FILE--
<?php
function test($type, $variable, $check) {
    $serialized = msgpack_serialize($variable);
    $unserialized = msgpack_unserialize($serialized);

    echo $type, PHP_EOL;
    var_dump($variable);
    var_dump($unserialized);
    var_dump($check($unserialized));

    echo PHP_EOL;
}

test('double NaN:', NAN, "is_nan");
test('double Inf:', INF, "is_infinite");
test('double -Inf:', -INF, "is_infinite");
test('double 0.0:', 0.0, function($z) {
        $h = bin2hex(pack("E", $z));
        if ($h === "0000000000000000") {
            return true;
        }
        var_dump($h);
    }
);
test('double -0.0:', -0.0, function($z) {
        $h = bin2hex(pack("E", $z));
        if ($h === "8000000000000000") {
            return true;
        }
        var_dump($h);
    }
);

--EXPECTF--
double NaN:
float(NAN)
float(NAN)
bool(true)

double Inf:
float(INF)
float(INF)
bool(true)

double -Inf:
float(-INF)
float(-INF)
bool(true)

double 0.0:
float(0)
float(0)
bool(true)

double -0.0:
float(-0)
float(-0)
bool(true)
