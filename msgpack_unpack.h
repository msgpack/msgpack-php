
#ifndef MSGPACK_UNPACK_H
#define MSGPACK_UNPACK_H

#include "ext/standard/php_var.h"

#define MSGPACK_EMBED_STACK_SIZE 1024

#include "msgpack/unpack_define.h"

typedef enum
{
    MSGPACK_UNPACK_SUCCESS     =  2,
    MSGPACK_UNPACK_EXTRA_BYTES =  1,
    MSGPACK_UNPACK_CONTINUE    =  0,
    MSGPACK_UNPACK_PARSE_ERROR = -1,
    MSGPACK_UNPACK_NOMEM_ERROR = -2
} msgpack_unpack_return;

typedef struct msgpack_var_hash {
       void *first;
       void *last;
       void *first_dtor;
       void *last_dtor;
       HashTable *allowed_classes;
} msgpack_var_hash;

typedef struct msgpack_unpack_data {
    zval *retval;
    const char *eof;
    int type;
    unsigned int count;
    long deps;
    long stack[MSGPACK_EMBED_STACK_SIZE];
    msgpack_var_hash var_hash;
} msgpack_unpack_data;

void msgpack_unserialize_var_init(msgpack_var_hash *var_hashx);
void msgpack_unserialize_var_destroy(msgpack_var_hash *var_hashx, zend_bool err);

int msgpack_unserialize_uint8(
    msgpack_unpack_data *unpack, uint8_t data, zval **obj);
int msgpack_unserialize_uint16(
    msgpack_unpack_data *unpack, uint16_t data, zval **obj);
int msgpack_unserialize_uint32(
    msgpack_unpack_data *unpack, uint32_t data, zval **obj);
int msgpack_unserialize_uint64(
    msgpack_unpack_data *unpack, uint64_t data, zval **obj);
int msgpack_unserialize_int8(
    msgpack_unpack_data *unpack, int8_t data, zval **obj);
int msgpack_unserialize_int16(
    msgpack_unpack_data *unpack, int16_t data, zval **obj);
int msgpack_unserialize_int32(
    msgpack_unpack_data *unpack, int32_t data, zval **obj);
int msgpack_unserialize_int64(
    msgpack_unpack_data *unpack, int64_t data, zval **obj);
int msgpack_unserialize_float(
    msgpack_unpack_data *unpack, float data, zval **obj);
int msgpack_unserialize_double(
    msgpack_unpack_data *unpack, double data, zval **obj);
int msgpack_unserialize_nil(msgpack_unpack_data *unpack, zval **obj);
int msgpack_unserialize_true(msgpack_unpack_data *unpack, zval **obj);
int msgpack_unserialize_false(msgpack_unpack_data *unpack, zval **obj);
int msgpack_unserialize_str(
    msgpack_unpack_data *unpack, const char* base, const char* data,
    unsigned int len, zval **obj);
int msgpack_unserialize_ext(
    msgpack_unpack_data *unpack, const char* base, const char* data,
    unsigned int len, zval **obj);
int msgpack_unserialize_array(
    msgpack_unpack_data *unpack, unsigned int count, zval **obj);
int msgpack_unserialize_array_item(
    msgpack_unpack_data *unpack, zval **container, zval *obj);
int msgpack_unserialize_map(
    msgpack_unpack_data *unpack, unsigned int count, zval **obj);
int msgpack_unserialize_map_item(
    msgpack_unpack_data *unpack, zval **container, zval *key, zval *val);

/* template functions */
#define msgpack_unpack_struct(name)    struct msgpack_unpack ## name
#define msgpack_unpack_func(ret, name) static ret msgpack_unserialize ## name
#define msgpack_unpack_callback(name)  msgpack_unserialize ## name
#define msgpack_unpack_object          zval*
#define msgpack_unpack_user            msgpack_unpack_data

static inline msgpack_unpack_object msgpack_unserialize_root(msgpack_unpack_user* unpack)
{
    unpack->deps = 0;
    unpack->type = MSGPACK_SERIALIZE_TYPE_NONE;
    msgpack_unserialize_var_init(&unpack->var_hash);
    return NULL;
}

#define msgpack_unserialize_raw msgpack_unserialize_str
#define msgpack_unserialize_bin msgpack_unserialize_str

#include "msgpack/unpack_template.h"

typedef struct msgpack_unpack_context msgpack_unpack_t;

#endif
