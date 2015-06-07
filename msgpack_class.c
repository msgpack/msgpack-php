#include "php.h"

#include "php_msgpack.h"
#include "msgpack_pack.h"
#include "msgpack_unpack.h"
#include "msgpack_class.h"
#include "msgpack_convert.h"
#include "msgpack_errors.h"

typedef struct {
    long php_only;
    zend_object object;
} php_msgpack_base_t;

typedef struct {
    smart_string buffer;
    zval retval;
    long offset;
    msgpack_unpack_t mp;
    msgpack_unserialize_data_t var_hash;
    long php_only;
    zend_bool finished;
    int error;
    zend_object object;
} php_msgpack_unpacker_t;

static inline php_msgpack_base_t *msgpack_base_fetch_object(zend_object *obj) {
    return (php_msgpack_base_t *)((char*)(obj) - XtOffsetOf(php_msgpack_base_t, object));
}
#define Z_MSGPACK_BASE_P(zv) msgpack_base_fetch_object(Z_OBJ_P((zv)))

static inline php_msgpack_unpacker_t *msgpack_unpacker_fetch_object(zend_object *obj) {
    return (php_msgpack_unpacker_t *)((char*)(obj) - XtOffsetOf(php_msgpack_unpacker_t, object));
}
#define Z_MSGPACK_UNPACKER_P(zv) msgpack_unpacker_fetch_object(Z_OBJ_P((zv)))

/* MessagePack */
static zend_class_entry *msgpack_ce = NULL;
zend_object_handlers msgpack_handlers;

static ZEND_METHOD(msgpack, __construct);
static ZEND_METHOD(msgpack, setOption);
static ZEND_METHOD(msgpack, pack);
static ZEND_METHOD(msgpack, unpack);
static ZEND_METHOD(msgpack, unpacker);

ZEND_BEGIN_ARG_INFO_EX(arginfo_msgpack_base___construct, 0, 0, 0)
    ZEND_ARG_INFO(0, opt)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_msgpack_base_setOption, 0, 0, 2)
    ZEND_ARG_INFO(0, option)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_msgpack_base_pack, 0, 0, 1)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_msgpack_base_unpack, 0, 0, 1)
    ZEND_ARG_INFO(0, str)
    ZEND_ARG_INFO(0, object)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_msgpack_base_unpacker, 0, 0, 0)
ZEND_END_ARG_INFO()

static zend_function_entry msgpack_base_methods[] = {
    ZEND_ME(msgpack, __construct,
            arginfo_msgpack_base___construct, ZEND_ACC_PUBLIC)
    ZEND_ME(msgpack, setOption, arginfo_msgpack_base_setOption, ZEND_ACC_PUBLIC)
    ZEND_ME(msgpack, pack, arginfo_msgpack_base_pack, ZEND_ACC_PUBLIC)
    ZEND_ME(msgpack, unpack, arginfo_msgpack_base_unpack, ZEND_ACC_PUBLIC)
    ZEND_ME(msgpack, unpacker, arginfo_msgpack_base_unpacker, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};

/* MessagePackUnpacker */
static zend_class_entry *msgpack_unpacker_ce = NULL;
zend_object_handlers msgpack_unpacker_handlers;

static ZEND_METHOD(msgpack_unpacker, __construct);
static ZEND_METHOD(msgpack_unpacker, __destruct);
static ZEND_METHOD(msgpack_unpacker, setOption);
static ZEND_METHOD(msgpack_unpacker, feed);
static ZEND_METHOD(msgpack_unpacker, execute);
static ZEND_METHOD(msgpack_unpacker, data);
static ZEND_METHOD(msgpack_unpacker, reset);

ZEND_BEGIN_ARG_INFO_EX(arginfo_msgpack_unpacker___construct, 0, 0, 0)
    ZEND_ARG_INFO(0, opt)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_msgpack_unpacker___destruct, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_msgpack_unpacker_setOption, 0, 0, 2)
    ZEND_ARG_INFO(0, option)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_msgpack_unpacker_feed, 0, 0, 1)
    ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_msgpack_unpacker_execute, 1, 0, 0)
    ZEND_ARG_INFO(0, str)
    ZEND_ARG_INFO(1, offset)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_msgpack_unpacker_data, 0, 0, 0)
    ZEND_ARG_INFO(0, object)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_msgpack_unpacker_reset, 0, 0, 0)
ZEND_END_ARG_INFO()

static zend_function_entry msgpack_unpacker_methods[] = {
    ZEND_ME(msgpack_unpacker, __construct,
            arginfo_msgpack_unpacker___construct, ZEND_ACC_PUBLIC)
    ZEND_ME(msgpack_unpacker, __destruct,
            arginfo_msgpack_unpacker___destruct, ZEND_ACC_PUBLIC)
    ZEND_ME(msgpack_unpacker, setOption,
            arginfo_msgpack_unpacker_setOption, ZEND_ACC_PUBLIC)
    ZEND_ME(msgpack_unpacker, feed,
            arginfo_msgpack_unpacker_feed, ZEND_ACC_PUBLIC)
    ZEND_ME(msgpack_unpacker, execute,
            arginfo_msgpack_unpacker_execute, ZEND_ACC_PUBLIC)
    ZEND_ME(msgpack_unpacker, data,
            arginfo_msgpack_unpacker_data, ZEND_ACC_PUBLIC)
    ZEND_ME(msgpack_unpacker, reset,
            arginfo_msgpack_unpacker_reset, ZEND_ACC_PUBLIC)
    {NULL, NULL, NULL}
};
zend_object *php_msgpack_base_new(zend_class_entry *ce TSRMLS_DC) {
    php_msgpack_base_t *intern = ecalloc(1,
            sizeof(php_msgpack_base_t) +
            zend_object_properties_size(ce));

    zend_object_std_init(&intern->object, ce TSRMLS_CC);
    object_properties_init(&intern->object, ce);
    intern->object.handlers = &msgpack_handlers;
    return &intern->object;
}
static void php_msgpack_base_free(zend_object *object) {
    php_msgpack_base_t *intern = msgpack_base_fetch_object(object);
    if (!intern) {
        return;
    }
    zend_object_std_dtor(&intern->object);
}

zend_object *php_msgpack_unpacker_new(zend_class_entry *ce TSRMLS_DC) {
    php_msgpack_unpacker_t *intern = ecalloc(1,
            sizeof(php_msgpack_unpacker_t) +
            zend_object_properties_size(ce));

    zend_object_std_init(&intern->object, ce TSRMLS_CC);
    object_properties_init(&intern->object, ce);
    intern->object.handlers = &msgpack_unpacker_handlers;
    return &intern->object;
}

static void php_msgpack_unpacker_free(zend_object *object)
{
    php_msgpack_unpacker_t *intern = msgpack_unpacker_fetch_object(object);
    if (!intern) {
        return;
    }
    zend_object_std_dtor(&intern->object);
}


/* MessagePack */
static ZEND_METHOD(msgpack, __construct)
{
    zend_bool php_only = MSGPACK_G(php_only);
    php_msgpack_base_t *base = Z_MSGPACK_BASE_P(getThis());

    if (zend_parse_parameters(
            ZEND_NUM_ARGS() TSRMLS_CC, "|b", &php_only) == FAILURE)
    {
        return;
    }

    base->php_only = php_only;
}

static ZEND_METHOD(msgpack, setOption)
{
    long option;
    zval *value;
    php_msgpack_base_t *base = Z_MSGPACK_BASE_P(getThis());

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lz", &option, &value) == FAILURE) {
        return;
    }

    switch (option) {
        case MSGPACK_CLASS_OPT_PHPONLY:
            convert_to_boolean(value);
            base->php_only = (Z_TYPE_P(value) == IS_TRUE) ? 1 : 0;
            break;
        default:
            MSGPACK_WARNING("[msgpack] (MessagePack::setOption) "
                            "error setting msgpack option");
            RETURN_FALSE;
            break;
    }

    RETURN_TRUE;
}

static ZEND_METHOD(msgpack, pack)
{
    zval *parameter;
    smart_string buf = {0};
    int php_only = MSGPACK_G(php_only);
    php_msgpack_base_t *base = Z_MSGPACK_BASE_P(getThis());

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &parameter) == FAILURE) {
        return;
    }

    MSGPACK_G(php_only) = base->php_only;

    php_msgpack_serialize(&buf, parameter TSRMLS_CC);

    MSGPACK_G(php_only) = php_only;

    ZVAL_STRINGL(return_value, buf.c, buf.len);

    smart_string_free(&buf);
}

static ZEND_METHOD(msgpack, unpack)
{
    zend_string *str;
    zval *object = NULL;
    int php_only = MSGPACK_G(php_only);
    php_msgpack_base_t *base = Z_MSGPACK_BASE_P(getThis());

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S|z", &str, &object) == FAILURE) {
        return;
    }

    if (!str) {
        RETURN_NULL();
    }

    MSGPACK_G(php_only) = base->php_only;

    if (object == NULL) {
        php_msgpack_unserialize(return_value, str->val, str->len);
    } else {
        zval zv, *zv_p;
        zv_p = &zv;

        php_msgpack_unserialize(&zv, str->val, str->len TSRMLS_CC);

        if (msgpack_convert_template(return_value, object, &zv_p) != SUCCESS) {
            RETURN_NULL();
        }
    }

    MSGPACK_G(php_only) = php_only;
}

static ZEND_METHOD(msgpack, unpacker)
{
    zval temp, args[1], func_name, construct_return;
    php_msgpack_base_t *base = Z_MSGPACK_BASE_P(getThis());

    ZVAL_BOOL(&args[0], base->php_only);
    ZVAL_STRING(&func_name, "__construct");

    object_init_ex(return_value, msgpack_unpacker_ce);
    call_user_function_ex(CG(function_table), return_value, &func_name, &construct_return, 1, args, 0, NULL);

    zval_ptr_dtor(&func_name);
}

/* MessagePackUnpacker */
static ZEND_METHOD(msgpack_unpacker, __construct)
{
    zend_bool php_only = MSGPACK_G(php_only);
    php_msgpack_unpacker_t *unpacker = Z_MSGPACK_UNPACKER_P(getThis());

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &php_only) == FAILURE) {
        return;
    }

    unpacker->php_only = php_only;

    unpacker->buffer.c = NULL;
    unpacker->buffer.len = 0;
    unpacker->buffer.a = 0;
    unpacker->offset = 0;
    unpacker->finished = 0;
    unpacker->error = 0;

    template_init(&unpacker->mp);

    msgpack_unserialize_var_init(&unpacker->var_hash);

    (&unpacker->mp)->user.var_hash = &unpacker->var_hash;
}

static ZEND_METHOD(msgpack_unpacker, __destruct)
{
    php_msgpack_unpacker_t *unpacker = Z_MSGPACK_UNPACKER_P(getThis());
    smart_string_free(&unpacker->buffer);
    msgpack_unserialize_var_destroy(&unpacker->var_hash, unpacker->error);
}

static ZEND_METHOD(msgpack_unpacker, setOption)
{
    long option;
    zval *value;
    php_msgpack_unpacker_t *unpacker = Z_MSGPACK_UNPACKER_P(getThis());


    if (zend_parse_parameters(
            ZEND_NUM_ARGS() TSRMLS_CC, "lz", &option, &value) == FAILURE)
    {
        return;
    }

    switch (option)
    {
        case MSGPACK_CLASS_OPT_PHPONLY:
            convert_to_boolean(value);
            unpacker->php_only = Z_LVAL_P(value);
            break;
        default:
            MSGPACK_WARNING("[msgpack] (MessagePackUnpacker::setOption) "
                            "error setting msgpack option");
            RETURN_FALSE;
            break;
    }

    RETURN_TRUE;
}

static ZEND_METHOD(msgpack_unpacker, feed)
{
    zend_string *str;
    php_msgpack_unpacker_t *unpacker = Z_MSGPACK_UNPACKER_P(getThis());

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "S", &str) == FAILURE) {
        return;
    }

    if (!str) {
        RETURN_FALSE;
    }

    smart_string_appendl(&unpacker->buffer, str->val, str->len);

    RETURN_TRUE;
}

static ZEND_METHOD(msgpack_unpacker, execute)
{
    char *str = NULL, *data;
    size_t len, off, str_len = 0;
    int ret, error_display = MSGPACK_G(error_display), php_only = MSGPACK_G(php_only);
    zval *offset = NULL;
    php_msgpack_unpacker_t *unpacker = Z_MSGPACK_UNPACKER_P(getThis());

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sz/", &str, &str_len, &offset) == FAILURE) {
        return;
    }

    if (str) {
        data = str;
        len = str_len;
        if (offset != NULL && (Z_TYPE_P(offset) == IS_LONG || Z_TYPE_P(offset) == IS_DOUBLE)) {
            off = Z_LVAL_P(offset);
        } else {
            off = 0;
        }
    } else {
        data = unpacker->buffer.c;
        len = unpacker->buffer.len;
        off = unpacker->offset;
    }

    if (unpacker->finished) {
        msgpack_unserialize_set_return_value(&unpacker->var_hash, &unpacker->retval);
        msgpack_unserialize_var_destroy(&unpacker->var_hash, unpacker->error);
        unpacker->error = 0;

        template_init(&unpacker->mp);

        msgpack_unserialize_var_init(&unpacker->var_hash);

        (&unpacker->mp)->user.var_hash = &unpacker->var_hash;
    }
    (&unpacker->mp)->user.retval = &unpacker->retval;

    MSGPACK_G(error_display) = 0;
    MSGPACK_G(php_only) = unpacker->php_only;

    ret = template_execute(&unpacker->mp, data, len, &off);

    MSGPACK_G(error_display) = error_display;
    MSGPACK_G(php_only) = php_only;

    if (str != NULL) {
        if (offset != NULL) {
            ZVAL_LONG(offset, off);
        }
    } else {
        unpacker->offset = off;
    }

    switch (ret) {
        case MSGPACK_UNPACK_EXTRA_BYTES:
        case MSGPACK_UNPACK_SUCCESS:
            unpacker->finished = 1;
            unpacker->error = 0;
            RETURN_TRUE;
        default:
            unpacker->error = 1;
            RETURN_FALSE;
    }
}

static ZEND_METHOD(msgpack_unpacker, data)
{
    zval *object = NULL, func_name, reset_return;
    php_msgpack_unpacker_t *unpacker = Z_MSGPACK_UNPACKER_P(getThis());

    if (zend_parse_parameters( ZEND_NUM_ARGS() TSRMLS_CC, "|z", &object) == FAILURE) {
        return;
    }

    if (unpacker->finished) {
        msgpack_unserialize_set_return_value(&unpacker->var_hash, &unpacker->retval);
    }

    if (object == NULL) {
        ZVAL_COPY_VALUE(return_value, &unpacker->retval);
    } else {
        zval zv, *zv_p;
        zv_p = &zv;

        ZVAL_COPY_VALUE(zv_p, &unpacker->retval);

        if (msgpack_convert_object(return_value, object, &zv_p) != SUCCESS) {
            RETURN_NULL();
        }
    }

    ZVAL_STRING(&func_name, "reset");
    call_user_function_ex(CG(function_table), getThis(), &func_name, &reset_return, 0, NULL, 0, NULL);
    zval_ptr_dtor(&func_name);
}

static ZEND_METHOD(msgpack_unpacker, reset)
{
    smart_string buffer = {0};
    php_msgpack_unpacker_t *unpacker = Z_MSGPACK_UNPACKER_P(getThis());

    if (unpacker->buffer.len > unpacker->offset) {
        smart_string_appendl(&buffer, unpacker->buffer.c + unpacker->offset,
                          unpacker->buffer.len - unpacker->offset);
    }

    smart_string_free(&unpacker->buffer);

    unpacker->buffer.c = NULL;
    unpacker->buffer.len = 0;
    unpacker->buffer.a = 0;
    unpacker->offset = 0;
    unpacker->finished = 0;

    if (buffer.len > 0) {
        smart_string_appendl(&unpacker->buffer, buffer.c, buffer.len);
    }

    smart_string_free(&buffer);

    msgpack_unserialize_var_destroy(&unpacker->var_hash, unpacker->error);
    unpacker->error = 0;


    template_init(&unpacker->mp);

    msgpack_unserialize_var_init(&unpacker->var_hash);

    (&unpacker->mp)->user.var_hash = &unpacker->var_hash;
}

void msgpack_init_class()
{
    zend_class_entry ce;
    TSRMLS_FETCH();

    /* base */
    INIT_CLASS_ENTRY(ce, "MessagePack", msgpack_base_methods);
    msgpack_ce = zend_register_internal_class(&ce TSRMLS_CC);
    msgpack_ce->create_object = php_msgpack_base_new;
    memcpy(&msgpack_handlers, zend_get_std_object_handlers(),sizeof msgpack_handlers);
    msgpack_handlers.offset = XtOffsetOf(php_msgpack_base_t, object);
    msgpack_handlers.free_obj = php_msgpack_base_free;

    zend_declare_class_constant_long(msgpack_ce, ZEND_STRS("OPT_PHPONLY") - 1, MSGPACK_CLASS_OPT_PHPONLY TSRMLS_CC);

    /* unpacker */
    INIT_CLASS_ENTRY(ce, "MessagePackUnpacker", msgpack_unpacker_methods);
    msgpack_unpacker_ce = zend_register_internal_class(&ce TSRMLS_CC);
    msgpack_unpacker_ce->create_object = php_msgpack_unpacker_new;
    memcpy(&msgpack_unpacker_handlers, zend_get_std_object_handlers(),sizeof msgpack_unpacker_handlers);
    msgpack_unpacker_handlers.offset = XtOffsetOf(php_msgpack_unpacker_t, object);
    msgpack_handlers.free_obj = php_msgpack_base_free;

}
