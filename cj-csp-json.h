/*
 * MIT License
 *
 * Copyright (c) 2010 Serge Zaitsev
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef JSMN_H
#define JSMN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef JSMN_STATIC
#define JSMN_API static
#else
#define JSMN_API extern
#endif

/**
 * JSON type identifier. Basic types are:
 * 	o Object
 * 	o Array
 * 	o String
 * 	o Other primitive: number, boolean (true/false) or null
 */
typedef enum {
  JSMN_UNDEFINED = 0,
  JSMN_OBJECT = 1 << 0,
  JSMN_ARRAY = 1 << 1,
  JSMN_STRING = 1 << 2,
  JSMN_PRIMITIVE = 1 << 3
} jsmntype_t;

enum jsmnerr {
  /* Not enough tokens were provided */
  JSMN_ERROR_NOMEM = -1,
  /* Invalid character inside JSON string */
  JSMN_ERROR_INVAL = -2,
  /* The string is not a full JSON packet, more bytes expected */
  JSMN_ERROR_PART = -3
};

/**
 * JSON token description.
 * type		type (object, array, string etc.)
 * start	start position in JSON data string
 * end		end position in JSON data string
 */
typedef struct jsmntok {
  jsmntype_t type;
  int start;
  int end;
  int size;
#ifdef JSMN_PARENT_LINKS
  int parent;
#endif
} jsmntok_t;

/**
 * JSON parser. Contains an array of token blocks available. Also stores
 * the string being parsed now and current position in that string.
 */
typedef struct jsmn_parser {
  unsigned int pos;     /* offset in the JSON string */
  unsigned int toknext; /* next token to allocate */
  int toksuper;         /* superior token node, e.g. parent object or array */
} jsmn_parser;

/**
 * Create JSON parser over an array of tokens
 */
JSMN_API void jsmn_init(jsmn_parser *parser);

/**
 * Run JSON parser. It parses a JSON data string into and array of tokens, each
 * describing
 * a single JSON object.
 */
JSMN_API int jsmn_parse(jsmn_parser *parser, const char *js, const size_t len,
                        jsmntok_t *tokens, const unsigned int num_tokens);

#ifndef JSMN_HEADER
/**
 * Allocates a fresh unused token from the token pool.
 */
static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser, jsmntok_t *tokens,
                                   const size_t num_tokens) {
  jsmntok_t *tok;
  if (parser->toknext >= num_tokens) {
    return NULL;
  }
  tok = &tokens[parser->toknext++];
  tok->start = tok->end = -1;
  tok->size = 0;
#ifdef JSMN_PARENT_LINKS
  tok->parent = -1;
#endif
  return tok;
}

/**
 * Fills token type and boundaries.
 */
static void jsmn_fill_token(jsmntok_t *token, const jsmntype_t type,
                            const int start, const int end) {
  token->type = type;
  token->start = start;
  token->end = end;
  token->size = 0;
}

/**
 * Fills next available token with JSON primitive.
 */
static int jsmn_parse_primitive(jsmn_parser *parser, const char *js,
                                const size_t len, jsmntok_t *tokens,
                                const size_t num_tokens) {
  jsmntok_t *token;
  int start;

  start = parser->pos;

  for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
    switch (js[parser->pos]) {
#ifndef JSMN_STRICT
    /* In strict mode primitive must be followed by "," or "}" or "]" */
    case ':':
#endif
    case '\t':
    case '\r':
    case '\n':
    case ' ':
    case ',':
    case ']':
    case '}':
      goto found;
    default:
                   /* to quiet a warning from gcc*/
      break;
    }
    if (js[parser->pos] < 32 || js[parser->pos] >= 127) {
      parser->pos = start;
      return JSMN_ERROR_INVAL;
    }
  }
#ifdef JSMN_STRICT
  /* In strict mode primitive must be followed by a comma/object/array */
  parser->pos = start;
  return JSMN_ERROR_PART;
#endif

found:
  if (tokens == NULL) {
    parser->pos--;
    return 0;
  }
  token = jsmn_alloc_token(parser, tokens, num_tokens);
  if (token == NULL) {
    parser->pos = start;
    return JSMN_ERROR_NOMEM;
  }
  jsmn_fill_token(token, JSMN_PRIMITIVE, start, parser->pos);
#ifdef JSMN_PARENT_LINKS
  token->parent = parser->toksuper;
#endif
  parser->pos--;
  return 0;
}

/**
 * Fills next token with JSON string.
 */
static int jsmn_parse_string(jsmn_parser *parser, const char *js,
                             const size_t len, jsmntok_t *tokens,
                             const size_t num_tokens) {
  jsmntok_t *token;

  int start = parser->pos;
  
  /* Skip starting quote */
  parser->pos++;
  
  for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
    char c = js[parser->pos];

    /* Quote: end of string */
    if (c == '\"') {
      if (tokens == NULL) {
        return 0;
      }
      token = jsmn_alloc_token(parser, tokens, num_tokens);
      if (token == NULL) {
        parser->pos = start;
        return JSMN_ERROR_NOMEM;
      }
      jsmn_fill_token(token, JSMN_STRING, start + 1, parser->pos);
#ifdef JSMN_PARENT_LINKS
      token->parent = parser->toksuper;
#endif
      return 0;
    }

    /* Backslash: Quoted symbol expected */
    if (c == '\\' && parser->pos + 1 < len) {
      int i;
      parser->pos++;
      switch (js[parser->pos]) {
      /* Allowed escaped symbols */
      case '\"':
      case '/':
      case '\\':
      case 'b':
      case 'f':
      case 'r':
      case 'n':
      case 't':
        break;
      /* Allows escaped symbol \uXXXX */
      case 'u':
        parser->pos++;
        for (i = 0; i < 4 && parser->pos < len && js[parser->pos] != '\0';
             i++) {
          /* If it isn't a hex character we have an error */
          if (!((js[parser->pos] >= 48 && js[parser->pos] <= 57) ||   /* 0-9 */
                (js[parser->pos] >= 65 && js[parser->pos] <= 70) ||   /* A-F */
                (js[parser->pos] >= 97 && js[parser->pos] <= 102))) { /* a-f */
            parser->pos = start;
            return JSMN_ERROR_INVAL;
          }
          parser->pos++;
        }
        parser->pos--;
        break;
      /* Unexpected symbol */
      default:
        parser->pos = start;
        return JSMN_ERROR_INVAL;
      }
    }
  }
  parser->pos = start;
  return JSMN_ERROR_PART;
}

/**
 * Parse JSON string and fill tokens.
 */
JSMN_API int jsmn_parse(jsmn_parser *parser, const char *js, const size_t len,
                        jsmntok_t *tokens, const unsigned int num_tokens) {
  int r;
  int i;
  jsmntok_t *token;
  int count = parser->toknext;

  for (; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
    char c;
    jsmntype_t type;

    c = js[parser->pos];
    switch (c) {
    case '{':
    case '[':
      count++;
      if (tokens == NULL) {
        break;
      }
      token = jsmn_alloc_token(parser, tokens, num_tokens);
      if (token == NULL) {
        return JSMN_ERROR_NOMEM;
      }
      if (parser->toksuper != -1) {
        jsmntok_t *t = &tokens[parser->toksuper];
#ifdef JSMN_STRICT
        /* In strict mode an object or array can't become a key */
        if (t->type == JSMN_OBJECT) {
          return JSMN_ERROR_INVAL;
        }
#endif
        t->size++;
#ifdef JSMN_PARENT_LINKS
        token->parent = parser->toksuper;
#endif
      }
      token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
      token->start = parser->pos;
      parser->toksuper = parser->toknext - 1;
      break;
    case '}':
    case ']':
      if (tokens == NULL) {
        break;
      }
      type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
#ifdef JSMN_PARENT_LINKS
      if (parser->toknext < 1) {
        return JSMN_ERROR_INVAL;
      }
      token = &tokens[parser->toknext - 1];
      for (;;) {
        if (token->start != -1 && token->end == -1) {
          if (token->type != type) {
            return JSMN_ERROR_INVAL;
          }
          token->end = parser->pos + 1;
          parser->toksuper = token->parent;
          break;
        }
        if (token->parent == -1) {
          if (token->type != type || parser->toksuper == -1) {
            return JSMN_ERROR_INVAL;
          }
          break;
        }
        token = &tokens[token->parent];
      }
#else
      for (i = parser->toknext - 1; i >= 0; i--) {
        token = &tokens[i];
        if (token->start != -1 && token->end == -1) {
          if (token->type != type) {
            return JSMN_ERROR_INVAL;
          }
          parser->toksuper = -1;
          token->end = parser->pos + 1;
          break;
        }
      }
      /* Error if unmatched closing bracket */
      if (i == -1) {
        return JSMN_ERROR_INVAL;
      }
      for (; i >= 0; i--) {
        token = &tokens[i];
        if (token->start != -1 && token->end == -1) {
          parser->toksuper = i;
          break;
        }
      }
#endif
      break;
    case '\"':
      r = jsmn_parse_string(parser, js, len, tokens, num_tokens);
      if (r < 0) {
        return r;
      }
      count++;
      if (parser->toksuper != -1 && tokens != NULL) {
        tokens[parser->toksuper].size++;
      }
      break;
    case '\t':
    case '\r':
    case '\n':
    case ' ':
      break;
    case ':':
      parser->toksuper = parser->toknext - 1;
      break;
    case ',':
      if (tokens != NULL && parser->toksuper != -1 &&
          tokens[parser->toksuper].type != JSMN_ARRAY &&
          tokens[parser->toksuper].type != JSMN_OBJECT) {
#ifdef JSMN_PARENT_LINKS
        parser->toksuper = tokens[parser->toksuper].parent;
#else
        for (i = parser->toknext - 1; i >= 0; i--) {
          if (tokens[i].type == JSMN_ARRAY || tokens[i].type == JSMN_OBJECT) {
            if (tokens[i].start != -1 && tokens[i].end == -1) {
              parser->toksuper = i;
              break;
            }
          }
        }
#endif
      }
      break;
#ifdef JSMN_STRICT
    /* In strict mode primitives are: numbers and booleans */
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case 't':
    case 'f':
    case 'n':
      /* And they must not be keys of the object */
      if (tokens != NULL && parser->toksuper != -1) {
        const jsmntok_t *t = &tokens[parser->toksuper];
        if (t->type == JSMN_OBJECT ||
            (t->type == JSMN_STRING && t->size != 0)) {
          return JSMN_ERROR_INVAL;
        }
      }
#else
    /* In non-strict mode every unquoted value is a primitive */
    default:
#endif
      r = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
      if (r < 0) {
        return r;
      }
      count++;
      if (parser->toksuper != -1 && tokens != NULL) {
        tokens[parser->toksuper].size++;
      }
      break;

#ifdef JSMN_STRICT
    /* Unexpected char in strict mode */
    default:
      return JSMN_ERROR_INVAL;
#endif
    }
  }

  if (tokens != NULL) {
    for (i = parser->toknext - 1; i >= 0; i--) {
      /* Unmatched opened object or array */
      if (tokens[i].start != -1 && tokens[i].end == -1) {
        return JSMN_ERROR_PART;
      }
    }
  }

  return count;
}

/**
 * Creates a new parser based over a given buffer with an array of tokens
 * available.
 */
JSMN_API void jsmn_init(jsmn_parser *parser) {
  parser->pos = 0;
  parser->toknext = 0;
  parser->toksuper = -1;
}

#endif /* JSMN_HEADER */

#ifdef __cplusplus
}
#endif

#endif /* JSMN_H */
#ifndef __CJ_CSP_H__
#define __CJ_CSP_H__

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
// Errors
//

/** Error values are always negative. */
typedef enum CjError {
  /** No error. */
  CJ_ERROR_OK = 0,
  /** JSMN Library: Not enough tokens were provided. */
  CJ_ERROR_JSMN_NOMEM = -1,
  /** JSMN Library: Invalid character inside JSON string. */
  CJ_ERROR_JSMN_INVAL = -2,
  /** JSMN Library: The string is not a full JSON packet, more bytes expected */
  CJ_ERROR_JSMN_PART = -3,
  /** JSMN Library: Unknown error. */
  CJ_ERROR_JSMN = -4,
  /** Unknown error. */
  CJ_ERROR = -5,
  /** A memory allocation failed. */
  CJ_ERROR_NOMEM = -6,
  /** The provided argument is out of range or NULL. */
  CJ_ERROR_ARG = -7,
  /** Unknown JSON type encountered. */
  CJ_ERROR_JSON_TYPE = -8,
  /** csp-json.meta is not a JSON object type. */
  CJ_ERROR_META_IS_NOT_OBJECT = -9,
  /** csp-json.meta.id is not a string type. */
  CJ_ERROR_META_ID_NOT_STRING = -10,
  /** csp-json.meta.algo is not a string type. */
  CJ_ERROR_META_ALGO_NOT_STRING = -11,
  /** csp-json.meta has a field that is not recognized. */
  CJ_ERROR_META_UNKNOWN_FIELD = -12,
  /** csp-json.domains is not an array. */
  CJ_ERROR_DOMAINS_IS_NOT_ARRAY = -13,
  /** csp-json.domains[i] is not an object. */
  CJ_ERROR_DOMAIN_IS_NOT_OBJECT = -14,
  /** csp-json.domains[i] has an unknown type (eg. "values" key is known). */
  CJ_ERROR_DOMAIN_UNKNOWN_TYPE = -15,
  /** csp-json.domains[i].values is not a JSON array. */
  CJ_ERROR_DOMAIN_VALUES_IS_NOT_ARRAY = -16,
  /** csp-json.domains[i].values[j] is not an integer. */
  CJ_ERROR_DOMAIN_VALUES_IS_NOT_INT = -17,
  /** csp-json.vars is not an array. */
  CJ_ERROR_VARS_IS_NOT_ARRAY = -18,
  /** csp-json.vars[i] int not an int. */
  CJ_ERROR_VAR_IS_NOT_INT = -19,
  /** csp-json.constraintDefs is not an array. */
  CJ_ERROR_CONSTRAINTDEFS_IS_NOT_ARRAY = -20,
  /** csp-json.constraintDefs[i] is not an object. */
  CJ_ERROR_CONSTRAINTDEF_IS_NOT_OBJECT = -21,
  /** csp-json.constraintDefs[i] is an unknown type (eg. noGoods is known). */
  CJ_ERROR_CONSTRAINTDEF_UNKNOWN_TYPE = -22,
  /** csp-json.constraintDefs[i].noGoods is not an array. */
  CJ_ERROR_NOGOODS_IS_NOT_ARRAY = -23,
  /** csp-json.constraintDefs[i].noGoods[j] is not a tuple. */
  CJ_ERROR_NOGOODS_ARRAY_HAS_NOT_A_TUPLE = -24,
  /** csp-json.constraintDefs[i].noGoods[j] has different arity than at j-1. */
  CJ_ERROR_NOGOODS_ARRAY_DIFFERENT_ARITIES = -25,
  /** csp-json.constraintDefs[i].noGoods[j][k] is not an integer. */
  CJ_ERROR_NOGOODS_ARRAY_VALUE_IS_NOT_INT = -26,
  /** csp-json.constraints is not an array. */
  CJ_ERROR_CONSTRAINTS_IS_NOT_ARRAY = -27,
  /** csp-json.constraints[i] is not an object. */
  CJ_ERROR_CONSTRAINT_IS_NOT_OBJECT = -28,
  /** csp-json.constraints[i].id is not an integer. */
  CJ_ERROR_CONSTRAINT_ID_IS_NOT_INT = -29,
  /** csp-json.constraints[i].vars is not an array. */
  CJ_ERROR_CONSTRAINT_VARS_IS_NOT_ARRAY = -30,
  /** csp-json.constraints[i].vars[j] is not an integer. */
  CJ_ERROR_CONSTRAINT_VAR_IS_NOT_INT = -31,
  /** csp-json.constraints[i] has an unknown field (eg. "id" key is known). */
  CJ_ERROR_CONSTRAINT_UNKNOWN_FIELD = -32,
  /** csp-json (the top-level object) is not an object. */
  CJ_ERROR_CSPJSON_IS_NOT_OBJECT = -33,
  /** csp-json (the top-level object) is missing or has extra fields. */
  CJ_ERROR_CSPJSON_BAD_FIELD_COUNT = -34,
  /** csp-json (the top-level object) has an unknown field (eg. "meta" is known) */
  CJ_ERROR_CSPJSON_UNKNOWN_FIELD = -35,
  /** CjIntTuples[i] is not an array nor integer, or is of inconsistent type. */
  CJ_ERROR_INTTUPLES_ITEM_TYPE = -36,
  /** Expected an array, got something else. */
  CJ_ERROR_IS_NOT_ARRAY = -37,
  /** Validation failed because of invalid domains.size. */
  CJ_ERROR_VALIDATION_DOMAINS_SIZE = -38,
  CJ_ERROR_VALIDATION_DOMAINS_TYPE = -39,
  CJ_ERROR_VALIDATION_VARS_ARITY = -40,
  CJ_ERROR_VALIDATION_VARS_SIZE = -41,
  CJ_ERROR_VALIDATION_VAR_RANGE = -42,
  CJ_ERROR_VALIDATION_CONSTRAINTDEFS_SIZE = -43,
  CJ_ERROR_VALIDATION_CONSTRAINTDEF_TYPE = -44,
  CJ_ERROR_VALIDATION_CONSTRAINTS_SIZE = -45,
  CJ_ERROR_VALIDATION_CONSTRAINT_ID_RANGE = -46,
  CJ_ERROR_VALIDATION_CONSTRAINT_VARS_ARITY = -47,
  CJ_ERROR_VALIDATION_CONSTRAINT_VARS_SIZE = -48,
  CJ_ERROR_VALIDATION_CONSTRAINT_VAR_RANGE = -49,
  CJ_ERROR_VALIDATION_SOLUTION_ARITY = -50,
  CJ_ERROR_VALIDATION_SOLUTION_VARS_SIZE_MISMATCH = -51,
} CjError;

////////////////////////////////////////////////////////////////////////////////
// CjIntTuples
//
// A representation of an array of tuples of ints.
// Can also represent an array of ints if arity is -1.
//

/**
 * A 2D array of tuples of integers, or a 1D array of integers.
 *
 * 1) 2D: [[1,2], [3,4], [5,6]] has arity =  2, size = 2.
 * 2) 2D:                  [[]] has arity =  0, size = 1.
 * 3) 1D:               [1,2,3] has arity = -1, size = 3.
 */
typedef struct CjIntTuples {
  /** The number of tuples */
  int size;
  /** The arity of each tuple */
  int arity;
  /**
   * Holds `size * abs(arity)` entries.
   * If 2D use `data[i*arity + j]` where i in [0, size) and j in [0, arity).
   * If 1D use `data[i]` where i in [0, size).
   */
  int* data;
} CjIntTuples;

/** Zero/null init a CjIntTuples. */
CjIntTuples cjIntTuplesInit();

/**
 * Initialize and allocate a CjIntTuples and return CJ_ERROR_OK on success.
 * Free the created object with cjIntTuplesFree.
 * @arg arity is -1 for a 1D array, arity is >= 0 for 2D array.
 */
CjError cjIntTuplesAlloc(int size, int arity, CjIntTuples* out);

/** Free a CjIntTuples. */
void cjIntTuplesFree(CjIntTuples* inout);

/**
 * Allocate an array of CjIntTuples and cjIntTuplesInit() each item.
 * @return null on memory allocation error.
 */
CjIntTuples* cjIntTuplesArray(int size);

/** (1) free each item (2) free the array (3) set pointer to null. */
void cjIntTuplesArrayFree(CjIntTuples** inout, int size);

////////////////////////////////////////////////////////////////////////////////
// CjMeta
//
// The CSP-JSON metadata object.
//

typedef struct CjMeta {
  /** Owned by this struct - freed in cjMetaFree() */
  char* id;
  /** Owned by this struct - freed in cjMetaFree() */
  char* algo;
  /** Unparsed JSON string, since params is generator dependent.
   * Owned by this struct - freed in cjMetaFree()
   */
  char* paramsJSON;
} CjMeta;

/** Null init a CjMeta. */
CjMeta cjMetaInit();

/** Free id, algo, paramsJSON */
void cjMetaFree(CjMeta* inout);

////////////////////////////////////////////////////////////////////////////////
// CjDomain
//
// The CSP-JSON Domain object.
//

/** The Domain of a CSP variable. */
typedef struct CjDomain {
  enum {CJ_DOMAIN_UNDEF, CJ_DOMAIN_VALUES, CJ_DOMAIN_SIZE} type;
  union {
    /**
     * Explicitly list the values of the domain, one by one.
     * This union field is only used if type == CJ_DOMAIN_VALUES.
     **/
    CjIntTuples values;
  };
} CjDomain;

/** Zero/null init a CjDomain. */
CjDomain cjDomainInit();

/**
 * Init & allocate a domain using a values definition.
 * Return 0 on success.
 * Free the resulting struct with cjDomainFree().
 */
CjError cjDomainValuesAlloc(int size, CjDomain* out);
void cjDomainFree(CjDomain* inout);

/**
 * Allocate an array of CjDomain and cjDomainInit() each item.
 * @return null on memory allocation error.
 */
CjDomain* cjDomainArray(int size);

/** (1) free each item (2) free the array (3) set pointer to null. */
void cjDomainArrayFree(CjDomain** inout, int size);

////////////////////////////////////////////////////////////////////////////////
// CjConstraintDef

/** A constraint definition. */
typedef struct CjConstraintDef {
  enum {
    CJ_CONSTRAINT_DEF_UNDEF,
    CJ_CONSTRAINT_DEF_NO_GOODS,
    CJ_CONSTRAINT_DEF_SIZE
  } type;

  union {
    /**
     * List the combination of values that are invalid.
     * This union field is used only if type == CJ_CONSTRAINT_DEF_NO_GOODS.
     */
    CjIntTuples noGoods;
  };
} CjConstraintDef;


/** Zero/null init a CjConstraintDef. */
CjConstraintDef cjConstraintDefInit();

/**
 * Init & allocate a constraint def based on a no-goods definition.
 * Return 0 on success.
 * Free the resulting struct with cjConstraintDefFree().
 */
CjError cjConstraintDefNoGoodAlloc(int size, int arity, CjConstraintDef* out);
void cjConstraintDefFree(CjConstraintDef* inout);

/**
 * Allocate an array of CjConstraintDef and cjConstraintDefInit() each item.
 * @return null on memory allocaton error.
 */
CjConstraintDef* cjConstraintDefArray(int size);

/** (1) free each item (2) free the array (3) set pointer to null. */
void cjConstraintDefArrayFree(CjConstraintDef** inout, int size);

////////////////////////////////////////////////////////////////////////////////
// CjConstraint: Instantiating a constraint between variables.

/** A constraint instantiation between variables. */
typedef struct CjConstraint {
  /** References an entry in constraintDefs */
  int id;
  /** Arity -1 referencing the list of variables involved in constraint. */
  CjIntTuples vars;
} CjConstraint;

/** Zero/null init a CjConstraint. */
CjConstraint cjConstraintInit();

/**
 * Init & allocate a constraint based on a constraintDef reference id.
 * Return 0 on success.
 * Free the resulting struct with cjConstraintDefFree().
 */
CjError cjConstraintAlloc(int size, CjConstraint* out);
void cjConstraintFree(CjConstraint* inout);

/**
 * Allocate an array of CjConstraintDef and cjConstraintDefInit() each item.
 * @return null on memory allocation error.
 */
CjConstraint* cjConstraintArray(int size);

/** (1) free each item (2) free the array (3) set pointer to null. */
void cjConstraintArrayFree(CjConstraint** inout, int size);

////////////////////////////////////////////////////////////////////////////////
// CjCsp
//
// The CSP-JSON instance object itself.
//

typedef struct CjCsp {
  CjMeta meta;

  int domainsSize;
  CjDomain* domains;

  /** Each variable references a domain above. Arity is 0. */
  CjIntTuples vars;

  int constraintDefsSize;
  CjConstraintDef* constraintDefs;

  int constraintsSize;
  CjConstraint* constraints;
} CjCsp;

/**
 * Zero/null Init a cjCsp.
 * Free the resulting struct with cjCspFree().
 */
CjCsp cjCspInit();
void cjCspFree(CjCsp* inout);

/**
 * @return CJ_ERROR_OK only if the CSP instance is valid.
 * Eg. check that the indexes in vars are valid in domains.
 */
CjError cjCspValidate(const CjCsp* csp);

/**
 * Transforms the CSP in-place to a normal form (eg. sort domain and no-good
 * values).
 */
CjError cjCspNormalize(const CjCsp* csp);

/**
 * @return CJ_ERROR_OK if solution solves csp,
 *         CJ_ERROR_NOT_SOLUTION if solution validates but does not solve csp,
 *         other error if the solution or csp does not validate.
 */
CjError cjCspIsSolved(const CjCsp* csp, const CjIntTuples* solution, int* solved);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __CJ_CSP_H__
#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>


CjIntTuples cjIntTuplesInit() {
  CjIntTuples x;
  x.size = 0;
  x.arity = 0;
  x.data = NULL;
  return x;
}

CjError cjIntTuplesAlloc(int size, int arity, CjIntTuples* out) {
  if (size < 0 || arity < -1) {
    return CJ_ERROR_ARG;
  }
  out->size = size;
  out->arity = arity;
  out->data = NULL;
  if (size > 0 && abs(arity) > 0) {
    out->data = (int*) malloc(sizeof(int) * size * abs(arity));
    if (!out->data) {
      *out = cjIntTuplesInit();
      return CJ_ERROR_NOMEM;
    }
  }
  return CJ_ERROR_OK;
}

void cjIntTuplesFree(CjIntTuples* inout) {
  if (!inout) { return; }
  free(inout->data);
  inout->data = NULL;
  inout->arity = 0;
  inout->size = 0;
}

CjIntTuples* cjIntTuplesArray(int size) {
  CjIntTuples* xs = (CjIntTuples*) malloc(sizeof(CjIntTuples) * size);
  if (!xs) { return NULL; }
  for (int i = 0; i < size; ++i) {
    xs[i] = cjIntTuplesInit();
  }
  return xs;
}

void cjIntTuplesArrayFree(CjIntTuples** inout, int size) {
  if (!inout) { return; }
  if (!(*inout)) { return; }
  for (int i = 0; i < size; ++i) {
    cjIntTuplesFree(&((*inout)[i]));
  }
  free(*inout);
  *inout = NULL;
}

CjMeta cjMetaInit() {
  CjMeta x;
  x.id = NULL;
  x.algo = NULL;
  x.paramsJSON = NULL;
  return x;
}

void cjMetaFree(CjMeta* inout) {
  if (!inout) { return; }
  free(inout->id);
  free(inout->algo);
  free(inout->paramsJSON);
  *inout = cjMetaInit();
}

CjDomain cjDomainInit() {
  CjDomain x;
  x.type = CJ_DOMAIN_UNDEF;
  return x;
}

CjError cjDomainValuesAlloc(int size, CjDomain* out) {
  if (!out) { return CJ_ERROR_ARG; }
  const int arity = -1;
  out->type = CJ_DOMAIN_VALUES;
  int stat = cjIntTuplesAlloc(size, arity, &out->values);
  if (stat != CJ_ERROR_OK) {
    *out = cjDomainInit();
    return stat;
  }
  return CJ_ERROR_OK;
}

void cjDomainFree(CjDomain* inout) {
  if (!inout) { return; }
  switch (inout->type) {
    case CJ_DOMAIN_VALUES:
      cjIntTuplesFree(&inout->values);
      break;
    default:
      assert(0);
      break;
  }
  inout->type = CJ_DOMAIN_UNDEF;
}

CjDomain* cjDomainArray(int size) {
  CjDomain* xs = (CjDomain*) malloc(sizeof(CjDomain) * size);
  if (!xs) { return NULL; }
  for (int i = 0; i < size; ++i) {
    xs[i] = cjDomainInit();
  }
  return xs;
}

void cjDomainArrayFree(CjDomain** inout, int size) {
  if (!inout) { return; }
  if (!(*inout)) { return; }
  for (int i = 0; i < size; ++i) {
    cjDomainFree(&((*inout)[i]));
  }
  free(*inout);
  *inout = NULL;
}

CjConstraintDef cjConstraintDefInit() {
  CjConstraintDef x;
  x.type = CJ_CONSTRAINT_DEF_UNDEF;
  return x;
}

CjError cjConstraintDefNoGoodAlloc(int size, int arity, CjConstraintDef* out) {
  if (!out) { return CJ_ERROR_ARG; }
  out->type = CJ_CONSTRAINT_DEF_NO_GOODS;
  int stat = cjIntTuplesAlloc(size, arity, &out->noGoods);
  if (stat != CJ_ERROR_OK) { return stat; }
  return CJ_ERROR_OK;
}

void cjConstraintDefFree(CjConstraintDef* inout) {
  if (!inout) { return; }
  switch (inout->type) {
    case CJ_CONSTRAINT_DEF_NO_GOODS:
      cjIntTuplesFree(&inout->noGoods);
      break;
    default:
      assert(0);
      break;
  }
}

CjConstraintDef* cjConstraintDefArray(int size) {
  CjConstraintDef* xs = (CjConstraintDef*) malloc(sizeof(CjConstraintDef) * size);
  if (!xs) { return NULL; }
  for (int i = 0; i < size; ++i) {
    xs[i] = cjConstraintDefInit();
  }
  return xs;
}

void cjConstraintDefArrayFree(CjConstraintDef** inout, int size) {
  if (!inout) { return; }
  if (!(*inout)) { return; }
  for (int i = 0; i < size; ++i) {
    cjConstraintDefFree(&((*inout)[i]));
  }
  free(*inout);
  *inout = NULL;
}

CjConstraint cjConstraintInit() {
  CjConstraint x;
  x.id = -1;
  x.vars = cjIntTuplesInit();
  return x;
}

CjError cjConstraintAlloc(int size, CjConstraint* out) {
  const int arity = -1;
  if (!out) { return CJ_ERROR_ARG; }
  out->id = -1;
  int stat = cjIntTuplesAlloc(size, arity, &out->vars);
  if (stat != CJ_ERROR_OK) { return stat; }
  return CJ_ERROR_OK;
}

void cjConstraintFree(CjConstraint* inout) {
  if (!inout) { return; }
  inout->id = -1;
  cjIntTuplesFree(&inout->vars);
}

CjConstraint* cjConstraintArray(int size) {
  CjConstraint* xs = (CjConstraint*) malloc(sizeof(CjConstraint) * size);
  if (!xs) { return NULL; }
  for (int i = 0; i < size; ++i) {
    xs[i] = cjConstraintInit();
  }
  return xs;
}

void cjConstraintArrayFree(CjConstraint** inout, int size) {
  if (!inout) { return; }
  if (!(*inout)) { return; }
  for (int i = 0; i < size; ++i) {
    cjConstraintFree(&((*inout)[i]));
  }
  free(*inout);
  *inout = NULL;
}

CjCsp cjCspInit() {
  CjCsp x;

  x.meta = cjMetaInit();

  x.domainsSize = 0;
  x.domains = NULL;

  x.vars = cjIntTuplesInit();

  x.constraintDefsSize = 0;
  x.constraintDefs = NULL;

  x.constraintsSize = 0;
  x.constraints = NULL;

  return x;
}

void cjCspFree(CjCsp* inout) {
  if (!inout) { return; }
  cjMetaFree(&inout->meta);
  cjDomainArrayFree(&inout->domains, inout->domainsSize);
  cjConstraintDefArrayFree(&inout->constraintDefs, inout->constraintDefsSize);
  cjConstraintArrayFree(&inout->constraints, inout->constraintsSize);
  *inout = cjCspInit();
}

CjError cjCspValidate(const CjCsp* csp) {
  if (!csp) { return CJ_ERROR_ARG; }

  // Check domains
  if (csp->domainsSize < 0) { return CJ_ERROR_VALIDATION_DOMAINS_SIZE; }
  for (int iDom = 0; iDom < csp->domainsSize; ++iDom) {
    if (csp->domains[iDom].type <= CJ_DOMAIN_UNDEF) { return CJ_ERROR_VALIDATION_DOMAINS_TYPE; }
    if (csp->domains[iDom].type >= CJ_DOMAIN_SIZE) { return CJ_ERROR_VALIDATION_DOMAINS_TYPE; }
  }

  // Check vars
  if (csp->vars.arity != -1) { return CJ_ERROR_VALIDATION_VARS_ARITY; }
  if (csp->vars.size < 0) { return CJ_ERROR_VALIDATION_VARS_SIZE; }
  for (int iVar = 0; iVar < csp->vars.size; ++iVar) {
    if (csp->vars.data[iVar] < 0) { return CJ_ERROR_VALIDATION_VAR_RANGE; }
    if (csp->domainsSize <= csp->vars.data[iVar]) { return CJ_ERROR_VALIDATION_VAR_RANGE; }
  }

  // Check constraintDefs
  if (csp->constraintDefsSize < 0) { return CJ_ERROR_VALIDATION_CONSTRAINTDEFS_SIZE; }
  for (int iCDef = 0; iCDef < csp->constraintDefsSize; ++iCDef) {
    if (csp->constraintDefs[iCDef].type <= CJ_CONSTRAINT_DEF_UNDEF) { return CJ_ERROR_VALIDATION_CONSTRAINTDEF_TYPE; }
    if (csp->constraintDefs[iCDef].type >= CJ_CONSTRAINT_DEF_SIZE) { return CJ_ERROR_VALIDATION_CONSTRAINTDEF_TYPE; }
  }

  // Check constraints
  if (csp->constraintsSize < 0) { return CJ_ERROR_VALIDATION_CONSTRAINTS_SIZE; }
  for (int iC = 0; iC < csp->constraintsSize; ++iC) {
    CjConstraint* c = &csp->constraints[iC];
    if (c->id < 0) { return CJ_ERROR_VALIDATION_CONSTRAINT_ID_RANGE; }
    if (c->id >= csp->constraintDefsSize) { return CJ_ERROR_VALIDATION_CONSTRAINT_ID_RANGE; }
    if (c->vars.arity != -1) { return CJ_ERROR_VALIDATION_CONSTRAINT_VARS_ARITY; }
    if (c->vars.size < 0) { return CJ_ERROR_VALIDATION_CONSTRAINT_VARS_SIZE; }
    for (int iVar = 0; iVar < c->vars.size; ++iVar) {
      if (c->vars.data[iVar] < 0) { return CJ_ERROR_VALIDATION_CONSTRAINT_VAR_RANGE; }
      if (c->vars.data[iVar] >= csp->vars.size) { return CJ_ERROR_VALIDATION_CONSTRAINT_VAR_RANGE; }
    }
    if (csp->constraintDefs[c->id].type == CJ_CONSTRAINT_DEF_NO_GOODS) {
      if (c->vars.size != csp->constraintDefs[c->id].noGoods.arity) {
        return CJ_ERROR_VALIDATION_CONSTRAINT_VARS_SIZE;
      }
    }
    else {
      return CJ_ERROR_CONSTRAINTDEF_UNKNOWN_TYPE;
    }
  }

  return CJ_ERROR_OK;
}

static int compareInts(const void* x, const void* y) {
   return ( *(int*)x - *(int*)y );
}

static int compareIntTuples2(const void* xPtr, const void* yPtr) {
  int x0 = ((int*) xPtr)[0];
  int y0 = ((int*) yPtr)[0];
  int x1 = ((int*) xPtr)[1];
  int y1 = ((int*) yPtr)[1];

  if (x0 < y0) {
    return -1;
  }
  else if (x0 > y0) {
    return 1;
  }
  return x1 - y1;
}

CjError cjCspNormalize(const CjCsp* csp) {
  if (!csp) { return CJ_ERROR_ARG; }

  for (int iDom = 0; iDom < csp->domainsSize; ++iDom) {
    if (csp->domains[iDom].type != CJ_DOMAIN_VALUES) {
      return CJ_ERROR_CONSTRAINTDEF_UNKNOWN_TYPE;
    }
    qsort(csp->domains[iDom].values.data, csp->domains[iDom].values.size, sizeof(int), compareInts);
  }

  for (int iCDef = 0; iCDef < csp->constraintDefsSize; ++iCDef) {
    if (csp->constraintDefs[iCDef].type != CJ_CONSTRAINT_DEF_NO_GOODS) {
      return CJ_ERROR_CONSTRAINTDEF_UNKNOWN_TYPE;
    }
    if (csp->constraintDefs[iCDef].noGoods.arity != 2) {
      return CJ_ERROR_NOGOODS_ARRAY_DIFFERENT_ARITIES;
    }
    qsort(
      csp->constraintDefs[iCDef].noGoods.data,
      csp->constraintDefs[iCDef].noGoods.size,
      sizeof(int) * csp->constraintDefs[iCDef].noGoods.arity,
      compareIntTuples2);
  }

  return CJ_ERROR_OK;
}

CjError cjCspIsSolved(const CjCsp* csp, const CjIntTuples* solution, int* solved) {
  if (!csp || !solution) { return CJ_ERROR_ARG; }

  CjError err = cjCspValidate(csp);
  if (err != CJ_ERROR_OK) { return err; }

  if (solution->arity != -1) {
    return CJ_ERROR_VALIDATION_SOLUTION_ARITY;
  }
  if (csp->vars.size != solution->size) {
    return CJ_ERROR_VALIDATION_SOLUTION_VARS_SIZE_MISMATCH;
  }

  // Check variable assignment is within the domain.
  for (int iVar = 0; iVar < solution->size; ++iVar) {
    int value = solution->data[iVar];
    CjDomain* domain = &csp->domains[csp->vars.data[iVar]];
    if (domain->type != CJ_DOMAIN_VALUES) {
      return CJ_ERROR_DOMAIN_UNKNOWN_TYPE;
    }
    bool valueInDomain = false;
    for (int iVal = 0; iVal < domain->values.size; ++iVal) {
      if (value == domain->values.data[iVal]) {
        valueInDomain = true;
        break;
      }
    }
    if (!valueInDomain) {
      *solved = false;
      return CJ_ERROR_OK;
    }
  }

  // Check that variable assignments satisfy constraints.
  for (int iConstraint = 0; iConstraint < csp->constraintsSize; ++iConstraint) {
    CjConstraint* constraint = &csp->constraints[iConstraint];
    CjConstraintDef* def = &csp->constraintDefs[constraint->id];
    if (def->type != CJ_CONSTRAINT_DEF_NO_GOODS) {
      return CJ_ERROR_CONSTRAINTDEF_UNKNOWN_TYPE;
    }
    bool valuesAllowed = true;
    for (int iTuple = 0; iTuple < def->noGoods.size; ++iTuple) {
      bool tupleMatchesSolution = true;
      for (int iVar = 0; iVar < def->noGoods.arity; ++iVar) {
        int solutionVal = solution->data[constraint->vars.data[iVar]];
        int tupleVal = def->noGoods.data[iTuple * def->noGoods.arity + iVar];
        if (solutionVal != tupleVal) {
          tupleMatchesSolution = false;
          break;
        }
      }
      if (tupleMatchesSolution) {
        valuesAllowed = false;
        break;
      }
    }
    if (! valuesAllowed) {
      *solved = false;
      return CJ_ERROR_OK;
    }
  }

  *solved = true;
  return CJ_ERROR_OK;
}
#ifndef __CJ_CSP_IO_H__
#define __CJ_CSP_IO_H__

#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
// cjIntTuples Parsing and Printing
//

/**
 * Parse into ts which needs to be freed prior to call.
 * @arg defaultArity specifies the arity to use for an size 0 array
 *                   where you can infer the arity from the data.
 *                   Use -1 for 1D, 0+ for 2D.
 * @return CJ_ERROR_OK on success
 */
CjError cjIntTuplesParse(
  const int defaultArity,
  const char* json,
  const size_t jsonLen,
  CjIntTuples* ts);

/** Print from ts. @return CJ_ERROR_OK on success */
CjError cjIntTuplesJsonPrint(FILE* f, const CjIntTuples* ts);

////////////////////////////////////////////////////////////////////////////////
// CjConstraintDef Parsing and Printing
//

/**
 * Parse into cdef which needs to be freed prior to call.
 * @return CJ_ERROR_OK on success
 */
CjError cjConstraintDefParse(
  const char* json,
  const size_t jsonLen,
  CjConstraintDef* cdef);

/** Print from cdef. @return CJ_ERROR_OK on success */
CjError cjConstraintDefJsonPrint(FILE* f, const CjConstraintDef* cdef);

////////////////////////////////////////////////////////////////////////////////
// cjCsp Parsing and Printing
//

/**
 * @param json does not have to be null terminated.
 * @param jsonLen specifies the length of the json arg.
 * @param csp parse into this variable. Needs to be freed prior to call.
 * @return CJ_ERROR_OK on success
 * free CjCsp with CjCspFree().
 * */
CjError cjCspJsonParse(const char* json, const size_t jsonLen, CjCsp* csp);

/** return CJ_ERROR_OK on success */
CjError cjCspJsonPrint(FILE* f, const CjCsp* csp);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __CJ_CSP_IO_H__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSMN_STRICT
#define JSMN_PARENT_LINKS

//#define CJ_DEBUG

////////////////////////////////////////////////////////////////////////////////
// json helpers
//

/**
 * Copy a JSON field into a new allocated string.
 * Return CJ_ERROR_OK on success.
 * Free the output string with free().
 *
 * JSON strings will not have enclosing quotes. Braces are included.
 */
static int jsonStrCpy(int includeQuotes, const char* json, jsmntok_t* t, char** out) {
  if (!json || !t || !out) { return CJ_ERROR_ARG; }
  const int offset = includeQuotes && t->type == JSMN_STRING ? 1 : 0;
  *out = malloc(t->end - t->start + 1 + 2*offset);
  if (!(*out)) { return CJ_ERROR_NOMEM; }
  strncpy(*out, json + t->start - offset, t->end - t->start + 2*offset);
  (*out)[t->end - t->start + 2*offset] = '\0';
  return CJ_ERROR_OK;
}

/** Return 1 if equal, 0 otherwise. */
static int jsonEq(const char *json, jsmntok_t* tok, const char *s) {
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 1;
  }
  return 0;
}

/** return 1 if character is numeric, 0 otherwise. */
static int jsonIsNumeric(char c) {
  switch (c) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return 1;
  }
  return 0;
}

/** Return 1 if tok is a JSON int, 0 otherwise. */
static int jsonIsInt(const char *json, jsmntok_t* tok) {
  if (tok->type != JSMN_PRIMITIVE) { return 0; }

  const char first = (json + tok->start)[0];
  if (first != '-' && !jsonIsNumeric(first)) { return 0; }

  for (int iChar = 1; iChar < tok->size; ++iChar) {
    const char c = (json + tok->start)[iChar];
    if (!jsonIsNumeric(c)) { return 0; }
  }

  return 1;
}

static void logTok(const char* prefix, const char* json, jsmntok_t* t) {
#ifdef CJ_DEBUG
  if (!prefix || !json || !t) { return; }
  printf("%s{", prefix);
  switch (t->type) {
    case JSMN_UNDEFINED: printf("undef"); break;
    case JSMN_OBJECT: printf("object"); break;
    case JSMN_ARRAY: printf("array"); break;
    case JSMN_STRING: printf("string"); break;
    case JSMN_PRIMITIVE: printf("prim"); break;
  }
  printf("|s:%d|e:%d|size:%d|", t->start, t->end, t->size);
  for (size_t i = 0; i < t->end - t->start; ++i) {
    printf("%c", (json + t->start)[i]);
  }
  printf("}\n");
#endif
}

static int jsonConsumeAny(const char* json, jsmntok_t* t) {
  logTok("consumeAny:", json, t);
  if (!json || !t) { return CJ_ERROR_ARG; }

  if (t->type == JSMN_OBJECT) {
    int consumed = 1;
    for (int iChild = 0; iChild < t->size; ++iChild) {
      int stat = jsonConsumeAny(json, t + consumed + 1);
      if (stat < 0) { return stat; }
      consumed += 1 + stat;
    }
    return consumed;
  }
  else if (t->type == JSMN_STRING || t->type == JSMN_PRIMITIVE) {
    return 1;
  }
  else if (t->type == JSMN_ARRAY) {
    int consumed = 1;
    for (int iChild = 0; iChild < t->size; ++iChild) {
      int stat = jsonConsumeAny(json, t + consumed);
      if (stat < 0) { return stat; }
      consumed += stat;
    }
    return consumed;
  }
  else {
    return CJ_ERROR_JSON_TYPE;
  }
}

////////////////////////////////////////////////////////////////////////////////
// cjIntTuples
//

static int cjIntTuplesParseTok(
  const int defaultArity, const char* json, jsmntok_t* t, CjIntTuples* ts)
{
  logTok("CjIntTuples:", json, t);
  if (!json || !t || !ts) { return CJ_ERROR_ARG; }
  if (t->type != JSMN_ARRAY) { return CJ_ERROR_IS_NOT_ARRAY; }
  int consumed = 1;

  if (t->size == 0) {
    *ts = cjIntTuplesInit();
    ts->arity = defaultArity;
    return consumed;
  }

  // 2D case (array of tuples)
  if (t[consumed].type == JSMN_ARRAY) {
    int arity = t[consumed].size;
    int stat = cjIntTuplesAlloc(t->size, arity, ts);
    if (stat != CJ_ERROR_OK) { return stat; }

    for (int iChild = 0; iChild < t->size; ++iChild) {
      logTok("CjIntTuples-item:", json, &t[consumed]);
      if (t[consumed].type != JSMN_ARRAY || t[consumed].size != arity) {
        cjIntTuplesFree(ts);
        return CJ_ERROR_INTTUPLES_ITEM_TYPE;
      }

      size_t jSize = t[consumed].size;
      ++consumed;

      for (int jItem = 0; jItem < jSize; ++jItem, ++consumed) {
        if (! jsonIsInt(json, t + consumed)) {
          cjIntTuplesFree(ts);
          return CJ_ERROR_INTTUPLES_ITEM_TYPE;
        }
        ts->data[iChild*arity + jItem] = strtol(json + t[consumed].start, NULL, 10);
      }
    }
  }
  // 1D case (array of ints)
  else if (jsonIsInt(json, t + consumed)) {
    const int arity = -1;
    int stat = cjIntTuplesAlloc(t->size, arity, ts);
    if (stat != CJ_ERROR_OK) { return stat; }

    for (int iChild = 0; iChild < t->size; ++iChild, ++consumed) {
      logTok("CjIntTuples-item:", json, &t[consumed]);
      if (!jsonIsInt(json, t + consumed)) {
        cjIntTuplesFree(ts);
        return CJ_ERROR_INTTUPLES_ITEM_TYPE;
      }
      ts->data[iChild] = strtol(json + t[consumed].start, NULL, 10);
    }
  }
  // Error case (eg. array of objects)
  else {
    *ts = cjIntTuplesInit();
    return CJ_ERROR_INTTUPLES_ITEM_TYPE;
  }

  return consumed;
}

////////////////////////////////////////////////////////////////////////////////
// cjCsp
//

static int cjCspJsonParseMeta(const char* json, jsmntok_t* t, CjCsp* csp) {
  logTok("meta:", json, t);
  if (!json || !t || !csp) { return CJ_ERROR_ARG; }
  if (t->type != JSMN_OBJECT || t->size != 3) { return CJ_ERROR_META_IS_NOT_OBJECT; }

  int consumed = 1;
  for (int iChild = 0; iChild < t->size; ++iChild) {
    logTok("meta-child:", json, &t[consumed]);
    if (jsonEq(json, t + consumed, "id")) {
      if (t[consumed + 1].type != JSMN_STRING) { return CJ_ERROR_META_ID_NOT_STRING; }
      int stat = jsonStrCpy(0, json, t + consumed + 1, &csp->meta.id);
      if (stat != CJ_ERROR_OK) { return stat; }
      consumed += 2;
    }
    else if (jsonEq(json, t + consumed, "algo")) {
      if (t[consumed + 1].type != JSMN_STRING) { return CJ_ERROR_META_ALGO_NOT_STRING; }
      int stat = jsonStrCpy(0, json, t + consumed + 1, &csp->meta.algo);
      if (stat != CJ_ERROR_OK) { return stat; }
      consumed += 2;
    }
    else if (jsonEq(json, t + consumed, "params")) {
      int stat = jsonConsumeAny(json, t + consumed + 1);
      if (stat < 0) { return stat; }
      int cpyStat = jsonStrCpy(1, json, t + consumed + 1, &csp->meta.paramsJSON);
      if (cpyStat != CJ_ERROR_OK) { return cpyStat; }
      consumed += 1 + stat;
    }
    else {
      return CJ_ERROR_META_UNKNOWN_FIELD;
    }
  }

  return consumed;
}

static int cjCspJsonParseDomain(const char* json, jsmntok_t* t, CjDomain* domain) {
  logTok("values:", json, t);
  if (!json || !t || !domain) { return CJ_ERROR_ARG; }
  if (t->type != JSMN_OBJECT || t->size != 1) { return CJ_ERROR_DOMAIN_IS_NOT_OBJECT; }
  if (! jsonEq(json, t + 1, "values")) { return CJ_ERROR_DOMAIN_UNKNOWN_TYPE; }
  if (t[2].type != JSMN_ARRAY) { return CJ_ERROR_DOMAIN_VALUES_IS_NOT_ARRAY; }

  const int defaultArity = -1;
  int stat = cjIntTuplesParseTok(defaultArity, json, &t[2], &domain->values);
  if (stat < 0) {
    return stat;
  }
  domain->type = CJ_DOMAIN_VALUES;
  return 2 + stat;
}

static int cjCspJsonParseDomains(const char* json, jsmntok_t* t, CjCsp* csp) {
  logTok("domains:", json, t);
  if (t->type != JSMN_ARRAY) { return CJ_ERROR_DOMAINS_IS_NOT_ARRAY; }

  if (t->size > 0) {
    csp->domains = cjDomainArray(t->size);
    if (!csp->domains) { return CJ_ERROR_NOMEM; }
    csp->domainsSize = t->size;
  }
  csp->domainsSize = t->size;

  int consumed = 1;
  for (int iChild = 0; iChild < t->size; ++iChild) {
    logTok("domains-child:", json, &t[consumed]);
    int stat = cjCspJsonParseDomain(json, t + consumed, &csp->domains[iChild]);
    if (stat < 0) {
      cjDomainArrayFree(&csp->domains, csp->domainsSize);
      csp->domainsSize = 0;
      return stat;
    }
    consumed += stat;
  }

  return consumed;
}

static int cjCspJsonParseVars(const char* json, jsmntok_t* t, CjCsp* csp) {
  logTok("vars:", json, t);
  if (!json || !t || !csp) { return CJ_ERROR_ARG; }
  if (t->type != JSMN_ARRAY) { return CJ_ERROR_VARS_IS_NOT_ARRAY; }

  const int defaultArity = -1;
  return cjIntTuplesParseTok(defaultArity, json, t, &csp->vars);
}

static int cjCspJsonParseNoGoods(const char* json, jsmntok_t* t, CjConstraintDef* constraintDef) {
  logTok("noGoods:", json, t);
  if (!json || !t || !constraintDef) { return CJ_ERROR_ARG; }
  if (t->type != JSMN_ARRAY) { return CJ_ERROR_NOGOODS_IS_NOT_ARRAY; }

  const int defaultArity = 0;
  int stat = cjIntTuplesParseTok(defaultArity, json, t, &constraintDef->noGoods);
  if (stat < 0) {
    return stat;
  }
  constraintDef->type = CJ_CONSTRAINT_DEF_NO_GOODS;
  return stat;
}

static int cjCspJsonParseConstraintsDef(const char* json, jsmntok_t* t, CjConstraintDef* constraintDef) {
  logTok("noGoods:", json, t);
  if (!json || !t || !constraintDef) { return CJ_ERROR_ARG; }
  if (t->type != JSMN_OBJECT) { return CJ_ERROR_CONSTRAINTDEF_IS_NOT_OBJECT; }

  if (jsonEq(json, t + 1, "noGoods")) {
    int stat = cjCspJsonParseNoGoods(json, t + 2, constraintDef);
    if (stat < 0) { return stat; }
    return 2 + stat;
  }
  else {
    return CJ_ERROR_CONSTRAINTDEF_UNKNOWN_TYPE;
  }
}

static int cjCspJsonParseConstraintsDefs(const char* json, jsmntok_t* t, CjCsp* csp) {
  logTok("constraintDefs:", json, t);
  if (!json || !t || !csp) { return CJ_ERROR_ARG; }
  if (t->type != JSMN_ARRAY) { return CJ_ERROR_CONSTRAINTDEFS_IS_NOT_ARRAY; }

  if (t->size > 0) {
    csp->constraintDefs = cjConstraintDefArray(t->size);
    if (!csp->constraintDefs) { return CJ_ERROR_NOMEM; }
  }
  csp->constraintDefsSize = t->size;

  int consumed = 1;
  for (int iChild = 0; iChild < t->size; ++iChild) {
    logTok("constraintDefs-child:", json, &t[consumed]);
    int stat = cjCspJsonParseConstraintsDef(json, t + consumed, &csp->constraintDefs[iChild]);
    if (stat < 0) {
      cjConstraintDefArrayFree(&csp->constraintDefs, csp->constraintDefsSize);
      csp->constraintDefsSize = 0;
      return stat;
    }
    consumed += stat;
  }

  return consumed;
}

static int cjCspJsonParseConstraint(const char* json, jsmntok_t* t, CjConstraint* constraint) {
  logTok("constraint:", json, t);
  if (!json || !t || !constraint) { return CJ_ERROR_ARG; }
  if (t->type != JSMN_OBJECT) { return CJ_ERROR_CONSTRAINT_IS_NOT_OBJECT; }

  *constraint = cjConstraintInit();

  int consumed = 1;
  for (int iChild = 0; iChild < t->size; ++iChild) {
    logTok("constraint-child:", json, &t[consumed]);
    if (jsonEq(json, t + consumed, "id")) {
      if (! jsonIsInt(json, t + consumed + 1)) { return CJ_ERROR_CONSTRAINT_ID_IS_NOT_INT; }
      constraint->id = strtol(json + t[consumed + 1].start, NULL, 10);
      consumed += 2;
    }
    else if (jsonEq(json, t + consumed, "vars")) {
      if (t[consumed + 1].type != JSMN_ARRAY) { return CJ_ERROR_CONSTRAINT_VARS_IS_NOT_ARRAY; }

      const int defaultArity = -1;
      int stat = cjIntTuplesParseTok(defaultArity, json, &t[consumed + 1], &constraint->vars);
      if (stat < 0) {
        return stat;
      }
      consumed += 1 + stat;
    }
    else {
      return CJ_ERROR_CONSTRAINT_UNKNOWN_FIELD;
    }
  }

  return consumed;
}

static int cjCspJsonParseConstraints(const char* json, jsmntok_t* t, CjCsp* csp) {
  logTok("constraints:", json, t);
  if (!json || !t || !csp) { return CJ_ERROR_ARG; }
  if (t->type != JSMN_ARRAY) { return CJ_ERROR_CONSTRAINTS_IS_NOT_ARRAY; }

  if (t->size > 0) {
    csp->constraints = cjConstraintArray(t->size);
    if (!csp->constraints) { return CJ_ERROR_NOMEM; }
    csp->constraintsSize = t->size;
  }
  csp->constraintsSize = t->size;

  int consumed = 1;
  for (int iChild = 0; iChild < t->size; ++iChild) {
    int stat = cjCspJsonParseConstraint(json, t + consumed, &csp->constraints[iChild]);
    if (stat < 0) {
      cjConstraintArrayFree(&csp->constraints, csp->constraintsSize);
      csp->constraintsSize = 0;
      return stat;
    }
    consumed += stat;
  }

  return consumed;
}

/** Return negative on error, otherwise number of tokens consumed. */
static int cjCspJsonParseTop(const char* json, jsmntok_t* t, CjCsp* csp) {
  logTok("top:", json, t);
  if (!json || !t || !csp) { return CJ_ERROR_ARG; }
  if (t->type != JSMN_OBJECT) { return CJ_ERROR_CSPJSON_IS_NOT_OBJECT; }
  if (t->size != 5) { return CJ_ERROR_CSPJSON_BAD_FIELD_COUNT; }

  int consumed = 1;
  for (int iChild = 0; iChild < t->size; ++iChild) {
    logTok("top-child:", json, &t[consumed]);
    if (jsonEq(json, t + consumed, "meta")) {
      int stat = cjCspJsonParseMeta(json, t + consumed + 1, csp);
      if (stat < 0) { return stat; }
      consumed += 1 + stat;
    }
    else if (jsonEq(json, t + consumed, "domains")) {
      int stat = cjCspJsonParseDomains(json, t + consumed + 1, csp);
      if (stat < 0) { return stat; }
      consumed += 1 + stat;
    }
    else if (jsonEq(json, t + consumed, "vars")) {
      int stat = cjCspJsonParseVars(json, t + consumed + 1, csp);
      if (stat < 0) { return stat; }
      consumed += 1 + stat;
    }
    else if (jsonEq(json, t + consumed, "constraintDefs")) {
      int stat = cjCspJsonParseConstraintsDefs(json, t + consumed + 1, csp);
      if (stat < 0) { return stat; }
      consumed += 1 + stat;
    }
    else if (jsonEq(json, t + consumed, "constraints")) {
      int stat = cjCspJsonParseConstraints(json, t + consumed + 1, csp);
      if (stat < 0) { return stat; }
      consumed += 1 + stat;
    }
    else {
      return CJ_ERROR_CSPJSON_UNKNOWN_FIELD;
    }
  }

  return consumed;
}

static CjError jsmnErrorToCjError(int jsmnErr) {
  switch (jsmnErr) {
    case JSMN_ERROR_NOMEM: return CJ_ERROR_JSMN_NOMEM;
    case JSMN_ERROR_INVAL: return CJ_ERROR_JSMN_INVAL;
    case JSMN_ERROR_PART:  return CJ_ERROR_JSMN_PART;
    default:               return CJ_ERROR_JSMN;
  }
}


/** jsmn_parse() tokens into (*t). Call free(*t) after use. */
static CjError jsmnTokenize(const char* json, const size_t jsonLen, jsmntok_t** t, int* numTokens) {
  if (!json || !t || !numTokens) { return CJ_ERROR_ARG; }

  jsmn_parser p;
  jsmn_init(&p);

  // Calculate number of tokens needed
  *numTokens = jsmn_parse(&p, json, jsonLen, NULL, 0);
  if (*numTokens < 0) {
    return jsmnErrorToCjError(*numTokens);
  }
  if (*numTokens == 0) {
    return CJ_ERROR_OK;
  }
  *t = malloc(sizeof(jsmntok_t) * (*numTokens));
  if (! (*t)) {
    return CJ_ERROR_NOMEM;
  }

  // Tokenize
  jsmn_init(&p);
  *numTokens = jsmn_parse(&p, json, jsonLen, *t, *numTokens);
  if (*numTokens < 0) {
    free(*t);
    *t = NULL;
    return jsmnErrorToCjError(*numTokens);
  }
  return CJ_ERROR_OK;
}

////////////////////////////////////////////////////////////////////////////////
// CjIntTuples IO
//

CjError cjIntTuplesParse(
  const int defaultArity, const char* json, const size_t jsonLen, CjIntTuples* ts)
{
  if (!json || !ts) { return CJ_ERROR_ARG; }
  if (defaultArity < -1) { return CJ_ERROR_ARG; }

  *ts = cjIntTuplesInit();

  jsmntok_t* t = NULL;
  int numTokens = 0;
  CjError stat = jsmnTokenize(json, jsonLen, &t, &numTokens);
  if (stat != CJ_ERROR_OK) { return stat; }
  if (numTokens == 0) { free(t); return CJ_ERROR_ARG; }

  int consumedOrStat = cjIntTuplesParseTok(defaultArity, json, t, ts);
  free(t);
  if (consumedOrStat < 0)       { return consumedOrStat; }
  else if (consumedOrStat == 0) { return CJ_ERROR_ARG; }
  else                          { return CJ_ERROR_OK; }
}

CjError cjIntTuplesJsonPrint(FILE* f, const CjIntTuples* ts) {
  if (!f || !ts) { return CJ_ERROR_ARG; }
  if (ts->size < 0 || ts->arity < -1) { return CJ_ERROR_ARG; }
  fprintf(f, "[");
  for (int s = 0; s < ts->size; ++s) {
    if (s > 0) { fprintf(f, ", "); }
    if (ts->arity >= 0) { fprintf(f, "["); }
    for (int a = 0; a < abs(ts->arity); ++a) {
      if (a > 0) { fprintf(f, ", "); }
      fprintf(f, "%d", ts->data[s*abs(ts->arity) + a]);
    }
    if (ts->arity >= 0) { fprintf(f, "]"); }
  }
  fprintf(f, "]");

  return CJ_ERROR_OK;
}

////////////////////////////////////////////////////////////////////////////////
// CjConstraintDef IO
//

CjError cjConstraintDefParse(
  const char* json,
  const size_t jsonLen,
  CjConstraintDef* cdef)
{
  if (!json || !cdef) { return CJ_ERROR_ARG; }

  jsmntok_t* t = NULL;
  int numTokens = 0;
  CjError stat = jsmnTokenize(json, jsonLen, &t, &numTokens);
  if (stat != CJ_ERROR_OK) { return stat; }
  if (numTokens == 0) { free(t); return CJ_ERROR_ARG; }

  int consumedOrStat = cjCspJsonParseConstraintsDef(json, t, cdef);
  free(t);
  if (consumedOrStat < 0)       { return consumedOrStat; }
  else if (consumedOrStat == 0) { return CJ_ERROR_ARG; }
  else                          { return CJ_ERROR_OK; }
}

CjError cjConstraintDefJsonPrint(FILE* f, const CjConstraintDef* cdef) {
  if (!f || !cdef) { return CJ_ERROR_ARG; }

  if (cdef->type == CJ_CONSTRAINT_DEF_NO_GOODS) {
    fprintf(f, "{\"noGoods\": ");
    CjError err = cjIntTuplesJsonPrint(f, &cdef->noGoods);
    if (err != CJ_ERROR_OK) { return err; }
    fprintf(f, "}");
    return CJ_ERROR_OK;
  }
  else {
    return CJ_ERROR_CONSTRAINTDEF_UNKNOWN_TYPE;
  }
}


////////////////////////////////////////////////////////////////////////////////
// CjCsp IO
//

CjError cjCspJsonParse(const char* json, const size_t jsonLen, CjCsp* csp) {
  if (!json || !csp) { return CJ_ERROR_ARG; }

  *csp = cjCspInit();

  jsmntok_t* t = NULL;
  int numTokens = 0;
  CjError stat = jsmnTokenize(json, jsonLen, &t, &numTokens);
  if (stat != CJ_ERROR_OK) { return stat; }
  if (numTokens == 0) { free(t); return CJ_ERROR_ARG; }

  int consumedOrStat = cjCspJsonParseTop(json, t, csp);
  free(t);
  if (consumedOrStat < 0)       { return consumedOrStat; }
  else if (consumedOrStat == 0) { return CJ_ERROR_ARG; }
  else                          { return CJ_ERROR_OK; }
}

CjError cjCspJsonPrint(FILE* f, const CjCsp* csp) {
  if (!f || !csp) { return CJ_ERROR_ARG; }
  CjError err = CJ_ERROR_OK;

  fprintf(f, "{\n");

  fprintf(f, "  \"meta\": {\n");
  fprintf(f, "    \"id\": \"%s\",\n", csp->meta.id);
  fprintf(f, "    \"algo\": \"%s\",\n", csp->meta.algo);
  fprintf(f, "    \"params\": %s\n", csp->meta.paramsJSON);
  fprintf(f, "  },\n");

  if (csp->domainsSize == 0) {
    fprintf(f, "  \"domains\": [],\n");
  } else {
    fprintf(f, "  \"domains\": [\n");
    for (int iDom = 0; iDom < csp->domainsSize; ++iDom) {
      if (csp->domains[iDom].type == CJ_DOMAIN_VALUES) {
        fprintf(f, "    {\"values\": ");
        cjIntTuplesJsonPrint(f, &csp->domains[iDom].values);
        fprintf(f, "}");
      }
      else {
        return CJ_ERROR_DOMAIN_UNKNOWN_TYPE;
      }
      if (iDom != csp->domainsSize - 1) { fprintf(f, ",\n"); }
      else { fprintf(f, "\n");  }
    }
    fprintf(f, "  ],\n");
  }

  fprintf(f, "  \"vars\": ");
  cjIntTuplesJsonPrint(f, &csp->vars);
  fprintf(f, ",\n");

  if (csp->constraintDefsSize == 0) {
    fprintf(f, "  \"constraintDefs\": [],\n");
  }
  else {
    fprintf(f, "  \"constraintDefs\": [\n");
    for (int iDef = 0; iDef < csp->constraintDefsSize; ++iDef) {
      fprintf(f, "    ");
      err = cjConstraintDefJsonPrint(f, &csp->constraintDefs[iDef]);
      if (err != CJ_ERROR_OK) { return err; }
      if (iDef != csp->constraintDefsSize - 1) { fprintf(f, ",\n"); }
      else { fprintf(f, "\n");  }
    }
    fprintf(f, "  ],\n");
  }

  if (csp->constraintsSize == 0) {
    fprintf(f, "  \"constraints\": []\n");
  }
  else {
    fprintf(f, "  \"constraints\": [\n");
    for (int i = 0; i < csp->constraintsSize; ++i) {
      fprintf(f, "    {\"id\": %d, \"vars\": ", csp->constraints[i].id);
      cjIntTuplesJsonPrint(f, &csp->constraints[i].vars);
      fprintf(f, "}");
      if (i != csp->constraintsSize - 1) { fprintf(f, ",\n"); }
      else { fprintf(f, "\n");  }
    }
    fprintf(f, "  ]\n");
  }

  fprintf(f, "}\n");

  return CJ_ERROR_OK;
}

