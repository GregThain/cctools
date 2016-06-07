/*
Copyright (C) 2016- The University of Notre Dame
This software is distributed under the GNU General Public License.
See the file COPYING for details.
*/
#include <stdio.h>

#include <string.h>
#include <stdarg.h>
#include "jx.h"
#include "jx_eval.h"
#include "jx_print.h"
#include "jx_function.h"
#include "xxmalloc.h"
#include "stringtools.h"

#define STR "str"
#define RANGE "range"
#define FOREACH "foreach"
#define JOIN "join"
#define DBG "dbg"

const char *jx_function_name_to_string(jx_function_t func) {
	switch (func) {
	case JX_FUNCTION_STR: return STR;
	case JX_FUNCTION_RANGE: return RANGE;
	case JX_FUNCTION_FOREACH: return FOREACH;
	case JX_FUNCTION_JOIN: return JOIN;
	case JX_FUNCTION_DBG: return DBG;
	default: return "???";
	}
}

jx_function_t jx_function_name_from_string(const char *name) {
	if (!strcmp(name, STR)) return JX_FUNCTION_STR;
	else if (!strcmp(name, RANGE)) return JX_FUNCTION_RANGE;
	else if (!strcmp(name, FOREACH)) return JX_FUNCTION_FOREACH;
	else if (!strcmp(name, JOIN)) return JX_FUNCTION_JOIN;
	else if (!strcmp(name, DBG)) return JX_FUNCTION_DBG;
	else return JX_FUNCTION_INVALID;
}

struct jx *jx_function_dbg(struct jx_function *f, struct jx *context) {
	struct jx *result;
	// we want to detect more than one arg, so try to match twice
	if (jx_function_parse_args(f->arguments, 2, JX_ANY, &result, JX_ANY, &result) != 1) {
		struct jx *err = jx_object(NULL);
		jx_insert_string(err, "error", "SyntaxError");
		jx_insert_string(err, "message", "only one argument is allowed");
		jx_insert_string(err, "file", __FILE__);
		jx_insert_integer(err, "line", __LINE__);
		jx_insert(err, jx_string("func"), jx_function(f->function, jx_copy(f->arguments)));
		return jx_error(err);
	}
	fprintf(stderr, "dbg  in: ");
	jx_print_stream(result, stderr);
	fprintf(stderr, "\n");
	result = jx_eval(result, context);
	fprintf(stderr, "dbg out: ");
	jx_print_stream(result, stderr);
	fprintf(stderr, "\n");
	return result;
}

struct jx *jx_function_str( struct jx_function *f, struct jx *context ) {
	struct jx *args;
	struct jx *err;
	struct jx *result;

	switch (jx_array_length(f->arguments)) {
	case 0:
		return jx_string("");
	case 1:
		args = jx_eval(f->arguments->u.items->value, context);
		break;
	default:
		err = jx_object(NULL);
		jx_insert_string(err, "error", "SyntaxError");
		jx_insert_string(err, "message", "at most one argument is allowed");
		jx_insert_string(err, "file", __FILE__);
		jx_insert_integer(err, "line", __LINE__);
		jx_insert(err, jx_string("func"), jx_function(f->function, jx_copy(f->arguments)));
		return jx_error(err);

	}
	if (!args) return jx_null();
	switch (args->type) {
	case JX_ERROR:
	case JX_STRING:
		return args;
	default:
		result = jx_string(jx_print_string(args));
		jx_delete(args);
		return result;
	}
}

struct jx *jx_function_foreach( struct jx_function *f, struct jx *context ) {
	char *symbol = NULL;
	struct jx *array = NULL;
	struct jx *body = NULL;
	struct jx *result = NULL;
	struct jx *err;

	if ((jx_function_parse_args(f->arguments, 3, JX_SYMBOL, &symbol, JX_ANY, &array, JX_ANY, &body) != 3)) {
		err = jx_object(NULL);
		jx_insert_string(err, "error", "SyntaxError");
		jx_insert_string(err, "message", "invalid arguments");
		jx_insert_string(err, "file", __FILE__);
		jx_insert_integer(err, "line", __LINE__);
		jx_insert(err, jx_string("func"), jx_function(f->function, jx_copy(f->arguments)));
		result =  jx_error(err);
		goto DONE;
	}
	// shuffle around to avoid leaking memory
	result = array;
	array = jx_eval(array, context);
	jx_delete(result);
	result = NULL;
	if (!jx_istype(array, JX_ARRAY)) {
		err = jx_object(NULL);
		jx_insert_string(err, "error", "SyntaxError");
		jx_insert_string(err, "message", "second argument must evaluate to an array");
		jx_insert_string(err, "file", __FILE__);
		jx_insert_integer(err, "line", __LINE__);
		jx_insert(err, jx_string("func"), jx_function(f->function, jx_copy(f->arguments)));
		result =  jx_error(err);
		goto DONE;
	}

	result = jx_array(NULL);
	for (struct jx_item *i = array->u.items; i; i = i->next) {
		struct jx *local_context = jx_copy(context);
		if (!local_context) local_context = jx_object(NULL);
		jx_insert(local_context, jx_string(symbol), jx_copy(i->value));
		struct jx *local_result = jx_eval(body, local_context);
		jx_array_append(result, local_result);
		jx_delete(local_context);
	}

DONE:
	if (symbol) free(symbol);
	jx_delete(array);
	jx_delete(body);
	return result;
}

// see https://docs.python.org/2/library/functions.html#range
struct jx *jx_function_range( struct jx_function *f, struct jx *context ) {
	jx_int_t start, stop, step;
	struct jx *err;
	struct jx *args = jx_eval(f->arguments, context);
	if (jx_istype(args, JX_ERROR)) {
		return args;
	}
	switch (jx_function_parse_args(args, 3, JX_INTEGER, &start, JX_INTEGER, &stop, JX_INTEGER, &step)) {
	case 1:
		stop = start;
		start = 0;
		step = 1;
		break;
	case 2:
		step = 1;
		break;
	case 3:
		break;
	default:
		err = jx_object(NULL);
		jx_insert_string(err, "error", "SyntaxError");
		jx_insert_string(err, "message", "invalid arguments");
		jx_insert_string(err, "file", __FILE__);
		jx_insert_integer(err, "line", __LINE__);
		jx_insert(err, jx_string("func"), jx_function(f->function, jx_copy(f->arguments)));
		return jx_error(err);
	}
	jx_delete(args);

	if (step == 0) {
		err = jx_object(NULL);
		jx_insert_string(err, "error", "SyntaxError");
		jx_insert_string(err, "message", "step must be nonzero");
		jx_insert_string(err, "file", __FILE__);
		jx_insert_integer(err, "line", __LINE__);
		jx_insert(err, jx_string("func"), jx_function(f->function, jx_copy(f->arguments)));
		return jx_error(err);
	}

	struct jx *result = jx_array(NULL);

	if (((stop - start) * step) < 0) {
		// step is pointing the wrong way
		return result;
	}

	for (jx_int_t i = start; stop >= start ? i < stop : i > stop; i += step) {
		jx_array_append(result, jx_integer(i));
	}

	return result;
}

struct jx *jx_function_join(struct jx_function *f, struct jx *context) {
	char *sep = NULL;
	struct jx *result;
	struct jx *array = NULL;
	struct jx *args = jx_eval(f->arguments, context);
	struct jx *err;
	if (jx_istype(args, JX_ERROR)) {
		return args;
	}
	switch (jx_function_parse_args(args, 2, JX_ARRAY, &array, JX_STRING, &sep)) {
	case 1:
	case 2:
		break;
	default:
		err = jx_object(NULL);
		jx_insert_string(err, "error", "SyntaxError");
		jx_insert_string(err, "message", "invalid arguments");
		jx_insert_string(err, "file", __FILE__);
		jx_insert_integer(err, "line", __LINE__);
		jx_insert(err, jx_string("func"), jx_function(f->function, jx_copy(f->arguments)));
		result = jx_error(err);
		goto DONE;
	}
	if (!sep) sep = xxstrdup(" ");

	if (jx_array_length(array) == 0) {
		result = jx_string("");
		goto DONE;
	}
	struct jx_item *i = array->u.items;
	if (!jx_istype(i->value, JX_STRING)) {
		err = jx_object(NULL);
		jx_insert_string(err, "error", "SyntaxError");
		jx_insert_string(err, "message", "array items must be strings");
		jx_insert_string(err, "file", __FILE__);
		jx_insert_integer(err, "line", __LINE__);
		jx_insert(err, jx_string("func"), jx_function(f->function, jx_copy(f->arguments)));
		result = jx_error(err);
		goto DONE;
	}

	result = jx_string(i->value->u.string_value);
	for (i = i->next; i; i = i->next) {
		if (!jx_istype(i->value, JX_STRING)) {
			jx_delete(result);
			err = jx_object(NULL);
			jx_insert_string(err, "error", "SyntaxError");
			jx_insert_string(err, "message", "array items must be strings");
			jx_insert_string(err, "file", __FILE__);
			jx_insert_integer(err, "line", __LINE__);
			jx_insert(err, jx_string("func"), jx_function(f->function, jx_copy(f->arguments)));
			result = jx_error(err);
			goto DONE;
		}
		result->u.string_value = string_combine(result->u.string_value, sep);
		result->u.string_value = string_combine(result->u.string_value, i->value->u.string_value);
	}

DONE:
	free(sep);
	jx_delete(array);
	jx_delete(args);
	return result;
}

int jx_function_parse_args(struct jx *array, int argc, ...) {
	if (!jx_istype(array, JX_ARRAY)) return 0;

	va_list ap;
	int matched = 0;
	struct jx_item *item = array->u.items;

	va_start(ap, argc);
	for (int i = 0; i < argc; i++) {
		if (!item) goto DONE;
		jx_type_t t = va_arg(ap, jx_type_t);

		if (t == (jx_type_t) JX_ANY) {
			if (!item->value) goto DONE;
			*va_arg(ap, struct jx **) = jx_copy(item->value);
		} else {
			switch (t) {
			case JX_INTEGER:
				if (!jx_istype(item->value, JX_INTEGER)) goto DONE;
				*va_arg(ap, jx_int_t *) = item->value->u.integer_value;
				break;
			case JX_BOOLEAN:
				if (!jx_istype(item->value, JX_BOOLEAN)) goto DONE;
				*va_arg(ap, int *) = item->value->u.boolean_value;
				break;
			case JX_DOUBLE:
				if (!jx_istype(item->value, JX_DOUBLE)) goto DONE;
				*va_arg(ap, double *) = item->value->u.double_value;
				break;
			case JX_STRING:
				if (!jx_istype(item->value, JX_STRING)) goto DONE;
				*va_arg(ap, char **) = xxstrdup(item->value->u.string_value);
				break;
			case JX_SYMBOL:
				if (!jx_istype(item->value, JX_SYMBOL)) goto DONE;
				*va_arg(ap, char **) = xxstrdup(item->value->u.symbol_name);
				break;
			case JX_OBJECT:
				if (!jx_istype(item->value, JX_OBJECT)) goto DONE;
				*va_arg(ap, struct jx **) = jx_copy(item->value);
				break;
			case JX_ARRAY:
				if (!jx_istype(item->value, JX_ARRAY)) goto DONE;
				*va_arg(ap, struct jx **) = jx_copy(item->value);
				break;
			case JX_FUNCTION:
				if (!jx_istype(item->value, JX_FUNCTION)) goto DONE;
				*va_arg(ap, struct jx **) = jx_copy(item->value);
				break;
			default:
				goto DONE;
			}
		}
		matched++;
		item = item->next;
	}

DONE:
	va_end(ap);
	return matched;
}

