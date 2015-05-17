
#include "php.h"
#include "php_ini.h"
#include "ext/standard/php_smart_string.h"
#include "ext/standard/php_incomplete_class.h"
#include "ext/standard/php_var.h"

#include "php_msgpack.h"
#include "msgpack_pack.h"
#include "msgpack_errors.h"

#include "msgpack/pack_define.h"
#define msgpack_pack_user smart_string*
#define msgpack_pack_inline_func(name) \
    static inline void msgpack_pack ## name
#define msgpack_pack_inline_func_cint(name) \
    static inline void msgpack_pack ## name
#define msgpack_pack_append_buffer(user, buf, len) \
    smart_string_appendl(user, (const void*)buf, len)
#include "msgpack/pack_template.h"
#define Z_REF_AWARE_P(_obj) \
    (Z_ISREF_P(_obj)?Z_REFVAL_P(_obj):_obj)

inline static int msgpack_check_ht_is_map(zval *array)
{
    int count = zend_hash_num_elements(Z_ARRVAL_P(Z_REF_AWARE_P(array)));

    if (count != (Z_ARRVAL_P(Z_REF_AWARE_P(array)))->nNextFreeElement) {
        return 1;
    } else {
        int i;
        HashPosition pos = {0};
        zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(Z_REF_AWARE_P(array)), &pos);
        for (i = 0; i < count; i++) {
            if (zend_hash_get_current_key_type_ex(Z_ARRVAL_P(Z_REF_AWARE_P(array)), &pos) != HASH_KEY_IS_LONG) {
                return 1;
            }
            zend_hash_move_forward_ex(Z_ARRVAL_P(Z_REF_AWARE_P(array)), &pos);
        }
    }
    return 0;
}

inline static int msgpack_var_add(
    HashTable *var_hash, zval *var, zval **var_old)
{
    char id[32], *p;
    int len;
    zend_string *zstring;
    zval zv;

    if ((Z_TYPE_P(Z_REF_AWARE_P(var)) == IS_OBJECT) && Z_OBJCE_P(var)) {
        p = zend_print_long_to_buf(
            id + sizeof(id) - 1,
            (((size_t)Z_OBJCE_P(Z_REF_AWARE_P(var)) << 5)
             | ((size_t)Z_OBJCE_P(Z_REF_AWARE_P(var)) >> (sizeof(long) * 8 - 5)))
            + (long)Z_OBJ_HANDLE_P(Z_REF_AWARE_P(var)));
        len = id + sizeof(id) - 1 - p;
    } else if (Z_TYPE_P(Z_REF_AWARE_P(var)) == IS_ARRAY) {
        p = zend_print_long_to_buf(id + sizeof(id) - 1, (long)Z_REF_AWARE_P(var));
        len = id + sizeof(id) - 1 - p;
    } else {
        return FAILURE;
    }

    zstring = zend_string_init(p, len, 0);
    if (var_old && (*var_old = zend_hash_find(var_hash, zstring)) != NULL) {
        if (!Z_ISREF_P(var)) {
            ZVAL_LONG(&zv, -1);
            zend_hash_next_index_insert(var_hash, &zv);
        }
        zend_string_release(zstring);
        return FAILURE;
    }

    ZVAL_LONG(&zv, zend_hash_num_elements(var_hash) + 1);
    zend_hash_add(var_hash, zstring, &zv);
    zend_string_release(zstring);

    return SUCCESS;
}

inline static void msgpack_serialize_string(
    smart_string *buf, char *str, size_t len)
{
    msgpack_pack_raw(buf, len);
    msgpack_pack_raw_body(buf, str, len);
}

inline static void msgpack_serialize_class(
    smart_string *buf, zval *val, zval *retval_ptr, HashTable *var_hash,
    char *class_name, uint32_t name_len, zend_bool incomplete_class TSRMLS_DC)
{
    int count;
    HashTable *ht = HASH_OF(retval_ptr);

    count = zend_hash_num_elements(ht);
    if (incomplete_class) {
        --count;
    }

    if (count > 0) {
        msgpack_pack_map(buf, count + 1);
        msgpack_pack_nil(buf);
        msgpack_serialize_string(buf, class_name, name_len);

        zend_string *key_str;
        ulong key_long;
        zval *value, *data, nval, *nvalp;
        ZVAL_NULL(&nval);
        nvalp = &nval;

        ZEND_HASH_FOREACH_KEY_VAL(ht, key_long, key_str, value) {
            if (incomplete_class && strcmp(key_str->val, MAGIC_MEMBER) == 0) {
                continue;
            }

            if (Z_TYPE_P(Z_REF_AWARE_P(value)) != IS_STRING) {
                MSGPACK_NOTICE(
                        "[msgpack] (%s) __sleep should return an array only "
                        "containing the names of instance-variables to serialize",
                        __FUNCTION__);
                continue;
            }
            zend_string *val_zstring = zval_get_string(value);
            if ((data = zend_hash_find(Z_OBJPROP_P(val), val_zstring)) != NULL) {
                msgpack_serialize_string(buf, Z_STRVAL_P(value), Z_STRLEN_P(value));
                msgpack_serialize_zval(buf, data, var_hash TSRMLS_CC);
            } else {
                zend_class_entry *ce = Z_OBJCE_P(val);
                if (ce) {
                    zend_string *priv_name, *prot_name;
                    do
                    {
                        priv_name = zend_mangle_property_name(ce->name->val, ce->name->len,
                                                              Z_STRVAL_P(value), Z_STRLEN_P(value),
                                                              ce->type & ZEND_INTERNAL_CLASS);
                        if ((data = zend_hash_find(Z_OBJPROP_P(val), priv_name)) != NULL) {
                            msgpack_serialize_string(buf, priv_name->val, priv_name->len);
                            pefree(priv_name, ce->type & ZEND_INTERNAL_CLASS);
                            msgpack_serialize_zval(buf, data, var_hash TSRMLS_CC);
                            break;
                        }

                        pefree(priv_name, ce->type & ZEND_INTERNAL_CLASS);

                        prot_name = zend_mangle_property_name("*", 1,
                                Z_STRVAL_P(value), Z_STRLEN_P(value),
                                ce->type & ZEND_INTERNAL_CLASS);

                        if ((data = zend_hash_find(Z_OBJPROP_P(val), prot_name)) != NULL) {
                            msgpack_serialize_string(buf, prot_name->val, prot_name->len);
                            pefree(prot_name, ce->type & ZEND_INTERNAL_CLASS);
                            msgpack_serialize_zval(buf, data, var_hash TSRMLS_CC);
                            break;
                        }
                        pefree(prot_name, ce->type & ZEND_INTERNAL_CLASS);

                        MSGPACK_NOTICE(
                            "[msgpack] (%s) \"%s\" returned as member "
                            "variable from __sleep() but does not exist",
                            __FUNCTION__, Z_STRVAL_P(value));
                        msgpack_serialize_string(buf, Z_STRVAL_P(value), Z_STRLEN_P(value));
                        msgpack_serialize_zval(buf, nvalp, var_hash TSRMLS_CC);
                    } while (0);
                } else {
                    msgpack_serialize_string(buf, Z_STRVAL_P(value), Z_STRLEN_P(value));
                    msgpack_serialize_zval(buf, nvalp, var_hash TSRMLS_CC);
              }
            }
            zend_string_release(val_zstring);
        } ZEND_HASH_FOREACH_END();
    }
}

inline static void msgpack_serialize_array(
    smart_string *buf, zval *val, HashTable *var_hash, zend_bool object,
    char* class_name, uint32_t name_len, zend_bool incomplete_class TSRMLS_DC)
{
    HashTable *ht;
    size_t n;
    zend_bool hash = 1;

    if (object)
    {
        ht = Z_OBJPROP_P(Z_REF_AWARE_P(val));
    }
    else
    {
        ht = HASH_OF(Z_REF_AWARE_P(val));
    }

    if (ht)
    {
        n = zend_hash_num_elements(ht);
    }
    else
    {
        n = 0;
    }

    if (n > 0 && incomplete_class)
    {
        --n;
    }

    if (object)
    {
		if (MSGPACK_G(php_only))
		{
			if (Z_ISREF_P(val))
			{
				msgpack_pack_map(buf, n + 2);
				msgpack_pack_nil(buf);
				msgpack_pack_long(buf, MSGPACK_SERIALIZE_TYPE_REFERENCE);
			}
			else
			{
				msgpack_pack_map(buf, n + 1);
			}

			msgpack_pack_nil(buf);

			msgpack_serialize_string(buf, class_name, name_len);
		}
		else
		{
			msgpack_pack_array(buf, n);
			hash = 0;
		}
    }
    else if (n == 0)
    {
        hash = 0;
        msgpack_pack_array(buf, n);
    }
    else if (Z_ISREF_P(val) && MSGPACK_G(php_only))
    {
        msgpack_pack_map(buf, n + 1);
        msgpack_pack_nil(buf);
        msgpack_pack_long(buf, MSGPACK_SERIALIZE_TYPE_REFERENCE);
    }
    else if (!msgpack_check_ht_is_map(val))
    {
        hash = 0;
        msgpack_pack_array(buf, n);
    }
    else
    {
        msgpack_pack_map(buf, n);
    }

    if (n > 0)
    {
        if (object || hash)
        {

            zend_string *key_str;
            ulong key_long;
            zval *value;


            ZEND_HASH_FOREACH_KEY_VAL(ht, key_long, key_str, value) {
                if (key_str && incomplete_class && strcmp(key_str->val, MAGIC_MEMBER) == 0) {
                    continue;
                }
                if (key_str) {
                    msgpack_serialize_string(buf, key_str->val, key_str->len);
                } else {
                    msgpack_pack_long(buf, key_long);
                }

                if ((Z_TYPE_P(Z_REF_AWARE_P(value)) == IS_ARRAY && Z_ARRVAL_P(Z_REF_AWARE_P(value))->u.v.nApplyCount > 1)) {
                    msgpack_pack_nil(buf);
                } else {
                    if (Z_TYPE_P(Z_REF_AWARE_P(value)) == IS_ARRAY) {
                        Z_ARRVAL_P(Z_REF_AWARE_P(value))->u.v.nApplyCount++;
                    }

                    msgpack_serialize_zval(buf, value, var_hash TSRMLS_CC);

                    if (Z_TYPE_P(Z_REF_AWARE_P(value)) == IS_ARRAY) {
                        Z_ARRVAL_P(Z_REF_AWARE_P(value))->u.v.nApplyCount--;
                    }
                }
            } ZEND_HASH_FOREACH_END();
        }
        else
        {
            zval *data;
            uint i;

            for (i = 0; i < n; i++)
            {
                if ((data = zend_hash_index_find(ht, i)) == NULL ||
                    !data || &data == &val ||
                    (Z_TYPE_P(Z_REF_AWARE_P(data)) == IS_ARRAY &&
                     Z_ARRVAL_P(Z_REF_AWARE_P(data))->u.v.nApplyCount > 1))
                {
                    msgpack_pack_nil(buf);
                }
                else
                {
                    if (Z_TYPE_P(Z_REF_AWARE_P(data)) == IS_ARRAY)
                    {
                        Z_ARRVAL_P(Z_REF_AWARE_P(data))->u.v.nApplyCount++;
                    }

                    msgpack_serialize_zval(buf, data, var_hash TSRMLS_CC);

                    if (Z_TYPE_P(Z_REF_AWARE_P(data)) == IS_ARRAY)
                    {
                        Z_ARRVAL_P(Z_REF_AWARE_P(data))->u.v.nApplyCount--;
                    }
                }
            }
        }
    }
}

inline static void msgpack_serialize_object(
    smart_string *buf, zval *val, HashTable *var_hash,
    char* class_name, uint32_t name_len, zend_bool incomplete_class TSRMLS_DC)
{
    zval retval, fname;
    int res;
    zend_class_entry *ce = NULL;
    zend_string *sleep_zstring;

    if (Z_OBJCE_P(Z_REF_AWARE_P(val))) {
        ce = Z_OBJCE_P(Z_REF_AWARE_P(val));
    }

    if (ce && ce->serialize != NULL) {
        unsigned char *serialized_data = NULL;
        size_t serialized_length;

        if (ce->serialize(val, &serialized_data, &serialized_length, (zend_serialize_data *)var_hash) == SUCCESS && !EG(exception)) {
            /* has custom handler */
            msgpack_pack_map(buf, 2);

            msgpack_pack_nil(buf);
            msgpack_pack_long(buf, MSGPACK_SERIALIZE_TYPE_CUSTOM_OBJECT);

            msgpack_serialize_string(buf, ce->name->val, ce->name->len);
            msgpack_pack_raw(buf, serialized_length);
            msgpack_pack_raw_body(buf, serialized_data, serialized_length);
        } else {
            msgpack_pack_nil(buf);
        }

        if (serialized_data) {
            efree(serialized_data);
        }

        return;
    }

    sleep_zstring = zend_string_init("__sleep", sizeof("__sleep") - 1, 0);
    ZVAL_STRING(&fname, "__sleep");

    if (ce && ce != PHP_IC_ENTRY &&
        zend_hash_exists(&ce->function_table, sleep_zstring))
    {
        zend_string_release(sleep_zstring);

        if ((res = call_user_function_ex(CG(function_table), val, &fname, &retval, 0, 0, 1, NULL)) == SUCCESS && !EG(exception))
        {
            if (HASH_OF(&retval)) {
                msgpack_serialize_class(
                        buf, val, &retval, var_hash,
                        class_name, name_len, incomplete_class TSRMLS_CC);
            } else {
                MSGPACK_NOTICE(
                        "[msgpack] (%s) __sleep should return an array only "
                        "containing the names of instance-variables "
                        "to serialize", __FUNCTION__);
                msgpack_pack_nil(buf);
            }
            zval_ptr_dtor(&retval);
            zval_ptr_dtor(&fname);
            return;
        }
    } else {
        zval_ptr_dtor(&fname);
        zend_string_release(sleep_zstring);
    }

    msgpack_serialize_array(
        buf, val, var_hash, 1,
        class_name, name_len, incomplete_class TSRMLS_CC);
}

void msgpack_serialize_zval(
    smart_string *buf, zval *val, HashTable *var_hash TSRMLS_DC)
{
    zval *var_already;

    if (Z_TYPE_P(val) == IS_INDIRECT) {
        val = Z_INDIRECT_P(val);
    }

    if (MSGPACK_G(php_only) &&
        var_hash &&
        msgpack_var_add(var_hash, val, &var_already) == FAILURE)
    {
        if (Z_ISREF_P(val))
        {
            if (Z_TYPE_P(Z_REF_AWARE_P(val)) == IS_ARRAY)
            {
                msgpack_pack_map(buf, 2);

                msgpack_pack_nil(buf);
                msgpack_pack_long(buf, MSGPACK_SERIALIZE_TYPE_RECURSIVE);

                msgpack_pack_long(buf, 0);
                msgpack_pack_long(buf, Z_LVAL_P(var_already));

                return;
            }
            else if (Z_TYPE_P(Z_REF_AWARE_P(val)) == IS_OBJECT)
            {
                msgpack_pack_map(buf, 2);

                msgpack_pack_nil(buf);
                msgpack_pack_long(buf, MSGPACK_SERIALIZE_TYPE_OBJECT_REFERENCE);

                msgpack_pack_long(buf, 0);
                msgpack_pack_long(buf, Z_LVAL_P(var_already));

                return;
            }
        }
        else if (Z_TYPE_P(Z_REF_AWARE_P(val)) == IS_OBJECT)
        {
            msgpack_pack_map(buf, 2);

            msgpack_pack_nil(buf);
            msgpack_pack_long(buf, MSGPACK_SERIALIZE_TYPE_OBJECT);

            msgpack_pack_long(buf, 0);
            msgpack_pack_long(buf, Z_LVAL_P(var_already));

            return;
        }
    }

    switch (Z_TYPE_P(Z_REF_AWARE_P(val)))
    {
        case IS_NULL:
            msgpack_pack_nil(buf);
            break;
        case IS_TRUE:
            msgpack_pack_true(buf);
            break;
        case IS_FALSE:
            msgpack_pack_false(buf);
            break;
        case IS_LONG:
            msgpack_pack_long(buf, zval_get_long(val));
            break;
        case IS_DOUBLE:
            msgpack_pack_double(buf, Z_DVAL_P(val));
            break;
        case IS_STRING:
            msgpack_serialize_string(
                buf, Z_STRVAL_P(val), Z_STRLEN_P(val));
            break;
        case IS_ARRAY:
            msgpack_serialize_array(
                buf, val, var_hash, 0, NULL, 0, 0 TSRMLS_CC);
            break;
        case IS_OBJECT:
            {
                PHP_CLASS_ATTRIBUTES;
                PHP_SET_CLASS_ATTRIBUTES(Z_REF_AWARE_P(val));

                msgpack_serialize_object(
                    buf, val, var_hash, class_name->val, class_name->len,
                    incomplete_class TSRMLS_CC);

                PHP_CLEANUP_CLASS_ATTRIBUTES();
            }
            break;
        default:
            MSGPACK_WARNING(
                "[msgpack] (%s) type is unsupported, encoded as null",
                __FUNCTION__);
            msgpack_pack_nil(buf);
            break;
    }
    return;
}
