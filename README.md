# Msgpack for PHP
[![Build Status](https://secure.travis-ci.org/msgpack/msgpack-php.png)](https://travis-ci.org/msgpack/msgpack-php)

This extension provide API for communicating with MessagePack serialization. 

MessagePack is a binary-based efficient object serialization library.
It enables to exchange structured objects between many languages like JSON.
But unlike JSON, it is very fast and small.

## Requirement
- PHP 5.0 +

## Install

### Install from PECL
Msgpack is an PECL extension, thus you can simply install it by:
````
pecl install msgpack
````
### Compile Msgpack from source
````
$/path/to/phpize
$./configure 
$make && make install
````

### Example
```php
<?php
$data = array(0=>1,1=>2,2=>3);
$msg = msgpack_pack($data);
$data = msgpack_unpack($msg);
?>
```
# Resources
 * [msgpack](http://msgpack.org/)
