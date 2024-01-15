/*********************************************************************************************************
 *  ------------------------------------------------------------------------------------------------------
 *  file description
 *  ------------------------------------------------------------------------------------------------------
 *         \file  json.c
 *        \brief  This is a C language version of json parser
 *       \author  Lamdonn
 *        \email  Lamdonn@163.com
 *      \details  v1.7.0
 *    \copyright  Copyright (C) 2023 Lamdonn.
 ********************************************************************************************************/
#include "json.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

/* dump buffer define */
typedef struct
{
	char* address;			  				/* buffer base address */
	int size;				  				/* size of buffer */
	int end;								/* end of buffer used */
} BUFFER;

/* number type define */
typedef union
{
	int int_;								/* int type */
	double float_;							/* float type */
} number;

static malloc_t json_malloc = malloc;		/* memory allocation function */
static free_t json_free = free;				/* memory release function */
static realloc_t json_realloc = realloc;	/* memory reallocate function */
static const char* error = NULL;			/* pointer to error message */
static const char* lbegin = NULL;			/* beginning of line */
static int eline = 0;						/* line of error message */
static int etype = 0;						/* type of error message */

/* predeclare these prototypes. */
static const char* parse_value(const char* text, char* key, json_t* out);
static int print_value(json_t json, BUFFER* buf, int depth, int format);

/* set error message and type */
#define _error(t)							(error=text,etype=(t))
#define _type(info)							(*(char*)(&(info)))
#define _key(obj)							(*(char**)json_key_address(obj))
#define _int(obj)							(*(int*)json_value_address(obj))
#define _float(obj)							(*(double*)json_value_address(obj))
#define _string(obj)						(*(char**)json_value_address(obj))
#define _child(obj)							(*(json_t*)json_value_address(obj))

static void* temp_realloc(void* block, size_t size)
{
	void* b = json_malloc(size);
	if (!b) return NULL;
	if (block)
	{
		memcpy(b, block, size);
		json_free(block);
	}
	return b;
}

/**
 *  \brief provide new malloc and free functions for json parser.
 *  \param[in] _malloc: malloc fuction
 *  \param[in] _free: free fuction
 *  \param[in] _realloc: realloc fuction, if not specified, the default combination is malloc and free
 *  \return 1 success or 0 fail
 */
int json_set_hooks(malloc_t _malloc, free_t _free, realloc_t _realloc)
{
	if (!_malloc || !_free) return 0;
	if (_malloc == malloc && _free == free)
	{
		json_malloc = malloc;
		json_free = free;
		json_realloc = realloc;
	}
	else
	{
		json_malloc = _malloc;
		json_free = _free;
		json_realloc = _realloc ? _realloc : temp_realloc;
	}
	return 1;
}

static void json_report_error(void)
{
	const char* e = error;
	if (!e) return;
	printf("Parsing error, code %d line %d column %d, near [", etype, eline, (int)(error - lbegin));
	if (*e)
	{
		do
		{
			putchar(*e++);
		} while ((!isalnum(*e) == !isalnum(*(e - 1))) && (*(e + 1) > ' '));
	}
	printf("].\r\n");
}

/**
 *  \brief for analysing failed parses
 *  \param[out] *line: error line
 *  \param[out] *column: error column
 *  \return error type
 */
int json_error_info(int* line, int* column)
{
	if (!etype) return JSON_E_OK;
	if (line) *line = eline;
	if (column) *column = (int)(error - lbegin);
	return etype;
}

/**
 *  \brief string case compare.
 *  \param[in] *s1: address of string 1
 *  \param[in] *s2: address of string 2
 *  \return 0 equal, positive s1 > s2, negative s1 < s2
 */
static int string_case_compare(const char* s1, const char* s2)
{
	if (s1 == s2) return 0;
	if (!s1) return -1;
	if (!s2) return 1;
	while (tolower(*s1) == tolower(*s2))
	{
		if (*s1 == 0) return 0;
		s1++, s2++;
	}
	return tolower(*(const unsigned char*)s1) - tolower(*(const unsigned char*)s2);
}

/**
 *  \brief copy the string to the newly created location.
 *  \param[in] *str: address of source string
 *  \return address of new string
 */
static char* json_strdup(const char* str)
{
	int size = (int)strlen(str) + 1;
	char* s;
	s = (char*)json_malloc(size);
	if (!s) return NULL;
	memcpy(s, str, size);
	return s;
}

/**
 *  \brief get the smallest power of 2 not greater than x.
 *  \param[in] x: positive integer
 *  \return the smallest power of 2 not greater than x
 */
static int pow2gt(int x)
{
	int b = sizeof(int) * 8;
	int i = 1;
	--x;
	while (i < b) { x |= (x >> i); i <<= 1; }
	return x + 1;
}

/**
 *  \brief confirm whether buf still has the required capacity, otherwise add capacity.
 *  \param[in] buf: buf handle
 *  \param[in] needed: required capacity
 *  \return 1 success or 0 fail
 */
static int expansion(BUFFER *buf, int needed)
{
	char* address;
	int size;
	if (!buf || !buf->address) return 0;
	needed += buf->end;
	if (needed <= buf->size) return 1; /* there is still enough space in the current buf */
	size = pow2gt(needed);
	address = (char*)json_realloc(buf->address, size);
	if (!address) return 0;
	buf->size = size;
	buf->address = address;
	return 1;
}

#define buf_append(n)		expansion(buf, (n)) /* append n size space for buf */
#define buf_putc(c)			(buf->address[buf->end++]=(c)) /* put a non zero character into buf */
#define buf_puts(s, len)	do{for(int i=0;i<len;i++)buf_putc((s)[i]);}while(0) /* put the specified length character set to buf */
#define buf_end()			(&(buf->address[buf->end])) /* obtain the address at the tail of buf */

/**
 *  \brief get hex4 value from string.
 *  \param[in] *text: address of text
 *  \return hex4 value
 */
static unsigned int get_hex4(const char* text)
{
	unsigned int h = 0;
	if (*text >= '0' && *text <= '9') h += (*text) - '0';
	else if (*text >= 'A' && *text <= 'F') h += 10 + (*text) - 'A';
	else if (*text >= 'a' && *text <= 'f') h += 10 + (*text) - 'a';
	else return 0;
	h = h << 4; text++;
	if (*text >= '0' && *text <= '9') h += (*text) - '0';
	else if (*text >= 'A' && *text <= 'F') h += 10 + (*text) - 'A';
	else if (*text >= 'a' && *text <= 'f') h += 10 + (*text) - 'a';
	else return 0;
	h = h << 4; text++;
	if (*text >= '0' && *text <= '9') h += (*text) - '0';
	else if (*text >= 'A' && *text <= 'F') h += 10 + (*text) - 'A';
	else if (*text >= 'a' && *text <= 'f') h += 10 + (*text) - 'a';
	else return 0;
	h = h << 4; text++;
	if (*text >= '0' && *text <= '9') h += (*text) - '0';
	else if (*text >= 'A' && *text <= 'F') h += 10 + (*text) - 'A';
	else if (*text >= 'a' && *text <= 'f') h += 10 + (*text) - 'a';
	else return 0;
	return h;
}

/**
 *  \brief convert utf16 to utf8.
 *  \param[in] **in: address of input char pointer
 *  \param[in] **out: address of output char pointer
 *  \return none
 */
static void json_utf(const char** in, char** out)
{
	const char* p1 = *in;
	char* p2 = *out;
	int len = 0;
	unsigned int uc, uc2;
	static unsigned char mask_first_byte[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
	uc = get_hex4(p1 + 1); p1 += 4; /* get the unicode char */
	if ((uc >= 0xDC00 && uc <= 0xDFFF) || uc == 0) return; /* check for invalid */
	if (uc >= 0xD800 && uc <= 0xDBFF) /* UTF16 surrogate pairs */
	{
		if (p1[1] != '\\' || p1[2] != 'u') return; /* missing second-half of surrogate */
		uc2 = get_hex4(p1 + 3);
		p1 += 6;
		if (uc2 < 0xDC00 || uc2>0xDFFF)	return; /* invalid second-half of surrogate */
		uc = 0x10000 + (((uc & 0x3FF) << 10) | (uc2 & 0x3FF));
	}
	len = 4;
	if (uc < 0x80) len = 1;
	else if (uc < 0x800) len = 2;
	else if (uc < 0x10000) len = 3;
	p2 += len;
	switch (len)
	{
	case 4: *--p2 = ((uc | 0x80) & 0xBF); uc >>= 6;
	case 3: *--p2 = ((uc | 0x80) & 0xBF); uc >>= 6;
	case 2: *--p2 = ((uc | 0x80) & 0xBF); uc >>= 6;
	case 1: *--p2 = (uc | mask_first_byte[len]);
	}
	p2 += len;
}

/**
 *  \brief skip meaningless characters.
 *  \param[in] *in: address of character
 *  \return the address of the next meaningful character
 */
static const char* skip(const char* in)
{
	while (in && *in && (unsigned char)(*in) <= ' ')
	{
		/* when a newline character is encountered, record the current parsing line */
		if (*in == '\n')
		{
			eline++;
			lbegin = in;
		}
		in++;
	}
	return in;
}

/**
 *  \brief get the address of the hidden member key of the json structure.
 *  \param[in] json: json handle
 *  \return address of the hidden member key or NULL fail
 */
static void* json_key_address(json_t json)
{
	if (!(json->info & JSON_WITH_KEY)) return NULL;
	return (void*)(json + 1);
}

/**
 *  \brief get the address of the hidden member value of the json structure.
 *  \param[in] json: json handle
 *  \return address of value or NULL fail
 */
static void* json_value_address(json_t json)
{
	char* base = (void *)(json + 1);
	if (!json || _type(json->info) == JSON_TYPE_NULL) return NULL;
	if (_type(json->info) == JSON_TYPE_BOOL) return NULL;
	if (json->info & JSON_WITH_KEY) base += sizeof(char*);
	return (void*)base;
}

/**
 *  \brief get the address of key.
 *  \param[in] json: json handle
 *  \return address of key or NULL fail
 */
const char* json_key(json_t json)
{
	if (!json) return NULL;
	return _key(json);
}

/**
 *  \brief if the value type of json is bool, get the bool value
 *  \param[in] json: json handle
 *  \return JSON_TRUE or JSON_FALSE
 */
int json_value_bool(json_t json)
{
	if (!json) return -1;
	if (_type(json->info) != JSON_TYPE_BOOL) return -1;
	return (json->info & JSON_VALUE_B_TRUE) ? JSON_TRUE : JSON_FALSE;
}

/**
 *  \brief if the value type of json is int, get the int value
 *  \param[in] json: json handle
 *  \return int value
 */
int json_value_int(json_t json)
{
	if (!json) return 0;
	if (_type(json->info) != JSON_TYPE_NUMBER) return 0;
	if (!(json->info & JSON_NUMBER_INT)) return 0;
	return _int(json);
}

/**
 *  \brief if the value type of json is float, get the float value
 *  \param[in] json: json handle
 *  \return float value
 */
double json_value_float(json_t json)
{
	if (!json) return 0;
	if (_type(json->info) != JSON_TYPE_NUMBER) return 0;
	if (json->info & JSON_NUMBER_INT) return 0;
	return _float(json);
}

/**
 *  \brief if the value type of json is string, get the string value
 *  \param[in] json: json handle
 *  \return string value
 */
const char* json_value_string(json_t json)
{
	if (!json) return NULL;
	if (_type(json->info) != JSON_TYPE_STRING) return NULL;
	return _string(json);
}

/**
 *  \brief if the value type of json is array, get the array value
 *  \param[in] json: json handle
 *  \return array value
 */
json_t json_value_array(json_t json)
{
	if (!json) return NULL;
	if (_type(json->info) != JSON_TYPE_ARRAY) return NULL;
	return _child(json);
}

/**
 *  \brief if the value type of json is object, get the object value
 *  \param[in] json: json handle
 *  \return object value
 */
json_t json_value_object(json_t json)
{
	if (!json) return NULL;
	if (_type(json->info) != JSON_TYPE_OBJECT) return NULL;
	return _child(json);
}

/**
 *  \brief create new json item.
 *  \param[in] info: information of json
 *  \param[in] *key: json key, create json for object with key, otherwise create json for array
 *  \return newly created json handle
 */
static json_t json_new(int info, char* key)
{
	json_t json;
	int size = sizeof(JSON); /* original size */

	/* determine whether the size of the key member needs to be added */
	if (key) { info |= JSON_WITH_KEY; size += sizeof(char*); }
	else info &= (~JSON_WITH_KEY);

	/* append the size of the value member */
	if (_type(info) == JSON_TYPE_NUMBER) size += sizeof(number);
	else if (_type(info) == JSON_TYPE_STRING) size += sizeof(char*);
	else if (_type(info) == JSON_TYPE_ARRAY || _type(info) == JSON_TYPE_OBJECT) size += sizeof(json_t);

	/* allocate json space and initialize */
	json = (json_t)json_malloc(size);
	if (json)
	{
		memset(json, 0, size);
		json->info = info;
		if (key) _key(json) = key;
	}

	return json;
}

/**
 *  \brief delete the json entity and its sub-entities.
 *  \param[in] json: json handle
 *  \return none
 */
void json_delete(json_t json)
{
	json_t next;

	while (json)
	{
		next = json->next;

		/* delete recursively */
		if (_type(json->info) == JSON_TYPE_ARRAY || _type(json->info) == JSON_TYPE_OBJECT)
		{
			json_delete(_child(json)); /* recursively delete child objects */
		}
		if (_type(json->info) == JSON_TYPE_STRING && _string(json))
		{
			json_free(_string(json)); /* delete string value */
		}
		if (json->info & JSON_WITH_KEY && _key(json))
		{
			json_free(_key(json)); /* delete key */
		}

		json_free(json); /* delete self */

		json = next;
	}
}

/**
 *  \brief parse the input text to generate numbers, and fill the results into json.
 *  \param[in] *text: number text
 *  \param[in] *key: address of key
 *  \param[out] *out: the address used to receive the parsed json object
 *  \return the new address of the transformed text
 */
static const char* parse_number(const char* text, char* key, json_t* out)
{
	json_t n;
	double num = 0;
	int sign = 1, scale = 0, e_sign = 1, e_scale = 0;
	int isint = 1; // int, accurate parsing of integer parts

	if (*text == '-') /* sign part */
	{
		sign = -1;
		text++;
		if (!(*text >= '0' && *text <= '9')) { _error(JSON_E_NUMBER); return NULL; }
	}
	while (*text == '0') text++; /* skip zero */
	if (*text >= '1' && *text <= '9') /* integer part */
	{
		do
		{
			num = (num * 10.0) + (*text++ - '0'); /* carry addition */
		} while (*text >= '0' && *text <= '9');
	}
	if (*text == '.') /* fractional part */
	{
		text++;
		if (!(*text >= '0' && *text <= '9')) { _error(JSON_E_NUMBER); return NULL; }
		do
		{
			num = (num * 10.0) + (*text++ - '0');
			scale--;
		} while (*text >= '0' && *text <= '9');
		isint = 0;
	}
	if (*text == 'e' || *text == 'E') /* exponent part */
	{
		text++;
		if (*text == '+') text++;
		else if (*text == '-') /* with sign */
		{
			e_sign = -1;
			text++;
		}
		if (!(*text >= '0' && *text <= '9')) { _error(JSON_E_NUMBER); return NULL; }
		while (*text >= '0' && *text <= '9') /* num */
		{
			e_scale = (e_scale * 10) + (*text++ - '0');
		}
		isint = 0;
	}

	/* create json */
	n = json_new(JSON_TYPE_NUMBER, key);
	if (!n) { _error(JSON_E_MEMORY); return NULL; }
	*out = n;

	num = (double)sign * num * pow(10.0, (scale + e_scale * e_sign));
	if (isint && INT_MIN <= num && num <= INT_MAX)
	{
		n->info |= JSON_NUMBER_INT;
		_int(n) = (int)num;
	}
	else _float(n) = num;

	return text;
}

/**
 *  \brief parse the input text to buffer, and fill the results into buf.
 *  \param[in] *text: number text
 *  \param[out] **buf: the address used to receive the parsed string pointer
 *  \return the new address of the transformed text
 */
static const char* parse_string_buffer(const char* text, char** buf)
{
	const char* p1 = text + 1;
	char* p2;
	char* out;
	int len = 0;

	*buf = NULL;

	/* not a string! */
	if (*text != '\"') { _error(JSON_E_GRAMMAR); return NULL; }

	/* get length */
	while (*p1 && *p1 != '\"')
	{
		if (*p1++ == '\\') p1++; /* skip escaped quotes. */
		len++;
	}

	out = (char*)json_malloc(len + 1);
	if (!out) { _error(JSON_E_MEMORY); return NULL; }

	/* copy text to new space */
	p1 = text + 1;
	p2 = out;
	while (*p1 && *p1 != '\"')
	{
		/* normal character */
		if (*p1 != '\\')
		{
			*p2++ = *p1++;
		}
		/* escape character */
		else
		{
			p1++;
			if (*p1 == 'b') { *p2++ = '\b'; }
			else if (*p1 == 'f') { *p2++ = '\f'; }
			else if (*p1 == 'n') { *p2++ = '\n'; }
			else if (*p1 == 'r') { *p2++ = '\r'; }
			else if (*p1 == 't') { *p2++ = '\t'; }
			else if (*p1 == 'u') { json_utf(&p1, &p2); }
			else { *p2++ = *p1; }
			p1++;
		}
	}

	*p2 = 0;
	if (*p1 == '\"') p1++;

	*buf = out;

	return p1;
}

/**
 *  \brief parse the input text and fill the result into json.
 *  \param[in] *text: string text
 *  \param[in] *key: address of key
 *  \param[out] *out: the address used to receive the parsed json object
 *  \return the new address of the transformed text
 */
static const char* parse_string(const char* text, char* key, json_t* out)
{
	json_t n;
	const char* t;
	char* buf;

	t = parse_string_buffer(text, &buf);
	if (!t) return NULL;

	n = json_new(JSON_TYPE_STRING, key);
	if (!n) { _error(JSON_E_MEMORY); json_free(buf); return NULL; }
	*out = n;

	_string(n) = buf;

	return t;
}

/**
 *  \brief parse array.
 *  \param[in] *text: text
 *  \param[in] *key: address of key
 *  \param[out] *out: the address used to receive the parsed json object
 *  \return address of the end of the parsed text, or NULL fail
 */
static const char* parse_array(const char* text, char* key, json_t* out)
{
	json_t n, prev = NULL, child = NULL;

	/* not an array! */
	if (*text != '[') { _error(JSON_E_INVALID); return NULL; } 

	/* create json object */
	n = json_new(JSON_TYPE_ARRAY, key);
	if (!n) { _error(JSON_E_MEMORY); return NULL; }
	if (key) _key(n) = NULL;
	*out = n; /* output json object */

	/* empty array. */
	text = skip(text + 1);
	if (*text == ']') return text + 1; 

	/* parse each member of the array */
	do {
		if (prev) text++; /* skip ',' */

		/* parse value */
		text = skip(parse_value(skip(text), NULL, &child));
		if (!text) goto FAIL;

		/* link */
		if (prev) prev->next = child;
		else _child(n) = child;
		prev = child;
	} while (*text == ',');

	if (*text != ']') { _error(JSON_E_INVALID); goto FAIL; }
	
	if (key) _key(n) = key;
	
	return text + 1;

FAIL:
	json_delete(n);
	*out = NULL;
	return NULL;
}

/**
 *  \brief parse object.
 *  \param[in] *text: text
 *  \param[in] *key: address of key
 *  \param[out] *out: the address used to receive the parsed json object
 *  \return address of the end of the parsed text, or NULL fail
 */
static const char* parse_object(const char* text, char* key, json_t* out)
{
	json_t n, prev = NULL, child = NULL;
	char* k = NULL;

	/* not an object! */
	if (*text != '{') { _error(JSON_E_INVALID); return NULL; }

	/* create json object */
	n = json_new(JSON_TYPE_OBJECT, key);
	if (!n) { _error(JSON_E_MEMORY); return NULL; }
	if (key) _key(n) = NULL;
	*out = n; /* output json object */

	/* empty object */
	text = skip(text + 1);
	if (*text == '}') return text + 1;

	/* parse each json object */
	do {
		if (prev) text++; /* skip ',' */

		/* parse key */
		text = skip(parse_string_buffer(skip(text), &k));
		if (!text) goto FAIL;

		/* parse indicator ':' */
		if (*text != ':') { _error(JSON_E_INDICATOR); goto FAIL; }

		/* parse value */
		text = skip(parse_value(skip(text + 1), k, &child));
		if (!text) goto FAIL;

		/* link */
		if (prev) prev->next = child;
		else _child(n) = child;
		prev = child;
	} while (*text == ',');

	if (*text != '}') { _error(JSON_E_INVALID); k = NULL; goto FAIL; }

	if (key) _key(n) = key;

	return text + 1;

FAIL:
	if (k) json_free(k);
	json_delete(n);
	*out = NULL;
	return NULL;
}

/**
 *  \brief parser core, parse text.
 *  \param[in] *text: text
 *  \param[in] *key: address of key
 *  \param[out] *out: the address used to receive the parsed json object
 *  \return address of the end of the parsed text, or NULL fail
 */
static const char* parse_value(const char* text, char* key, json_t* out)
{
	*out = NULL;
	if (!strncmp(text, "null", 4))
	{
		*out = json_new(JSON_TYPE_NULL, key);
		if (!*out) { _error(JSON_E_MEMORY); return NULL; }
		return text + 4;
	}
	if (!strncmp(text, "false", 5))
	{
		*out = json_new(JSON_TYPE_BOOL, key);
		if (!*out) { _error(JSON_E_MEMORY); return NULL; }
		return text + 5;
	}
	if (!strncmp(text, "true", 4))
	{
		*out = json_new(JSON_TYPE_BOOL | JSON_VALUE_B_TRUE, key);
		if (!*out) { _error(JSON_E_MEMORY); return NULL; }
		return text + 4;
	}
	if (*text == '-' || (*text >= '0' && *text <= '9')) return parse_number(text, key, out);
	if (*text == '\"') return parse_string(text, key, out);
	if (*text == '[') return parse_array(text, key, out);
	if (*text == '{') return parse_object(text, key, out);

	_error(JSON_E_INVALID);

	return NULL;
}

/**
 *  \brief convert numbers in json to text and append to buf.
 *  \param[in] json: json handle
 *  \param[in] buf: buf handle
 *  \return 1 success or 0 fail
 */
static int print_number(json_t json, BUFFER* buf)
{
	double f;
	int len = 0;

	/* the number type is an integer */
	if (json->info & JSON_NUMBER_INT)
	{
		if (!buf_append(20)) return 0; // 64-bit integer takes up to 19 numeric characters, and sign
		len = sprintf(buf_end(), "%d", _int(json));
		buf->end += len;
	}
	/* the type of number is a floating point type */
	else
	{
		if (!buf_append(64)) return 0;
		f = _float(json);
		/* use full transformation within bounded space */
		if (fabs(floor(f) - f) <= DBL_EPSILON && fabs(f) < 1.0e60) len = sprintf(buf_end(), "%.1lf", f);
		/* use exponential form conversion beyond the limited range */
		else if (fabs(f) < 1.0e-6 || fabs(f) > 1.0e9) len = sprintf(buf_end(), "%e", f);
		/* default conversion */
		else
		{
			len = sprintf(buf_end(), "%lf", f);
			/* remove the invalid 0 in the decimal part */
			while (len > 0 && buf_end()[len-1] == '0' && buf_end()[len-2] != '.') len--;
		}
		buf->end += len;
	}

	return 1;
}

/**
 *  \brief store c string conversion to buf.
 *  \param[in] *str: address of string
 *  \param[in] buf: buf handle
 *  \return 1 success or 0 fail
 */
static int print_string_buffer(const char* str, BUFFER* buf)
{
	const char* p;
	int len = 0, escape = 0;

	/* empty string */
	if (!str)
	{
		if (!buf_append(2)) return 0;
		buf_putc('\"');
		buf_putc('\"');
		return 1;
	}

	/* get length */
	p = str;
	while (*p)
	{
		len++;
		if (*p == '\"' || *p == '\\' || *p == '\b' || *p == '\f' || *p == '\n' || *p == '\r' || *p == '\t') /* escape character */
		{
			escape = 1;
			len++;
		}
		else if ((unsigned char)(*p) < ' ') /* control character */
		{
			escape = 1;
			len += 5; // utf
		}
		p++;
	}

	if (!buf_append(len + 2)) return 0; // \" \"
	buf_putc('\"');

	/* without escape characters */
	if (!escape)
	{
		buf_puts(str, len);
		buf_putc('\"');
		return 1;
	}

	p = str;
	while (*p)
	{
		if ((unsigned char)(*p) >= ' ' && *p != '\"' && *p != '\\')
		{
			buf_putc(*p++);
		}
		else
		{
			/* escape and print */
			buf_putc('\\');
			if (*p == '\\') buf_putc('\\');
			else if (*p == '\"') buf_putc('\"');
			else if (*p == '\b') buf_putc('b');
			else if (*p == '\f') buf_putc('f');
			else if (*p == '\n') buf_putc('n');
			else if (*p == '\r') buf_putc('r');
			else if (*p == '\t') buf_putc('t');
			else 
			{
				sprintf(buf_end(), "u%04x", (unsigned char)(*p));
				buf->end += 5;
			}
			p++;
		}
	}

	buf_putc('\"');

	return 1;
}

/**
 *  \brief store json string conversion to buf.
 *  \param[in] json: json handle
 *  \param[in] buf: buf handle
 *  \return 1 success or 0 fail
 */
static int print_string(json_t json, BUFFER* buf)
{
	return print_string_buffer(_string(json), buf);
}

/**
 *  \brief render a array to text.
 *  \param[in] json: json handle
 *  \param[in] buf: buf handle
 *  \param[in] depth: print depth, indentation
 *  \param[in] format: 0 gives unformatted, otherwise gives formatted
 *  \return 1 success or 0 fail
 */
static int print_array(json_t json, BUFFER* buf, int depth, int format)
{
	int i = 0, count = 0;
	json_t child = _child(json);

	/* empty array */
	if (!child)
	{
		if (!buf_append(2)) return 0;
		buf_putc('[');
		buf_putc(']');
		return 1;
	}

	if (format)
	{
		while (child) /* check if there are arrays or objects in the children */
		{
			if ((_type(child->info) == JSON_TYPE_ARRAY || _type(child->info)== JSON_TYPE_OBJECT) && _child(child)) { count++; break; }
			child = child->next;
		}
	}

	if (!buf_append((format && count) ? 2 : 1)) return 0;
	buf_putc('[');
	if (format && count) buf_putc('\n');

	/* print children */
	child = _child(json);
	while (child)
	{
		/* print starting indent */
		if (format && count)
		{
			if (!buf_append(depth + 1)) return 0;
			for (i = 0; i < depth + 1; i++) buf_putc('\t');
		}

		/* print value */
		if (!print_value(child, buf, depth + 1, format)) return 0;
		
		/* print member separator ',' */
		if (child->next)
		{
			if (!buf_append(format ? 2 : 1)) return 0;
			buf_putc(',');
			if (format)
			{
				if (count) buf_putc('\n');
				else buf_putc(' ');
			}
		}

		child = child->next;
	}

	/* print ending indent */
	if (!buf_append((format && count) ? depth + 2 : 1)) return 0;
	if (format && count) 
	{
		buf_putc('\n');
		for (i = 0; i < depth; i++) buf_putc('\t');
	}
	buf_putc(']');

	return 1;
}

/**
 *  \brief render a object to text.
 *  \param[in] json: json handle
 *  \param[in] buf: buf handle
 *  \param[in] depth: print depth, indentation
 *  \param[in] format: 0 gives unformatted, otherwise gives formatted
 *  \return 1 success or 0 fail
 */
static int print_object(json_t json, BUFFER* buf, int depth, int format)
{
	int i;
	json_t child = _child(json);

	/* empty object */
	if (!child)
	{
		if (!buf_append(2)) return 0;
		buf_putc('{');
		buf_putc('}');
		return 1;
	}

	if (!buf_append(format ? 2 : 1)) return 0;
	buf_putc('{');
	if (format) buf_putc('\n');

	/* print children */
	while (child)
	{
		/* print starting indent */
		if (format)
		{
			if (!buf_append(depth + 1)) return 0;
			for (i = 0; i < depth + 1; i++) buf_putc('\t');
		}

		/* print key */
		if (!print_string_buffer(_key(child), buf)) return 0;

		/* print indicator ':' */
		if (!buf_append(format ? 2 : 1)) return 0;
		buf_putc(':');
		if (format) buf_putc('\t');

		/* print value */
		if (!print_value(child, buf, depth + 1, format)) return 0;

		/* print separator ',' */
		if (!buf_append((child->next ? 1 : 0) + (format ? 1 : 0))) return 0;
		if (child->next) buf_putc(',');
		if (format) buf_putc('\n');

		child = child->next;
	}

	/* print ending indent */
	if (!buf_append(format ? depth + 1 : 1)) return 0;
	if (format) { for (i = 0; i < depth; i++) buf_putc('\t'); }
	buf_putc('}');

	return 1;
}

/**
 *  \brief render a value to text.
 *  \param[in] json: json handle
 *  \param[in] buf: buf handle
 *  \param[in] depth: print depth, indentation
 *  \param[in] format: 0 gives unformatted, otherwise gives formatted
 *  \return 1 success or 0 fail
 */
static int print_value(json_t json, BUFFER* buf, int depth, int format)
{
	if (!json) return 0;

	switch (_type(json->info))
	{
	case JSON_TYPE_UNKNOW:
	case JSON_TYPE_NULL:
	{
		if (!buf_append(4)) return 0;
		buf_puts("null", 4);
		return 1;
	}
	case JSON_TYPE_BOOL:
	{
		if (json->info & JSON_VALUE_B_TRUE)
		{
			if (!buf_append(4)) return 0;
			buf_puts("true", 4);
		}
		else
		{
			if (!buf_append(5)) return 0;
			buf_puts("false", 5);
		}
		return 1;
	}
	case JSON_TYPE_NUMBER: return print_number(json, buf);
	case JSON_TYPE_STRING: return print_string(json, buf);
	case JSON_TYPE_ARRAY: return print_array(json, buf, depth, format);
	case JSON_TYPE_OBJECT: return print_object(json, buf, depth, format);
	}

	return 1;
}

/**
 *  \brief json text parser, with extra options.
 *  \param[in] *text: address of text
 *  \param[in] check_end: check whether there are meaningless characters after the text after parsing
 *  \param[out] **return_end: output the text address after parsing
 *  \return the address of the next meaningful character
 */
json_t json_loads_options(const char* text, int check_end, const char** return_end)
{
	json_t json = NULL;

	if (!text) return NULL;

	/* reset error message */
	error = NULL;
	lbegin = text;
	eline = 1;
	etype = JSON_E_OK;

	/* start parsing the json text */
	text = parse_value(skip(text), NULL, &json);
	if (!text) return NULL; /* parse failure. error is set. */

	/* check whether there are meaningless characters after the text after parsing */
	if (check_end)
	{
		text = skip(text);
		if (*text)
		{
			json_delete(json);
			_error(JSON_E_END);
			return NULL;
		}
	}

	/* return the text address after parsing */
	if (return_end) *return_end = text;

	return json;
}

/**
 *  \brief json text parser.
 *  \param[in] *text: address of text
 *  \return the address of the next meaningful character
 */
json_t json_loads(const char* text)
{
	return json_loads_options(text, 0, NULL);
}

/**
 *  \brief convert json to text, using a buffered strategy.
 *  \param[in] json: json handle
 *  \param[in] preset: preset is a guess at the final size, guessing well reduces reallocation
 *  \param[in] unformat: unformat=0 gives formatted, otherwise gives unformatted
 *  \param[out] *len: address that receives the length of printed characters
 *  \return address of converted text, free the char* when finished
 */
char* json_dumps(json_t json, int preset, int unformat, int* len)
{
	BUFFER p;

	/* create and initialize dump buffer */
	if (preset < 1) preset = 1;
	p.address = (char*)json_malloc(preset);
	if (!p.address) return NULL;
	p.size = preset;
	p.end = 0;

	/* print json object */
	if (!print_value(json, &p, 0, !unformat)) { free(p.address); return NULL; }

	p.address[p.end] = '\0'; /* add string terminator */
	if (len) *len = p.end; /* output length */

	return p.address;
}

/**
 *  \brief get the size(count) of json, the _type is an array or object.
 *  \param[in] json: json handle
 *  \return the number of items in an array (or object)
 */
int json_get_size(json_t json)
{
	json_t c;
	int i = 0;
	if (!json) return 0;
	if (_type(json->info) != JSON_TYPE_ARRAY && _type(json->info) != JSON_TYPE_OBJECT) return 0;
	c = _child(json);
	while (c) { i++; c = c->next; }
	return i;
}

/**
 *  \brief match child object
 *  \param[in] json: json handle
 *  \param[in] *key: match key value, all matches if NULL
 *  \param[in] index: index
 *  \return its previous node (NULL for list head) or json itself fail
 */
static json_t json_prev(json_t json, const char* key, int index)
{
	json_t c, t = json, prev = NULL;
	c = _child(json);
	while (c)
	{
		if (!key || !string_case_compare(_key(c), key))
		{
			t = prev;
			index--;
			if (index < 0) break;
		}
		prev = c;
		c = c->next;
	}
	return t;
}

/**
 *  \brief detach from json
 *  \param[in] json: json handle
 *  \param[in] *key: match key value, all matches if NULL
 *  \param[in] index: index
 *  \return child item or NULL fail
 */
json_t json_get_child(json_t json, const char* key, int index)
{
	json_t prev;
	if (!json) return NULL;
	if (index < 0) return NULL; 
	if (key && _type(json->info) == JSON_TYPE_ARRAY) return NULL;
	if (_type(json->info) != JSON_TYPE_ARRAY && _type(json->info) != JSON_TYPE_OBJECT) return NULL;
	prev = json_prev(json, key, index);
	return (prev != json) ? (prev ? prev->next : _child(json)) : NULL;
}

/**
 *  \brief get the child of json object by index continuously.
 *  \param[in] json: json handle
 *  \param[in] index: index
 *  \param[in] ...: unexposed parameters, continuously enter index until the INT_MIN ends
 *  \return the specify child json object, or NULL fail
 */
json_t json_get_by_indexs(json_t json, int index, ...)
{
	json_t c = json;
	va_list args;
	if (!json) return NULL;
	va_start(args, index);
	while (c && index != INT_MIN)
	{
		c = json_get_child(c, NULL, index);
		index = va_arg(args, int);
	}
	va_end(args);
	return c;
}

/**
 *  \brief get the child of json object by key continuously.
 *  \param[in] json: json handle
 *  \param[in] *key: address of key
 *  \param[in] ...: unexposed parameters, continuously enter key until the NULL ends
 *  \return the specify child json object, or NULL fail
 */
json_t json_get_by_keys(json_t json, char* key, ...)
{
	json_t c = json;
	va_list args;
	if (!json) return NULL;
	va_start(args, key);
	while (c && key != NULL)
	{
		c = json_get_child(c, key, 0);
		key = va_arg(args, char*);
	}
	va_end(args);
	return c;
}

/**
 *  \brief attach a json object inito json by index.
 *  \param[in] json: json handle
 *  \param[in] index: index
 *  \param[in] item: another json handle
 *  \return attach item or NULL fail
 */
json_t json_attach(json_t json, int index, json_t item)
{
	json_t c;
	json_t prev = NULL;

	/* validity check */
	if (!json) return NULL;
	if (!item) return NULL;
	if (index < 0) return NULL;
	if (!(_type(json->info) == JSON_TYPE_ARRAY && !(item->info & JSON_WITH_KEY)) &&
		!(_type(json->info) == JSON_TYPE_OBJECT && (item->info & JSON_WITH_KEY))) return NULL;

	c = _child(json);
	while (c && index > 0)
	{
		prev = c;
		c = c->next;
		index--;
	}

	if (!c && !prev)
	{
		_child(json) = item;
		return item;
	}

	/* adjust link */
	if (prev) prev->next = item;
	item->next = c;

	if (c == _child(json)) _child(json) = item;

	return item;
}

/**
 *  \brief detach from json
 *  \param[in] json: json handle
 *  \param[in] *key: match key value, all matches if NULL
 *  \param[in] index: index
 *  \return detach item or NULL fail
 */
json_t json_detach(json_t json, char *key, int index)
{
	json_t c;
	json_t prev = NULL;

	/* validity check */
	if (!json) return NULL;
	if (index < 0) return NULL;
	if (key && _type(json->info) == JSON_TYPE_ARRAY) return NULL;
	if (_type(json->info) != JSON_TYPE_ARRAY && _type(json->info) != JSON_TYPE_OBJECT) return NULL;

	/* adjust link */
	prev = json_prev(json, key, index); /* the previous object of the target object */
	if (prev == json) return NULL;
	if (prev) 
	{
		c = prev->next;
		prev->next = c->next;
	}
	else 
	{
		c = _child(json);
		_child(json) = c->next;
	}

	c->next = NULL; /* detach */

	return c;
}

/**
 *  \brief detach from json
 *  \param[in] json: json handle
 *  \param[in] *key: match key value, all matches if NULL
 *  \param[in] index: index
 *  \param[in] item: replaced item
 *  \return replace item or NULL fail
 */
json_t json_replace(json_t json, char *key, int index, json_t item)
{
	json_t c;
	json_t prev = NULL;

	if (!json) return NULL;
	if (!item) return NULL;
	if (index < 0) return NULL;
	if (key && _type(json->info) == JSON_TYPE_ARRAY) return NULL;
	if (_type(json->info) != JSON_TYPE_ARRAY && _type(json->info) != JSON_TYPE_OBJECT) return NULL;

	/* adjust link */
	prev = json_prev(json, key, index);
	if (prev == json) return NULL;
	if (prev)
	{
		c = prev->next;
		prev->next = item;
	}
	else 
	{
		c = _child(json);
		_child(json) = item;
	}
	item->next = c->next;

	c->next = NULL;
	json_delete(c);

	return item;
}

/**
 *  \brief create a null type json object.
 *  \return new json object
 */
json_t json_create_null(char* key)
{
	json_t item;
	char* k = NULL;

	if (key)
	{
		k = json_strdup(key);
		if (!k) return NULL;
	}

	item = json_new(JSON_TYPE_NULL, k);
	if (!item) { json_free(k); return NULL; }

	return item;
}

/**
 *  \brief create a false bool type json object.
 *  \param[in] b: bool value, 0-false, other-true
 *  \return new json object
 */
json_t json_create_bool(char* key, int b)
{
	json_t item;
	char* k = NULL;

	if (key)
	{
		k = json_strdup(key);
		if (!k) return NULL;
	}

	item = json_new(JSON_TYPE_BOOL, k);
	if (!item) { json_free(k); return NULL; }
	if (b != JSON_FALSE) item->info |= JSON_VALUE_B_TRUE;

	return item;
}

/**
 *  \brief create a int number type json object.
 *  \param[in] num: number
 *  \return new json object
 */
json_t json_create_int(char* key, int num)
{
	json_t item;
	char* k = NULL;

	if (key)
	{
		k = json_strdup(key);
		if (!k) return NULL;
	}

	item = json_new(JSON_TYPE_NUMBER | JSON_NUMBER_INT, k);
	if (!item) { json_free(k); return NULL; }
	_int(item) = num;

	return item;
}

/**
 *  \brief create a number type json object.
 *  \param[in] num: number
 *  \return new json object
 */
json_t json_create_float(char* key, double num)
{
	json_t item;
	char* k = NULL;

	if (key)
	{
		k = json_strdup(key);
		if (!k) return NULL;
	}

	item = json_new(JSON_TYPE_NUMBER, k);
	if (!item) { json_free(k); return NULL; }
	_float(item) = num;

	return item;
}

/**
 *  \brief create a string type json object.
 *  \param[in] *string: address of string
 *  \return new json object
 */
json_t json_create_string(char* key, const char* string)
{
	json_t item;
	char* k = NULL;
	char* s = NULL;

	if (key)
	{
		k = json_strdup(key);
		if (!k) return NULL;
	}

	item = json_new(JSON_TYPE_STRING, k);
	if (!item) { json_free(k); return NULL; }

	s = json_strdup(string);
	if (!s) { json_free(k); json_free(item); return NULL; }

	_string(item) = s;

	return item;
}

/**
 *  \brief create a object type json object.
 *  \return new json object
 */
json_t json_create_object(char* key)
{
	json_t item;
	char* k = NULL;

	if (key)
	{
		k = json_strdup(key);
		if (!k) return NULL;
	}

	item = json_new(JSON_TYPE_OBJECT, k);
	if (!item) { json_free(k); return NULL; }

	return item;
}

/**
 *  \brief create a array type json object.
 *  \return new json object
 */
json_t json_create_array(char* key)
{
	json_t item;
	char* k = NULL;

	if (key)
	{
		k = json_strdup(key);
		if (!k) return NULL;
	}

	item = json_new(JSON_TYPE_ARRAY, k);
	if (!item) { json_free(k); return NULL; }

	return item;
}

/**
 *  \brief create a array type json object, and assign to the specified integer array.
 *  \param[in] *numbers: address of integer array
 *  \param[in] count: array count
 *  \return new json object
 */
json_t json_create_array_int(char* key, const int* numbers, int count)
{
	int i;
	json_t n = NULL, p = NULL;
	json_t a = json_create_array(key);

	if (!a) return NULL;
	for (i = 0; i < count; i++)
	{
		n = json_create_int(NULL, numbers[i]);
		if (!n) { json_delete(a); return NULL; }
		if (!i) _child(a) = n;
		else p->next = n;
		p = n;
	}

	return a;
}

/**
 *  \brief create a array type json object, and assign to the specified float array.
 *  \param[in] *numbers: address of float array
 *  \param[in] count: array count
 *  \return new json object
 */
json_t json_create_array_float(char* key, const float* numbers, int count)
{
	int i;
	json_t n = NULL, p = NULL;
	json_t a = json_create_array(key);

	if (!a) return NULL;
	for (i = 0; i < count; i++)
	{
		n = json_create_float(NULL, numbers[i]);
		if (!n) { json_delete(a); return NULL; }
		if (!i) _child(a) = n;
		else p->next = n;
		p = n;
	}

	return a;
}

/**
 *  \brief create a array type json object, and assign to the specified double array.
 *  \param[in] *numbers: address of double array
 *  \param[in] count: array count
 *  \return new json object
 */
json_t json_create_array_double(char* key, const double* numbers, int count)
{
	int i;
	json_t n = NULL, p = NULL;
	json_t a = json_create_array(key);

	if (!a) return NULL;
	for (i = 0; i < count; i++)
	{
		n = json_create_float(NULL, numbers[i]);
		if (!n) { json_delete(a); return NULL; }
		if (!i) _child(a) = n;
		else p->next = n;
		p = n;
	}

	return a;
}

/**
 *  \brief create a array type json object, and assign to the specified string array.
 *  \param[in] **strings: address of string array
 *  \param[in] count: array count
 *  \return new json object
 */
json_t json_create_array_string(char* key, const char** strings, int count)
{
	int i;
	json_t n = NULL, p = NULL;
	json_t a = json_create_array(key);

	if (!a) return NULL;
	for (i = 0; i < count; i++)
	{
		n = json_create_string(NULL, strings[i]);
		if (!n) { json_delete(a); return NULL; }
		if (!i) _child(a) = n;
		else p->next = n;
		p = n;
	}

	return a;
}

/**
 *  \brief set the key of json.
 *  \param[in] json: json handle
 *  \param[in] *key: new key
 *  \return 1 success or 0 fail
 */
int json_set_key(json_t json, const char* key)
{
	char* old;
	char* k;

	if (!json) return 0;
	if (!(json->info & JSON_WITH_KEY)) return 0;

	old = _key(json);
	if (old == key || !strcmp(old, key)) return 1;

	k = json_strdup(key);
	if (!k) return 0;

	json_free(old);
	_key(json) = k;

	return 1;
}

/**
 *  \brief set the value of json to bool.
 *  \param[in] json: json handle
 *  \param[in] b: new bool
 *  \return 1 success or 0 fail
 */
int json_set_bool(json_t json, int b)
{
	if (!json) return 0;
	if (_type(json->info) != JSON_TYPE_BOOL) return 0;
	if (b == JSON_FALSE) json->info &= (~JSON_VALUE_B_TRUE);
	else json->info |= JSON_VALUE_B_TRUE;
	return 1;
}

/**
 *  \brief set the value of json to int.
 *  \param[in] json: json handle
 *  \param[in] num: new number
 *  \return 1 success or 0 fail
 */
int json_set_int(json_t json, int num)
{
	if (!json) return 0;
	if (_type(json->info) != JSON_TYPE_NUMBER) return 0;
	_int(json) = num;
	json->info |= JSON_NUMBER_INT;
	return 1;
}

/**
 *  \brief set the value of json to float.
 *  \param[in] json: json handle
 *  \param[in] num: new number
 *  \return 1 success or 0 fail
 */
int json_set_float(json_t json, double num)
{
	if (!json) return 0;
	if (_type(json->info) != JSON_TYPE_NUMBER) return 0;
	_float(json) = num;
	json->info &= (~JSON_NUMBER_INT);
	return 1;
}

/**
 *  \brief set the value of json to string.
 *  \param[in] json: json handle
 *  \param[in] *string: new string
 *  \return 1 success or 0 fail
 */
int json_set_string(json_t json, const char* string)
{
	char* old;
	char* s;

	if (!json) return 0;
	if (_type(json->info) != JSON_TYPE_STRING) return 0;

	old = _string(json);
	if (old && (old == string || !strcmp(old, string))) return 1;

	s = json_strdup(string);
	if (!s) return 0;

	if (old) json_free(old);
	_string(json) = s;

	return 1;
}

/**
 *  \brief duplication json.
 *  \param[in] json: json handle
 *  \return new json object
 */
json_t json_duplicate(json_t json)
{
	json_t n, temp, child, prev = NULL;
	char* key = NULL;

	if (!json) return NULL;

	/* copy key */
	if (json->info & JSON_WITH_KEY)
	{
		key = json_strdup(_key(json));
		if (!key) return NULL;
	}

	/* create new json */
	n = json_new(json->info, key);
	if (!n) return NULL;

	/* copy string type json */
	if (_type(json->info) == JSON_TYPE_STRING)
	{
		_string(n) = json_strdup(_string(json));
		if (!_string(n)) { json_delete(n); return NULL; }
	}

	/* recursively copy child */
	if (_type(json->info) == JSON_TYPE_OBJECT || _type(json->info) == JSON_TYPE_ARRAY)
	{
		temp = _child(json);
		while (temp)
		{
			child = json_duplicate(temp);
			if (!child) { json_delete(n); return NULL; }
			if (prev)
			{
				prev->next = child;
				prev = child;
			}
			else
			{
				_child(n) = child;
				prev = child;
			}
			temp = temp->next;
		}
	}

	return n;
}

/**
 *  \brief minify json text, remove the character that does not affect the analysis.
 *  \param[in] *text: the address of the source text
 *  \return none
 */
void json_minify(char* text)
{
	char* t = text;
	while (*text)
	{
		if (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') text++; /* whitespace characters. */			
		else if (*text == '/' && text[1] == '/') /* double-slash comments, to end of line. */
		{
			while (*text && *text != '\n') text++;
		}
		else if (*text == '/' && text[1] == '*') /* multiline comments. */
		{
			while (*text && !(*text == '*' && text[1] == '/')) text++;
			text += 2;
		}
		else if (*text == '\"') /* string literals, which are \" sensitive. */
		{
			*t++ = *text++;
			while (*text && *text != '\"')
			{
				if (*text == '\\') *t++ = *text++;
				*t++ = *text++;
			}
			*t++ = *text++;
		}
		else *t++ = *text++; /* all other characters. */
	}
	*t = 0;	/* and null-terminate. */
}

/**
 *  \brief load a json file, parse and generate json objects.
 *  \param[in] *filename: file name
 *  \return json handle
 */
json_t json_file_load(char* filename)
{
	FILE* f;
	json_t json = NULL;
	long len;
	char* text;

	/* open file and get the length of file */
	f = fopen(filename, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);

	/* read file */
	text = (char*)json_malloc(len + 1);
	if (!text) { fclose(f); return NULL; }
	fread(text, 1, len, f);
	fclose(f);
	text[len] = 0;
	
	/* load text */
	json = json_loads(text);
	if (!json) json_report_error();

	json_free(text);

	return json;
}

/**
 *  \brief according to the json object, generate a file.
 *  \param[in] json: json handle
 *  \param[in] *filename: file name
 *  \return length of file or -1 fail
 */
int json_file_dump(json_t json, char* filename)
{
	FILE* f;
	char* out;
	int len;
	if (!json) return -1;
	out = json_dumps(json, 0, 0, &len);
	if (!out) return -1;
	f = fopen(filename, "w");
	if (!f) { json_free(out); return -1; }
	fwrite(out, 1, len, f);
	fclose(f);
	json_free(out);
	return len;
}
