
#include "php.h"
#include "php_ini.h"
#include "ext/standard/php_incomplete_class.h"

#include "php_msgpack.h"
#include "msgpack_pack.h"
#include "msgpack_unpack.h"
#include "msgpack_errors.h"

#define VAR_ENTRIES_MAX 1024

typedef struct {
    zval data[VAR_ENTRIES_MAX];
    long used_slots;
    void *next;
} var_entries;

#define MSGPACK_UNSERIALIZE_FINISH_ITEM(_unpack, _count) \
    _unpack->stack[_unpack->deps-1]--;                   \
    if (_unpack->stack[_unpack->deps-1] == 0) {          \
        _unpack->deps--;                                 \
    }

#define MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(_unpack, _key, _val) \
    zval_ptr_dtor(_key);                                        \
    zval_ptr_dtor(_val);                                        \
    MSGPACK_UNSERIALIZE_FINISH_ITEM(_unpack, 2);

inline static void msgpack_var_push(msgpack_unserialize_data_t *var_hashx, zval **rval) {
    var_entries *var_hash, *prev = NULL;

    if (!var_hashx) {
        return;
    }

    var_hash = var_hashx->first;

    while (var_hash && var_hash->used_slots == VAR_ENTRIES_MAX) {
        prev = var_hash;
        var_hash = var_hash->next;
    }

    if (!var_hash) {
        var_hash = ecalloc(1, sizeof(var_entries));
        var_hash->used_slots = 0;
        var_hash->next = 0;

        if (!var_hashx->first) {
            var_hashx->first = var_hash;
        } else {
            prev->next = var_hash;
        }
    }

    *rval = &var_hash->data[var_hash->used_slots++];
}

inline static int msgpack_var_access(msgpack_unserialize_data_t *var_hashx, long id, zval **store)
{
    var_entries *var_hash = var_hashx->first;

    while (id >= VAR_ENTRIES_MAX && var_hash && var_hash->used_slots == VAR_ENTRIES_MAX) {
        var_hash = var_hash->next;
        id -= VAR_ENTRIES_MAX;
    }

    if (!var_hash) {
        return !SUCCESS;
    }

    if (id < 0 || id >= var_hash->used_slots) {
        return !SUCCESS;
    }

    *store = &var_hash->data[id];

    return SUCCESS;
}

inline static void msgpack_stack_push(msgpack_unserialize_data_t *var_hashx, zval **rval)
{
    var_entries *var_hash, *prev = NULL;

    if (!var_hashx) {
        return;
    }

    var_hash = var_hashx->first_dtor;

    while (var_hash && var_hash->used_slots == VAR_ENTRIES_MAX) {
        prev = var_hash;
        var_hash = var_hash->next;
    }

    if (!var_hash) {
        var_hash = ecalloc(1, sizeof(var_entries));
        var_hash->used_slots = 0;
        var_hash->next = 0;

        if (!var_hashx->first_dtor) {
            var_hashx->first_dtor = var_hash;
        } else {
            prev->next = var_hash;
        }
    }

    *rval = &var_hash->data[var_hash->used_slots++];
}

inline static zend_class_entry* msgpack_unserialize_class(
    zval **container, char *class_name, size_t name_len, zend_bool init_class)
{
    zend_class_entry *ce;
    zend_bool incomplete_class = 0;
    zval user_func, retval, args[1], arg_func_name, *container_val;
    int func_call_status;
    zend_string *class_zstring;

    container_val = Z_ISREF_P(*container) ? Z_REFVAL_P(*container) : *container;


    do {
        /* Try to find class directly */
        class_zstring = zend_string_init(class_name, name_len, 0);
        ce = zend_lookup_class(class_zstring);
        zend_string_release(class_zstring);
        if (ce != NULL) {
            break;
        }

        /* Check for unserialize callback */
        if ((PG(unserialize_callback_func) == NULL) ||
            (PG(unserialize_callback_func)[0] == '\0'))
        {
            incomplete_class = 1;
            ce = PHP_IC_ENTRY;
            break;
        }

        /* Call unserialize callback */
        ZVAL_STRING(&user_func, PG(unserialize_callback_func));
        ZVAL_STRING(&arg_func_name, class_name);

        args[0] = arg_func_name;

        func_call_status = call_user_function_ex(CG(function_table), NULL, &user_func, &retval, 1, args, 0, NULL);
        zval_ptr_dtor(&arg_func_name);
        zval_ptr_dtor(&user_func);
        if (func_call_status != SUCCESS) {
            MSGPACK_WARNING("[msgpack] (%s) defined (%s) but not found",
                            __FUNCTION__, class_name);

            incomplete_class = 1;
            ce = PHP_IC_ENTRY;
            break;
        }

        /* The callback function may have defined the class */
        class_zstring = zend_string_init(class_name, name_len, 0);
        if ((ce = zend_lookup_class(class_zstring)) == NULL) {
            MSGPACK_WARNING("[msgpack] (%s) Function %s() hasn't defined "
                            "the class it was called for",
                            __FUNCTION__, class_name);

            incomplete_class = 1;
            ce = PHP_IC_ENTRY;
        }
        zend_string_release(class_zstring);
    }
    while(0);

    if (EG(exception)) {
        MSGPACK_WARNING("[msgpack] (%s) Exception error", __FUNCTION__);
        return NULL;
    }

    if (init_class || incomplete_class) {
        object_init_ex(container_val, ce);
    }

    /* store incomplete class name */
    if (incomplete_class)
    {
        php_store_class_name(container_val, class_name, name_len);
    }

    return ce;
}

void msgpack_serialize_var_init(msgpack_serialize_data_t *var_hash)
{
    HashTable **var_hash_ptr = (HashTable **)var_hash;

    if (MSGPACK_G(serialize).level) {
        *var_hash_ptr = MSGPACK_G(serialize).var_hash;
    } else {
        ALLOC_HASHTABLE(*var_hash_ptr);
        zend_hash_init(*var_hash_ptr, 10, NULL, NULL, 0);
        MSGPACK_G(serialize).var_hash = *var_hash_ptr;
    }
    ++MSGPACK_G(serialize).level;
}

void msgpack_serialize_var_destroy(msgpack_serialize_data_t *var_hash)
{
    HashTable **var_hash_ptr = (HashTable **)var_hash;

    --MSGPACK_G(serialize).level;
    if (!MSGPACK_G(serialize).level) {
        zend_hash_destroy(*var_hash_ptr);
        FREE_HASHTABLE(*var_hash_ptr);
    }
}

void msgpack_unserialize_var_init(msgpack_unserialize_data_t *var_hashx)
{
    var_hashx->first = 0;
    var_hashx->first_dtor = 0;
}

void msgpack_unserialize_var_destroy(msgpack_unserialize_data_t *var_hashx, zend_bool err)
{
    void *next;
    var_entries *var_hash = var_hashx->first;
    while (var_hash) {
        next = var_hash->next;
        efree(var_hash);
        var_hash = next;
    }

    var_hash = var_hashx->first_dtor;
    while (var_hash) {
        next = var_hash->next;
        efree(var_hash);
        var_hash = next;
    }
}

void msgpack_unserialize_set_return_value(msgpack_unserialize_data_t *var_hashx, zval *return_value) {
    var_entries *var_hash;
    if ((var_hash = var_hashx->first) != NULL) {
        ZVAL_COPY_VALUE(return_value, &var_hash->data[0]);
    } else if ((var_hash = var_hashx->first_dtor) != NULL) {
        ZVAL_COPY_VALUE(return_value, &var_hash->data[0]);
    }

}

void msgpack_unserialize_init(msgpack_unserialize_data *unpack)
{
    unpack->deps = 0;
    unpack->type = MSGPACK_SERIALIZE_TYPE_NONE;
}

int msgpack_unserialize_uint8(
    msgpack_unserialize_data *unpack, uint8_t data, zval **obj)
{
    msgpack_stack_push(unpack->var_hash, obj);
    ZVAL_LONG(*obj, data);

    return 0;
}

int msgpack_unserialize_uint16(
    msgpack_unserialize_data *unpack, uint16_t data, zval **obj)
{

    msgpack_stack_push(unpack->var_hash, obj);
    ZVAL_LONG(*obj, data);

    return 0;
}

int msgpack_unserialize_uint32(
    msgpack_unserialize_data *unpack, uint32_t data, zval **obj)
{
    msgpack_stack_push(unpack->var_hash, obj);
    ZVAL_LONG(*obj, data);

    return 0;
}

int msgpack_unserialize_uint64(
    msgpack_unserialize_data *unpack, uint64_t data, zval **obj)
{
    msgpack_stack_push(unpack->var_hash, obj);
    ZVAL_LONG(*obj, data);

    return 0;
}

int msgpack_unserialize_int8(
    msgpack_unserialize_data *unpack, int8_t data, zval **obj)
{
    msgpack_stack_push(unpack->var_hash, obj);
    ZVAL_LONG(*obj, data);

    return 0;
}

int msgpack_unserialize_int16(
    msgpack_unserialize_data *unpack, int16_t data, zval **obj)
{
    msgpack_stack_push(unpack->var_hash, obj);
    ZVAL_LONG(*obj, data);

    return 0;
}

int msgpack_unserialize_int32(
    msgpack_unserialize_data *unpack, int32_t data, zval **obj)
{
    msgpack_stack_push(unpack->var_hash, obj);
    ZVAL_LONG(*obj, data);

    return 0;
}

int msgpack_unserialize_int64(
    msgpack_unserialize_data *unpack, int64_t data, zval **obj)
{
    msgpack_stack_push(unpack->var_hash, obj);
    ZVAL_LONG(*obj, data);

    return 0;
}

int msgpack_unserialize_float(
    msgpack_unserialize_data *unpack, float data, zval **obj)
{
    msgpack_stack_push(unpack->var_hash, obj);
    ZVAL_DOUBLE(*obj, data);

    return 0;
}

int msgpack_unserialize_double(
    msgpack_unserialize_data *unpack, double data, zval **obj)
{
    msgpack_stack_push(unpack->var_hash, obj);
    ZVAL_DOUBLE(*obj, data);

    return 0;
}

int msgpack_unserialize_nil(msgpack_unserialize_data *unpack, zval **obj)
{
    msgpack_stack_push(unpack->var_hash, obj);
    ZVAL_NULL(*obj);

    return 0;
}

int msgpack_unserialize_true(msgpack_unserialize_data *unpack, zval **obj)
{
    msgpack_stack_push(unpack->var_hash, obj);
    ZVAL_BOOL(*obj, 1);

    return 0;
}

int msgpack_unserialize_false(msgpack_unserialize_data *unpack, zval **obj)
{
    msgpack_stack_push(unpack->var_hash, obj);
    ZVAL_BOOL(*obj, 0);

    return 0;
}

int msgpack_unserialize_raw(
    msgpack_unserialize_data *unpack, const char* base,
    const char* data, unsigned int len, zval **obj)
{
    msgpack_stack_push(unpack->var_hash, obj);
    if (len == 0) {
        ZVAL_STRINGL(*obj, "", 0);
    } else {
        ZVAL_STRINGL(*obj, data, len);
    }

    return 0;
}

int msgpack_unserialize_array(
    msgpack_unserialize_data *unpack, unsigned int count, zval **obj)
{
    msgpack_var_push(unpack->var_hash, obj);
    array_init(*obj);

    if (count) unpack->stack[unpack->deps++] = count;

    return 0;
}

int msgpack_unserialize_array_item(
    msgpack_unserialize_data *unpack, zval **container, zval *obj)
{
    add_next_index_zval(*container, obj);

    MSGPACK_UNSERIALIZE_FINISH_ITEM(unpack, 1);

    return 0;
}

int msgpack_unserialize_map(
    msgpack_unserialize_data *unpack, unsigned int count, zval **obj)
{
    msgpack_var_push(unpack->var_hash, obj);
    if (count) unpack->stack[unpack->deps++] = count;

    unpack->type = MSGPACK_SERIALIZE_TYPE_NONE;

    if (count == 0) {
        if (MSGPACK_G(php_only)) {
            object_init(*obj);
        } else {
            array_init(*obj);
        }
    }

    return 0;
}

int msgpack_unserialize_map_item(
    msgpack_unserialize_data *unpack, zval **container, zval *key, zval *val)
{
    long deps;

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
                ce = msgpack_unserialize_class(container, Z_STRVAL_P(val), Z_STRLEN_P(val), 1);

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

                    ce = msgpack_unserialize_class(container, Z_STRVAL_P(key), Z_STRLEN_P(key), 0);
                    if (ce == NULL) {
                        MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);

                        return 0;
                    }

                    /* implementing Serializable */
                    if (ce->unserialize == NULL) {
                        MSGPACK_WARNING(
                            "[msgpack] (%s) Class %s has no unserializer",
                            __FUNCTION__, ce->name);

                        MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);

                        return 0;
                    }

                    ce->unserialize(*container, ce, Z_STRVAL_P(val), Z_STRLEN_P(val) + 1, NULL);

                    MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);

                    return 0;
                case MSGPACK_SERIALIZE_TYPE_RECURSIVE:
                case MSGPACK_SERIALIZE_TYPE_OBJECT:
                case MSGPACK_SERIALIZE_TYPE_OBJECT_REFERENCE:
                {
                    zval *rval;
                    int type = unpack->type;

                    unpack->type = MSGPACK_SERIALIZE_TYPE_NONE;
                    if (msgpack_var_access(unpack->var_hash, Z_LVAL_P(val) - 1, &rval) != SUCCESS)  {
                        MSGPACK_WARNING("[msgpack] (%s) Invalid references value: %ld",
                            __FUNCTION__, Z_LVAL_P(val) - 1);

                        MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);
                        return 0;
                    }

                    if (container != NULL) {
                        zval_ptr_dtor(*container);
                    }

                    *container = rval;
                    if (type == MSGPACK_SERIALIZE_TYPE_OBJECT_REFERENCE) {
                        ZVAL_MAKE_REF(*container);
                    }
                    if (Z_REFCOUNTED_P(*container)) {
                     Z_TRY_ADDREF_P(*container);
                    }


                    MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);

                    return 0;
                }
            }
        }
    }

    zval *container_val = Z_ISREF_P(*container) ? Z_REFVAL_P(*container) : *container;

    if (Z_TYPE_P(container_val) != IS_ARRAY && Z_TYPE_P(container_val) != IS_OBJECT) {
        array_init(container_val);
    }

    if (Z_TYPE_P(container_val) == IS_OBJECT && Z_OBJCE_P(container_val) != PHP_IC_ENTRY) {
        const char *class_name, *prop_name;
        size_t prop_len;
        zend_string *key_zstring = zval_get_string(key);

        zend_unmangle_property_name_ex(key_zstring, &class_name, &prop_name, &prop_len);
        zend_update_property(Z_OBJCE_P(container_val), container_val, prop_name, prop_len, val);

        zend_string_release(key_zstring);
        zval_ptr_dtor(key);
        zval_ptr_dtor(val);
    } else {
        switch (Z_TYPE_P(key)) {
            case IS_LONG:
                if ((val = zend_hash_index_update(HASH_OF(container_val), Z_LVAL_P(key), val)) == NULL) {
                    zval_ptr_dtor(val);
                    MSGPACK_WARNING(
                            "[msgpack] (%s) illegal offset type, skip this decoding",
                            __FUNCTION__);
                }
                zval_ptr_dtor(key);
                break;
            case IS_STRING:
                if ((val = zend_hash_update(HASH_OF(container_val), Z_STR(*key), val)) == NULL) {
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
                    if ((key = zend_hash_next_index_insert(HASH_OF(container_val), key)) == NULL) {
                        zval_ptr_dtor(val);
                    }
                    if ((val = zend_hash_next_index_insert(HASH_OF(container_val), val)) == NULL) {
                        zval_ptr_dtor(val);
                    }
                } else {
                    convert_to_string(key);
                    if ((zend_symtable_update(HASH_OF(container_val), zend_string_init(Z_STRVAL_P(key), Z_STRLEN_P(key), 0), val)) == NULL) {
                        zval_ptr_dtor(val);
                    }
                    zval_ptr_dtor(key);
                }
                break;
        }
    }

    deps = unpack->deps - 1;
    unpack->stack[deps]--;
    if (unpack->stack[deps] == 0)
    {
        unpack->deps--;

        /* wakeup */
        zend_string *wakeup_zstring = zend_string_init("__wakeup", sizeof("__wakeup") - 1, 0);

        if (MSGPACK_G(php_only) &&
            Z_TYPE_P(container_val) == IS_OBJECT &&
            Z_OBJCE_P(container_val) != PHP_IC_ENTRY &&
            zend_hash_exists(&Z_OBJCE_P(container_val)->function_table, wakeup_zstring))
        {
            zval f, h;
            ZVAL_STRING(&f, "__wakeup");

            call_user_function_ex(CG(function_table), container_val, &f, &h, 0, NULL, 1, NULL TSRMLS_CC);

            zval_ptr_dtor(&h);
            zval_ptr_dtor(&f);
        }
        zend_string_release(wakeup_zstring);
    }

    return 0;
}
