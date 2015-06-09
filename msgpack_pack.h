
#ifndef MSGPACK_PACK_H
#define MSGPACK_PACK_H

#include "ext/standard/php_var.h"

typedef HashTable* msgpack_serialize_data_t;

enum msgpack_serialize_type
{
    MSGPACK_SERIALIZE_TYPE_NONE =  0,
    MSGPACK_SERIALIZE_TYPE_REFERENCE =  1,
    MSGPACK_SERIALIZE_TYPE_RECURSIVE,
    MSGPACK_SERIALIZE_TYPE_CUSTOM_OBJECT,
    MSGPACK_SERIALIZE_TYPE_OBJECT,
    MSGPACK_SERIALIZE_TYPE_OBJECT_REFERENCE,
};

void msgpack_serialize_var_init(msgpack_serialize_data_t *var_hash);
void msgpack_serialize_var_destroy(msgpack_serialize_data_t *var_hash);
void msgpack_serialize_zval(smart_str *buf, zval *val, HashTable *var_hash);

#endif
