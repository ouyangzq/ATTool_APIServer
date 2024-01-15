/*********************************************************************************************************
 *  ------------------------------------------------------------------------------------------------------
 *  file description
 *  ------------------------------------------------------------------------------------------------------
 *         \file  json.h
 *        \brief  This is a C language version of json parser
 *       \author  Lamdonn
 *        \email  Lamdonn@163.com
 *      \details  v1.7.0
 *    \copyright  Copyright (C) 2023 Lamdonn.
 ********************************************************************************************************/
#ifndef __json_H
#define __json_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdarg.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>

/* json types define @def JSON_TYPE */
/* normal types */
#define JSON_TYPE_UNKNOW        (0) /* unknow type */
#define JSON_TYPE_NULL          (1) /* base type, null */
#define JSON_TYPE_BOOL          (2) /* base type, bool */
#define JSON_TYPE_NUMBER        (3) /* base type, number */
#define JSON_TYPE_STRING        (4) /* base type, string */
#define JSON_TYPE_ARRAY         (5) /* extension type, array */
#define JSON_TYPE_OBJECT        (6) /* extension type, object */
/* other information flag */
#define JSON_VALUE_B_TRUE       (1<<8) /* the value range of bool type json */
#define JSON_NUMBER_INT         (1<<9) /* extended type flag of number type json, 1 integer or 0 floating point */
#define JSON_WITH_KEY           (1<<10) /* whether the key exists in the json object */

/* bool define */
#define JSON_FALSE              (0) /* false */
#define JSON_TRUE               (1) /* true */

/* error type define */
#define JSON_E_OK               (0) // ok
#define JSON_E_INVALID          (1) // invalid
#define JSON_E_GRAMMAR          (2) // common grammatical error
#define JSON_E_END              (3) // extra invalid text at the end of the text
#define JSON_E_KEY              (4) // an error occurred while parsing key
#define JSON_E_VALUE            (5) // an error occurred while parsing value
#define JSON_E_MEMORY           (6) // memory allocation failed
#define JSON_E_NUMBER           (7) // invalid number
#define JSON_E_INDICATOR        (8) // missing indicator ':'

/* head and tail of json array or object */
#define JSON_HEAD               (0)
#define JSON_TAIL               INT_MAX

/* json define */
typedef struct _JSON_ {
    /* link */
    struct _JSON_* next; /* protected, not readable and writable */

    /* information */
    int info; /* protected, readable only */

    /* The space behind the structure is variable key and value */
    /* [char *key] */
    /* [int value / double value / char* value / json_t child] */
} JSON, *json_t;

/* memory hooks type define */
typedef void* (*malloc_t)(size_t size);
typedef void (*free_t)(void* block);
typedef void* (*realloc_t)(void* block, size_t size);

/* for different platforms, set different memory hook functions, the default is POSIX standard */
int json_set_hooks(malloc_t _malloc, free_t _free, realloc_t _realloc);

/* load json */
json_t json_loads(const char* text);
json_t json_loads_options(const char* text, int check_end, const char** return_end);
json_t json_file_load(char* filename);

/* when loading fails, use this method to locate the error */
int json_error_info(int* line, int* column);

/* dump json */
char* json_dumps(json_t json, int preset, int unformat, int* len);
int json_file_dump(json_t json, char* filename);

/* get json key and value */
/* before getting the value, it is recommended to judge whether it is the target type before getting it */
const char* json_key(json_t json);
int json_value_bool(json_t json);
int json_value_int(json_t json);
double json_value_float(json_t json);
const char* json_value_string(json_t json);
json_t json_value_array(json_t json);
json_t json_value_object(json_t json);

/* get information of array/object json */
/* This type of method is only valid for json that is an array or object */
int json_get_size(json_t json);
json_t json_get_child(json_t json, const char* key, int index);
json_t json_get_by_indexs(json_t json, int index, ...);
json_t json_get_by_keys(json_t json, char* key, ...);

/* create json */
/* if the key is NULL, it can only be inserted into array json, and if it is not NULL, it can only be inserted into object json */
json_t json_create_null(char* key);
json_t json_create_bool(char* key, int b);
json_t json_create_int(char* key, int num);
json_t json_create_float(char* key, double num);
json_t json_create_string(char* key, const char* string);
json_t json_create_object(char* key);
json_t json_create_array(char* key);
json_t json_create_array_int(char* key, const int* numbers, int count);
json_t json_create_array_float(char* key, const float* numbers, int count);
json_t json_create_array_double(char* key, const double* numbers, int count);
json_t json_create_array_string(char* key, const char** strings, int count);

/* a method symmetrical to the create method */
void json_delete(json_t json);

/* modify json basic type value */
/* such methods will not modify the json type, only the same type can be successfully modified, if the type is different, replace it on the parent node to modify it */
int json_set_key(json_t json, const char* key);
int json_set_bool(json_t json, int b);
int json_set_int(json_t json, int num);
int json_set_float(json_t json, double num);
int json_set_string(json_t json, const char* string);

/* json storage structure adjustment method */
json_t json_attach(json_t json, int index, json_t item);
json_t json_detach(json_t json, char *key, int index);
json_t json_replace(json_t json, char *key, int index, json_t item);

/* json deep copy */
json_t json_duplicate(json_t json);

/* json format text minify */
void json_minify(char* text);

/* detach and delete */
#define json_detach_by_index(json, index)       json_detach((json), NULL, (index))
#define json_detach_by_key(json, key)           json_detach((json), (key), 0)
#define json_erase(json, key, index)            json_delete(json_detach((json), (key), (index)))
#define json_erase_by_index(json, index)        json_erase(json, NULL, index)
#define json_erase_by_key(json, key)            json_erase(json, key, 0)

/* add json item to array */
/* The default is the tail plug, the insertion efficiency is low, it can be changed to the head plug according to the actual situation */
#define json_add_null_to_array(json)            (json_isarray(json) ? json_attach((json), JSON_TAIL, json_create_null(NULL)) : NULL)
#define json_add_bool_to_array(json, b)         (json_isarray(json) ? json_attach((json), JSON_TAIL, json_create_bool(NULL, (b))) : NULL)
#define json_add_int_to_array(json, n)          (json_isarray(json) ? json_attach((json), JSON_TAIL, json_create_int(NULL, (n))) : NULL)
#define json_add_float_to_array(json, n)        (json_isarray(json) ? json_attach((json), JSON_TAIL, json_create_float(NULL, (n))) : NULL)
#define json_add_string_to_array(json, s)       (json_isarray(json) ? json_attach((json), JSON_TAIL, json_create_string(NULL, (s))) : NULL)
#define json_add_array_to_array(json)           (json_isarray(json) ? json_attach((json), JSON_TAIL, json_create_array(NULL)) : NULL)
#define json_add_object_to_array(json)          (json_isarray(json) ? json_attach((json), JSON_TAIL, json_create_object(NULL)) : NULL)

/* add json item to object */
/* The default is the tail plug, the insertion efficiency is low, it can be changed to the head plug according to the actual situation */
#define json_add_null_to_object(json, key)      ((json_isobject(json) && (key)) ? json_attach((json), JSON_TAIL, json_create_null(key)) : NULL)
#define json_add_bool_to_object(json, key, b)   ((json_isobject(json) && (key)) ? json_attach((json), JSON_TAIL, json_create_bool(key, (b))) : NULL)
#define json_add_int_to_object(json, key, n)    ((json_isobject(json) && (key)) ? json_attach((json), JSON_TAIL, json_create_int(key, (n))) : NULL)
#define json_add_float_to_object(json, key, n)  ((json_isobject(json) && (key)) ? json_attach((json), JSON_TAIL, json_create_float(key, (n))) : NULL)
#define json_add_string_to_object(json, key, s) ((json_isobject(json) && (key)) ? json_attach((json), JSON_TAIL, json_create_string(key, (s))) : NULL)
#define json_add_array_to_object(json, key)     ((json_isobject(json) && (key)) ? json_attach((json), JSON_TAIL, json_create_array(key)) : NULL)
#define json_add_object_to_object(json, key)    ((json_isobject(json) && (key)) ? json_attach((json), JSON_TAIL, json_create_object(key)) : NULL)

/* continuously get child items */
#define json_to_index(json, i, ...)             json_get_by_indexs((json), (i), ##__VA_ARGS__, INT_MIN)
#define json_to_key(json, key, ...)             json_get_by_keys((json), (key), ##__VA_ARGS__, NULL)

/* json type */
#define json_type(json)                         ((json) ? (*(char *)(&((json)->info))) : 0)

/* judge json type */
#define json_isnull(json)                       (json_type(json) == JSON_TYPE_NULL)
#define json_isbool(json)                       (json_type(json) == JSON_TYPE_BOOL)
#define json_isnumber(json)                     (json_type(json) == JSON_TYPE_NUMBER)
#define json_isint(json)                        (json_type(json) == JSON_TYPE_NUMBER && ((json)->info & JSON_NUMBER_INT))
#define json_isfloat(json)                      (json_type(json) == JSON_TYPE_NUMBER && !((json)->info & JSON_NUMBER_INT))
#define json_isstring(json)                     (json_type(json) == JSON_TYPE_STRING)
#define json_isarray(json)                      (json_type(json) == JSON_TYPE_ARRAY)
#define json_isobject(json)                     (json_type(json) == JSON_TYPE_OBJECT)

/* iterative traversal over an array or object */
#define json_array_for_each(json, item)         for ((item) = json_value_array(json); (item); (item) = (item)->next)
#define json_object_for_each(json, item)        for ((item) = json_value_object(json); (item); (item) = (item)->next)

#ifdef __cplusplus
}
#endif

#endif
