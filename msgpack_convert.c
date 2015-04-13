
#include "php.h"

#include "php_msgpack.h"
#include "msgpack_convert.h"
#include "msgpack_errors.h"

#if (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 3)
#   define Z_REFCOUNT_P(pz)    ((pz)->refcount)
#   define Z_SET_ISREF_P(pz)   (pz)->is_ref = 1
#   define Z_UNSET_ISREF_P(pz) (pz)->is_ref = 0
#endif

#define MSGPACK_CONVERT_COPY_ZVAL(_pz, _ppz)   \
    ALLOC_INIT_ZVAL(_pz);                      \
    *(_pz) = **(_ppz);                         \
    if (PZVAL_IS_REF(*(_ppz))) {               \
        if (Z_REFCOUNT_P(*(_ppz)) > 0) {       \
            zval_copy_ctor(_pz);               \
        } else {                               \
            FREE_ZVAL(*(_ppz));                \
        }                                      \
        INIT_PZVAL(_pz);                       \
        Z_SET_ISREF_P(_pz);                    \
    } else {                                   \
        zval_copy_ctor(_pz);                   \
        INIT_PZVAL(_pz);                       \
    }

#define MSGPACK_CONVERT_UPDATE_PROPERTY(_ht, _key, _val, _var)   \
    if ((val = zend_symtable_update(_ht, _key, _val)) != NULL) { \
        zend_hash_add(_var, _key, _val);                         \
        return SUCCESS;                                          \
    }

static inline int  msgpack_convert_long_to_properties(
    HashTable *ht, HashTable **properties, HashPosition *prop_pos,
    uint key_index, zval *val, HashTable *var)
{
    TSRMLS_FETCH();

    if (*properties != NULL)
    {
        zend_string *key_str;
        ulong key_long;
        zval *data;

        //non-iteration
        zval *dataval = NULL, *tplval = NULL;

        ZEND_HASH_FOREACH_KEY_VAL(ht, key_long, key_str, data) {
            if(key_str) {
                switch (Z_TYPE_P(data))
                {
                    case IS_ARRAY:
                        {
                            HashTable *dataht;
                            dataht = HASH_OF(val);
                            if ((dataval = zend_hash_index_find(dataht, key_long)) != NULL) {
                                MSGPACK_WARNING(
                                        "[msgpack] (%s) "
                                        "can't get data value by index",
                                        __FUNCTION__);
                                return FAILURE;
                            }

                            if (msgpack_convert_array(tplval, data, &dataval) == SUCCESS) {
                                return (zend_symtable_update(ht, key_str, tplval) != NULL);
                            }
                            // TODO: de we need to call dtor?
                            return FAILURE;
                        }
                    case IS_OBJECT:
                        {
                            if (msgpack_convert_object(tplval, data, &val) == SUCCESS) {
                                return (zend_symtable_update(ht, key_str, tplval) != NULL);
                            }
                            // TODO: de we need to call dtor?
                            return FAILURE;
                        }
                    default:
                        return (zend_symtable_update(ht, key_str, tplval) != NULL);
                }
            }
        } ZEND_HASH_FOREACH_END();
        *properties = NULL;
    }
    return (zend_hash_index_update(ht, key_index, val) != NULL);
}

static inline int  msgpack_convert_string_to_properties(
    zval *object, char *key, uint key_len, zval *val, HashTable *var)
{
    zval *data = NULL;
    HashTable *ht;
    zend_class_entry *ce;
    zend_string *priv_name, *prot_name;

    TSRMLS_FETCH();

    ht = HASH_OF(object);
    ce = Z_OBJ_P(object)->ce;

    /* private */
    priv_name = zend_mangle_property_name(ce->name->val, ce->name->len, key, key_len - 1, 1);
     if ((data = zend_hash_find(ht, priv_name)) != NULL) {
         MSGPACK_CONVERT_UPDATE_PROPERTY(ht, priv_name, val, var);
     }

     /* protected */
     prot_name = zend_mangle_property_name("*", 1, key, key_len - 1, 1);
     if ((data = zend_hash_find(ht, prot_name)) != NULL) {
         MSGPACK_CONVERT_UPDATE_PROPERTY(ht, prot_name, val, var);
     }

    /* public */
    MSGPACK_CONVERT_UPDATE_PROPERTY(ht, zend_string_init(key, key_len, 1), val, var);

    return FAILURE;
}

int msgpack_convert_array(zval *return_value, zval *tpl, zval **value)
{
    TSRMLS_FETCH();

    if (Z_TYPE_P(tpl) == IS_ARRAY)
    {
        HashTable *ht, *htval;
        int num;
        zend_string *key_str;
        ulong key_long;
        zval *data;


        ht = HASH_OF(tpl);
        // TODO: maybe need to release memory?
        array_init(return_value);

        num = zend_hash_num_elements(ht);
        if (num <= 0)
        {
            MSGPACK_WARNING(
                "[msgpack] (%s) template array length is 0",
                __FUNCTION__);
            zval_ptr_dtor(*value);
            return FAILURE;
        }

        /* string */
        if (ht->nNumOfElements != ht->nNextFreeElement)
        {
            htval = HASH_OF(*value);
            if (!htval)
            {
                MSGPACK_WARNING(
                    "[msgpack] (%s) input data is not array",
                    __FUNCTION__);
                zval_ptr_dtor(*value);
                return FAILURE;
            }

            ZEND_HASH_FOREACH_KEY_VAL(ht, key_long, key_str, data) {
                if (key_str) {
                    int (*convert_function)(zval *, zval *, zval **) = NULL;
                    zval **dataval, *val;

                    switch (Z_TYPE_P(data))
                    {
                        case IS_ARRAY:
                            convert_function = msgpack_convert_array;
                            break;
                        case IS_OBJECT:
                            convert_function = msgpack_convert_object;
                            break;
                        default:
                            break;
                    }

                    if (!data) {
                        MSGPACK_WARNING(
                            "[msgpack] (%s) can't get data",
                            __FUNCTION__);
                        zval_ptr_dtor(*value);
                        return FAILURE;
                    }

                    MSGPACK_CONVERT_COPY_ZVAL(val, dataval);

                    if (convert_function)
                    {
                        zval *rv;
                        if (convert_function(rv, data, &val) != SUCCESS)
                        {
                            zval_ptr_dtor(val);
                            return FAILURE;
                        }
                        add_assoc_zval_ex(return_value, key_str->val, key_str->len, rv);
                    }
                    else
                    {
                        add_assoc_zval_ex(return_value, key_str->val, key_str->len, val);
                    }


                }

            } ZEND_HASH_FOREACH_END();


            zval_ptr_dtor(*value);

            return SUCCESS;
        }
        else
        {
            /* index */
            int (*convert_function)(zval *, zval *, zval **) = NULL;
            int first_loop = 0;

            if (Z_TYPE_P(*value) != IS_ARRAY)
            {
                MSGPACK_WARNING(
                    "[msgpack] (%s) unserialized data must be array.",
                    __FUNCTION__);
                zval_ptr_dtor(*value);
                return FAILURE;
            }

            ZEND_HASH_FOREACH_KEY_VAL(ht, key_long, key_str, data) {
                if (first_loop) {
                    if (!key_long && !key_str) {
                        MSGPACK_WARNING(
                                "[msgpack] (%s) first element in template array is empty",
                                __FUNCTION__);
                        zval_ptr_dtor(*value);
                        return FAILURE;
                    }

                    if (!data) {
                        MSGPACK_WARNING(
                                "[msgpack] (%s) invalid template: empty array?",
                                __FUNCTION__);
                        zval_ptr_dtor(*value);
                        return FAILURE;
                    }

                    switch (Z_TYPE_P(data)) {
                        case IS_ARRAY:
                            convert_function = msgpack_convert_array;
                            break;
                        case IS_OBJECT:
                        case IS_STRING:
                            convert_function = msgpack_convert_object;
                            break;
                        default:
                            break;
                    }

                    htval = HASH_OF(*value);
                    num = zend_hash_num_elements(htval);
                    if (num <= 0) {
                        MSGPACK_WARNING(
                                "[msgpack] (%s) array length is 0 in unserialized data",
                                __FUNCTION__);
                        zval_ptr_dtor(*value);
                        return FAILURE;
                    }


                    first_loop = 1;
                } else {
                    if (!data) {
                        MSGPACK_WARNING(
                                "[msgpack] (%s) can't get next data in indexed array",
                                __FUNCTION__);
                        continue;
                    } else if (key_long) {
                        zval *aryval, *rv;
                        MSGPACK_CONVERT_COPY_ZVAL(aryval, data);
                        if (convert_function)
                        {
                            if (convert_function(rv, data, &aryval) != SUCCESS)
                            {
                                zval_ptr_dtor(aryval);
                                MSGPACK_WARNING(
                                    "[msgpack] (%s) "
                                    "convert failure in HASH_KEY_IS_LONG "
                                    "in indexed array",
                                    __FUNCTION__);
                                zval_ptr_dtor(*value);
                                return FAILURE;
                            }
                            add_next_index_zval(return_value, rv);
                        }
                        else
                        {
                            add_next_index_zval(return_value, aryval);
                        }
                        break;
                    } else if (key_str) {
                        MSGPACK_WARNING(
                            "[msgpack] (%s) key is string",
                            __FUNCTION__);
                        zval_ptr_dtor(*value);
                        return FAILURE;
                    } else {
                        MSGPACK_WARNING(
                            "[msgpack] (%s) key is not string nor array",
                            __FUNCTION__);
                        zval_ptr_dtor(*value);
                        return FAILURE;
                    }
                }
            } ZEND_HASH_FOREACH_END();

            zval_ptr_dtor(*value);
            return SUCCESS;
        }
    }
    else
    {
        // shouldn't reach
        MSGPACK_WARNING(
            "[msgpack] (%s) template is not array",
            __FUNCTION__);
        zval_ptr_dtor(*value);
        return FAILURE;
    }

    // shouldn't reach
    zval_ptr_dtor(*value);
    return FAILURE;
}

int msgpack_convert_object(zval *return_value, zval *tpl, zval **value)
{
    zend_class_entry *ce, **pce;
    TSRMLS_FETCH();

    switch (Z_TYPE_P(tpl))
    {
        case IS_STRING:
            if (zend_lookup_class(
                    Z_STRVAL_P(tpl), Z_STRLEN_P(tpl),
                    &pce TSRMLS_CC) != SUCCESS)
            {
                MSGPACK_ERROR("[msgpack] (%s) Class '%s' not found",
                              __FUNCTION__, Z_STRVAL_P(tpl));
                return FAILURE;
            }
            ce = *pce;
            break;
        case IS_OBJECT:
            ce = zend_get_class_entry(tpl TSRMLS_CC);
            break;
        default:
            MSGPACK_ERROR("[msgpack] (%s) object type is unsupported",
                          __FUNCTION__);
            return FAILURE;
    }

    if (Z_TYPE_PP(value) == IS_OBJECT)
    {
        zend_class_entry *vce;

        vce = zend_get_class_entry(*value TSRMLS_CC);
        if (strcmp(ce->name, vce->name) == 0)
        {
            *return_value = **value;
            zval_copy_ctor(return_value);
            zval_ptr_dtor(value);
            return SUCCESS;
        }
    }

    object_init_ex(return_value, ce);

    /* Run the constructor if there is one */
    if (ce->constructor
        && (ce->constructor->common.fn_flags & ZEND_ACC_PUBLIC))
    {
        zval *retval_ptr = NULL;
        zval ***params = NULL;
        int num_args = 0;
        zend_fcall_info fci;
        zend_fcall_info_cache fcc;

#if ZEND_MODULE_API_NO >= 20090626
        fci.size = sizeof(fci);
        fci.function_table = EG(function_table);
        fci.function_name = NULL;
        fci.symbol_table = NULL;
        fci.object_ptr = return_value;
        fci.retval_ptr_ptr = &retval_ptr;
        fci.param_count = num_args;
        fci.params = params;
        fci.no_separation = 1;

        fcc.initialized = 1;
        fcc.function_handler = ce->constructor;
        fcc.calling_scope = EG(scope);
        fcc.called_scope = Z_OBJCE_P(return_value);
        fcc.object_ptr = return_value;
#else
        fci.size = sizeof(fci);
        fci.function_table = EG(function_table);
        fci.function_name = NULL;
        fci.symbol_table = NULL;
        fci.object_pp = &return_value;
        fci.retval_ptr_ptr = &retval_ptr;
        fci.param_count = num_args;
        fci.params = params;
        fci.no_separation = 1;

        fcc.initialized = 1;
        fcc.function_handler = ce->constructor;
        fcc.calling_scope = EG(scope);
        fcc.object_pp = &return_value;
#endif

        if (zend_call_function(&fci, &fcc TSRMLS_CC) == FAILURE)
        {
            if (params)
            {
                efree(params);
            }
            if (retval_ptr)
            {
                zval_ptr_dtor(&retval_ptr);
            }

            MSGPACK_WARNING(
                "[msgpack] (%s) Invocation of %s's constructor failed",
                __FUNCTION__, ce->name);

            return FAILURE;
        }
        if (retval_ptr)
        {
            zval_ptr_dtor(&retval_ptr);
        }
        if (params)
        {
            efree(params);
        }
    }

    switch (Z_TYPE_PP(value))
    {
        case IS_ARRAY:
        {
            char *key;
            uint key_len;
            int key_type;
            ulong key_index;
            zval **data;
            HashPosition pos;
            HashTable *ht, *ret;
            HashTable *var = NULL;
            int num;

            ht = HASH_OF(*value);
            ret = HASH_OF(return_value);

            num = zend_hash_num_elements(ht);
            if (num <= 0)
            {
                zval_ptr_dtor(value);
                break;
            }

            /* string - php_only mode? */
            if (ht->nNumOfElements != ht->nNextFreeElement
                || ht->nNumOfElements != ret->nNumOfElements)
            {
                HashTable *properties = NULL;
                HashPosition prop_pos;

                ALLOC_HASHTABLE(var);
                zend_hash_init(var, num, NULL, NULL, 0);

                zend_hash_internal_pointer_reset_ex(ht, &pos);
                for (;; zend_hash_move_forward_ex(ht, &pos))
                {
                    key_type = zend_hash_get_current_key_ex(
                        ht, &key, &key_len, &key_index, 0, &pos);

                    if (key_type == HASH_KEY_NON_EXISTANT)
                    {
                        break;
                    }

                    if (zend_hash_get_current_data_ex(
                            ht, (void *)&data, &pos) != SUCCESS)
                    {
                        continue;
                    }

                    if (key_type == HASH_KEY_IS_STRING)
                    {
                        zval *val;
                        MSGPACK_CONVERT_COPY_ZVAL(val, data);
                        if (msgpack_convert_string_to_properties(
                                return_value, key, key_len, val, var) != SUCCESS)
                        {
                            zval_ptr_dtor(&val);
                            MSGPACK_WARNING(
                                "[msgpack] (%s) "
                                "illegal offset type, skip this decoding",
                                __FUNCTION__);
                        }
                    }
                }

                /* index */
                properties = Z_OBJ_HT_P(return_value)->get_properties(
                    return_value TSRMLS_CC);

                if (HASH_OF(tpl))
                {
                    properties = HASH_OF(tpl);
                }
                zend_hash_internal_pointer_reset_ex(properties, &prop_pos);

                zend_hash_internal_pointer_reset_ex(ht, &pos);
                for (;; zend_hash_move_forward_ex(ht, &pos))
                {
                    key_type = zend_hash_get_current_key_ex(
                        ht, &key, &key_len, &key_index, 0, &pos);

                    if (key_type == HASH_KEY_NON_EXISTANT)
                    {
                        break;
                    }

                    if (zend_hash_get_current_data_ex(
                            ht, (void *)&data, &pos) != SUCCESS)
                    {
                        continue;
                    }

                    switch (key_type)
                    {
                        case HASH_KEY_IS_LONG:
                        {
                            zval *val;
                            MSGPACK_CONVERT_COPY_ZVAL(val, data);
                            if (msgpack_convert_long_to_properties(
                                    ret, &properties, &prop_pos,
                                    key_index, val, var) != SUCCESS)
                            {
                                zval_ptr_dtor(&val);
                                MSGPACK_WARNING(
                                    "[msgpack] (%s) "
                                    "illegal offset type, skip this decoding",
                                    __FUNCTION__);
                            }
                            break;
                        }
                        case HASH_KEY_IS_STRING:
                            break;
                        default:
                            MSGPACK_WARNING(
                                "[msgpack] (%s) key is not string nor array",
                                __FUNCTION__);
                            break;
                    }
                }

                zend_hash_destroy(var);
                FREE_HASHTABLE(var);
            }
            else
            {
                HashPosition valpos;
                int (*convert_function)(zval *, zval *, zval **) = NULL;
                zval **arydata, *aryval;

                /* index */
                zend_hash_internal_pointer_reset_ex(ret, &pos);
                zend_hash_internal_pointer_reset_ex(ht, &valpos);
                for (;; zend_hash_move_forward_ex(ret, &pos),
                        zend_hash_move_forward_ex(ht, &valpos))
                {
                    key_type = zend_hash_get_current_key_ex(
                        ret, &key, &key_len, &key_index, 0, &pos);

                    if (key_type == HASH_KEY_NON_EXISTANT)
                    {
                        break;
                    }

                    if (zend_hash_get_current_data_ex(
                            ret, (void *)&data, &pos) != SUCCESS)
                    {
                        continue;
                    }

                    switch (Z_TYPE_PP(data))
                    {
                        case IS_ARRAY:
                            convert_function = msgpack_convert_array;
                            break;
                        case IS_OBJECT:
                        //case IS_STRING: -- may have default values of
                        // class members, so it's not wise to allow
                            convert_function = msgpack_convert_object;
                            break;
                        default:
                            break;
                    }

                    if (zend_hash_get_current_data_ex(
                            ht, (void *)&arydata, &valpos) != SUCCESS)
                    {
                        MSGPACK_WARNING(
                            "[msgpack] (%s) can't get data value by index",
                            __FUNCTION__);
                        return FAILURE;
                    }

                    MSGPACK_CONVERT_COPY_ZVAL(aryval, arydata);

                    if (convert_function)
                    {
                        zval *rv;
                        ALLOC_INIT_ZVAL(rv);

                        if (convert_function(rv, *data, &aryval) != SUCCESS)
                        {
                            zval_ptr_dtor(&aryval);
                            MSGPACK_WARNING(
                                "[msgpack] (%s) "
                                "convert failure in convert_object",
                                __FUNCTION__);
                            return FAILURE;
                        }

                        zend_symtable_update(
                            ret, key, key_len, &rv, sizeof(rv), NULL);
                    }
                    else
                    {
                        zend_symtable_update(
                            ret, key, key_len, &aryval, sizeof(aryval), NULL);
                    }
                }
            }

            zval_ptr_dtor(value);
            break;
        }
        default:
        {
            HashTable *properties = NULL;
            HashPosition prop_pos;

            properties = Z_OBJ_HT_P(return_value)->get_properties(
                return_value TSRMLS_CC);
            zend_hash_internal_pointer_reset_ex(properties, &prop_pos);

            if (msgpack_convert_long_to_properties(
                    HASH_OF(return_value), &properties, &prop_pos,
                    0, *value, NULL) != SUCCESS)
            {
                MSGPACK_WARNING(
                    "[msgpack] (%s) illegal offset type, skip this decoding",
                    __FUNCTION__);
            }
            break;
        }
    }

    return SUCCESS;
}

int msgpack_convert_template(zval *return_value, zval *tpl, zval **value)
{
    TSRMLS_FETCH();

    switch (Z_TYPE_P(tpl))
    {
        case IS_ARRAY:
            return msgpack_convert_array(return_value, tpl, value);
            break;
        case IS_STRING:
        case IS_OBJECT:
            return msgpack_convert_object(return_value, tpl, value);
            break;
        default:
            MSGPACK_ERROR("[msgpack] (%s) Template type is unsupported",
                          __FUNCTION__);
            return FAILURE;
    }

    // shouldn't reach
    return FAILURE;
}

