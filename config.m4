dnl config.m4 for extension msgpack

PHP_ARG_WITH(msgpack, for msgpack support,
Make sure that the comment is aligned:
[  --with-msgpack             Include msgpack support])

if test "$PHP_MSGPACK" != "no"; then
  AC_MSG_CHECKING([for APC/APCU includes])
  if test -f "$phpincludedir/ext/apcu/apc_serializer.h"; then
    apc_inc_path="$phpincludedir"
    AC_MSG_RESULT([APCU in $apc_inc_path])
    AC_DEFINE(HAVE_APCU_SUPPORT,1,[Whether to enable apcu support])
  else
    AC_MSG_RESULT([not found])
  fi

  PHP_NEW_EXTENSION(msgpack, msgpack.c msgpack_pack.c msgpack_unpack.c msgpack_class.c msgpack_convert.c, $ext_shared)

  ifdef([PHP_INSTALL_HEADERS],
  [
    PHP_INSTALL_HEADERS([ext/msgpack], [php_msgpack.h])
  ], [
    PHP_ADD_MAKEFILE_FRAGMENT
  ])
fi
