<?php

define('ITERATIONS', 1000000);

# [igbinary serializer PHP extension](https://github.com/igbinary/igbinary)
if (!extension_loaded('igbinary')) {
    throw new DomainException('Required PHP module "igbinary" is not loaded');
}

# [MessagePack binary serializer PHP extension](https://github.com/msgpack/msgpack-php)
if (!extension_loaded('msgpack')) {
    throw new DomainException('Required PHP module "msgpack" is not loaded');
}
ini_set('igbinary.compact_strings', true);

function millitime() {
    return (int) (microtime(true)*1000);
}

function timed(Closure $callable, $data) {
    $s = millitime();
    for ($i = 0; $i < ITERATIONS; $i++) { $callable($data); }
    return millitime() - $s;
}

function report(string $name, array $serializer, array $data, string $dataName) {
    echo "------------------------------\n";
    echo sprintf("[%s] %s:\n", $dataName, $name);
    echo "------------------------------\n";
    $value = $serializer[0]($data);
    echo sprintf("Length:             %s bytes\n", strlen($value));
    echo sprintf("Base64 length:      %s bytes\n", strlen(base64_encode($value)));
    $time1 = timed($serializer[0], $data);
    echo sprintf("Time (serialize):   %s ms\n", $time1);
    $time2 = timed($serializer[1], $value);
    echo sprintf("Time (unserialize): %s ms\n", $time2);
    echo sprintf("Time (total):       %s ms\n", $time1 + $time2);
    if ($data !== $serializer[1]($value)) {
        throw new RuntimeException(
            sprintf('Unserialized data "%s" (by %s serializer) is not valid!', $dataName, $name)
        );
    }
}

$dataSet = json_decode(file_get_contents('x-dataset.json'), true, JSON_THROW_ON_ERROR);

$serializers = [
    "Native" => [
        function(array $data) { return serialize($data); },
        function(string $data) { return unserialize($data); }
    ],
    "Json" => [
        function(array $data) { return json_encode($data); },
        function(string $data) { return json_decode($data, true); }
    ],
    "Igbinary" => [
        function(array $data) { return igbinary_serialize($data); },
        function(string $data) { return igbinary_unserialize($data); }
    ],
    "MessagePack" => [
        function(array $data) { return msgpack_pack($data); },
        function(string $data) { return msgpack_unpack($data); }
    ]
];

foreach ($dataSet as $dataName => $data) {
    foreach ($serializers as $name => $serializer) {
        report($name, $serializer, $data, $dataName);
    }
}

