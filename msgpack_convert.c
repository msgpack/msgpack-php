
#include "php.h"

#include "php_msgpack.h"
#include "msgpack_convert.h"
#include "msgpack_errors.h"

inline int msgpack_convert_long_to_properties(
    HashTable *ht, zval *object, HashTable **properties, HashPosition *prop_pos,
    uint key_index, zval *val, HashTable *var)
{
    if (*properties != NULL) {
        zval *data, tplval, *dataval, prop_key_zv;
        zend_string *prop_key, *unmangled_prop_key;
        ulong prop_key_index;
        const char *class_name, *prop_name;
        size_t prop_len;

        for (;; zend_hash_move_forward_ex(*properties, prop_pos)) {
            if (zend_hash_get_current_key_ex(*properties, &prop_key, &prop_key_index, prop_pos) == HASH_KEY_IS_STRING) {
                zend_unmangle_property_name_ex(prop_key, &class_name, &prop_name, &prop_len);
                ZVAL_NEW_STR(&prop_key_zv, prop_key);
                unmangled_prop_key = zend_string_init(prop_name, prop_len, 0);

                if (var == NULL || !zend_hash_exists(var, unmangled_prop_key)) {
                    if ((data = zend_hash_find(ht, prop_key)) != NULL) {
                        switch (Z_TYPE_P(data)) {
                            case IS_ARRAY:
                                {
                                    HashTable *dataht;
                                    dataht = HASH_OF(val);

                                    if ((dataval = zend_hash_index_find(dataht, prop_key_index)) == NULL) {
                                        MSGPACK_WARNING("[msgpack] (%s) "
                                                "can't get data value by index",
                                                __FUNCTION__);
                                        zend_string_release(unmangled_prop_key);
                                        return FAILURE;
                                    }

                                    if (msgpack_convert_array(&tplval, data, &dataval) == SUCCESS) {
                                        zend_hash_move_forward_ex(*properties, prop_pos);

                                        zend_update_property(Z_OBJCE_P(object), object, prop_name, prop_len, &tplval);
                                        zend_string_release(unmangled_prop_key);
                                        return SUCCESS;
                                    }
                                    zend_string_release(unmangled_prop_key);
                                    return FAILURE;
                                }
                            case IS_OBJECT:
                                {
                                    if (msgpack_convert_object(&tplval, data, &val) == SUCCESS) {
                                        zend_hash_move_forward_ex(*properties, prop_pos);
                                        zend_update_property(Z_OBJCE_P(object), object, prop_name, prop_len, &tplval);
                                        zend_string_release(unmangled_prop_key);
                                        return SUCCESS;
                                    }
                                    zend_string_release(unmangled_prop_key);
                                    return FAILURE;
                                }
                            default:
                                zend_hash_move_forward_ex(*properties, prop_pos);
                                zend_update_property(Z_OBJCE_P(object), object, prop_name, prop_len, val);
                                zend_string_release(unmangled_prop_key);
                                return SUCCESS;
                        }
                    }
                }
                zend_string_release(unmangled_prop_key);
            } else {
                break;
            }
        }
        *properties = NULL;
    }
    zval key_zv;
    ZVAL_LONG(&key_zv, key_index);
    zend_std_write_property(object, &key_zv, val, NULL);
    return SUCCESS;
}

static inline int  msgpack_convert_string_to_properties(
    zval *object, char *key, uint key_len, zval *val, HashTable *var)
{
    zend_class_entry *ce = Z_OBJCE_P(object);
    HashTable *propers = Z_OBJPROP_P(object);
    zend_string *prot_name, *priv_name, *pub_name;
    zval pub_name_z;
    int return_code;

    ZVAL_STRINGL(&pub_name_z, key, key_len);
    priv_name = zend_mangle_property_name(ce->name->val, ce->name->len, key, key_len, 1);
    prot_name = zend_mangle_property_name("*", 1, key, key_len, 1);
    pub_name = zval_get_string(&pub_name_z);

    if (zend_hash_find(propers, priv_name) != NULL) {
        zend_update_property(ce, object, key, key_len, val);
        return_code = SUCCESS;
    } else if (zend_hash_find(propers, prot_name) != NULL) {
        zend_update_property(ce, object, key, key_len, val);
        return_code = SUCCESS;
    } else {
        zend_std_write_property(object, &pub_name_z, val, NULL);
        return_code = FAILURE;
    }
    zend_hash_add(var, pub_name, val);

    zend_string_release(priv_name);
    zend_string_release(prot_name);
    zend_string_release(pub_name);
    zval_ptr_dtor(&pub_name_z);

    return return_code;
}

int msgpack_convert_array(zval *return_value, zval *tpl, zval **value)
{
    if (Z_TYPE_P(tpl) != IS_ARRAY) {
        MSGPACK_WARNING("[msgpack] (%s) template is not array", __FUNCTION__);
        zval_ptr_dtor(*value);
        return FAILURE;
    }

    zend_string *key;
    int key_type;
    ulong key_index;
    zval *data, *arydata;
    HashPosition pos, valpos;
    HashTable *ht, *htval;

    ht = HASH_OF(tpl);
    array_init(return_value);

    if (zend_hash_num_elements(ht) <= 0) {
        MSGPACK_WARNING("[msgpack] (%s) template array length is 0",
                __FUNCTION__);
        zval_ptr_dtor(*value);
        return FAILURE;
    }

    /* string */
    if (ht->nNumOfElements != ht->nNextFreeElement) {
        htval = HASH_OF(*value);
        if (!htval) {
            MSGPACK_WARNING(
                    "[msgpack] (%s) input data is not array",
                    __FUNCTION__);
            zval_ptr_dtor(*value);
            return FAILURE;
        }

        zend_hash_internal_pointer_reset_ex(ht, &pos);
        zend_hash_internal_pointer_reset_ex(htval, &valpos);
        for (;; zend_hash_move_forward_ex(ht, &pos), zend_hash_move_forward_ex(htval, &valpos))
        {
            key_type = zend_hash_get_current_key_ex(ht, &key, &key_index, &pos);

            if (key_type == HASH_KEY_NON_EXISTENT) {
                break;
            }

            if ((data = zend_hash_get_current_data_ex(ht, &pos)) == NULL) {
                continue;
            }

            if (key_type == HASH_KEY_IS_STRING) {
                int (*convert_function)(zval *, zval *, zval **) = NULL;
                zval *dataval, *val;

                switch (Z_TYPE_P(data)) {
                    case IS_ARRAY:
                        convert_function = msgpack_convert_array;
                        break;
                    case IS_OBJECT:
                        // case IS_STRING:
                        convert_function = msgpack_convert_object;
                        break;
                    default:
                        break;
                }

                if ((dataval = zend_hash_get_current_data_ex(htval, &valpos)) == NULL) {
                    MSGPACK_WARNING("[msgpack] (%s) can't get data", __FUNCTION__);
                    zval_ptr_dtor(*value);
                    return FAILURE;
                }

                MSGPACK_CONVERT_COPY_ZVAL(val, dataval);

                if (convert_function) {
                    zval rv;
                    if (convert_function(&rv, data, &val) != SUCCESS) {
                        zval_ptr_dtor(val);
                        return FAILURE;
                    }
                    add_assoc_zval_ex(return_value, key->val, key->len, &rv);
                } else {
                    add_assoc_zval_ex(return_value, key->val, key->len, val);
                }
            }
        }

        zval_ptr_dtor(*value);

        return SUCCESS;
    } else {
        /* index */
        int (*convert_function)(zval *, zval *, zval **) = NULL;

        if (Z_TYPE_P(*value) != IS_ARRAY) {
            MSGPACK_WARNING("[msgpack] (%s) unserialized data must be array.", __FUNCTION__);
            zval_ptr_dtor(*value);
            return FAILURE;
        }

        zend_hash_internal_pointer_reset_ex(ht, &pos);
        key_type = zend_hash_get_current_key_ex(ht, &key, &key_index, &pos);

        if (key_type == HASH_KEY_NON_EXISTENT) {
            MSGPACK_WARNING(
                    "[msgpack] (%s) first element in template array is empty",
                    __FUNCTION__);
            zval_ptr_dtor(*value);
            return FAILURE;
        }

        if ((data = zend_hash_get_current_data_ex(ht, &pos)) == NULL) {
            MSGPACK_WARNING("[msgpack] (%s) invalid template: empty array?", __FUNCTION__);
            zval_ptr_dtor(*value);
            return FAILURE;
        }

        switch (Z_TYPE_P(data))
        {
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
        if (zend_hash_num_elements(htval) <= 0) {
            MSGPACK_WARNING("[msgpack] (%s) array length is 0 in unserialized data", __FUNCTION__);
            zval_ptr_dtor(*value);
            return FAILURE;
        }

        zend_hash_internal_pointer_reset_ex(htval, &valpos);
        for (;; zend_hash_move_forward_ex(htval, &valpos)) {
            key_type = zend_hash_get_current_key_ex(htval, &key, &key_index, &valpos);

            if (key_type == HASH_KEY_NON_EXISTENT) {
                break;
            }

            if ((arydata = zend_hash_get_current_data_ex(htval, &valpos)) == NULL) {
                MSGPACK_WARNING( "[msgpack] (%s) can't get next data in indexed array", __FUNCTION__);
                continue;
            }

            switch (key_type) {
                case HASH_KEY_IS_LONG: {
                        zval rv;
                        if (convert_function) {
                            if (convert_function(&rv, data, &arydata) != SUCCESS) {
                                MSGPACK_WARNING(
                                        "[msgpack] (%s) "
                                        "convert failure in HASH_KEY_IS_LONG "
                                        "in indexed array",
                                        __FUNCTION__);
                                zval_ptr_dtor(*value);
                                return FAILURE;
                            }
                            add_next_index_zval(return_value, &rv);
                        } else {
                            add_next_index_zval(return_value, arydata);
                        }
                        break;
                    }
                case HASH_KEY_IS_STRING:
                    MSGPACK_WARNING("[msgpack] (%s) key is string", __FUNCTION__);
                    zval_ptr_dtor(*value);
                    return FAILURE;
                default:
                    MSGPACK_WARNING("[msgpack] (%s) key is not string nor array", __FUNCTION__);
                    zval_ptr_dtor(*value);
                    return FAILURE;
            }
        }

        //zval_ptr_dtor(*value);
        return SUCCESS;
    }

    // shouldn't reach
    zval_ptr_dtor(*value);
    return FAILURE;

}

int msgpack_convert_object(zval *return_value, zval *tpl, zval **value) {
    zend_class_entry *ce;

    switch (Z_TYPE_P(tpl)) {
        case IS_STRING:
            if ((ce = zend_lookup_class(zval_get_string(tpl))) == NULL) {
                MSGPACK_ERROR("[msgpack] (%s) Class '%s' not found",
                              __FUNCTION__, Z_STRVAL_P(tpl));
                return FAILURE;
            }
            break;
        case IS_OBJECT:
            ce = Z_OBJCE_P(tpl);
            break;
        default:
            MSGPACK_ERROR("[msgpack] (%s) object type is unsupported",
                          __FUNCTION__);
            return FAILURE;
    }

    if (Z_TYPE_P(*value) == IS_OBJECT) {
        zend_class_entry *vce;

        vce = Z_OBJCE_P(*value);
        if (zend_string_equals(ce->name, vce->name)) {
            *return_value = **value;
            zval_copy_ctor(return_value);
            zval_ptr_dtor(*value);
            return SUCCESS;
        }
    }

    object_init_ex(return_value, ce);

    /* Run the constructor if there is one */
    if (ce->constructor && (ce->constructor->common.fn_flags & ZEND_ACC_PUBLIC)) {
        zval retval, params, function_name;
        zend_fcall_info fci;
        zend_fcall_info_cache fcc;

        fci.size = sizeof(fci);
        fci.function_table = EG(function_table);
        fci.function_name = function_name;
        fci.symbol_table = NULL;
        fci.object = Z_OBJ_P(return_value);
        fci.retval = &retval;
        fci.param_count = 0;
        fci.params = &params;
        fci.no_separation = 1;

        fcc.initialized = 1;
        fcc.function_handler = ce->constructor;
        fcc.calling_scope = EG(scope);
        fcc.called_scope = Z_OBJCE_P(return_value);
        fcc.object = Z_OBJ_P(return_value);

        if (zend_call_function(&fci, &fcc TSRMLS_CC) == FAILURE) {
            MSGPACK_WARNING(
                "[msgpack] (%s) Invocation of %s's constructor failed",
                __FUNCTION__, ce->name);

            return FAILURE;
        }
    }

    switch (Z_TYPE_P(*value)) {
        case IS_ARRAY:
        {
            HashTable *ht, *ret, *var = NULL;
            int num;
            zend_string *str_key;
            zval *data;
            ulong num_key;

            ht = HASH_OF(*value);
            ret = HASH_OF(return_value);

            num = zend_hash_num_elements(ht);
            if (num <= 0) {
                zval_ptr_dtor(*value);
                break;
            }

            /* string - php_only mode? */
            if (ht->nNumOfElements != ht->nNextFreeElement || ht->nNumOfElements != ret->nNumOfElements) {
                HashTable *properties = NULL;
                HashPosition prop_pos;

                ALLOC_HASHTABLE(var);
                zend_hash_init(var, num, NULL, NULL, 0);

                ZEND_HASH_FOREACH_STR_KEY_VAL(ht, str_key, data) {
                    if (str_key) {
                        if (msgpack_convert_string_to_properties(return_value, str_key->val, str_key->len, data, var) != SUCCESS) {
                            MSGPACK_WARNING("[msgpack] (%s) "
                                    "illegal offset type, skip this decoding",
                                    __FUNCTION__);
                        }
                    }
                } ZEND_HASH_FOREACH_END();

                /* index */
                properties = Z_OBJ_HT_P(return_value)->get_properties(return_value TSRMLS_CC);
                if (HASH_OF(tpl)) {
                    properties = HASH_OF(tpl);
                }
                zend_hash_internal_pointer_reset_ex(properties, &prop_pos);


                ZEND_HASH_FOREACH_KEY_VAL(ht, num_key, str_key, data) {
                    if (str_key == NULL) {
                        if (msgpack_convert_long_to_properties(ret, return_value, &properties, &prop_pos, num_key, data, var) != SUCCESS) {
                            MSGPACK_WARNING("[msgpack] (%s) "
                                    "illegal offset type, skip this decoding",
                                    __FUNCTION__);
                        }
                    }
                } ZEND_HASH_FOREACH_END();

                zend_hash_destroy(var);
                FREE_HASHTABLE(var);
            } else {
                int (*convert_function)(zval *, zval *, zval **) = NULL;
                const char *class_name, *prop_name;
                size_t prop_len;
                zval *aryval;

                num_key = 0;
                ZEND_HASH_FOREACH_STR_KEY_VAL(ret, str_key, data) {
                    aryval = zend_hash_index_find(ht, num_key);

                    if (data == NULL) {
                        MSGPACK_WARNING("[msgpack] (%s) can't get data value by index", __FUNCTION__);
                        return FAILURE;
                    }

                    switch (Z_TYPE_P(data)) {
                        case IS_ARRAY:
                            convert_function = msgpack_convert_array;
                            break;
                        case IS_OBJECT:
                        //case IS_STRING: -- may have default values of
                        // class members, so it's not wise to allow
                            convert_function = msgpack_convert_object;
                            break;
                    }

                    zend_unmangle_property_name_ex(str_key, &class_name, &prop_name, &prop_len);

                    if (convert_function) {
                        zval nv;
                        if (convert_function(&nv, data, &aryval) != SUCCESS) {
                            zval_ptr_dtor(aryval);
                            MSGPACK_WARNING("[msgpack] (%s) "
                                "convert failure in convert_object",
                                __FUNCTION__);
                            return FAILURE;
                        }

                        //zend_update_property(ce, return_value, str_key->val, str_key->len, &nv);
                    } else  {
                        zend_update_property(ce, return_value, prop_name, prop_len, aryval);
                    }
                    num_key++;
                } ZEND_HASH_FOREACH_END();
          }
            zval_ptr_dtor(*value);
            break;
        }
        default:
        {
            HashTable *properties = NULL;
            HashPosition prop_pos;

            properties = Z_OBJ_HT_P(return_value)->get_properties(return_value TSRMLS_CC);
            zend_hash_internal_pointer_reset_ex(properties, &prop_pos);

            if (msgpack_convert_long_to_properties(HASH_OF(return_value), return_value, &properties, &prop_pos, 0, *value, NULL) != SUCCESS) {
                MSGPACK_WARNING("[msgpack] (%s) illegal offset type, skip this decoding",
                    __FUNCTION__);
            }
            break;
        }
    }

    return SUCCESS;
}

int msgpack_convert_template(zval *return_value, zval *tpl, zval **value)
{
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

