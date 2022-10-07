#ifndef PHP_MSGPACK_H
#define PHP_MSGPACK_H

#include "Zend/zend_smart_str.h" /* for smart_string */

#define PHP_MSGPACK_VERSION "2.2.0RC2"

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
PHP_MSGPACK_API int php_msgpack_unserialize(
    zval *return_value, char *str, size_t str_len);

#ifdef WORDS_BIGENDIAN
# define MSGPACK_ENDIAN_BIG_BYTE 1
# define MSGPACK_ENDIAN_LITTLE_BYTE 0
#else
# define MSGPACK_ENDIAN_LITTLE_BYTE 1
# define MSGPACK_ENDIAN_BIG_BYTE 0
#endif

#if PHP_VERSION_ID < 80000
# define OBJ_FOR_PROP(zv) (zv)
#else
# define OBJ_FOR_PROP(zv) Z_OBJ_P(zv)
#endif

#if PHP_VERSION_ID >= 80000
# define IS_MAGIC_SERIALIZABLE(ce) (ce && ce->__serialize)
# define CALL_MAGIC_SERIALIZE(cep, zop, rvp) zend_call_known_instance_method_with_0_params((cep)->__serialize, zop, rvp)
# define CALL_MAGIC_UNSERIALIZE(cep, zop, rvp, zsdap) zend_call_known_instance_method_with_1_params((cep)->__unserialize, zop, rvp, zsdap)
#elif PHP_VERSION_ID >= 70400
# define IS_MAGIC_SERIALIZABLE(ce) (ce && zend_hash_str_exists(&ce->function_table, ZEND_STRL("__serialize")))
# define CALL_MAGIC_SERIALIZE(cep, zop, rvp) call_magic_serialize_fn(cep, zop, rvp, ZEND_STRL("__serialize"), 0, NULL)
# define CALL_MAGIC_UNSERIALIZE(cep, zop, rvp, zsdap) call_magic_serialize_fn(cep, zop, rvp, ZEND_STRL("__unserialize"), 1, zsdap)
static inline void call_magic_serialize_fn(zend_class_entry *ce, zend_object *object, zval *retval_ptr, const char *fn_str, size_t fn_len, int param_count, zval *params) {
	zval retval;
	zend_fcall_info fci;
	zend_fcall_info_cache fcic;

	fci.size = sizeof(fci);
	fci.object = object;
	fci.retval = retval_ptr ? retval_ptr : &retval;
	fci.param_count = param_count;
	fci.params = params;
	ZVAL_UNDEF(&fci.function_name); /* Unused */

	fcic.function_handler = zend_hash_str_find_ptr(&ce->function_table, fn_str, fn_len);
	fcic.object = object;
	fcic.called_scope = ce;

	int result = zend_call_function(&fci, &fcic);
	if (UNEXPECTED(result == FAILURE)) {
		if (!EG(exception)) {
			zend_error_noreturn(E_CORE_ERROR, "Couldn't execute method %s%s%s",
				fcic.function_handler->common.scope ? ZSTR_VAL(fcic.function_handler->common.scope->name) : "",
				fcic.function_handler->common.scope ? "::" : "", ZSTR_VAL(fcic.function_handler->common.function_name));
		}
	}

	if (!retval_ptr) {
		zval_ptr_dtor(&retval);
    }
}
#endif

#endif  /* PHP_MSGPACK_H */
