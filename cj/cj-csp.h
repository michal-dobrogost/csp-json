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
