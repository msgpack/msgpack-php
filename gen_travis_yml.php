#!/usr/bin/env php
# autogenerated file; do not edit
language: c

addons:
 apt:
  packages:
   - php-cli
   - php-pear

env:
 matrix:
<?php

$cur = "8.0";
$gen = include "./travis/pecl/gen-matrix.php";
$env = $gen([
	"PHP" => ["7.0", "7.1", "7.2", "7.3", "7.4"],
	"enable_debug" => "yes",
	"enable_maintainer_zts" => "yes",
	"enable_json" => "yes"
], [
	"PHP" => "master",
	"enable_debug" => "yes",
	"enable_zts" => "yes",
	"enable_json" => "yes"
], [
	"PHP" => $cur,
	"enable_debug",
	"enable_zts",
	"enable_json" => "yes"
], [
	"CFLAGS" => "'-O0 -g --coverage'",
	"CXXFLAGS" => "'-O0 -g --coverage'",
	"PHP" => $cur,
	"enable_json" => "yes"
]);
foreach ($env as $grp) {
	foreach ($grp as $e) {
		printf("  - %s\n", $e);
	}
}

?>

cache:
 directories:
  - $HOME/cache

before_cache:
 - find $HOME/cache -name '*.gcda' -o -name '*.gcno' -delete

install:
 - make -f travis/pecl/Makefile php

script:
 - make -f travis/pecl/Makefile ext PECL=msgpack
 - make -f travis/pecl/Makefile test

after_success:
 - test -n "$CFLAGS" && cd .libs && bash <(curl -s https://codecov.io/bash) -X xcode -X coveragepy