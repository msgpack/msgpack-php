#!/usr/bin/env php
<?php echo "# generated file; do not edit!\n"; ?>

name: ci
on:
  workflow_dispatch:
  push:
  pull_request:

jobs:
<?php
function yesno(array $env, string $key) : string {
    if (version_compare($env["PHP"], "8.0", "<") && $key === "zts") {
        $check = "maintainer_zts";
    } else {
        $check = $key;
    }
    if (($env["enable_$check"] ?? null) === "yes") {
        return $key;
    }
    return "no$key";
}
function jobname(string $id, array $env) : string {
    return sprintf("php-%s-%s-%s", $env["PHP"], yesno($env, "debug"), yesno($env, "zts"));
}
$gen = include __DIR__ . "/ci/gen-matrix.php";
$cur = "8.3";
$job = $gen->github([
"old-matrix" => [
// most useful for all additional versions except current
	"PHP" => ["7.4", "8.0", "8.1", "8.2"],
	"enable_debug" => "yes",
	"enable_maintainer_zts" => "yes",
	"enable_session" => "yes",
],
"master" => [
// master
    "PHP" => ["master"],
    "enable_debug" => "yes",
    "enable_zts" => "yes",
    "enable_session" => "yes",
],
"cur-none" => [
// everything disabled for current
    "PHP" => $cur,
    "enable_session" => "no",
],
"cur-dbg-zts" => [
// everything enabled for current, switching debug/zts
    "PHP" => $cur,
    "enable_debug",
    "enable_zts",
    "enable_session" => "yes",
],
"cur-cov" => [
// once everything enabled for current, with coverage
    "CFLAGS" => "-O0 -g --coverage",
    "CXXFLAGS" => "-O0 -g --coverage",
    "PHP" => $cur,
    "enable_session" => "yes",
]]);
foreach ($job as $id => $env) {
    printf("  %s:\n", $id);
    printf("    name: %s\n", jobname($id, $env));
    if ($env["PHP"] == "master") {
        printf("    continue-on-error: true\n");
    }
    printf("    env:\n");
    foreach ($env as $key => $val) {
        printf("      %s: \"%s\"\n", $key, $val);
    }
?>
    runs-on: ubuntu-20.04
    steps:
      - uses: actions/checkout@v2
        with:
          submodules: true
      - name: Install
        run: |
          sudo apt-get install -y \
            php-cli \
            php-pear \
            re2c
      - name: Prepare
        run: |
          make -f scripts/ci/Makefile php || make -f scripts/ci/Makefile clean php
      - name: Build
        run: |
          make -f scripts/ci/Makefile ext PECL=msgpack
      - name: Test
        run: |
          make -f scripts/ci/Makefile test
<?php if (isset($env["CFLAGS"]) && strpos($env["CFLAGS"], "--coverage") != false) : ?>
      - name: Coverage
        if: success()
        run: |
          cd .libs
          bash <(curl -s https://codecov.io/bash) -X xcode -X coveragepy
<?php endif; ?>

<?php
}
