
#include "php.h"
#include "php_ini.h"
#include "Zend/zend_smart_str.h"
#include "ext/standard/php_incomplete_class.h"
#include "ext/standard/php_var.h"

#include "php_msgpack.h"
#include "msgpack_pack.h"
#include "msgpack_errors.h"

#include "msgpack/pack_define.h"
#define msgpack_pack_user smart_str*
#define msgpack_pack_inline_func(name) \
    static inline void msgpack_pack ## name
#define msgpack_pack_inline_func_cint(name) \
    static inline void msgpack_pack ## name
#define msgpack_pack_append_buffer(user, buf, len) \
    smart_str_appendl(user, (const void*)buf, len)

#include "msgpack/pack_template.h"

static inline int msgpack_check_ht_is_map(zval *array) /* {{{ */ {
	uint32_t count;

	ZEND_ASSERT(Z_TYPE_P(array) == IS_ARRAY);

	count = zend_hash_num_elements(Z_ARRVAL_P(array));

	if (count != (Z_ARRVAL_P(array))->nNextFreeElement) {
		return 1;
	} else {
		zend_string *key;

		ZEND_HASH_FOREACH_STR_KEY(Z_ARRVAL_P(array), key) {
			if (key) {
				return 1;
			}
		} ZEND_HASH_FOREACH_END();
	}
	return 0;
}
/* }}} */

static inline int msgpack_var_add(HashTable *var_hash, zval *var, zend_long *var_old) /* {{{ */ {
	uint32_t len;
	char id[32], *p;
	zval *var_noref, *var_exists, zv;

	if (UNEXPECTED(Z_TYPE_P(var) == IS_REFERENCE)) {
		var_noref = Z_REFVAL_P(var);
	} else {
		var_noref = var;
	}

	if ((Z_TYPE_P(var_noref) == IS_OBJECT) && Z_OBJCE_P(var_noref)) {
		p = zend_print_long_to_buf(
				id + sizeof(id) - 1,
				(((size_t)Z_OBJCE_P(var_noref) << 5)
				 | ((size_t)Z_OBJCE_P(var_noref) >> (sizeof(long) * 8 - 5))) + (long)Z_OBJ_HANDLE_P(var_noref));
		len = id + sizeof(id) - 1 - p;
	} else if (Z_TYPE_P(var_noref) == IS_ARRAY) {
		p = zend_print_long_to_buf(id + sizeof(id) - 1, (long)(var_noref));
		len = id + sizeof(id) - 1 - p;
	} else {
		/* TODO: uninitialized var_old? */
		return 0;
	}

	if (var_old && (var_exists = zend_hash_str_find(var_hash, p, len)) != NULL) {
		*var_old = Z_LVAL_P(var_exists);
		if (!Z_ISREF_P(var)) {
			ZVAL_LONG(&zv, -1);
			zend_hash_next_index_insert(var_hash, &zv);
		}
		return 0;
	}

	ZVAL_LONG(&zv, zend_hash_num_elements(var_hash) + 1);
	zend_hash_str_add(var_hash, p, len, &zv);

	return 1;
}
/* }}} */

void msgpack_serialize_var_init(msgpack_serialize_data_t *var_hash) /* {{{ */ {
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
/* }}} */

void msgpack_serialize_var_destroy(msgpack_serialize_data_t *var_hash) /* {{{ */ {
	HashTable **var_hash_ptr = (HashTable **)var_hash;

	--MSGPACK_G(serialize).level;
	if (!MSGPACK_G(serialize).level) {
		zend_hash_destroy(*var_hash_ptr);
		FREE_HASHTABLE(*var_hash_ptr);
	}
}
/* }}} */

inline static void msgpack_serialize_string(smart_str *buf, char *str, size_t len) /* {{{ */ {
	msgpack_pack_raw(buf, len);
	msgpack_pack_raw_body(buf, str, len);
}
/* }}} */

static inline void msgpack_serialize_class(smart_str *buf, zval *val, zval *retval_ptr, HashTable *var_hash, char *class_name, uint32_t name_len, zend_bool incomplete_class) /* {{{ */ {
	uint32_t count;
	HashTable *ht = HASH_OF(retval_ptr);

	count = zend_hash_num_elements(ht);

	if (incomplete_class) {
		--count;
	}

	if (count > 0) {
		zend_string *key_str;
		zval *value, *data;

		msgpack_pack_map(buf, count + 1);
		msgpack_pack_nil(buf);
		msgpack_serialize_string(buf, class_name, name_len);

		ZEND_HASH_FOREACH_STR_KEY_VAL(ht, key_str, value) {
			if (incomplete_class && strcmp(ZSTR_VAL(key_str), MAGIC_MEMBER) == 0) {
				continue;
			}

			if (Z_TYPE_P(value) != IS_STRING) {
				ZVAL_DEREF(value);
				if (Z_TYPE_P(value) != IS_STRING) {
					MSGPACK_NOTICE(
							"[msgpack] (%s) __sleep should return an array only "
							"containing the names of instance-variables to serialize",
							__FUNCTION__);
					continue;
				}
			}
			if ((data = zend_hash_find(Z_OBJPROP_P(val), Z_STR_P(value))) != NULL) {
				msgpack_serialize_string(buf, Z_STRVAL_P(value), Z_STRLEN_P(value));
				msgpack_serialize_zval(buf, data, var_hash);
			} else {
				zval nval;
				zend_class_entry *ce = Z_OBJCE_P(val);

				ZVAL_NULL(&nval);
				if (ce) {
					zend_string *priv_name, *prot_name;
					do {
						priv_name = zend_mangle_property_name(ZSTR_VAL(ce->name), ZSTR_LEN(ce->name),
								Z_STRVAL_P(value), Z_STRLEN_P(value),
								ce->type & ZEND_INTERNAL_CLASS);
						if ((data = zend_hash_find(Z_OBJPROP_P(val), priv_name)) != NULL) {
							msgpack_serialize_string(buf, ZSTR_VAL(priv_name), ZSTR_LEN(priv_name));
							pefree(priv_name, ce->type & ZEND_INTERNAL_CLASS);
							msgpack_serialize_zval(buf, data, var_hash);
							break;
						}

						pefree(priv_name, ce->type & ZEND_INTERNAL_CLASS);

						prot_name = zend_mangle_property_name("*", 1,
								Z_STRVAL_P(value), Z_STRLEN_P(value),
								ce->type & ZEND_INTERNAL_CLASS);

						if ((data = zend_hash_find(Z_OBJPROP_P(val), prot_name)) != NULL) {
							msgpack_serialize_string(buf, ZSTR_VAL(prot_name), ZSTR_LEN(prot_name));
							pefree(prot_name, ce->type & ZEND_INTERNAL_CLASS);
							msgpack_serialize_zval(buf, data, var_hash);
							break;
						}
						pefree(prot_name, ce->type & ZEND_INTERNAL_CLASS);

						MSGPACK_NOTICE(
								"[msgpack] (%s) \"%s\" returned as member "
								"variable from __sleep() but does not exist",
								__FUNCTION__, Z_STRVAL_P(value));
						msgpack_serialize_string(buf, Z_STRVAL_P(value), Z_STRLEN_P(value));
						msgpack_serialize_zval(buf, &nval, var_hash);
					} while (0);
				} else {
					msgpack_serialize_string(buf, Z_STRVAL_P(value), Z_STRLEN_P(value));
					msgpack_serialize_zval(buf, &nval, var_hash);
				}
			}
		} ZEND_HASH_FOREACH_END();
	}
}
/* }}} */

static inline void msgpack_serialize_array(smart_str *buf, zval *val, HashTable *var_hash, zend_bool object, char* class_name, uint32_t name_len, zend_bool incomplete_class) /* {{{ */ {
	uint32_t n;
	HashTable *ht;
	zend_bool hash = 1;
	zend_bool is_ref = 0;

	if (UNEXPECTED(Z_TYPE_P(val) == IS_REFERENCE)) {
		is_ref = 1;
		val = Z_REFVAL_P(val);
	}

	if (object) {
		ht = Z_OBJPROP_P(val);
	} else {
		ZEND_ASSERT(Z_TYPE_P(val) == IS_ARRAY);
		ht = Z_ARRVAL_P(val);
	}

	if (ht) {
		n = zend_hash_num_elements(ht);
	} else {
		n = 0;
	}

	if (n > 0 && incomplete_class) {
		--n;
	}

	if (object) {
		if (MSGPACK_G(php_only)) {
			if (is_ref) {
				msgpack_pack_map(buf, n + 2);
				msgpack_pack_nil(buf);
				msgpack_pack_long(buf, MSGPACK_SERIALIZE_TYPE_REFERENCE);
			} else {
				msgpack_pack_map(buf, n + 1);
			}

			msgpack_pack_nil(buf);

			msgpack_serialize_string(buf, class_name, name_len);
		} else {
			hash = 0;
			msgpack_pack_array(buf, n);
		}
	} else if (n == 0) {
		hash = 0;
		msgpack_pack_array(buf, n);
	} else if (is_ref && MSGPACK_G(php_only)) {
		msgpack_pack_map(buf, n + 1);
		msgpack_pack_nil(buf);
		msgpack_pack_long(buf, MSGPACK_SERIALIZE_TYPE_REFERENCE);
	} else if (!msgpack_check_ht_is_map(val)) {
		hash = 0;
		msgpack_pack_array(buf, n);
	} else {
		msgpack_pack_map(buf, n);
	}

	if (n > 0) {
		if (object || hash) {
			zend_string *key_str;
			zend_ulong   key_long;
			zval *value, *value_noref;

			ZEND_HASH_FOREACH_KEY_VAL(ht, key_long, key_str, value) {
				if (key_str && incomplete_class && strcmp(ZSTR_VAL(key_str), MAGIC_MEMBER) == 0) {
					continue;
				}
				if (key_str && hash) {
					msgpack_serialize_string(buf, ZSTR_VAL(key_str), ZSTR_LEN(key_str));
				} else if (hash) {
					msgpack_pack_long(buf, key_long);
				}

				if (Z_TYPE_P(value) == IS_REFERENCE) {
					value_noref = Z_REFVAL_P(value);
				} else {
					value_noref = value;
				}

				if ((Z_TYPE_P(value_noref) == IS_ARRAY && ZEND_HASH_GET_APPLY_COUNT(Z_ARRVAL_P(value_noref)) > 1)) {
					msgpack_pack_nil(buf);
				} else {
					if (Z_TYPE_P(value_noref) == IS_ARRAY && ZEND_HASH_APPLY_PROTECTION(Z_ARRVAL_P(value_noref))) {
						ZEND_HASH_INC_APPLY_COUNT(Z_ARRVAL_P(value_noref));
					}
					msgpack_serialize_zval(buf, value, var_hash);
					if (Z_TYPE_P(value_noref) == IS_ARRAY && ZEND_HASH_APPLY_PROTECTION(Z_ARRVAL_P(value_noref))) {
						ZEND_HASH_DEC_APPLY_COUNT(Z_ARRVAL_P(value_noref));
					}
				}
			} ZEND_HASH_FOREACH_END();
		} else {
			uint32_t i;
			zval *data, *data_noref;

			for (i = 0; i < n; i++) {
				if ((data = zend_hash_index_find(ht, i)) == NULL || &data == &val ||
						(Z_TYPE_P(data) == IS_ARRAY && ZEND_HASH_GET_APPLY_COUNT(Z_ARRVAL_P(data)) > 1)) {
					msgpack_pack_nil(buf);
				} else if (Z_TYPE_P(data) == IS_REFERENCE && Z_TYPE_P(Z_REFVAL_P(data)) == IS_ARRAY &&
						ZEND_HASH_GET_APPLY_COUNT(Z_ARRVAL_P(Z_REFVAL_P(data))) > 1) {
					msgpack_pack_nil(buf);
				} else {
					if (Z_TYPE_P(data) == IS_REFERENCE) {
						data_noref = Z_REFVAL_P(data);
					} else {
						data_noref = data;
					}

					if (Z_TYPE_P(data_noref) == IS_ARRAY && ZEND_HASH_APPLY_PROTECTION(Z_ARRVAL_P(data_noref))) {
						ZEND_HASH_INC_APPLY_COUNT(Z_ARRVAL_P(data_noref));
					}

					msgpack_serialize_zval(buf, data, var_hash);

					if (Z_TYPE_P(data_noref) == IS_ARRAY && ZEND_HASH_APPLY_PROTECTION(Z_ARRVAL_P(data_noref))) {
						ZEND_HASH_DEC_APPLY_COUNT(Z_ARRVAL_P(data_noref));
					}
				}
			}
		}
	}
}
/* }}} */

static inline void msgpack_serialize_object(smart_str *buf, zval *val, HashTable *var_hash, char* class_name, uint32_t name_len, zend_bool incomplete_class) /* {{{ */ {
	int res;
	zval retval, fname;
	zval *val_noref;
	zend_class_entry *ce = NULL;
	zend_string *sleep_zstring;

	if (UNEXPECTED(Z_TYPE_P(val) == IS_REFERENCE)) {
		val_noref = Z_REFVAL_P(val);
	} else {
		val_noref = val;
	}

	if (Z_OBJCE_P(val_noref)) {
		ce = Z_OBJCE_P(val_noref);
	}

	if (ce && ce->serialize != NULL) {
		unsigned char *serialized_data = NULL;
		size_t serialized_length;

		if (ce->serialize(val_noref, &serialized_data, &serialized_length, (zend_serialize_data *)var_hash) == SUCCESS && !EG(exception)) {
			/* has custom handler */
			msgpack_pack_map(buf, 2);

			msgpack_pack_nil(buf);
			msgpack_pack_long(buf, MSGPACK_SERIALIZE_TYPE_CUSTOM_OBJECT);

			msgpack_serialize_string(buf, ZSTR_VAL(ce->name), ZSTR_LEN(ce->name));
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
	ZVAL_STR(&fname, sleep_zstring);

	if (ce && ce != PHP_IC_ENTRY &&
			zend_hash_exists(&ce->function_table, sleep_zstring)) {
		if ((res = call_user_function_ex(CG(function_table), val_noref, &fname, &retval, 0, 0, 1, NULL)) == SUCCESS) {

			if (EG(exception)) {
				zval_ptr_dtor(&retval);
				zval_ptr_dtor(&fname);
				return;
			}
			if (Z_TYPE(retval) == IS_ARRAY) {
				msgpack_serialize_class(
						buf, val_noref, &retval, var_hash,
						class_name, name_len, incomplete_class);
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
	} 

	zval_ptr_dtor(&fname);
	msgpack_serialize_array(
			buf, val, var_hash, 1,
			class_name, name_len, incomplete_class);
}
/* }}} */

void msgpack_serialize_zval(smart_str *buf, zval *val, HashTable *var_hash) /* {{{ */ {
	zval *val_noref;
	zend_long var_already;

	if (Z_TYPE_P(val) == IS_INDIRECT) {
		val = Z_INDIRECT_P(val);
	}

	if (MSGPACK_G(php_only) && var_hash && !msgpack_var_add(var_hash, val, &var_already)) {
		if (Z_ISREF_P(val)) {
			if (Z_TYPE_P(Z_REFVAL_P(val)) == IS_ARRAY) {
				msgpack_pack_map(buf, 2);

				msgpack_pack_nil(buf);
				msgpack_pack_long(buf, MSGPACK_SERIALIZE_TYPE_RECURSIVE);

				msgpack_pack_long(buf, 0);
				msgpack_pack_long(buf, var_already);

				return;
			} else if (Z_TYPE_P(Z_REFVAL_P(val)) == IS_OBJECT) {
				msgpack_pack_map(buf, 2);

				msgpack_pack_nil(buf);
				msgpack_pack_long(buf, MSGPACK_SERIALIZE_TYPE_OBJECT_REFERENCE);

				msgpack_pack_long(buf, 0);
				msgpack_pack_long(buf, var_already);

				return;
			}
		} else if (Z_TYPE_P(val) == IS_OBJECT) {
			msgpack_pack_map(buf, 2);

			msgpack_pack_nil(buf);
			msgpack_pack_long(buf, MSGPACK_SERIALIZE_TYPE_OBJECT);

			msgpack_pack_long(buf, 0);
			msgpack_pack_long(buf, var_already);

			return;
		}
	}

	if (UNEXPECTED(Z_TYPE_P(val) == IS_REFERENCE)) {
		val_noref = Z_REFVAL_P(val);
	} else {
		val_noref = val;
	}

	switch (Z_TYPE_P(val_noref)) {
		case IS_UNDEF:
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
			msgpack_pack_long(buf, zval_get_long(val_noref));
			break;
		case IS_DOUBLE:
			msgpack_pack_double(buf, Z_DVAL_P(val_noref));
			break;
		case IS_STRING:
			msgpack_serialize_string(buf, Z_STRVAL_P(val_noref), Z_STRLEN_P(val_noref));
			break;
		case IS_ARRAY:
			msgpack_serialize_array(buf, val, var_hash, 0, NULL, 0, 0);
			break;
		case IS_OBJECT:
			{
				PHP_CLASS_ATTRIBUTES;
				PHP_SET_CLASS_ATTRIBUTES(val_noref);

				msgpack_serialize_object(buf, val, var_hash, ZSTR_VAL(class_name), ZSTR_LEN(class_name), incomplete_class);

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
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
