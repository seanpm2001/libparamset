/*
 * Copyright 2013-2017 Guardtime, Inc.
 *
 * This file is part of the Guardtime client SDK.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES, CONDITIONS, OR OTHER LICENSES OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 * "Guardtime" and "KSI" are trademarks or registered trademarks of
 * Guardtime, Inc., and no license to trademarks is granted; Guardtime
 * reserves and retains all trademark rights.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "param_set_obj_impl.h"
#include "param_value.h"
#include "parameter.h"
#include "strn.h"


#define FORMAT_OK 0
#define VARIABLE_IS_NOT_USED(v) ((void)(v));
#define WILDCAR_EXPANDER_DEF_CHAR "*?"

static char *new_string(const char *str) {
	char *tmp = NULL;
	if (str == NULL) return NULL;
	tmp = (char*)malloc(strlen(str)*sizeof(char)+1);
	if (tmp == NULL) return NULL;
	return strcpy(tmp, str);
}

static int param_constraint_isFlagSet(const PARAM *obj, int state) {
	if (obj == NULL) return 0;
	if ((obj->constraints & state) ||
			(obj->constraints == 0 && state == 0)) return 1;
	return 0;
}

static int param_get_value(PARAM *param, const char *source, int prio, int at,
		int (*value_getter)(PARAM_VAL *, const char*, int, int, PARAM_VAL**),
		PARAM_VAL **value) {
	int res;
	PARAM_VAL *tmp = NULL;

	if (param == NULL || value == NULL) {
		res = PST_INVALID_ARGUMENT;
		goto cleanup;
	}

	if (param->arg == NULL) {
		res = PST_PARAMETER_EMPTY;
		goto cleanup;
	}

	if (value_getter == NULL) {
		res = ITERATOR_fetch(param->itr, source, prio, at, &tmp);
		if (res != PST_OK) goto cleanup;
	} else {
		res = value_getter(param->arg, source, prio, at, &tmp);
		if (res != PST_OK) goto cleanup;
	}

	*value = tmp;
	tmp = NULL;
	res = PST_OK;

cleanup:

	return res;
}

static int param_get_value_count(PARAM *param, const char *source, int prio,
		int (*counter)(PARAM_VAL *, const char *, int, int *),
		int *count) {
	int res;
	int tmp = 0;

	if (param == NULL) {
		res = PST_INVALID_ARGUMENT;
		goto cleanup;
	}

	if (param->arg == NULL) {
		tmp = 0;
	} else {
		res = counter(param->arg, source, prio, &tmp);
		if (res != PST_OK) goto cleanup;
	}

	*count = tmp;
	res = PST_OK;

cleanup:

	return res;
}

static int wrapper_returnStr(void **extra, const char* str, void** obj){
	VARIABLE_IS_NOT_USED(extra);
	if (obj == NULL) return PST_INVALID_ARGUMENT;
	*obj = (void*)str;
	return PST_OK;
}

static const char* wrapper_returnConstantPrintName(PARAM *param, char *buf, unsigned buf_len){
	VARIABLE_IS_NOT_USED(buf);
	VARIABLE_IS_NOT_USED(buf_len);
	if (param == NULL) return NULL;
	return param->print_name_buf;
}

static const char* wrapper_returnConstantPrintNameAlias(PARAM *param, char *buf, unsigned buf_len){
	VARIABLE_IS_NOT_USED(buf);
	VARIABLE_IS_NOT_USED(buf_len);
	if (param == NULL) return NULL;
	return param->print_name_alias_buf;
}

int PARAM_new(const char *flagName, const char *flagAlias, int constraints, int pars_opt, PARAM **newObj){
	int res;
	PARAM *tmp = NULL;
	char *tmpFlagName = NULL;
	char *tmpAlias = NULL;

	if (flagName == NULL || newObj == NULL) {
		res = PST_INVALID_ARGUMENT;
		goto cleanup;
	}

	tmp = (PARAM*)malloc(sizeof(PARAM));
	if (tmp == NULL) {
		res = PST_OUT_OF_MEMORY;
		goto cleanup;
	}

	tmp->flagName = NULL;
	tmp->flagAlias = NULL;
	tmp->helpText = NULL;
	tmp->arg = NULL;
	tmp->last_element = NULL;
	tmp->itr = NULL;
	tmp->highestPriority = 0;
	tmp->constraints = constraints;
	tmp->parsing_options = pars_opt;
	tmp->argCount = 0;
	tmp->controlFormat = NULL;
	tmp->controlContent = NULL;
	tmp->convert = NULL;
	tmp->extractObject = wrapper_returnStr;
	tmp->expand_wildcard = NULL;
	tmp->expand_wildcard_ctx = NULL;
	tmp->expand_wildcard_free = NULL;
	tmp->expand_wildcard_char = WILDCAR_EXPANDER_DEF_CHAR;
	tmp->getPrintName = wrapper_returnConstantPrintName;
	tmp->getPrintNameAlias = wrapper_returnConstantPrintNameAlias;
	tmp->print_name_buf[0] = '\0';
	tmp->print_name_alias_buf[0] = '\0';
	PST_snprintf(tmp->print_name_buf, sizeof(tmp->print_name_buf), "%s%s", (strlen(flagName) == 1 ? "-" : "--"), flagName);


	tmpFlagName = new_string(flagName);
	if (tmpFlagName == NULL) {
		res = PST_OUT_OF_MEMORY;
		goto cleanup;
	}


	if (flagAlias) {
		tmpAlias = new_string(flagAlias);
		if (tmpAlias == NULL) {
			res = PST_OUT_OF_MEMORY;
			goto cleanup;
		}

		PST_snprintf(tmp->print_name_alias_buf, sizeof(tmp->print_name_alias_buf), "%s%s", (strlen(flagAlias) == 1 ? "-" : "--"), flagAlias);
	}

	tmp->flagAlias = tmpAlias;
	tmp->flagName = tmpFlagName;
	*newObj = tmp;

	tmpAlias = NULL;
	tmpFlagName = NULL;
	tmp = NULL;

	res = PST_OK;

cleanup:

	free(tmpFlagName);
	free(tmpAlias);
	PARAM_free(tmp);
	return res;
}

void PARAM_free(PARAM *param) {
	if (param == NULL) return;
	free(param->flagName);
	free(param->flagAlias);
	if (param->itr) ITERATOR_free(param->itr);
	if (param->arg) PARAM_VAL_free(param->arg);
	if (param->helpText != NULL) free(param->helpText);

	if (param->expand_wildcard_ctx != NULL && param->expand_wildcard_free != NULL) {
		param->expand_wildcard_free(param->expand_wildcard_ctx);
	}

	free(param);
}

int PARAM_addControl(PARAM *param,
		int (*controlFormat)(const char *),
		int (*controlContent)(const char *),
		int (*convert)(const char*, char*, unsigned)) {
	if (param == NULL) return PST_INVALID_ARGUMENT;

	param->controlFormat = controlFormat;
	param->controlContent = controlContent;
	param->convert = convert;
	return PST_OK;
}

static int is_flag_set(int field, int flag) {
	if (((field & flag) == flag) ||
			(field == PST_PRSCMD_NONE && flag == PST_PRSCMD_NONE)) return 1;
	return 0;
}

int PARAM_isParseOptionSet(PARAM *param, int state) {
	if (param == NULL) return 0;
	return is_flag_set(param->parsing_options, state);
}

int PARAM_setParseOption(PARAM *param, int option) {
	if (param == NULL) return PST_INVALID_ARGUMENT;

	/**
	 * Give an error on some invalid configurations.
	 */
	if ((is_flag_set(option, PST_PRSCMD_HAS_NO_VALUE) || is_flag_set(option, PST_PRSCMD_DEFAULT))
		&& (is_flag_set(option, PST_PRSCMD_HAS_VALUE_SEQUENCE) || is_flag_set(option, PST_PRSCMD_HAS_VALUE) ||
			(is_flag_set(option, PST_PRSCMD_HAS_NO_VALUE) && is_flag_set(option, PST_PRSCMD_DEFAULT))))
		{
		return PST_PRSCMD_INVALID_COMBINATION;
	}


	param->parsing_options = option;
	return PST_OK;
}

int PARAM_setObjectExtractor(PARAM *param, int (*extractObject)(void **, const char *, void**)) {
	if (param == NULL) return PST_INVALID_ARGUMENT;

	param->extractObject = extractObject == NULL ? wrapper_returnStr : extractObject;
	return PST_OK;
}

int PARAM_setPrintName(PARAM *param, const char *constv, const char* (*getPrintName)(PARAM *param, char *buf, unsigned buf_len)) {
	if (param == NULL || (getPrintName == NULL && constv == NULL)) return PST_INVALID_ARGUMENT;


	if (constv != NULL) {
		param->getPrintName = wrapper_returnConstantPrintName;
		PST_strncpy(param->print_name_buf, constv, sizeof(param->print_name_buf));
	} else {
		param->getPrintName = getPrintName;
	}

	return PST_OK;
}

int PARAM_setPrintNameAlias(PARAM *param, const char *constv, const char* (*getPrintNameAlias)(PARAM *param, char *buf, unsigned buf_len)) {
	if (param == NULL || (getPrintNameAlias == NULL && constv == NULL)) return PST_INVALID_ARGUMENT;
	if (param->flagAlias == NULL) return PST_ALIAS_NOT_SPECIFIED;

	if (constv != NULL) {
		param->getPrintNameAlias = wrapper_returnConstantPrintNameAlias;
		PST_strncpy(param->print_name_alias_buf, constv, sizeof(param->print_name_alias_buf));
	} else {
		param->getPrintNameAlias = getPrintNameAlias;
	}

	return PST_OK;
}

const char* PARAM_getPrintName(PARAM *obj) {
	if (obj == NULL) return NULL;
	return obj->getPrintName(obj, obj->print_name_buf, sizeof(obj->print_name_buf));
}

const char* PARAM_getPrintNameAlias(PARAM *obj) {
	if (obj == NULL || obj->flagAlias == NULL) return NULL;
	return obj->getPrintNameAlias(obj, obj->print_name_alias_buf, sizeof(obj->print_name_alias_buf));
}

int PARAM_setHelpText(PARAM *param, const char *txt) {
	if (param == NULL || txt == NULL) return PST_INVALID_ARGUMENT;
	if (param->helpText != NULL) free(param->helpText);
	param->helpText = new_string(txt);
	return PST_OK;
}

const char* PARAM_getHelpText(PARAM *obj) {
	if (obj == NULL) return NULL;
	return obj->helpText;
}

int PARAM_addValue(PARAM *param, const char *value, const char* source, int prio) {
	int res;
	PARAM_VAL *newValue = NULL;
	PARAM_VAL *pLastValue = NULL;
	ITERATOR *tmpItr = NULL;
	const char *arg = NULL;
	char buf[1024];

	if (param == NULL) {
		res = PST_INVALID_ARGUMENT;
		goto cleanup;
	}

	/* If conversion function exists convert the argument. */
	if (param->convert) {
		res = param->convert(value, buf, sizeof(buf));
		if (res != PST_OK && res != PST_PARAM_CONVERT_NOT_PERFORMED) goto cleanup;

		arg = (res == PST_OK) ? buf : value;
	} else {
		arg = value;
	}

	/* Create new object and check the format. */
	res = PARAM_VAL_new(arg, source, prio, &newValue);
	if (res != PST_OK) goto cleanup;

	if (param->controlFormat)
		newValue->formatStatus = param->controlFormat(arg);
	if (newValue->formatStatus == FORMAT_OK && param->controlContent)
		newValue->contentStatus = param->controlContent(arg);

	if (param->arg == NULL) {
		param->arg = newValue;
		/* If iterator is not initialized, do it.*/
		if (param->itr == NULL) {
			res = ITERATOR_new(param->arg, &tmpItr);
			if (res != PST_OK) goto cleanup;

			param->itr = tmpItr;
			tmpItr = NULL;
		}
	} else{
		if (param->last_element == NULL) {
			res = PARAM_VAL_getElement(param->arg, NULL, PST_PRIORITY_NONE, PST_INDEX_LAST, &pLastValue);
			if (res != PST_OK) goto cleanup;
		} else {
			pLastValue = param->last_element;
		}

		/* The last element must exist and its next value must be NULL. */
		if (pLastValue == NULL || pLastValue->next != NULL) {
			res = PST_UNDEFINED_BEHAVIOUR;
			goto cleanup;
		}
		newValue->previous = pLastValue;
		pLastValue->next = newValue;
	}
	param->last_element = newValue;
	param->argCount++;

	if (param->highestPriority < prio)
		param->highestPriority = prio;

	newValue = NULL;
	res = PST_OK;

cleanup:

	PARAM_VAL_free(newValue);
	ITERATOR_free(tmpItr);

	return res;
}

int PARAM_getValue(PARAM *param, const char *source, int prio, int at, PARAM_VAL **value) {
	return param_get_value(param, source, prio, at, NULL, value);
}

int PARAM_getAtr(PARAM *param, const char *source, int prio, int at, PARAM_ATR *atr) {
	int res;
	PARAM_VAL *val = NULL;

	if (param == NULL || atr == NULL) {
		res = PST_INVALID_ARGUMENT;
		goto cleanup;
	}


	res = PARAM_getValue(param, source, prio, at, &val);
	if (res != PST_OK) goto cleanup;


	atr->cstr_value = val->cstr_value;
	atr->formatStatus = val->formatStatus;
	atr->contentStatus = val->contentStatus;
	atr->priority = val->priority;
	atr->source = val->source;
	atr->name = param->flagName;
	atr->alias = param->flagAlias;

	res = PST_OK;

cleanup:

	return res;
}

int PARAM_getName(PARAM *param, const char **name, const char **alias) {
	int res;

	if (param == NULL || (name == NULL && alias == NULL)) {
		res = PST_INVALID_ARGUMENT;
		goto cleanup;
	}

	if (name != NULL) {
		*name = param->flagName;
	}

	if (alias != NULL) {
		*alias = param->flagAlias;
	}

	res = PST_OK;

cleanup:

	return res;
}

int PARAM_clearAll(PARAM *param) {
	int res;

	if (param == NULL) {
		res = PST_INVALID_ARGUMENT;
		goto cleanup;
	}

	if (param->argCount == 0) {
		res = PST_OK;
		goto cleanup;
	}

	PARAM_VAL_free(param->arg);
	param->arg = NULL;
	param->argCount = 0;
	param->last_element = NULL;
	res = PST_OK;

cleanup:
	return res;
}

static int param_reser_iterator_if_needed_after_pop(PARAM *param, int popIndex) {
	ITERATOR *itr = NULL;

	if (param == NULL) return PST_INVALID_ARGUMENT;
	if (param->itr == NULL) return PST_OK;

	itr = param->itr;

	/**
	 * 1) Reset if the root value is changed.
	 * 2) Reset if the value popped is currently pointed by the iterator or is
	 *    already passed.
	 */
	if (itr->root != param->arg || itr->i <= popIndex) {
		return ITERATOR_set(itr, param->arg, itr->source, itr->priority, 0);
	}

	return PST_OK;
}

int PARAM_clearValue(PARAM *param, const char *source, int prio, int at) {
	int res;
	PARAM_VAL *pop = NULL;

	if (param == NULL) {
		res = PST_INVALID_ARGUMENT;
		goto cleanup;
	}

	if (param->argCount == 0) {
		res = PST_PARAMETER_EMPTY;
		goto cleanup;
	}

	res = PARAM_VAL_popElement(&(param->arg), source, prio, at, &pop);
	if (res != PST_OK) goto cleanup;

	res = param_reser_iterator_if_needed_after_pop(param, at);
	if (res != PST_OK) goto cleanup;

	if (param->last_element == pop) param->last_element = pop->previous;

	param->argCount--;
	PARAM_VAL_free(pop);

	res = PST_OK;

cleanup:
	return res;
}

int PARAM_getInvalid(PARAM *param, const char *source, int prio, int at, PARAM_VAL **value) {
	return param_get_value(param, source, prio, at, PARAM_VAL_getInvalid, value);
}

int PARAM_getValueCount(PARAM *param, const char *source, int prio, int *count) {

	if (param == NULL || count == NULL) return PST_INVALID_ARGUMENT;

	if (source == NULL && prio == PST_PRIORITY_NONE) {
		*count = param->argCount;
		return PST_OK;
	}

	return param_get_value_count(param, source, prio, PARAM_VAL_getElementCount, count);
}

int PARAM_getInvalidCount(PARAM *param, const char *source, int prio, int *count) {
	return param_get_value_count(param, source, prio, PARAM_VAL_getInvalidCount, count);
}

int PARAM_checkConstraints(const PARAM *param, int constraints) {
	int priority = 0;
	int count = 0;
	int res;
	int ret = 0;

	if (param == NULL) {
		return PARAM_INVALID_CONSTRAINT;
	}

	if (param->arg == NULL) {
		return 0;
	}

	/** PARAM_SINGLE_VALUE.*/
	if (constraints & PARAM_SINGLE_VALUE) {
		if (param_constraint_isFlagSet(param, PARAM_SINGLE_VALUE) && param->argCount > 1) {
			ret |= PARAM_SINGLE_VALUE;
		}
	}

	/**
	 * PARAM_SINGLE_VALUE_FOR_PRIORITY_LEVEL
	 * Check if there are some cases where a single priority level
	 * contains multiple values.
	 */
	if (constraints & PARAM_SINGLE_VALUE_FOR_PRIORITY_LEVEL) {
		if (param_constraint_isFlagSet(param, PARAM_SINGLE_VALUE_FOR_PRIORITY_LEVEL)) {
			/* Extract the first priority. */
			res = PARAM_VAL_getPriority(param->arg, PST_PRIORITY_LOWEST, &priority);
			if (res != PST_OK) return PARAM_INVALID_CONSTRAINT;

			/* Iterate through all priorities. */
			do {
				res = PARAM_VAL_getElementCount(param->arg, NULL, priority, &count);
				if (res != PST_OK) return PARAM_INVALID_CONSTRAINT;

				if (count > 1) ret |= PARAM_SINGLE_VALUE_FOR_PRIORITY_LEVEL;
			} while (PARAM_VAL_getPriority(param->arg, priority, &priority) != PST_PARAMETER_VALUE_NOT_FOUND);

		}
	}

	return ret;
}

static size_t param_add_constraint_error_to_buf(PARAM *param, const char *message, const char *prefix, char *buf, size_t buf_len) {
	const char *use_prefix = NULL;

	if (param == NULL || message == NULL || buf == NULL || buf_len == 0) return 0;

	use_prefix = prefix == NULL ? "" : prefix;
	return PST_snprintf(buf, buf_len, "%s%s%s.\n", use_prefix, message, PARAM_getPrintName(param));
}

char* PARAM_constraintErrorToString(PARAM *param, const char *prefix, char *buf, size_t buf_len) {
	int constraints = 0;
	size_t count = 0;

	if (param == NULL || buf == NULL || buf_len == 0) return NULL;

	constraints = PARAM_checkConstraints(param, PARAM_SINGLE_VALUE | PARAM_SINGLE_VALUE_FOR_PRIORITY_LEVEL);

	if (constraints & PARAM_SINGLE_VALUE) {
		count += param_add_constraint_error_to_buf(param, "Duplicated parameters ", prefix, buf + count, buf_len - count);
	}

	if (constraints & PARAM_SINGLE_VALUE_FOR_PRIORITY_LEVEL) {
		param_add_constraint_error_to_buf(param, "Duplicate parameters in priority levels ", prefix, buf + count, buf_len - count);
	}

	return buf;
}

int PARAM_getObject(PARAM *param, const char *source, int prio, int at, void **extra, void **obj) {
	int res;
	PARAM_VAL *value = NULL;

	if (param == NULL || obj == NULL) {
		res = PST_INVALID_ARGUMENT;
		goto cleanup;
	}

	res = PARAM_getValue(param, source, prio, at, &value);
	if (res != PST_OK) goto cleanup;

	if (value->formatStatus != 0 || value->contentStatus != 0) {
		res = PST_PARAMETER_INVALID_FORMAT;
		goto cleanup;
	}

	if (param->extractObject == NULL) {
		res = PST_PARAMETER_UNIMPLEMENTED_OBJ;
		goto cleanup;
	}

	res = param->extractObject(extra, value->cstr_value, obj);
	if (res != PST_OK) goto cleanup;

	res = PST_OK;

cleanup:

	return res;
}

int PARAM_setWildcardExpander(PARAM *param, const char* charList, void *ctx, void (*ctx_free)(void*), int (*expand_wildcard)(PARAM_VAL *param_value, void *ctx, int *value_shift)) {
	if (param == NULL || expand_wildcard == NULL) return PST_INVALID_ARGUMENT;
	param->expand_wildcard = expand_wildcard;
	param->expand_wildcard_ctx = ctx;
	param->expand_wildcard_free = ctx_free;

	if (charList != NULL) {
		param->expand_wildcard_char = charList;
	} else {
		param->expand_wildcard_char = WILDCAR_EXPANDER_DEF_CHAR;
	}

	return PST_OK;
}

int PARAM_expandWildcard(PARAM *param, int *count) {
	int res;
	int initial_count = 0;
	int i = 0;
	int parameter_shif_correction = 0;
	int expanded_count = 0;
	int counter = 0;
	PARAM_VAL *value = NULL;
	PARAM_VAL *pop = NULL;


	if (param == NULL) {
		res = PST_INVALID_ARGUMENT;
		goto cleanup;
	}

	if (param->expand_wildcard == NULL) {
		res = PST_PARAMETER_UNIMPLEMENTED_WILDCARD;
		goto cleanup;
	}

	if (PARAM_isParseOptionSet(param, PST_PRSCMD_EXPAND_WILDCARD)) {
		res = PARAM_getValueCount(param, NULL, PST_PRIORITY_NONE, &initial_count);
		if (res != PST_OK) goto cleanup;

		for (i = 0; i < initial_count; i++) {
			int absIndex = i + parameter_shif_correction;

			res = PARAM_getValue(param, NULL, PST_PRIORITY_NONE, absIndex, &value);
			if (res != PST_OK) goto cleanup;

			/* Check if there are wildcard characters. If not goto next value. */
			if (strpbrk(value->cstr_value, param->expand_wildcard_char) == NULL) continue;

			expanded_count = 0;
			res = param->expand_wildcard(value, param->expand_wildcard_ctx, &expanded_count);
			if (res != PST_OK) goto cleanup;

			res = PARAM_VAL_popElement(&value, NULL, PST_PRIORITY_NONE, 0, &pop);
			if (res != PST_OK) goto cleanup;


			param->argCount += expanded_count - 1;
			param->arg = value;

			res = param_reser_iterator_if_needed_after_pop(param, absIndex);
			if (res != PST_OK) goto cleanup;

			PARAM_VAL_free(pop);
			parameter_shif_correction += -1 + expanded_count;
			counter += expanded_count;
			pop = NULL;

		}

	}

	if (count != NULL) *count = counter;
	res = PST_OK;

cleanup:

	PARAM_VAL_free(pop);

	return res;
}

char* PARAM_toString(const PARAM *param, char *buf, size_t buf_len)  {
	char sub_buf[2048];

	if (param == NULL || buf == NULL || buf_len == 0) return 0;


	PST_snprintf(buf, buf_len, "%s(%i)->%s", param->flagName, param->argCount,
			PARAM_VAL_toString(param->arg, sub_buf, sizeof(sub_buf)));

	return buf;
}