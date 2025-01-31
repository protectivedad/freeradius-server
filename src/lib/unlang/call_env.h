#pragma once
/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/**
 * $Id$
 *
 * @file unlang/call_env.h
 * @brief Structures and functions for handling call environments.
 *
 * @copyright 2023 Network RADIUS SAS (legal@networkradius.com)
 */
RCSIDH(call_env_h, "$Id$")

#ifdef __cplusplus
extern "C" {
#endif

#include <freeradius-devel/util/dlist.h>

typedef struct call_env_parser_s	call_env_parser_t;
typedef struct call_env_parsed_s	call_env_parsed_t;
typedef struct call_env_method_s	call_env_method_t;
typedef struct call_env_s		call_env_t;

FR_DLIST_TYPES(call_env_parsed)
FR_DLIST_TYPEDEFS(call_env_parsed, call_env_parsed_head_t, call_env_parsed_entry_t)

#include <freeradius-devel/unlang/action.h>
#include <freeradius-devel/server/cf_parse.h>
#include <freeradius-devel/server/request.h>
#include <freeradius-devel/server/tmpl.h>

typedef enum {
	CALL_ENV_SUCCESS = 0,
	CALL_ENV_MISSING = -1,
	CALL_ENV_INVALID = -2
} call_env_result_t;

typedef enum {
	CALL_ENV_TYPE_VALUE_BOX = 1,
	CALL_ENV_TYPE_VALUE_BOX_LIST,
	CALL_ENV_TYPE_TMPL_ONLY
} call_env_dest_t;

/** Per method call config
 *
 * Similar to a conf_parser_t used to hold details of conf pairs
 * which are evaluated per call for each module method / xlat.
 *
 * This allows the conf pairs to be evaluated within the appropriate context
 * and use the appropriate dictionaries for where the module is in use.
 */
struct call_env_parser_s {
	char const	*name;		//!< Of conf pair to pass to tmpl_tokenizer.
	char const	*dflt;		//!< Default string to pass to the tmpl_tokenizer if no CONF_PAIR found.
	fr_token_t	dflt_quote;	//!< Default quoting for the default string.

	fr_type_t	type;		//!< To cast boxes to. Also contains flags controlling parser behaviour.
	conf_parser_flags_t	flags;		//!< Flags controlling parser behaviour.

	size_t		offset;		//!< Where to write results in the output structure when the tmpls are evaluated.

	union {
		struct {
			bool		required;	//!< Tmpl must produce output
			bool		concat;		//!< If the tmpl produced multiple boxes they should be concatenated.
			bool		single;		//!< If the tmpl produces more than one box this is an error.
			bool		multi;		//!< Multiple instances of the conf pairs are allowed.  Resulting
							///< boxes are stored in an array - one entry per conf pair.
			bool		nullable;	//!< Tmpl expansions are allowed to produce no output.
			bool		force_quote;	//!< Force quote method when parsing tmpl.  This is for corner cases
							///< where tmpls should always be parsed with a particular quoting
							///< regardless of how they are in the config file.  E.g. the `program`
							///< option of `rlm_exec` should always be parsed as T_BACK_QUOTED_STRING.
			call_env_dest_t	type;		//!< Type of structure boxes will be written to.
			size_t		size;		//!< Size of structure boxes will be written to.
			char const	*type_name;	//!< Name of structure type boxes will be written to.
			ssize_t		tmpl_offset;	//!< Where to write pointer to tmpl in the output structure.  Optional.
		} pair;

		struct {
			char const		*ident2;	//!< Second identifier for a section
			call_env_parser_t const	*subcs;		//!< Nested definitions for subsection.
			bool			required;	//!< Section is required.
    		} section;
  	};
};

#define CALL_ENV_TERMINATOR { NULL }

struct call_env_parsed_s {
	call_env_parsed_entry_t		entry;		//!< Entry in list of parsed call_env_parsers.
	tmpl_t				*tmpl;		//!< Tmpl produced from parsing conf pair.
	size_t				count;		//!< Number of CONF_PAIRs found, matching the #call_env_parser_t.
	size_t				multi_index;	//!< Array index for this instance.
	call_env_parser_t const		*rule;		//!< Used to produce this.
	bool				tmpl_only;	//!< Don't evaluate before module / xlat call.
							///< Only the tmpl reference is needed.
};

FR_DLIST_FUNCS(call_env_parsed, call_env_parsed_t, entry)

/** Helper macro for populating the size/type fields of a #call_env_method_t from the output structure type
 */
#define FR_CALL_ENV_METHOD_OUT(_inst) \
	.inst_size = sizeof(_inst), \
	.inst_type = STRINGIFY(_inst) \

struct call_env_method_s {
	size_t				inst_size;	//!< Size of per call env.
	char const			*inst_type;	//!< Type of per call env.
	call_env_parser_t const		*env;		//!< Parsing rules for call method env.
};

/** Structure containing both a talloc pool, a list of parsed call_env_pairs
 */
struct call_env_s {
	call_env_parsed_head_t		parsed;			//!< The per call parsed call environment.
	call_env_method_t const		*method;		//!< The method this call env is for.
};

/** Derive whether tmpl can only emit a single box.
 */
#define FR_CALL_ENV_SINGLE(_s, _f, _c) \
_Generic((((_s *)NULL)->_f), \
	fr_value_box_t			: __builtin_choose_expr(_c, false, true), \
	fr_value_box_t *		: __builtin_choose_expr(_c, false, true), \
	fr_value_box_list_t		: false, \
	fr_value_box_list_t *		: false \
)

/** Derive whether multi conf pairs are allowed from target field type.
 */
#define FR_CALL_ENV_MULTI(_s, _f) \
_Generic((((_s *)NULL)->_f), \
	fr_value_box_t			: false, \
	fr_value_box_t *		: true, \
	fr_value_box_list_t		: false, \
	fr_value_box_list_t *		: true \
)

/** Only FR_TYPE_STRING and FR_TYPE_OCTETS can be concatenated.
 */
#define FR_CALL_ENV_CONCAT(_c, _ct) \
__builtin_choose_expr(_ct == FR_TYPE_STRING, _c, \
__builtin_choose_expr(_ct == FR_TYPE_OCTETS, _c, \
__builtin_choose_expr(_c, (void)0, false)))

/** Mapping from field types to destination type enum
 */
#define FR_CALL_ENV_DST_TYPE(_s, _f) \
_Generic((((_s *)NULL)->_f), \
	fr_value_box_t			: CALL_ENV_TYPE_VALUE_BOX, \
	fr_value_box_t *		: CALL_ENV_TYPE_VALUE_BOX, \
	fr_value_box_list_t		: CALL_ENV_TYPE_VALUE_BOX_LIST, \
	fr_value_box_list_t *		: CALL_ENV_TYPE_VALUE_BOX_LIST \
)

#define FR_CALL_ENV_DST_SIZE(_s, _f) \
_Generic((((_s *)NULL)->_f), \
	fr_value_box_t			: sizeof(fr_value_box_t), \
	fr_value_box_t *		: sizeof(fr_value_box_t), \
	fr_value_box_list_t		: sizeof(fr_value_box_list_t), \
	fr_value_box_list_t *		: sizeof(fr_value_box_list_t) \
)

#define FR_CALL_ENV_DST_TYPE_NAME(_s, _f) \
_Generic((((_s *)NULL)->_f), \
	fr_value_box_t			: "fr_value_box_t", \
	fr_value_box_t *		: "fr_value_box_t", \
	fr_value_box_list_t		: "fr_value_box_list_t", \
	fr_value_box_list_t *		: "fr_value_box_list_t" \
)

#define FR_CALL_ENV_OFFSET(_name, _cast_type, _flags, _struct, _field, _dflt, _dflt_quote, _required, _nullable, _concat) \
	.name = _name, \
	.type = _cast_type, \
	.offset = offsetof(_struct, _field), \
	.dflt = _dflt, \
	.dflt_quote = _dflt_quote, \
	.pair = { \
		.required = _required, \
		.concat = FR_CALL_ENV_CONCAT(_concat, _cast_type), \
		.single = FR_CALL_ENV_SINGLE(_struct, _field, _concat), \
		.multi = FR_CALL_ENV_MULTI(_struct, _field), \
		.nullable = _nullable, \
		.type = FR_CALL_ENV_DST_TYPE(_struct, _field), \
		.size = FR_CALL_ENV_DST_SIZE(_struct, _field), \
		.type_name = FR_CALL_ENV_DST_TYPE_NAME(_struct, _field), \
		.tmpl_offset = -1 \
	}

/** Version of the above which sets optional field for pointer to tmpl
 */
#define FR_CALL_ENV_TMPL_OFFSET(_name, _cast_type, _flags, _struct, _field, _tmpl_field, _dflt, _dflt_quote, _required, _nullable, _concat) \
	.name = _name, \
	.type = _cast_type, \
	.offset = offsetof(_struct, _field), \
	.dflt = _dflt, \
	.dflt_quote = _dflt_quote, \
	.pair = { \
		.required = _required, \
		.concat = FR_CALL_ENV_CONCAT(_concat, _cast_type), \
		.single = FR_CALL_ENV_SINGLE(_struct, _field, _concat), \
		.multi = FR_CALL_ENV_MULTI(_struct, _field), \
		.nullable = _nullable, \
		.type = FR_CALL_ENV_DST_TYPE(_struct, _field), \
		.size = FR_CALL_ENV_DST_SIZE(_struct, _field), \
		.type_name = FR_CALL_ENV_DST_TYPE_NAME(_struct, _field), \
		.tmpl_offset = offsetof(_struct, _tmpl_field) \
	}

/** Version of the above which only sets the field for a pointer to the tmpl
 */
#define FR_CALL_ENV_TMPL_ONLY_OFFSET(_name, _cast_type, _flags, _struct, _tmpl_field, _dflt, _dflt_quote, _required) \
	.name = _name, \
	.type = _cast_type, \
	.dflt = _dflt, \
	.dflt_quote = _dflt_quote, \
	.pair = { \
		.required = _required, \
		.type = CALL_ENV_TYPE_TMPL_ONLY, \
		.tmpl_offset = offsetof(_struct, _tmpl_field) \
	}

#define FR_CALL_ENV_SUBSECTION(_name, _ident2, _subcs, _required ) \
	.name = _name, \
	.flags = CONF_FLAG_SUBSECTION, \
	.section = { \
		.ident2 = _ident2, \
		.subcs = _subcs, \
		.required = _required \
	}

unlang_action_t call_env_expand(TALLOC_CTX *ctx, request_t *request, call_env_result_t *result, void **env_data, call_env_t const *call_env);

call_env_t *call_env_alloc(TALLOC_CTX *ctx, char const *name, call_env_method_t const *call_env_method,
			   fr_dict_t const *namespace, CONF_SECTION *cs) CC_HINT(nonnull(3,4,5));

#ifdef __cplusplus
}
#endif
