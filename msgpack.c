
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h" /* for php_info */
#include "ext/standard/php_incomplete_class.h" /* for incomplete_class */
#include "ext/standard/php_var.h" /* for PHP_VAR_SERIALIZE */

#if HAVE_PHP_SESSION
#include "ext/session/php_session.h" /* for php_session_register_serializer */
#endif

#if defined(HAVE_APCU_SUPPORT)
#include "ext/apcu/apc_serializer.h"
#endif /* HAVE_APCU_SUPPORT */

#include "php_msgpack.h"
#include "msgpack_pack.h"
#include "msgpack_unpack.h"
#include "msgpack_class.h"
#include "msgpack_convert.h"
#include "msgpack_errors.h"
#include "msgpack/version.h"

ZEND_DECLARE_MODULE_GLOBALS(msgpack)

static ZEND_FUNCTION(msgpack_serialize);
static ZEND_FUNCTION(msgpack_unserialize);

ZEND_BEGIN_ARG_INFO_EX(arginfo_msgpack_serialize, 0, 0, 1)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_msgpack_unserialize, 0, 0, 1)
    ZEND_ARG_INFO(0, str)
    ZEND_ARG_INFO(0, object)
ZEND_END_ARG_INFO()

PHP_INI_BEGIN()
STD_PHP_INI_BOOLEAN(
    "msgpack.error_display", "1", PHP_INI_ALL, OnUpdateBool,
    error_display, zend_msgpack_globals, msgpack_globals)
STD_PHP_INI_BOOLEAN(
    "msgpack.php_only", "1", PHP_INI_ALL, OnUpdateBool,
    php_only, zend_msgpack_globals, msgpack_globals)
STD_PHP_INI_BOOLEAN(
    "msgpack.assoc", "1", PHP_INI_ALL, OnUpdateBool,
    assoc, zend_msgpack_globals, msgpack_globals)
STD_PHP_INI_BOOLEAN(
    "msgpack.illegal_key_insert", "0", PHP_INI_ALL, OnUpdateBool,
    illegal_key_insert, zend_msgpack_globals, msgpack_globals)
STD_PHP_INI_BOOLEAN(
    "msgpack.use_str8_serialization", "1", PHP_INI_ALL, OnUpdateBool,
    use_str8_serialization, zend_msgpack_globals, msgpack_globals)
STD_PHP_INI_BOOLEAN(
    "msgpack.force_f32", "0", PHP_INI_ALL, OnUpdateBool,
    force_f32, zend_msgpack_globals, msgpack_globals)
PHP_INI_END()

#if HAVE_PHP_SESSION
PS_SERIALIZER_FUNCS(msgpack);
#endif

#if defined(HAVE_APCU_SUPPORT)
/** Apc serializer function prototypes */
static int APC_SERIALIZER_NAME(msgpack) (APC_SERIALIZER_ARGS);
static int APC_UNSERIALIZER_NAME(msgpack) (APC_UNSERIALIZER_ARGS);
#endif

static zend_function_entry msgpack_functions[] = {
    ZEND_FE(msgpack_serialize, arginfo_msgpack_serialize)
    ZEND_FE(msgpack_unserialize, arginfo_msgpack_unserialize)
    ZEND_FALIAS(msgpack_pack, msgpack_serialize, arginfo_msgpack_serialize)
    ZEND_FALIAS(msgpack_unpack, msgpack_unserialize, arginfo_msgpack_unserialize)
    {NULL, NULL, NULL}
};

static void msgpack_init_globals(zend_msgpack_globals *msgpack_globals) /* {{{ */
{
    if (PG(display_errors)) {
        msgpack_globals->error_display = 1;
    } else {
        msgpack_globals->error_display = 0;
    }

    msgpack_globals->php_only = 1;
    msgpack_globals->assoc = 1;

    msgpack_globals->illegal_key_insert = 0;
    msgpack_globals->use_str8_serialization = 1;
    msgpack_globals->serialize.var_hash = NULL;
    msgpack_globals->serialize.level = 0;
}
/* }}} */

static ZEND_MINIT_FUNCTION(msgpack) /* {{{ */ {
    ZEND_INIT_MODULE_GLOBALS(msgpack, msgpack_init_globals, NULL);

    REGISTER_INI_ENTRIES();

#if HAVE_PHP_SESSION
    php_session_register_serializer("msgpack", PS_SERIALIZER_ENCODE_NAME(msgpack), PS_SERIALIZER_DECODE_NAME(msgpack));
#endif

#if defined(HAVE_APCU_SUPPORT)
    apc_register_serializer("msgpack",
        APC_SERIALIZER_NAME(msgpack),
        APC_UNSERIALIZER_NAME(msgpack),
        NULL);
#endif

    msgpack_init_class();

    REGISTER_LONG_CONSTANT("MESSAGEPACK_OPT_PHPONLY",
            MSGPACK_CLASS_OPT_PHPONLY, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MESSAGEPACK_OPT_ASSOC",
            MSGPACK_CLASS_OPT_ASSOC, CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("MESSAGEPACK_OPT_FORCE_F32",
            MSGPACK_CLASS_OPT_FORCE_F32, CONST_CS | CONST_PERSISTENT);

    return SUCCESS;
}
/* }}} */

static ZEND_MSHUTDOWN_FUNCTION(msgpack) /* {{{ */ {
    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}
/* }}} */

static ZEND_MINFO_FUNCTION(msgpack) /* {{{ */ {
    php_info_print_table_start();
    php_info_print_table_row(2, "MessagePack Support", "enabled");
#if HAVE_PHP_SESSION
    php_info_print_table_row(2, "Session Support", "enabled" );
#endif
#if defined(HAVE_APCU_SUPPORT)
    php_info_print_table_row(2, "MessagePack APCu Serializer ABI", APC_SERIALIZER_ABI);
#else
    php_info_print_table_row(2, "MessagePack APCu Serializer ABI", "no");
#endif
    php_info_print_table_row(2, "extension Version", PHP_MSGPACK_VERSION);
    php_info_print_table_row(2, "header Version", MSGPACK_VERSION);
    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();
}
/* }}} */

zend_module_entry msgpack_module_entry = {
    STANDARD_MODULE_HEADER,
    "msgpack",
    msgpack_functions,
    ZEND_MINIT(msgpack),
    ZEND_MSHUTDOWN(msgpack),
    NULL,
    NULL,
    ZEND_MINFO(msgpack),
    PHP_MSGPACK_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_MSGPACK
ZEND_GET_MODULE(msgpack)
#endif

#if HAVE_PHP_SESSION
PS_SERIALIZER_ENCODE_FUNC(msgpack) /* {{{ */
{
    smart_str buf = {0};
    msgpack_serialize_data_t var_hash;

    msgpack_serialize_var_init(&var_hash);
    msgpack_serialize_zval(&buf, &PS(http_session_vars), var_hash);
    msgpack_serialize_var_destroy(&var_hash);

    smart_str_0(&buf);

    return buf.s;
}

/* }}} */

PS_SERIALIZER_DECODE_FUNC(msgpack) /* {{{ */ {
    int ret;
    zend_string *key_str;
    zval tmp, *value;
    msgpack_unpack_t mp;
    size_t off = 0;

    msgpack_unserialize_init(&mp);

    ZVAL_UNDEF(&tmp);
    mp.user.retval = &tmp;
    mp.user.eof = val + vallen;

    ret = msgpack_unserialize_execute(&mp, val, vallen, &off);
    if (Z_TYPE_P(mp.user.retval) == IS_REFERENCE) {
        ZVAL_DEREF(mp.user.retval);
    }

    if (ret == MSGPACK_UNPACK_EXTRA_BYTES || ret == MSGPACK_UNPACK_SUCCESS) {
        msgpack_unserialize_var_destroy(&mp.user.var_hash, 0);

        switch(Z_TYPE_P(mp.user.retval)) {
        case IS_ARRAY:
        case IS_OBJECT:
            ZEND_HASH_FOREACH_STR_KEY_VAL(HASH_OF(mp.user.retval), key_str, value) {
                if (key_str) {
                    php_set_session_var(key_str, value, NULL);
                    php_add_session_var(key_str);
                    ZVAL_UNDEF(value);
                }
            } ZEND_HASH_FOREACH_END();
            break;
        }

        zval_ptr_dtor(&tmp);
    } else {
        msgpack_unserialize_var_destroy(&mp.user.var_hash, 1);
    }

    return SUCCESS;
}
/* }}} */
#endif

PHP_MSGPACK_API void php_msgpack_serialize(smart_str *buf, zval *val) /* {{{ */ {

    msgpack_serialize_data_t var_hash;

    msgpack_serialize_var_init(&var_hash);
    msgpack_serialize_zval(buf, val, var_hash);
    msgpack_serialize_var_destroy(&var_hash);
}
/* }}} */

PHP_MSGPACK_API int php_msgpack_unserialize(zval *return_value, char *str, size_t str_len) /* {{{ */ {
    int ret;
    size_t off = 0;
    msgpack_unpack_t mp;

    if (str_len <= 0) {
        RETVAL_NULL();
        return FAILURE;
    }

    msgpack_unserialize_init(&mp);

    mp.user.retval = return_value;
    mp.user.eof = str + str_len;

    ret = msgpack_unserialize_execute(&mp, str, (size_t)str_len, &off);

    switch (ret) {
        case MSGPACK_UNPACK_NOMEM_ERROR:
            MSGPACK_WARNING("[msgpack] (%s) Memory error", __FUNCTION__);
            break;
        case MSGPACK_UNPACK_PARSE_ERROR:
            MSGPACK_WARNING("[msgpack] (%s) Parse error", __FUNCTION__);
            break;
        case MSGPACK_UNPACK_CONTINUE:
            MSGPACK_WARNING("[msgpack] (%s) Insufficient data for unserializing", __FUNCTION__);
            break;
        case MSGPACK_UNPACK_EXTRA_BYTES:
        case MSGPACK_UNPACK_SUCCESS:
            msgpack_unserialize_var_destroy(&mp.user.var_hash, 0);
            if (off < str_len) {
                MSGPACK_WARNING("[msgpack] (%s) Extra bytes", __FUNCTION__);
            }
            if (Z_ISREF_P(return_value)) {
                /* this must not happen, but may happen on unserializing random invalid data */
                ZVAL_UNREF(return_value);
            }
            return SUCCESS;
        default:
            MSGPACK_WARNING("[msgpack] (%s) Unknown result", __FUNCTION__);
    }
    zval_dtor(return_value);
    msgpack_unserialize_var_destroy(&mp.user.var_hash, 1);
    RETVAL_FALSE;
    return FAILURE;
}
/* }}} */

static ZEND_FUNCTION(msgpack_serialize) /* {{{ */ {
    zval *parameter;
    smart_str buf = {0};

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "z", &parameter) == FAILURE) {
        return;
    }

    php_msgpack_serialize(&buf, parameter);

    if (buf.s) {
        smart_str_0(&buf);
        ZVAL_STR(return_value, buf.s);
    } else {
        RETURN_EMPTY_STRING();
    }

}
/* }}} */

static ZEND_FUNCTION(msgpack_unserialize) /* {{{ */ {
    zend_string *str;
    zval *object = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "S|z", &str, &object) == FAILURE) {
        return;
    }

    if (!str) {
        RETURN_NULL();
    }

    if (object == NULL) {
        php_msgpack_unserialize(return_value, ZSTR_VAL(str), ZSTR_LEN(str));
    } else {
        zval zv;
        php_msgpack_unserialize(&zv, ZSTR_VAL(str), ZSTR_LEN(str));

        if (msgpack_convert_template(return_value, object, &zv) != SUCCESS) {
            RETVAL_NULL();
        }
        zval_ptr_dtor(&zv);
    }
}
/* }}} */

#if defined(HAVE_APCU_SUPPORT)
static int APC_SERIALIZER_NAME(msgpack) ( APC_SERIALIZER_ARGS ) /* {{{ */ {
    smart_str res = {0};
    php_msgpack_serialize(&res, (zval *) value);

    if (res.s) {
        smart_str_0(&res);
        *buf = (unsigned char *) estrndup(ZSTR_VAL(res.s), ZSTR_LEN(res.s));
        *buf_len = ZSTR_LEN(res.s);
        return 1;
    }
    return 0;
}
/* }}} */

static int APC_UNSERIALIZER_NAME(msgpack) ( APC_UNSERIALIZER_ARGS ) /* {{{ */ {
    if (buf_len > 0 && php_msgpack_unserialize(value, buf, buf_len) == SUCCESS) {
        return 1;
    }
    return 0;
}
/* }}} */
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
