
#include "php.h"
#include "php_ini.h"
#include "ext/standard/php_incomplete_class.h"

#include "php_msgpack.h"
#include "msgpack_pack.h"
#include "msgpack_unpack.h"
#include "msgpack_errors.h"

#define VAR_ENTRIES_MAX 1024

typedef struct
{
    zval data[VAR_ENTRIES_MAX];
    long used_slots;
    void *next;
} var_entries;

#define MSGPACK_UNSERIALIZE_FINISH_ITEM(_unpack, _count) \
    msgpack_stack_pop(_unpack->var_hash, _count);        \
    _unpack->stack[_unpack->deps-1]--;                   \
    if (_unpack->stack[_unpack->deps-1] == 0) {          \
        _unpack->deps--;                                 \
    }

#define MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(_unpack, _key, _val) \
    zval_ptr_dtor(_key);                                        \
    zval_ptr_dtor(_val);                                        \
    MSGPACK_UNSERIALIZE_FINISH_ITEM(_unpack, 2);

inline static void msgpack_var_push(
    msgpack_unserialize_data_t *var_hashx, zval **rval)
{
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
        var_hash = emalloc(sizeof(var_entries));
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

inline static int msgpack_var_access(
    msgpack_unserialize_data_t *var_hashx, long id, zval ***store)
{
    var_entries *var_hash = var_hashx->first;

    while (id >= VAR_ENTRIES_MAX &&
           var_hash && var_hash->used_slots == VAR_ENTRIES_MAX)
    {
        var_hash = var_hash->next;
        id -= VAR_ENTRIES_MAX;
    }

    if (!var_hash)
    {
        return !SUCCESS;
    }

    if (id < 0 || id >= var_hash->used_slots)
    {
        return !SUCCESS;
    }

    // TODO
    //*store = &var_hash->data[id];

    return SUCCESS;
}

inline static void msgpack_stack_pop(
    msgpack_unserialize_data_t *var_hashx, long count)
{
    long i;
    var_entries *var_hash = var_hashx->first_dtor;

    while (var_hash && var_hash->used_slots == VAR_ENTRIES_MAX)
    {
        var_hash = var_hash->next;
    }

    if (!var_hash || count <= 0)
    {
        return;
    }

    for (i = count; i > 0; i--) {
        var_hash->used_slots--;
        if (var_hash->used_slots < 0) {
            var_hash->used_slots = 0;
            // TODO
            //var_hash->data[var_hash->used_slots] = NULL;
            break;
        } else {
            // TODO
            //var_hash->data[var_hash->used_slots] = NULL;
        }
    }
}

inline static zend_class_entry* msgpack_unserialize_class(
    zval **container, char *class_name, size_t name_len)
{
    zend_class_entry *ce;
    zend_bool incomplete_class = 0;
    zval user_func, retval, args[1], arg_func_name;
    TSRMLS_FETCH();

    do
    {
        /* Try to find class directly */
        if ((ce = zend_lookup_class(zend_string_init("Obj", name_len, 0))) != NULL) {
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
        if (call_user_function_ex(CG(function_table), NULL, &user_func, &retval, 1, args, 0, NULL) != SUCCESS)
        {
            MSGPACK_WARNING("[msgpack] (%s) defined (%s) but not found",
                            __FUNCTION__, class_name);

            incomplete_class = 1;
            ce = PHP_IC_ENTRY;
            break;
        }

        /* The callback function may have defined the class */
        if ((ce = zend_lookup_class(zend_string_init(class_name, name_len, 0))) == NULL) {
            MSGPACK_WARNING("[msgpack] (%s) Function %s() hasn't defined "
                            "the class it was called for",
                            __FUNCTION__, class_name);

            incomplete_class = 1;
            ce = PHP_IC_ENTRY;
        }
    }
    while(0);

    if (EG(exception)) {
        MSGPACK_WARNING("[msgpack] (%s) Exception error", __FUNCTION__);
        return NULL;
    }

    object_init_ex(*container, ce);

    /* store incomplete class name */
    if (incomplete_class)
    {
        php_store_class_name(*container, class_name, name_len);
    }

    return ce;
}

void msgpack_serialize_var_init(msgpack_serialize_data_t *var_hash)
{
    HashTable **var_hash_ptr = (HashTable **)var_hash;
    TSRMLS_FETCH();

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

void msgpack_unserialize_var_destroy(
    msgpack_unserialize_data_t *var_hashx, zend_bool err, zval *return_value)
{
    void *next;
    long i;
    var_entries *var_hash = var_hashx->first;

    if (var_hash) {
        ZVAL_DUP(return_value, &var_hash->data[0]);
            for (i = var_hash->used_slots - 1; i >= 0; i--) {
                //zval_ptr_dtor(&var_hash->data[i]);
            }
            //efree(var_hash);
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
    msgpack_var_push(unpack->var_hash, obj);
    ZVAL_LONG(*obj, data);

    return 0;
}

int msgpack_unserialize_uint16(
    msgpack_unserialize_data *unpack, uint16_t data, zval **obj)
{

    msgpack_var_push(unpack->var_hash, obj);
    ZVAL_LONG(*obj, data);

    return 0;
}

int msgpack_unserialize_uint32(
    msgpack_unserialize_data *unpack, uint32_t data, zval **obj)
{
    msgpack_var_push(unpack->var_hash, obj);
    ZVAL_LONG(*obj, data);

    return 0;
}

int msgpack_unserialize_uint64(
    msgpack_unserialize_data *unpack, uint64_t data, zval **obj)
{
    msgpack_var_push(unpack->var_hash, obj);
    ZVAL_LONG(*obj, data);

    return 0;
}

int msgpack_unserialize_int8(
    msgpack_unserialize_data *unpack, int8_t data, zval **obj)
{
    msgpack_var_push(unpack->var_hash, obj);
    ZVAL_LONG(*obj, data);

    return 0;
}

int msgpack_unserialize_int16(
    msgpack_unserialize_data *unpack, int16_t data, zval **obj)
{
    msgpack_var_push(unpack->var_hash, obj);
    ZVAL_LONG(*obj, data);

    return 0;
}

int msgpack_unserialize_int32(
    msgpack_unserialize_data *unpack, int32_t data, zval **obj)
{
    msgpack_var_push(unpack->var_hash, obj);
    ZVAL_LONG(*obj, data);

    return 0;
}

int msgpack_unserialize_int64(
    msgpack_unserialize_data *unpack, int64_t data, zval **obj)
{
    msgpack_var_push(unpack->var_hash, obj);
    ZVAL_LONG(*obj, data);

    return 0;
}

int msgpack_unserialize_float(
    msgpack_unserialize_data *unpack, float data, zval **obj)
{
    msgpack_var_push(unpack->var_hash, obj);
    ZVAL_DOUBLE(*obj, data);

    return 0;
}

int msgpack_unserialize_double(
    msgpack_unserialize_data *unpack, double data, zval **obj)
{
    msgpack_var_push(unpack->var_hash, obj);
    ZVAL_DOUBLE(*obj, data);

    return 0;
}

int msgpack_unserialize_nil(msgpack_unserialize_data *unpack, zval **obj)
{
    msgpack_var_push(unpack->var_hash, obj);
    ZVAL_NULL(*obj);

    return 0;
}

int msgpack_unserialize_true(msgpack_unserialize_data *unpack, zval **obj)
{
    msgpack_var_push(unpack->var_hash, obj);
    ZVAL_BOOL(*obj, 1);

    return 0;
}

int msgpack_unserialize_false(msgpack_unserialize_data *unpack, zval **obj)
{
    msgpack_var_push(unpack->var_hash, obj);
    ZVAL_BOOL(*obj, 0);

    return 0;
}

int msgpack_unserialize_raw(
    msgpack_unserialize_data *unpack, const char* base,
    const char* data, unsigned int len, zval **obj)
{
    msgpack_var_push(unpack->var_hash, obj);
    if (len == 0) {
        ZVAL_STRINGL(*obj, "", 0);
    } else {
        ZVAL_STRINGL(*obj, (char *)data, len);
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

    if (count == 0)
    {
        if (MSGPACK_G(php_only))
        {
            object_init(*obj);
        }
        else
        {
            array_init(*obj);
        }
    }


    return 0;
}

int msgpack_unserialize_map_item(
    msgpack_unserialize_data *unpack, zval **container, zval *key, zval *val)
{
    long deps;

    if (MSGPACK_G(php_only))
    {
        zend_class_entry *ce;
        if (Z_TYPE_P(key) == IS_NULL)
        {
            unpack->type = MSGPACK_SERIALIZE_TYPE_NONE;

            if (Z_TYPE_P(val) == IS_LONG)
            {
                switch (Z_LVAL_P(val))
                {
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
            }
            else if (Z_TYPE_P(val) == IS_STRING)
            {
                ce = msgpack_unserialize_class(
                    container, Z_STRVAL_P(val), Z_STRLEN_P(val));

                if (ce == NULL)
                {
                    MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);

                    return 0;
                }
            }

            MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);

            return 0;
        }
        else
        {
            switch (unpack->type)
            {
                case MSGPACK_SERIALIZE_TYPE_CUSTOM_OBJECT:
                    unpack->type = MSGPACK_SERIALIZE_TYPE_NONE;

                    ce = msgpack_unserialize_class(
                        container, Z_STRVAL_P(key), Z_STRLEN_P(key));
                    if (ce == NULL)
                    {
                        MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);

                        return 0;
                    }

#if (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION > 0)
                    /* implementing Serializable */
                    if (ce->unserialize == NULL)
                    {
                        MSGPACK_WARNING(
                            "[msgpack] (%s) Class %s has no unserializer",
                            __FUNCTION__, ce->name);

                        MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);

                        return 0;
                    }

                    ce->unserialize(
                        container, ce,
                        (const unsigned char *)Z_STRVAL_P(val),
                        Z_STRLEN_P(val) + 1, NULL TSRMLS_CC);
#endif

                    MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);

                    return 0;
                case MSGPACK_SERIALIZE_TYPE_RECURSIVE:
                case MSGPACK_SERIALIZE_TYPE_OBJECT:
                case MSGPACK_SERIALIZE_TYPE_OBJECT_REFERENCE:
                {
                    zval **rval;
                    int type = unpack->type;

                    unpack->type = MSGPACK_SERIALIZE_TYPE_NONE;
                    if (msgpack_var_access(
                            unpack->var_hash,
                            Z_LVAL_P(val) - 1, &rval) != SUCCESS)
                    {
                        MSGPACK_WARNING(
                            "[msgpack] (%s) Invalid references value: %ld",
                            __FUNCTION__, Z_LVAL_P(val) - 1);

                        MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);

                        return 0;
                    }

                    if (container != NULL)
                    {
                        zval_ptr_dtor(*container);
                    }

                    *container = *rval;

                    Z_TRY_ADDREF_P(*container);

                    if (type == MSGPACK_SERIALIZE_TYPE_OBJECT && Z_ISREF_P(*container))
                    {
                        ZVAL_UNREF(*container);
                    }
                    else if (type == MSGPACK_SERIALIZE_TYPE_OBJECT_REFERENCE && Z_ISREF_P(*container))
                    {
                        ZVAL_UNREF(*container);
                    }

                    MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);

                    return 0;
                }
            }
        }
    }

    int container_is_ref = 0;
    if (Z_ISREF_P(*container)) {
        container_is_ref = 1;
        ZVAL_DEREF(*container);
    }

    if (Z_TYPE_P(*container) != IS_ARRAY && Z_TYPE_P(*container) != IS_OBJECT) {
        array_init(*container);
    }

    if (Z_TYPE_P(*container) == IS_OBJECT) {
        zend_update_property(Z_OBJ_P(*container)->ce, *container, Z_STRVAL_P(key), Z_STRLEN_P(key), val);
    } else {
        switch (Z_TYPE_P(key)) {
            case IS_LONG:
                if ((val = zend_hash_index_update(HASH_OF(*container), Z_LVAL_P(key), val)) == NULL) {
                    zval_ptr_dtor(val);
                    MSGPACK_WARNING(
                            "[msgpack] (%s) illegal offset type, skip this decoding",
                            __FUNCTION__);
                }
                zval_ptr_dtor(key);
                break;
            case IS_STRING:
                if ((val = zend_symtable_update(HASH_OF(*container), zend_string_init(Z_STRVAL_P(key), Z_STRLEN_P(key), 0), val)) == NULL) {
                    zval_ptr_dtor(val);
                    MSGPACK_WARNING(
                            "[msgpack] (%s) illegal offset type, skip this decoding",
                            __FUNCTION__);
                }
                zval_ptr_dtor(key);
                break;
            default:
                MSGPACK_WARNING("[msgpack] (%s) illegal key type", __FUNCTION__);

                if (MSGPACK_G(illegal_key_insert))
                {
                    if ((key = zend_hash_next_index_insert(HASH_OF(*container), key)) == NULL) {
                        zval_ptr_dtor(val);
                    }
                    if ((val = zend_hash_next_index_insert(HASH_OF(*container), val)) == NULL) {
                        zval_ptr_dtor(val);
                    }
                }
                else
                {
                    convert_to_string(key);
                    if ((zend_symtable_update(HASH_OF(*container), zend_string_init(Z_STRVAL_P(key), Z_STRLEN_P(key), 0), val)) == NULL) {
                        zval_ptr_dtor(val);
                    }
                    zval_ptr_dtor(key);
                }
                break;
        }
    }

    if (container_is_ref) {
        ZVAL_MAKE_REF(*container);
        Z_TRY_ADDREF_P(*container);
    }

    msgpack_stack_pop(unpack->var_hash, 2);

    deps = unpack->deps - 1;
    unpack->stack[deps]--;
    if (unpack->stack[deps] == 0)
    {
        unpack->deps--;

        /* wakeup */
        if (MSGPACK_G(php_only) &&
            Z_TYPE_P(*container) == IS_OBJECT &&
            Z_OBJCE_P(*container) != PHP_IC_ENTRY &&
            zend_hash_exists(&Z_OBJ_P(*container)->ce->function_table, zend_string_init("__wakeup", sizeof("__wakeup") - 1, 0)))
        {
            zval f, h;
            ZVAL_STRING(&f, "__wakeup");

            call_user_function_ex(CG(function_table), *container, &f, &h, 0, NULL, 1, NULL TSRMLS_CC);
            zval_ptr_dtor(&h);
        }
    }

    return 0;
}
