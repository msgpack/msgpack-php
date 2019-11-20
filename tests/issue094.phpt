--TEST--
Issue #94 (PHP7 segmentation fault with references)
--SKIPIF--
<?php
if (!extension_loaded("msgpack")) {
   echo "skip";
}
--FILE--
<?php
$bad = unserialize('a:4:{i:1;a:1:{s:10:"verylongid";s:1:"1";}i:10;a:1:{s:10:"verylongid";s:2:"10";}i:16;a:1:{s:10:"verylongid";s:2:"16";}i:0;a:1:{s:8:"children";a:3:{i:16;R:6;i:10;R:4;i:1;R:2;}}}');
$p = msgpack_pack($bad);
print_r(msgpack_unpack($p));
--EXPECT--
Array
(
    [1] => Array
        (
            [verylongid] => 1
        )

    [10] => Array
        (
            [verylongid] => 10
        )

    [16] => Array
        (
            [verylongid] => 16
        )

    [0] => Array
        (
            [children] => Array
                (
                    [16] => Array
                        (
                            [verylongid] => 16
                        )

                    [10] => Array
                        (
                            [verylongid] => 10
                        )

                    [1] => Array
                        (
                            [verylongid] => 1
                        )

                )

        )

)
