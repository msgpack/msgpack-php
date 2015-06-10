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
    size_t used_slots;
    void *next;
} var_entries;

#define MSGPACK_UNSERIALIZE_FINISH_ITEM(_unpack, _count) \
	/* msgpack_stack_pop(_unpack->var_hash, _count);  */ \
    _unpack->stack[_unpack->deps-1]--;                   \
    if (_unpack->stack[_unpack->deps-1] == 0) {          \
        _unpack->deps--;                                 \
    }

#define MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(_unpack, _key, _val) \
    zval_ptr_dtor(_key);                                        \
    zval_ptr_dtor(_val);                                        \
    MSGPACK_UNSERIALIZE_FINISH_ITEM(_unpack, 2);

static zval *msgpack_var_push(msgpack_unserialize_data_t *var_hashx) /* {{{ */ {
    var_entries *var_hash, *prev = NULL;

    if (!var_hashx) {
        return NULL;
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

    return &var_hash->data[var_hash->used_slots++];
}
/* }}} */

static zval *msgpack_var_access(msgpack_unserialize_data_t *var_hashx, long id) /* {{{ */ {
    var_entries *var_hash = var_hashx->first;

    while (id >= VAR_ENTRIES_MAX && var_hash && var_hash->used_slots == VAR_ENTRIES_MAX) {
        var_hash = var_hash->next;
        id -= VAR_ENTRIES_MAX;
    }

    if (!var_hash) {
		return NULL;
    }

    if (id < 0 || id >= var_hash->used_slots) {
		return NULL;
    }

	return &var_hash->data[id];
}
/* }}} */

static zval *msgpack_stack_push(msgpack_unserialize_data_t *var_hashx) /* {{{ */ {
    var_entries *var_hash, *prev = NULL;

    if (!var_hashx) {
        return NULL;
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

    return &var_hash->data[var_hash->used_slots++];
}
/* }}} */

static inline void msgpack_stack_pop(msgpack_unserialize_data_t *var_hashx, uint32_t count) /* {{{ */ {
	uint32_t i;
	var_entries *var_hash = var_hashx->first_dtor;

	while (var_hash && var_hash->used_slots == VAR_ENTRIES_MAX) {
		var_hash = var_hash->next;
	}

	if (!var_hash || count <= 0) {
		return;
	}

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
}
/* }}} */

static zend_class_entry* msgpack_unserialize_class(zval **container, zend_string *class_name, zend_bool init_class) /* {{{ */ {
    zend_class_entry *ce;
    int func_call_status;
    zend_bool incomplete_class = 0;
    zval user_func, retval, args[1], *container_val;

    container_val = Z_ISREF_P(*container) ? Z_REFVAL_P(*container) : *container;

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
                            __FUNCTION__, class_name->val);

            incomplete_class = 1;
            ce = PHP_IC_ENTRY;
            break;
        }

        if ((ce = zend_lookup_class(class_name)) == NULL) {
            MSGPACK_WARNING("[msgpack] (%s) Function %s() hasn't defined "
                            "the class it was called for",
                            __FUNCTION__, class_name->val);

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
        object_init_ex(container_val, ce);
    }

    /* store incomplete class name */
    if (incomplete_class) {
        php_store_class_name(container_val, class_name->val, class_name->len);
    }

    return ce;
}
/* }}} */

void msgpack_unserialize_var_init(msgpack_unserialize_data_t *var_hashx) /* {{{ */ {
    var_hashx->first = 0;
    var_hashx->first_dtor = 0;
}
/* }}} */

void msgpack_unserialize_var_destroy(msgpack_unserialize_data_t *var_hashx, zend_bool err) /* {{{ */ {
    void *next;
    var_entries *var_hash = var_hashx->first;

    while (var_hash) {
		/*
		if (err) {
			for (i = var_hash->used_slots - 1; i > 0; i--) {
				zval_ptr_dtor(&var_hash->data[i]);
				ZVAL_UNDEF(&var_hash->data[i]);
			}
		}
		*/
        next = var_hash->next;
        efree(var_hash);
        var_hash = next;
    }

    var_hash = var_hashx->first_dtor;
    while (var_hash) {
		/*
		for (i = var_hash->used_slots - 1; i >= 0; i--) {
			zval_ptr_dtor(&var_hash->data[i]);
			ZVAL_UNDEF(&var_hash->data[i]);
		}
		*/
        next = var_hash->next;
        efree(var_hash);
        var_hash = next;
    }
}
/* }}} */

void msgpack_unserialize_set_return_value(msgpack_unserialize_data_t *var_hashx, zval *return_value) /* {{{ */ {
    var_entries *var_hash;
    if ((var_hash = var_hashx->first) != NULL) {
        ZVAL_COPY_VALUE(return_value, &var_hash->data[0]);
    } else if ((var_hash = var_hashx->first_dtor) != NULL) {
        ZVAL_COPY_VALUE(return_value, &var_hash->data[0]);
    }
}
/* }}} */

void msgpack_unserialize_init(msgpack_unserialize_data *unpack) /* {{{ */ {
    unpack->deps = 0;
    unpack->type = MSGPACK_SERIALIZE_TYPE_NONE;
}
/* }}} */

int msgpack_unserialize_uint8(msgpack_unserialize_data *unpack, uint8_t data, zval **obj) /* {{{ */ {
    *obj = msgpack_stack_push(unpack->var_hash);
    ZVAL_LONG(*obj, data);

    return 0;
}
/* }}} */

int msgpack_unserialize_uint16(msgpack_unserialize_data *unpack, uint16_t data, zval **obj) /* {{{ */ {

    *obj = msgpack_stack_push(unpack->var_hash);
    ZVAL_LONG(*obj, data);

    return 0;
}
/* }}} */

int msgpack_unserialize_uint32(msgpack_unserialize_data *unpack, uint32_t data, zval **obj) /* {{{ */ {
    *obj = msgpack_stack_push(unpack->var_hash);
    ZVAL_LONG(*obj, data);

    return 0;
}
/* }}} */

int msgpack_unserialize_uint64(msgpack_unserialize_data *unpack, uint64_t data, zval **obj) /* {{{ */ {
    *obj = msgpack_stack_push(unpack->var_hash);
    ZVAL_LONG(*obj, data);

    return 0;
}
/* }}} */

int msgpack_unserialize_int8(msgpack_unserialize_data *unpack, int8_t data, zval **obj) /* {{{ */ {
    *obj = msgpack_stack_push(unpack->var_hash);
    ZVAL_LONG(*obj, data);

    return 0;
}
/* }}} */

int msgpack_unserialize_int16(msgpack_unserialize_data *unpack, int16_t data, zval **obj) /* {{{ */ {
    *obj = msgpack_stack_push(unpack->var_hash);
    ZVAL_LONG(*obj, data);

    return 0;
}
/* }}} */

int msgpack_unserialize_int32(msgpack_unserialize_data *unpack, int32_t data, zval **obj) /* {{{ */ {
    *obj = msgpack_stack_push(unpack->var_hash);
    ZVAL_LONG(*obj, data);

    return 0;
}
/* }}} */

int msgpack_unserialize_int64(msgpack_unserialize_data *unpack, int64_t data, zval **obj) /* {{{ */ {
    *obj = msgpack_stack_push(unpack->var_hash);
    ZVAL_LONG(*obj, data);

    return 0;
}
/* }}} */

int msgpack_unserialize_float(msgpack_unserialize_data *unpack, float data, zval **obj) /* {{{ */ {
    *obj = msgpack_stack_push(unpack->var_hash);
    ZVAL_DOUBLE(*obj, data);

    return 0;
}
/* }}} */

int msgpack_unserialize_double(msgpack_unserialize_data *unpack, double data, zval **obj) /* {{{ */ {
    *obj = msgpack_stack_push(unpack->var_hash);
    ZVAL_DOUBLE(*obj, data);

    return 0;
}
/* }}} */

int msgpack_unserialize_nil(msgpack_unserialize_data *unpack, zval **obj) /* {{{ */ {
    *obj = msgpack_stack_push(unpack->var_hash);
    ZVAL_NULL(*obj);

    return 0;
}
/* }}} */

int msgpack_unserialize_true(msgpack_unserialize_data *unpack, zval **obj) /* {{{ */ {
    *obj = msgpack_stack_push(unpack->var_hash);
    ZVAL_BOOL(*obj, 1);

    return 0;
}
/* }}} */

int msgpack_unserialize_false(msgpack_unserialize_data *unpack, zval **obj) /* {{{ */ {
    *obj = msgpack_stack_push(unpack->var_hash);
    ZVAL_BOOL(*obj, 0);

    return 0;
}
/* }}} */

int msgpack_unserialize_raw(msgpack_unserialize_data *unpack, const char* base, const char* data, unsigned int len, zval **obj) /* {{{ */ {
    *obj = msgpack_stack_push(unpack->var_hash);

	if (len == 0) {
		ZVAL_EMPTY_STRING(*obj);
	} else {
		ZVAL_STRINGL(*obj, data, len);
	}

    return 0;
}
/* }}} */

int msgpack_unserialize_array(msgpack_unserialize_data *unpack, unsigned int count, zval **obj) /* {{{ */ {
    *obj = msgpack_var_push(unpack->var_hash);

    array_init(*obj);

    if (count) {
		unpack->stack[unpack->deps++] = count;
	}

    return 0;
}
/* }}} */

int msgpack_unserialize_array_item(msgpack_unserialize_data *unpack, zval **container, zval *obj) /* {{{ */ {
    add_next_index_zval(*container, obj);

    MSGPACK_UNSERIALIZE_FINISH_ITEM(unpack, 1);

    return 0;
}
/* }}} */

int msgpack_unserialize_map(msgpack_unserialize_data *unpack, unsigned int count, zval **obj) /* {{{ */ {
    *obj = msgpack_var_push(unpack->var_hash);

    if (count) {
		unpack->stack[unpack->deps++] = count;
	}

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
/* }}} */

int msgpack_unserialize_map_item(msgpack_unserialize_data *unpack, zval **container, zval *key, zval *val) /* {{{ */ {
    long deps;
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
                            __FUNCTION__, ce->name);

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
                    if ((rval = msgpack_var_access(unpack->var_hash, Z_LVAL_P(val) - 1)) == NULL)  {
                        MSGPACK_WARNING("[msgpack] (%s) Invalid references value: %ld",
                            __FUNCTION__, Z_LVAL_P(val) - 1);

                        MSGPACK_UNSERIALIZE_FINISH_MAP_ITEM(unpack, key, val);
                        return 0;
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
                if (zend_hash_index_update(HASH_OF(container_val), Z_LVAL_P(key), val) == NULL) {
                    zval_ptr_dtor(val);
                    MSGPACK_WARNING(
                            "[msgpack] (%s) illegal offset type, skip this decoding",
                            __FUNCTION__);
                }
                zval_ptr_dtor(key);
                break;
            case IS_STRING:
                if (zend_hash_update(HASH_OF(container_val), Z_STR(*key), val) == NULL) {
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
					zend_string *skey = zval_get_string(key);
                    if ((zend_symtable_update(HASH_OF(container_val), skey, val)) == NULL) {
                        zval_ptr_dtor(val);
                    }
					zend_string_release(skey);
                }
                break;
        }
    }

	/* msgpack_stack_pop(unpack->var_hash, 2); */

    deps = unpack->deps - 1;
    unpack->stack[deps]--;
    if (unpack->stack[deps] == 0) {
        /* wakeup */
        unpack->deps--;
        if (MSGPACK_G(php_only) &&
            Z_TYPE_P(container_val) == IS_OBJECT &&
            Z_OBJCE_P(container_val) != PHP_IC_ENTRY &&
            zend_hash_str_exists(&Z_OBJCE_P(container_val)->function_table, "__wakeup", sizeof("__wakeup") - 1)) {
			zval f, h;
            ZVAL_STRING(&f, "__wakeup");

            call_user_function_ex(CG(function_table), container_val, &f, &h, 0, NULL, 1, NULL);

            zval_ptr_dtor(&h);
            zval_ptr_dtor(&f);
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
