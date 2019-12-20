<?php

ini_set('memory_limit', '32G');


// PARFS speed test

echo "PHP Version: ".phpversion()." @ ".gethostname()."\n";

$sz = function ($f, $fu, $data, $iterations=10, $gz=1) {
    $raw_size = 0;
    $t0 = microtime(1);
    foreach (range(1, $iterations) as $XX) {
        if ($f == 'json_encode')
            $y = json_encode($data, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
        else
            $y = $f($data);
        if (! $raw_size)
            $raw_size = strlen($y);
        if ($gz)
            $y = gzdeflate($y);
    }
    $time = microtime(1) - $t0;
    $zip_size = strlen($y);

    $t0 = microtime(1);
    foreach (range(1, $iterations) as $XX) {
        if ($gz)
            $z = gzinflate($y);
        else
            $z = $y;
        if ($f == 'json_encode')
            $z = json_decode($z, 1);
        #elseif ($f == 'serialize')
        #    $z = unserialize(gzinflate($y), false);  // PHP7 ONLY
        else
            $z = $fu($z);
    }
    $time2 = microtime(1) - $t0;

    return str_pad($f.":", 20).
        " ".str_pad(number_format($raw_size), 10, " ",STR_PAD_LEFT).
        " ".str_pad(number_format($zip_size), 10, " ",STR_PAD_LEFT).
        " ".str_pad(number_format($time2 / $iterations, 4), 9, " ",STR_PAD_LEFT).
        " ".str_pad(number_format($time / $iterations, 4), 10, " ",STR_PAD_LEFT);
};


$size = 1500;
$data = [];
for($i=0; $i<$size; $i++){
    for($j=0; $j<$size; $j++){
        $data[$i][$j] = [$i, "a$i" => "b$j"];
    }
}
$data_set = "array $size x $size";

$iterations = 2;
$mem = number_format(memory_get_peak_usage(1) / 1000000, 1);
echo "Data set: `$data_set`, performed $iterations iterations, data-set-memory-usage: ${mem}M\n",
    "ALGO                   SIZE-RAW  SIZE-GZIP  UNPACK1/sec PACK1/sec  << time per one iteration", "\n",
#   $sz("igbinary_serialize", "igbinary_unserialize", $data, $iterations), "\n",
   $sz("igbinary_serialize", "igbinary_unserialize", $data, $iterations, 0), "\n",
#   $sz("msgpack_pack", "msgpack_unpack", $data, $iterations), "\n",
   $sz("msgpack_pack", "msgpack_unpack", $data, $iterations, 0), "\n"
#           $sz("json_encode", "json_decode", $sd),
#           $sz("serialize", "unserialize", $sd)
;

/*
 RESULTS:
 
 > ./igbinary-vs-msgpack.php 
PHP Version: 5.6.20 @                                                                                                                            
Data set: `array 1500 x 1500`, performed 2 iterations, data-set-memory-usage: 1,306.0M                                                                                            
ALGO                   SIZE-RAW  SIZE-GZIP  UNPACK1/sec PACK1/sec  << time per one iteration
igbinary_serialize:  34,866,787 34,866,787    1.8073     2.1464
msgpack_pack:        34,348,503 34,348,503   51.9373     1.3356

> ./igbinary-vs-msgpack.php 
PHP Version: 7.0.8 @ 
Data set: `array 1500 x 1500`, performed 2 iterations, data-set-memory-usage: 1,101.0M
ALGO                   SIZE-RAW  SIZE-GZIP  UNPACK1/sec PACK1/sec  << time per one iteration
igbinary_serialize:  34,866,787 34,866,787    3.3561     0.6330
msgpack_pack:        34,348,503 34,348,503   91.5198     1.6248

 
 */

