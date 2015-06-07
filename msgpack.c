
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
    "msgpack.illegal_key_insert", "0", PHP_INI_ALL, OnUpdateBool,
    illegal_key_insert, zend_msgpack_globals, msgpack_globals)
PHP_INI_END()

#if HAVE_PHP_SESSION
PS_SERIALIZER_FUNCS(msgpack);
#endif

static zend_function_entry msgpack_functions[] = {
    ZEND_FE(msgpack_serialize, arginfo_msgpack_serialize)
    ZEND_FE(msgpack_unserialize, arginfo_msgpack_unserialize)
    ZEND_FALIAS(msgpack_pack, msgpack_serialize, arginfo_msgpack_serialize)
    ZEND_FALIAS(msgpack_unpack, msgpack_unserialize, arginfo_msgpack_unserialize)
    {NULL, NULL, NULL}
};

static void msgpack_init_globals(zend_msgpack_globals *msgpack_globals)
{
    TSRMLS_FETCH();

    if (PG(display_errors))
    {
        msgpack_globals->error_display = 1;
    }
    else
    {
        msgpack_globals->error_display = 0;
    }

    msgpack_globals->php_only = 1;

    msgpack_globals->illegal_key_insert = 0;
    msgpack_globals->serialize.var_hash = NULL;
    msgpack_globals->serialize.level = 0;
}

static ZEND_MINIT_FUNCTION(msgpack)
{
    ZEND_INIT_MODULE_GLOBALS(msgpack, msgpack_init_globals, NULL);

    REGISTER_INI_ENTRIES();

#if HAVE_PHP_SESSION
    php_session_register_serializer("msgpack",
                                    PS_SERIALIZER_ENCODE_NAME(msgpack),
                                    PS_SERIALIZER_DECODE_NAME(msgpack));
#endif

    msgpack_init_class();

    REGISTER_LONG_CONSTANT("MESSAGEPACK_OPT_PHPONLY",
            MSGPACK_CLASS_OPT_PHPONLY, CONST_CS | CONST_PERSISTENT);

    return SUCCESS;
}

static ZEND_MSHUTDOWN_FUNCTION(msgpack)
{
    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}

static ZEND_MINFO_FUNCTION(msgpack)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "MessagePack Support", "enabled");
#if HAVE_PHP_SESSION
    php_info_print_table_row(2, "Session Support", "enabled" );
#endif
    php_info_print_table_row(2, "extension Version", PHP_MSGPACK_VERSION);
    php_info_print_table_row(2, "header Version", MSGPACK_VERSION);
    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();
}

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
PS_SERIALIZER_ENCODE_FUNC(msgpack)
{
    smart_string buf = {0};
    zend_string *z_string;
    msgpack_serialize_data_t var_hash;

    msgpack_serialize_var_init(&var_hash);
    msgpack_serialize_zval(&buf, &PS(http_session_vars), var_hash);
    msgpack_serialize_var_destroy(&var_hash);

    z_string = zend_string_init(buf.c, buf.len, 0);
    smart_string_free(&buf);

    return z_string;
}

PS_SERIALIZER_DECODE_FUNC(msgpack)
{
    int ret;
    zend_string *key_str;
    zval tmp, *value;
    size_t off = 0;
    msgpack_unpack_t mp;
    msgpack_unserialize_data_t var_hash;

    template_init(&mp);

    msgpack_unserialize_var_init(&var_hash);

    mp.user.retval = &tmp;
    mp.user.var_hash = &var_hash;

    ret = template_execute(&mp, val, vallen, &off);

    if (ret == MSGPACK_UNPACK_EXTRA_BYTES || ret == MSGPACK_UNPACK_SUCCESS) {
        msgpack_unserialize_set_return_value(&var_hash, &tmp);
        msgpack_unserialize_var_destroy(&var_hash, 0);

        ZEND_HASH_FOREACH_STR_KEY_VAL(HASH_OF(&tmp), key_str, value) {
            if (key_str) {
                php_set_session_var(key_str, value, NULL);
                php_add_session_var(key_str);
            }
        } ZEND_HASH_FOREACH_END();
        zval_ptr_dtor(&tmp);
    } else {
        msgpack_unserialize_var_destroy(&var_hash, 1);
    }

    return SUCCESS;
}
#endif

PHP_MSGPACK_API void php_msgpack_serialize(smart_string *buf, zval *val TSRMLS_DC)
{

    msgpack_serialize_data_t var_hash;

    msgpack_serialize_var_init(&var_hash);

    msgpack_serialize_zval(buf, val, var_hash TSRMLS_CC);

    msgpack_serialize_var_destroy(&var_hash);
}

PHP_MSGPACK_API void php_msgpack_unserialize(
    zval *return_value, char *str, size_t str_len TSRMLS_DC)
{
    int ret;
    size_t off = 0;
    msgpack_unpack_t mp;
    msgpack_unserialize_data_t var_hash;

    if (str_len <= 0)
    {
        RETURN_NULL();
    }

    template_init(&mp);

    msgpack_unserialize_var_init(&var_hash);

    RETVAL_NULL();
    mp.user.retval = return_value;
    mp.user.var_hash = &var_hash;

    ret = template_execute(&mp, str, (size_t)str_len, &off);

    switch (ret) {
        case MSGPACK_UNPACK_PARSE_ERROR:
            zval_dtor(return_value);
            ZVAL_FALSE(return_value);
            msgpack_unserialize_var_destroy(&var_hash, 1);
            MSGPACK_WARNING("[msgpack] (%s) Parse error", __FUNCTION__);
            break;
        case MSGPACK_UNPACK_CONTINUE:
            msgpack_unserialize_var_destroy(&var_hash, 0);
            ZVAL_FALSE(return_value);
            MSGPACK_WARNING(
                "[msgpack] (%s) Insufficient data for unserializing",
                __FUNCTION__);
            break;
        case MSGPACK_UNPACK_EXTRA_BYTES:
        case MSGPACK_UNPACK_SUCCESS:
            msgpack_unserialize_set_return_value(&var_hash, return_value);
            msgpack_unserialize_var_destroy(&var_hash, 0);
            if (off < str_len) {
                MSGPACK_WARNING("[msgpack] (%s) Extra bytes", __FUNCTION__);
            }
            break;
        default:
            zval_dtor(return_value);
            ZVAL_FALSE(return_value);
            msgpack_unserialize_var_destroy(&var_hash, 0);
            MSGPACK_WARNING("[msgpack] (%s) Unknown result", __FUNCTION__);
            break;
    }
}

static ZEND_FUNCTION(msgpack_serialize)
{
    zval *parameter;
    smart_string buf = {0};

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &parameter) == FAILURE) {
        return;
    }

    php_msgpack_serialize(&buf, parameter TSRMLS_CC);
    smart_string_0(&buf);

    ZVAL_STRINGL(return_value, buf.c, buf.len);
    smart_string_free(&buf);
}

static ZEND_FUNCTION(msgpack_unserialize)
{
    zend_string *str;
    zval *object = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S|z", &str, &object) == FAILURE) {
        return;
    }

    if (!str) {
        RETURN_NULL();
    }

    if (object == NULL) {
        php_msgpack_unserialize(return_value, str->val, str->len);
    } else {
        zval zv, *zv_p;
        zv_p = &zv;

        php_msgpack_unserialize(&zv, str->val, str->len);

        if (msgpack_convert_template(return_value, object, &zv_p) != SUCCESS) {
            RETURN_NULL();
        }

    }
}
