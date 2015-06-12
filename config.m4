dnl config.m4 for extension msgpack

PHP_ARG_WITH(msgpack, for msgpack support,
Make sure that the comment is aligned:
[  --with-msgpack             Include msgpack support])

if test "$PHP_MSGPACK" != "no"; then
  PHP_NEW_EXTENSION(msgpack, msgpack.c msgpack_pack.c msgpack_unpack.c msgpack_class.c msgpack_convert.c, $ext_shared)

  ifdef([PHP_INSTALL_HEADERS],
  [
    PHP_INSTALL_HEADERS([ext/msgpack], [php_msgpack.h])
  ], [
    PHP_ADD_MAKEFILE_FRAGMENT
  ])
fi
