
#ifndef MSGPACK_CONVERT_H
#define MSGPACK_CONVERT_H

extern int msgpack_convert_object(zval *return_value, zval *object, zval **value);
extern int msgpack_convert_array(zval *return_value, zval *tpl, zval **value);
extern int msgpack_convert_template(zval *return_value, zval *tpl, zval **value);
extern int msgpack_convert_long_to_properties(HashTable *ht, HashTable **properties, HashPosition *prop_pos,
    uint key_index, zval *val, HashTable *var);
    
#endif
