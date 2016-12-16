#ifndef PHP_MSGPACK_H
#define PHP_MSGPACK_H

#include "Zend/zend_smart_str.h" /* for smart_string */

#define PHP_MSGPACK_VERSION "2.0.3-dev"

extern zend_module_entry msgpack_module_entry;
#define phpext_msgpack_ptr &msgpack_module_entry

#ifdef PHP_WIN32
#   define PHP_MSGPACK_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#   define PHP_MSGPACK_API __attribute__ ((visibility("default")))
#else
#   define PHP_MSGPACK_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

ZEND_BEGIN_MODULE_GLOBALS(msgpack)
    zend_bool error_display;
    zend_bool php_only;
    zend_bool illegal_key_insert;
    zend_bool use_str8_serialization;
    struct {
        void *var_hash;
        unsigned level;
    } serialize;
ZEND_END_MODULE_GLOBALS(msgpack)

ZEND_EXTERN_MODULE_GLOBALS(msgpack)

#ifdef ZTS
#define MSGPACK_G(v) TSRMG(msgpack_globals_id, zend_msgpack_globals *, v)
#else
#define MSGPACK_G(v) (msgpack_globals.v)
#endif

PHP_MSGPACK_API void php_msgpack_serialize(
    smart_str *buf, zval *val);
PHP_MSGPACK_API void php_msgpack_unserialize(
    zval *return_value, char *str, size_t str_len);

#endif  /* PHP_MSGPACK_H */
