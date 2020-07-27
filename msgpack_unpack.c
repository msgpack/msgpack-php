#include "php.h"
#include "php_ini.h"
#include "ext/standard/php_incomplete_class.h"

#include "php_msgpack.h"
#include "msgpack_pack.h"
#include "msgpack_unpack.h"
#include "msgpack_errors.h"

#define VAR_ENTRIES_MAX 1024

#if PHP_VERSION_ID < 70400
# define zval_try_get_string zval_get_string
#endif
#if PHP_VERSION_ID < 70200
# define zend_string_init_interned zend_string_init
#endif

typedef struct {
    zend_long used_slots;
    void *next;
    zval data[VAR_ENTRIES_MAX];
} var_entries;

#define MSGPACK_UNSERIALIZE_ALLOC_STACK(_unpack)                 \
    if (UNEXPECTED(_unpack->deps == 0)) {                        \
        *obj = _unpack->retval;                                  \
    } else {                                                     \
        *obj = msgpack_stack_push(&_unpack->var_hash);            \
    }

#define MSGPACK_UNSERIALIZE_ALLOC_VALUE(_unpack)                 \
    if (UNEXPECTED(_unpack->deps <= 0)) {                        \
        *obj = _unpack->retval;                                  \
    } else {                                                     \
        *obj = msgpack_var_push(&_unpack->var_hash);              \
    }

#define MSGPACK_UNSERIALIZE_DEC_DEP(_unpack)                     \
    _unpack->stack[_unpack->deps-1]--;                           \
    if (_unpack->stack[_unpack->deps-1] <= 0) {                  \
        _unpack->deps--;                                         \
    }

#define MSGPACK_UNSERIALIZE_FINISH_ITEM(_unpack, _v1, _v2)       \
    if ((_v2) && MSGPACK_IS_STACK_VALUE((_v2))) {                \
        msgpack_stack_pop(&_unpack->var_hash, (_v2));             \
    }                                                            \
    if ((_v1) && MSGPACK_IS_STACK_VALUE((_v1))) {                \
        msgpack_stack_pop(&_unpack->var_hash, (_v1));             \
    }                                                            \
    MSGPACK_UNSERIALIZE_DEC_DEP(_unpack);

#define MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(_unpack, _key, _val) \
    zval_ptr_dtor(_key);                                         \
    zval_ptr_dtor(_val);                                         \
    MSGPACK_UNSERIALIZE_FINISH_ITEM(_unpack, _key, _val);

#define UNSET_MAGIC_METHODS(_ce)                                 \
    __get = _ce->__get;                                          \
    _ce->__get = NULL;                                           \
    __set = _ce->__set;                                          \
    _ce->__set = NULL;                                           \

#define RESET_MAGIC_METHODS(_ce)                                 \
    ce->__set = __set;                                           \
    ce->__get = __get;                                           \

#define MSGPACK_IS_STACK_VALUE(_v)   (Z_TYPE_P((zval *)(_v)) < IS_ARRAY)

#define MSGPACK_VALIDATE_INPUT_DATA_LENGTH(data, len, eof) \
    ((data) + (len) <= (eof))
#define MSGPACK_VALIDATE_INPUT(user, data, len) \
    if (!MSGPACK_VALIDATE_INPUT_DATA_LENGTH(data, len, (user)->eof)) { \
        return MSGPACK_UNPACK_PARSE_ERROR; \
    }

static zval *msgpack_var_push(msgpack_unserialize_data_t *var_hashx) /* {{{ */ {
    var_entries *var_hash, *prev = NULL;

    if (!var_hashx) {
        return NULL;
    }

    var_hash = var_hashx->last;

    while (var_hash && var_hash->used_slots == VAR_ENTRIES_MAX) {
        prev = var_hash;
        var_hash = var_hash->next;
    }

    if (!var_hash) {
        var_hash = emalloc(sizeof(var_entries));
        var_hash->used_slots = 0;
        var_hash->next = 0;

        if (!var_hashx->first) {
            var_hashx->first = var_hash;
        } else {
            prev->next = var_hash;
        }
        var_hashx->last = var_hash;
    }

    return &var_hash->data[var_hash->used_slots++];
}
/* }}} */

static inline void msgpack_var_replace(zval *old, zval *new) /* {{{ */ {
    if (!MSGPACK_IS_STACK_VALUE(old) && Z_TYPE_P(old) != IS_REFERENCE) {
        ZVAL_INDIRECT(old, new);
    }
}
/* }}} */

static zval *msgpack_var_access(msgpack_unserialize_data_t *var_hashx, zend_long id) /* {{{ */ {
    var_entries *var_hash = var_hashx->first;

    while (id > VAR_ENTRIES_MAX && var_hash && var_hash->used_slots == VAR_ENTRIES_MAX) {
        var_hash = var_hash->next;
        id -= VAR_ENTRIES_MAX;
    }

    if (!var_hash) {
        return NULL;
    }

    if (id > 0 && id <= var_hash->used_slots) {
        zval *zv = &var_hash->data[id - 1];
        if (UNEXPECTED(Z_TYPE_P(zv) == IS_INDIRECT)) {
            zv = Z_INDIRECT_P(zv);
        }
        return zv;
    }

    return NULL;
}
/* }}} */

static zval *msgpack_stack_push(msgpack_unserialize_data_t *var_hashx) /* {{{ */ {
    var_entries *var_hash, *prev = NULL;

    if (!var_hashx) {
        return NULL;
    }

    var_hash = var_hashx->last_dtor;

    while (var_hash && var_hash->used_slots == VAR_ENTRIES_MAX) {
        prev = var_hash;
        var_hash = var_hash->next;
    }

    if (!var_hash) {
        var_hash = emalloc(sizeof(var_entries));
        var_hash->used_slots = 0;
        var_hash->next = 0;

        if (!var_hashx->first_dtor) {
            var_hashx->first_dtor = var_hash;
        } else {
            prev->next = var_hash;
        }
        var_hashx->last_dtor = var_hash;
    }

    return &var_hash->data[var_hash->used_slots++];
}
/* }}} */

static void msgpack_stack_pop(msgpack_unserialize_data_t *var_hashx, zval *v) /* {{{ */ {
    var_entries *var_hash = var_hashx->last_dtor;

    while (var_hash && var_hash->used_slots == VAR_ENTRIES_MAX) {
        var_hash = var_hash->next;
    }

    if (!var_hash) {
        return;
    }

    ZEND_ASSERT((&(var_hash->data[var_hash->used_slots -1]) == v));
    if ((&(var_hash->data[var_hash->used_slots -1]) == v)) {
        var_hash->used_slots--;
        ZVAL_UNDEF(v);
    }

    /*
    for (i = count; i > 0; i--) {
        var_hash->used_slots--;
        if (var_hash->used_slots < 0) {
            var_hash->used_slots = 0;
            ZVAL_UNDEF(&var_hash->data[var_hash->used_slots]);
            break;
        } else {
            ZVAL_UNDEF(&var_hash->data[var_hash->used_slots]);
        }
    }
    */
}
/* }}} */

static inline zval *update_property(const zend_class_entry *ce, HashTable *props, zend_string *mangled, zval *val) {
    const char *class_name, *prop_name;
    size_t prop_len;
    zval *new_val;
    zend_string *unmangled, *new_key;
    zend_property_info *existing_propinfo;

    if (SUCCESS != zend_unmangle_property_name_ex(mangled, &class_name, &prop_name, &prop_len)) {
        return NULL;
    }

    unmangled = zend_string_init(prop_name, prop_len, 0);
    existing_propinfo = zend_hash_find_ptr(&ce->properties_info, unmangled);

    if ((class_name == NULL || !strcmp(class_name, "*") || !strcasecmp(class_name, ZSTR_VAL(ce->name)))
            && (existing_propinfo != NULL)
            && (existing_propinfo->flags & ZEND_ACC_PPP_MASK)) {
        if (existing_propinfo->flags & ZEND_ACC_PROTECTED) {
            new_key = zend_mangle_property_name(
                "*", 1, ZSTR_VAL(unmangled), ZSTR_LEN(unmangled), 0);
        } else if (existing_propinfo->flags & ZEND_ACC_PRIVATE) {
            if (class_name != NULL && strcmp(class_name, "*") != 0) {
                new_key = zend_mangle_property_name(
                    class_name, strlen(class_name),
                        ZSTR_VAL(unmangled), ZSTR_LEN(unmangled),
                        0);
            } else {
                new_key = zend_mangle_property_name(
                    ZSTR_VAL(existing_propinfo->ce->name), ZSTR_LEN(existing_propinfo->ce->name),
                    ZSTR_VAL(unmangled), ZSTR_LEN(unmangled),
                    0);
            }
        } else {
            ZEND_ASSERT(existing_propinfo->flags & ZEND_ACC_PUBLIC);
            new_key = unmangled;
        }
    } else {
        new_key = mangled;
    }

    Z_TRY_ADDREF_P(val);
    new_val = zend_hash_update_ind(props, new_key, val);

    if (new_key != unmangled) {
        zend_string_release(unmangled);
    }
    if (new_key != mangled) {
        zend_string_release(new_key);
    }

    return new_val;
}

static zend_class_entry* msgpack_unserialize_class(zval **container, zend_string *class_name, zend_bool init_class) /* {{{ */ {
    zend_class_entry *ce;
    int func_call_status;
    zend_bool incomplete_class = 0;
    zval user_func, retval, args[1], *container_val, container_tmp, *val;
    zend_string *str_key;

    container_val = Z_ISREF_P(*container) ? Z_REFVAL_P(*container) : *container;
    ZVAL_UNDEF(&container_tmp);

    do {
        /* Try to find class directly */
        ce = zend_lookup_class(class_name);
        if (ce != NULL) {
            break;
        }

        /* Check for unserialize callback */
        if ((PG(unserialize_callback_func) == NULL) ||
            (PG(unserialize_callback_func)[0] == '\0')) {
            incomplete_class = 1;
            ce = PHP_IC_ENTRY;
            break;
        }

        /* Call unserialize callback */
        ZVAL_STRING(&user_func, PG(unserialize_callback_func));
        ZVAL_STR(&args[0], class_name);

        func_call_status = call_user_function_ex(CG(function_table), NULL, &user_func, &retval, 1, args, 0, NULL);
        zval_ptr_dtor(&user_func);
        if (func_call_status != SUCCESS) {
            MSGPACK_WARNING("[msgpack] (%s) defined (%s) but not found",
                            __FUNCTION__, ZSTR_VAL(class_name));

            incomplete_class = 1;
            ce = PHP_IC_ENTRY;
            break;
        }

        if ((ce = zend_lookup_class(class_name)) == NULL) {
            MSGPACK_WARNING("[msgpack] (%s) Function %s() hasn't defined "
                            "the class it was called for",
                            __FUNCTION__, ZSTR_VAL(class_name));

            incomplete_class = 1;
            ce = PHP_IC_ENTRY;
        }
    }
    while(0);

    if (EG(exception)) {
        MSGPACK_WARNING("[msgpack] (%s) Exception error", __FUNCTION__);
        return NULL;
    }

    if (init_class || incomplete_class) {
        if (Z_TYPE_P(container_val) == IS_ARRAY) {
            ZVAL_COPY_VALUE(&container_tmp, container_val);
        }
        object_init_ex(container_val, ce);

        if (Z_TYPE(container_tmp) != IS_UNDEF) {
            HashTable *props = Z_OBJPROP_P(container_val);
            ZEND_HASH_FOREACH_STR_KEY_VAL(HASH_OF(&container_tmp), str_key, val)
            {
                update_property(ce, props, str_key, val);
            }
            ZEND_HASH_FOREACH_END();
            zval_dtor(&container_tmp);
        }

    }

    /* store incomplete class name */
    if (incomplete_class) {
        php_store_class_name(container_val, ZSTR_VAL(class_name), ZSTR_LEN(class_name));
    }

    return ce;
}
/* }}} */

void msgpack_unserialize_var_init(msgpack_unserialize_data_t *var_hashx) /* {{{ */ {
    var_hashx->first = var_hashx->last = NULL;
    var_hashx->first_dtor = var_hashx->last_dtor = NULL;
}
/* }}} */

void msgpack_unserialize_var_destroy(msgpack_unserialize_data_t *var_hashx, zend_bool err) /* {{{ */ {
    zend_long i;
    void *next;
    var_entries *var_hash = var_hashx->first;

    while (var_hash) {
        if (err) {
            for (i = var_hash->used_slots; i > 0; i--) {
                zval_ptr_dtor(&var_hash->data[i - 1]);
            }
        }
        next = var_hash->next;
        efree(var_hash);
        var_hash = next;
    }

    var_hash = var_hashx->first_dtor;
    while (var_hash) {
        for (i = var_hash->used_slots; i > 0; i--) {
            zval_ptr_dtor(&var_hash->data[i-1]);
        }
        next = var_hash->next;
        efree(var_hash);
        var_hash = next;
    }
}
/* }}} */

void msgpack_unserialize_init(msgpack_unserialize_data *unpack) /* {{{ */ {
    unpack->deps = 0;
    unpack->type = MSGPACK_SERIALIZE_TYPE_NONE;
    msgpack_unserialize_var_init(&unpack->var_hash);
}
/* }}} */

int msgpack_unserialize_uint8(msgpack_unserialize_data *unpack, uint8_t data, zval **obj) /* {{{ */ {
    MSGPACK_UNSERIALIZE_ALLOC_STACK(unpack);

    ZVAL_LONG(*obj, data);

    return 0;
}
/* }}} */

int msgpack_unserialize_uint16(msgpack_unserialize_data *unpack, uint16_t data, zval **obj) /* {{{ */ {
    MSGPACK_UNSERIALIZE_ALLOC_STACK(unpack);

    ZVAL_LONG(*obj, data);

    return 0;
}
/* }}} */

int msgpack_unserialize_uint32(msgpack_unserialize_data *unpack, uint32_t data, zval **obj) /* {{{ */ {
    MSGPACK_UNSERIALIZE_ALLOC_STACK(unpack);

    if (data <= (uint32_t) ZEND_LONG_MAX) {
        ZVAL_LONG(*obj, data);
    } else {
        ZVAL_DOUBLE(*obj, data);
    }

    return 0;
}
/* }}} */

/* see zend_print_ulong_to_buf() in Zend/zend_operators.h */
static inline char *print_u64_to_buf(char *buf, uint64_t num) {
	*buf = '\0';
	do {
		*--buf = (char) (num % 10) + '0';
		num /= 10;
	} while (num > 0);
	return buf;
}

int msgpack_unserialize_uint64(msgpack_unserialize_data *unpack, uint64_t data, zval **obj) /* {{{ */ {
    MSGPACK_UNSERIALIZE_ALLOC_STACK(unpack);

    if (data <= (uint64_t) ZEND_LONG_MAX) {
        ZVAL_LONG(*obj, data);
    } else if (data <= 1L<<52) {
        ZVAL_DOUBLE(*obj, data);
    } else {
        char buf[0x20] = {0}, *ptr = print_u64_to_buf(&buf[sizeof(buf)-1], data);
        ZVAL_STRING(*obj, ptr);
    }

    return 0;
}
/* }}} */

int msgpack_unserialize_int8(msgpack_unserialize_data *unpack, int8_t data, zval **obj) /* {{{ */ {
    MSGPACK_UNSERIALIZE_ALLOC_STACK(unpack);

    ZVAL_LONG(*obj, data);

    return 0;
}
/* }}} */

int msgpack_unserialize_int16(msgpack_unserialize_data *unpack, int16_t data, zval **obj) /* {{{ */ {
    MSGPACK_UNSERIALIZE_ALLOC_STACK(unpack);

    ZVAL_LONG(*obj, data);

    return 0;
}
/* }}} */

int msgpack_unserialize_int32(msgpack_unserialize_data *unpack, int32_t data, zval **obj) /* {{{ */ {
    MSGPACK_UNSERIALIZE_ALLOC_STACK(unpack);

    ZVAL_LONG(*obj, data);

    return 0;
}
/* }}} */

int msgpack_unserialize_int64(msgpack_unserialize_data *unpack, int64_t data, zval **obj) /* {{{ */ {
    MSGPACK_UNSERIALIZE_ALLOC_STACK(unpack);

    ZVAL_LONG(*obj, data);

    return 0;
}
/* }}} */

int msgpack_unserialize_float(msgpack_unserialize_data *unpack, float data, zval **obj) /* {{{ */ {
    MSGPACK_UNSERIALIZE_ALLOC_STACK(unpack);

    ZVAL_DOUBLE(*obj, data);

    return 0;
}
/* }}} */

int msgpack_unserialize_double(msgpack_unserialize_data *unpack, double data, zval **obj) /* {{{ */ {
    MSGPACK_UNSERIALIZE_ALLOC_STACK(unpack);

    ZVAL_DOUBLE(*obj, data);

    return 0;
}
/* }}} */

int msgpack_unserialize_nil(msgpack_unserialize_data *unpack, zval **obj) /* {{{ */ {
    MSGPACK_UNSERIALIZE_ALLOC_STACK(unpack);

    ZVAL_NULL(*obj);

    return 0;
}
/* }}} */

int msgpack_unserialize_true(msgpack_unserialize_data *unpack, zval **obj) /* {{{ */ {
    MSGPACK_UNSERIALIZE_ALLOC_STACK(unpack);

    ZVAL_BOOL(*obj, 1);

    return 0;
}
/* }}} */

int msgpack_unserialize_false(msgpack_unserialize_data *unpack, zval **obj) /* {{{ */ {
    MSGPACK_UNSERIALIZE_ALLOC_STACK(unpack);

    ZVAL_BOOL(*obj, 0);

    return 0;
}
/* }}} */

int msgpack_unserialize_str(msgpack_unserialize_data *unpack, const char* base, const char* data, unsigned int len, zval **obj) /* {{{ */ {
    MSGPACK_VALIDATE_INPUT(unpack, data, len);
    MSGPACK_UNSERIALIZE_ALLOC_STACK(unpack);

    if (len == 0) {
        ZVAL_EMPTY_STRING(*obj);
    } else {
        /* TODO: check malformed input? */
        if (len < 1<<8) {
            zend_string *zs = zend_string_init_interned(data, len, 0);
            ZVAL_STR(*obj, zs);
        } else {
            ZVAL_STRINGL(*obj, data, len);
        }
    }

    return 0;
}
/* }}} */

int msgpack_unserialize_ext(msgpack_unserialize_data *unpack, const char* base, const char* data, unsigned int len, zval **obj) /* {{{ */ {
    MSGPACK_VALIDATE_INPUT(unpack, data, len);
    MSGPACK_UNSERIALIZE_ALLOC_STACK(unpack);

    ZVAL_NULL(*obj);

    return 0;
}
/* }}} */

int msgpack_unserialize_array(msgpack_unserialize_data *unpack, unsigned int count, zval **obj) /* {{{ */ {
    MSGPACK_UNSERIALIZE_ALLOC_VALUE(unpack);

    array_init_size(*obj, MIN(count, 1<<16));

    if (count) {
        unpack->stack[unpack->deps++] = count;
    }

    return 0;
}
/* }}} */

int msgpack_unserialize_array_item(msgpack_unserialize_data *unpack, zval **container, zval *obj) /* {{{ */ {
    zval *nval;

    if (!*container || Z_TYPE_P(*container) != IS_ARRAY) {
        return -1;
    }

    nval = zend_hash_next_index_insert(Z_ARRVAL_P(*container), obj);

    if (MSGPACK_IS_STACK_VALUE(obj)) {
        MSGPACK_UNSERIALIZE_FINISH_ITEM(unpack, obj, NULL);
    } else {
        msgpack_var_replace(obj, nval);
        MSGPACK_UNSERIALIZE_DEC_DEP(unpack);
    }

    return 0;
}
/* }}} */

int msgpack_unserialize_map(msgpack_unserialize_data *unpack, unsigned int count, zval **obj) /* {{{ */ {
    MSGPACK_UNSERIALIZE_ALLOC_VALUE(unpack);

    if (count) {
        unpack->stack[unpack->deps++] = count;
    }

    unpack->type = MSGPACK_SERIALIZE_TYPE_NONE;
    unpack->count = count;

    if (count == 0) {
        if (MSGPACK_G(php_only)) {
            object_init(*obj);
        } else {
            array_init(*obj);
        }
    } else {
        ZVAL_NULL(*obj);
    }

    return 0;
}
/* }}} */

int msgpack_unserialize_map_item(msgpack_unserialize_data *unpack, zval **container, zval *key, zval *val) /* {{{ */ {
    long deps;
    zval *nval;
    zval *container_val;

    if (MSGPACK_G(php_only)) {
        zend_class_entry *ce;

        if (Z_TYPE_P(key) == IS_NULL) {
            unpack->type = MSGPACK_SERIALIZE_TYPE_NONE;

            if (Z_TYPE_P(val) == IS_LONG) {
                switch (Z_LVAL_P(val)) {
                    case MSGPACK_SERIALIZE_TYPE_REFERENCE:
                        ZVAL_MAKE_REF(*container);
                        break;
                    case MSGPACK_SERIALIZE_TYPE_RECURSIVE:
                        unpack->type = MSGPACK_SERIALIZE_TYPE_RECURSIVE;
                        break;
                    case MSGPACK_SERIALIZE_TYPE_CUSTOM_OBJECT:
                        unpack->type = MSGPACK_SERIALIZE_TYPE_CUSTOM_OBJECT;
                        break;
                    case MSGPACK_SERIALIZE_TYPE_OBJECT_REFERENCE:
                        unpack->type = MSGPACK_SERIALIZE_TYPE_OBJECT_REFERENCE;
                        break;
                    case MSGPACK_SERIALIZE_TYPE_OBJECT:
                        unpack->type = MSGPACK_SERIALIZE_TYPE_OBJECT;
                        break;
                    default:
                        break;
                }
            } else if (Z_TYPE_P(val) == IS_STRING) {
                ce = msgpack_unserialize_class(container, Z_STR_P(val), 1);
                if (ce == NULL) {
                    MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);
                    return 0;
                }
            }
            MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);
            return 0;
        } else {
            switch (unpack->type) {
                case MSGPACK_SERIALIZE_TYPE_CUSTOM_OBJECT:
                    unpack->type = MSGPACK_SERIALIZE_TYPE_NONE;

                    ce = msgpack_unserialize_class(container, Z_STR_P(key), 0);
                    if (ce == NULL) {
                        MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);
                        return 0;
                    }
                    /* implementing Serializable */
                    if (ce->unserialize == NULL) {
                        MSGPACK_WARNING(
                            "[msgpack] (%s) Class %s has no unserializer",
                            __FUNCTION__, ZSTR_VAL(ce->name));

                        MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);

                        return 0;
                    }

                    ce->unserialize(*container, ce, (const unsigned char *)Z_STRVAL_P(val), Z_STRLEN_P(val) + 1, NULL);

                    MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);

                    return 0;
                case MSGPACK_SERIALIZE_TYPE_RECURSIVE:
                case MSGPACK_SERIALIZE_TYPE_OBJECT:
                case MSGPACK_SERIALIZE_TYPE_OBJECT_REFERENCE:
                {
                    zval *rval;
                    int type = unpack->type;

                    unpack->type = MSGPACK_SERIALIZE_TYPE_NONE;
                    if ((rval = msgpack_var_access(&unpack->var_hash, Z_LVAL_P(val) - 1)) == NULL)  {
                        if (UNEXPECTED(Z_LVAL_P(val) == 1 /* access the retval */)) {
                            rval = unpack->retval;
                        } else {
                            MSGPACK_WARNING("[msgpack] (%s) Invalid references value: %ld",
                                    __FUNCTION__, Z_LVAL_P(val) - 1);

                            MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);
                            return 0;
                        }
                    }

                    if (container != NULL) {
                        zval_ptr_dtor(*container);
                    }

                    ZVAL_COPY_VALUE(*container, rval);
                    if (type == MSGPACK_SERIALIZE_TYPE_OBJECT_REFERENCE) {
                        ZVAL_MAKE_REF(*container);
                    }

                    Z_TRY_ADDREF_P(*container);

                    MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);
                    return 0;
                }
            }
        }
    }

    container_val = Z_ISREF_P(*container) ? Z_REFVAL_P(*container) : *container;

    if (Z_TYPE_P(container_val) == IS_OBJECT) {
        switch (Z_TYPE_P(key)) {
            case IS_LONG:
                if ((nval = zend_hash_index_update(Z_OBJPROP_P(container_val), Z_LVAL_P(key), val)) == NULL) {
                    zval_ptr_dtor(val);
                    MSGPACK_WARNING(
                            "[msgpack] (%s) illegal offset type, skip this decoding",
                            __FUNCTION__);
                }
                break;
            case IS_STRING:
                if (Z_OBJCE_P(container_val) != PHP_IC_ENTRY) {
                    nval = update_property(Z_OBJCE_P(container_val), Z_OBJPROP_P(container_val), Z_STR_P(key), val);
                    zval_ptr_dtor(key);
                    zval_ptr_dtor(val);
                } else {
                    if ((nval = zend_symtable_update(Z_OBJPROP_P(container_val), Z_STR_P(key), val)) == NULL) {
                        zval_ptr_dtor(val);
                        MSGPACK_WARNING(
                                "[msgpack] (%s) illegal offset type, skip this decoding",
                                __FUNCTION__);
                    }
                    zval_ptr_dtor(key);
                }
                break;
            default:
                MSGPACK_WARNING("[msgpack] (%s) illegal key type", __FUNCTION__);

                if (MSGPACK_G(illegal_key_insert)) {
                    if ((nval = zend_hash_next_index_insert(Z_OBJPROP_P(container_val), key)) == NULL) {
                        zval_ptr_dtor(key);
                    }
                    if ((nval = zend_hash_next_index_insert(Z_OBJPROP_P(container_val), val)) == NULL) {
                        zval_ptr_dtor(val);
                    }
                } else {
                    zend_string *skey = zval_try_get_string(key);
#if PHP_VERSION_ID >= 70400
                    if (!skey && EG(exception)) {
                        zend_object_release(EG(exception));
                        EG(exception) = NULL;
                    }
#endif
                    if (!skey || !(nval = zend_symtable_update(Z_OBJPROP_P(container_val), skey, val))) {
                        zval_ptr_dtor(val);
                    }
                    if (skey) {
                        zend_string_release(skey);
                    }
                }
                break;
        }
    } else {
        if (Z_TYPE_P(container_val) != IS_ARRAY) {
            array_init_size(container_val, MIN(unpack->count, 1<<16));
#if PHP_VERSION_ID >= 70300
            /* avoid re-alloc due packed..mixed conversion */
            zend_hash_real_init_mixed(Z_ARRVAL_P(container_val));
#endif
        }
        switch (Z_TYPE_P(key)) {
            case IS_LONG:
                if ((nval = zend_hash_index_update(Z_ARRVAL_P(container_val), Z_LVAL_P(key), val)) == NULL) {
                    zval_ptr_dtor(val);
                    MSGPACK_WARNING(
                            "[msgpack] (%s) illegal offset type, skip this decoding",
                            __FUNCTION__);
                }
                break;
            case IS_STRING:
                if ((nval = zend_symtable_update(Z_ARRVAL_P(container_val), Z_STR_P(key), val)) == NULL) {
                    zval_ptr_dtor(val);
                    MSGPACK_WARNING(
                            "[msgpack] (%s) illegal offset type, skip this decoding",
                            __FUNCTION__);
                }
                zval_ptr_dtor(key);
                break;
            default:
                MSGPACK_WARNING("[msgpack] (%s) illegal key type", __FUNCTION__);

                if (MSGPACK_G(illegal_key_insert)) {
                    if ((nval = zend_hash_next_index_insert(Z_ARRVAL_P(container_val), key)) == NULL) {
                        zval_ptr_dtor(key);
                    }
                    if ((nval = zend_hash_next_index_insert(Z_ARRVAL_P(container_val), val)) == NULL) {
                        zval_ptr_dtor(val);
                    }
                } else {
                    zend_string *skey = zval_try_get_string(key);
#if PHP_VERSION_ID >= 70400
                    if (!skey && EG(exception)) {
                        zend_object_release(EG(exception));
                        EG(exception) = NULL;
                    }
#endif
                    if (!skey || !(nval = zend_symtable_update(Z_ARRVAL_P(container_val), skey, val))) {
                        zval_ptr_dtor(val);
                    }
                    if (skey) {
                        zend_string_release(skey);
                    }
                }
                break;
        }
    }

    if (MSGPACK_IS_STACK_VALUE(val)) {
        msgpack_stack_pop(&unpack->var_hash, val);
    } else {
        msgpack_var_replace(val, nval);
    }
    if (MSGPACK_IS_STACK_VALUE(key)) {
        /* just in case for malformed data */
        msgpack_stack_pop(&unpack->var_hash, key);
    }

    deps = unpack->deps - 1;
    unpack->stack[deps]--;
    if (unpack->stack[deps] == 0) {
        /* wakeup */
        unpack->deps--;
        if (MSGPACK_G(php_only) &&
                Z_TYPE_P(container_val) == IS_OBJECT &&
                Z_OBJCE_P(container_val) != PHP_IC_ENTRY &&
                zend_hash_str_exists(&Z_OBJCE_P(container_val)->function_table, "__wakeup", sizeof("__wakeup") - 1)) {
            zval wakeup, r;
            ZVAL_STRING(&wakeup, "__wakeup");
            call_user_function_ex(CG(function_table), container_val, &wakeup, &r, 0, NULL, 1, NULL);
            zval_ptr_dtor(&r);
            zval_ptr_dtor(&wakeup);
        }
    }

    return 0;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
