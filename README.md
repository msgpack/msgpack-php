# Msgpack for PHP
[![Build Status](https://secure.travis-ci.org/msgpack/msgpack-php.png)](https://travis-ci.org/msgpack/msgpack-php)

This extension provide API for communicating with MessagePack serialization. 

MessagePack is a binary-based efficient object serialization library.
It enables to exchange structured objects between many languages like JSON.
But unlike JSON, it is very fast and small.

## Requirement
- PHP 7.0 +

## Install

### Install from PECL
Msgpack is an PECL extension, thus you can simply install it by:

```shell
$ pecl install msgpack
```

### Compile Msgpack from source
```shell
$ /path/to/phpize
$ ./configure --with-php-config=/path/to/php-config
$ make && make install
```

### Example
```php
<?php
$data = array(0 => 1, 1 => 2, 2 => 3);
$msg = msgpack_pack($data);
$data = msgpack_unpack($msg);
```

### Advanced Example
```php
<?php
$data = array(0 => 1, 1 => 2, 2 => 3);
$packer = new \MessagePack(false);
// ^ same as $packer->setOption(\MessagePack::OPT_PHPONLY, false);
$packed = $packer->pack($data);

$unpacker = new \MessagePackUnpacker(false);
// ^ same as $unpacker->setOption(\MessagePack::OPT_PHPONLY, false);
$unpacker->feed($packed);
$unpacker->execute();
$unpacked = $unpacker->data();
$unpacker->reset();
```

### Advanced Streaming Example
```php
<?php
$data1 = array(0 => 1, 1 => 2, 2 => 3);
$data2 = array("a" => 1, "b" => 2, "c" => 3);

$packer = new \MessagePack(false);
$packed1 = $packer->pack($data1);
$packed2 = $packer->pack($data2);

$unpacker = new \MessagePackUnpacker(false);
$buffer = "";
$nread = 0;

//Simulating streaming data :)
$buffer .= $packed1;
$buffer .= $packed2;

while(true) {
   if($unpacker->execute($buffer, $nread)) {
       $msg = $unpacker->data();
       
       var_dump($msg);
       
       $unpacker->reset();
       $buffer = substr($buffer, $nread);
       $nread = 0;
       if(!empty($buffer)) {
            continue;
       }
   }
   break;
}

```

## Resources
 * [msgpack](http://msgpack.org/)
