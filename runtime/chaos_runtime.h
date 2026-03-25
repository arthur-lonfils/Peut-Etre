/*
 * Peut-Être — Chaos Runtime (C Target)
 * This header is injected into every generated C file.
 * It is entirely self-contained: no external linking required.
 *
 * "Honnêtement, à un près, on va pas chipoter."
 */

#ifndef CHAOS_RUNTIME_H
#define CHAOS_RUNTIME_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ============================================================
 * RNG
 * ============================================================ */

static int _pe_chaos_enabled = 1;

static void _pe_chaos_init(void) {
    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());
}

static void _pe_chaos_init_seed(unsigned int seed) {
    srand(seed);
}

/* Returns 0..99 */
static inline int _pe_roll(void) {
    return rand() % 100;
}

/* ============================================================
 * Runtime Value Type (dynamically typed)
 * ============================================================ */

typedef enum {
    PE_TYPE_INT,
    PE_TYPE_FLOAT,
    PE_TYPE_STRING,
    PE_TYPE_BOOL,
    PE_TYPE_NULL,
} PeType;

typedef struct {
    PeType type;
    union {
        long    i;
        double  f;
        char   *s;
        int     b;  /* 0 or 1 */
    } val;
} PeValue;

/* Constructors */
static inline PeValue _pe_int(long v) {
    PeValue r; r.type = PE_TYPE_INT; r.val.i = v; return r;
}
static inline PeValue _pe_float(double v) {
    PeValue r; r.type = PE_TYPE_FLOAT; r.val.f = v; return r;
}
static inline PeValue _pe_string(const char *v) {
    PeValue r; r.type = PE_TYPE_STRING; r.val.s = (char *)v; return r;
}
static inline PeValue _pe_bool(int v) {
    PeValue r; r.type = PE_TYPE_BOOL; r.val.b = !!v; return r;
}
static inline PeValue _pe_null(void) {
    PeValue r; r.type = PE_TYPE_NULL; r.val.i = 0; return r;
}

/* Probabilistic boolean: evaluates to 1 with given probability (0-100) */
static inline PeValue _pe_prob_bool(int probability) {
    return _pe_bool(_pe_roll() < probability);
}

/* ============================================================
 * Truthiness
 * ============================================================ */

static inline int _pe_truthy(PeValue v) {
    switch (v.type) {
    case PE_TYPE_INT:    return v.val.i != 0;
    case PE_TYPE_FLOAT:  return v.val.f != 0.0;
    case PE_TYPE_STRING: return v.val.s != NULL && v.val.s[0] != '\0';
    case PE_TYPE_BOOL:   return v.val.b;
    case PE_TYPE_NULL:   return 0;
    }
    return 0;
}

/* ============================================================
 * Chaos #1: Math Apathy
 * 5% chance arithmetic result is off by ±1
 * "Honnêtement, à un près, on va pas chipoter."
 * ============================================================ */

static inline long _pe_math_fudge_i(long result) {
    if (_pe_chaos_enabled && _pe_roll() < 5) {
        long nudge = (rand() % 2 == 0) ? 1 : -1;
        fprintf(stderr, "\033[33m[CHAOS] Honn\xC3\xAAtement, \xC3\xA0 un pr\xC3\xA8s, "
                        "on va pas chipoter. (%ld -> %ld)\033[0m\n",
                result, result + nudge);
        return result + nudge;
    }
    return result;
}

static inline double _pe_math_fudge_f(double result) {
    if (_pe_chaos_enabled && _pe_roll() < 5) {
        double nudge = (rand() % 2 == 0) ? 1.0 : -1.0;
        fprintf(stderr, "\033[33m[CHAOS] Honn\xC3\xAAtement, \xC3\xA0 un pr\xC3\xA8s, "
                        "on va pas chipoter. (%g -> %g)\033[0m\n",
                result, result + nudge);
        return result + nudge;
    }
    return result;
}

/* Get numeric value as double for mixed-type arithmetic */
static inline double _pe_as_num(PeValue v) {
    switch (v.type) {
    case PE_TYPE_INT:   return (double)v.val.i;
    case PE_TYPE_FLOAT: return v.val.f;
    case PE_TYPE_BOOL:  return (double)v.val.b;
    default:            return 0.0;
    }
}

static inline int _pe_is_float(PeValue a, PeValue b) {
    return a.type == PE_TYPE_FLOAT || b.type == PE_TYPE_FLOAT;
}

static inline PeValue _pe_add(PeValue a, PeValue b) {
    /* String concatenation */
    if (a.type == PE_TYPE_STRING && b.type == PE_TYPE_STRING) {
        size_t la = strlen(a.val.s), lb = strlen(b.val.s);
        char *s = malloc(la + lb + 1);
        memcpy(s, a.val.s, la);
        memcpy(s + la, b.val.s, lb + 1);
        return _pe_string(s);
    }
    if (_pe_is_float(a, b))
        return _pe_float(_pe_math_fudge_f(_pe_as_num(a) + _pe_as_num(b)));
    return _pe_int(_pe_math_fudge_i(a.val.i + b.val.i));
}

static inline PeValue _pe_sub(PeValue a, PeValue b) {
    if (_pe_is_float(a, b))
        return _pe_float(_pe_math_fudge_f(_pe_as_num(a) - _pe_as_num(b)));
    return _pe_int(_pe_math_fudge_i(a.val.i - b.val.i));
}

static inline PeValue _pe_mul(PeValue a, PeValue b) {
    if (_pe_is_float(a, b))
        return _pe_float(_pe_math_fudge_f(_pe_as_num(a) * _pe_as_num(b)));
    return _pe_int(_pe_math_fudge_i(a.val.i * b.val.i));
}

static inline PeValue _pe_div(PeValue a, PeValue b) {
    double bv = _pe_as_num(b);
    if (bv == 0.0) {
        fprintf(stderr, "\033[31mOups, division par z\xC3\xA9ro. C'est la vie.\033[0m\n");
        return _pe_int(0);
    }
    if (_pe_is_float(a, b))
        return _pe_float(_pe_math_fudge_f(_pe_as_num(a) / bv));
    return _pe_int(_pe_math_fudge_i(a.val.i / b.val.i));
}

static inline PeValue _pe_mod(PeValue a, PeValue b) {
    if (b.val.i == 0) {
        fprintf(stderr, "\033[31mOups, modulo z\xC3\xA9ro. C'est la vie.\033[0m\n");
        return _pe_int(0);
    }
    return _pe_int(_pe_math_fudge_i(a.val.i % b.val.i));
}

static inline PeValue _pe_neg(PeValue a) {
    if (a.type == PE_TYPE_FLOAT) return _pe_float(-a.val.f);
    return _pe_int(-a.val.i);
}

/* ============================================================
 * Comparisons
 * ============================================================ */

static inline PeValue _pe_eq(PeValue a, PeValue b) {
    if (_pe_is_float(a, b))
        return _pe_bool(_pe_as_num(a) == _pe_as_num(b));
    if (a.type == PE_TYPE_STRING && b.type == PE_TYPE_STRING)
        return _pe_bool(strcmp(a.val.s, b.val.s) == 0);
    return _pe_bool(a.val.i == b.val.i);
}

static inline PeValue _pe_neq(PeValue a, PeValue b) {
    return _pe_bool(!_pe_truthy(_pe_eq(a, b)));
}

/* kif_kif: == with 5% chance of returning true anyway */
static inline PeValue _pe_fuzzy_eq(PeValue a, PeValue b) {
    if (_pe_chaos_enabled && _pe_roll() < 5) {
        fprintf(stderr, "\033[33m[CHAOS] Ouais, kif-kif, c'est pareil "
                        "(\xC3\xA0 peu pr\xC3\xA8s).\033[0m\n");
        return _pe_bool(1);
    }
    return _pe_eq(a, b);
}

static inline PeValue _pe_lt(PeValue a, PeValue b) {
    return _pe_bool(_pe_as_num(a) < _pe_as_num(b));
}

static inline PeValue _pe_gt(PeValue a, PeValue b) {
    return _pe_bool(_pe_as_num(a) > _pe_as_num(b));
}

static inline PeValue _pe_lte(PeValue a, PeValue b) {
    return _pe_bool(_pe_as_num(a) <= _pe_as_num(b));
}

static inline PeValue _pe_gte(PeValue a, PeValue b) {
    return _pe_bool(_pe_as_num(a) >= _pe_as_num(b));
}

/* Logical */
static inline PeValue _pe_and(PeValue a, PeValue b) {
    return _pe_bool(_pe_truthy(a) && _pe_truthy(b));
}

static inline PeValue _pe_or(PeValue a, PeValue b) {
    return _pe_bool(_pe_truthy(a) || _pe_truthy(b));
}

static inline PeValue _pe_not(PeValue a) {
    return _pe_bool(!_pe_truthy(a));
}

/* ============================================================
 * Chaos #2: Conditional Laziness
 * 5% chance if-condition evaluates to false regardless
 * "Flemme de vérifier."
 * ============================================================ */

static inline int _pe_lazy_cond(PeValue cond) {
    if (_pe_chaos_enabled && _pe_roll() < 5) {
        fprintf(stderr, "\033[33m[CHAOS] Flemme de v\xC3\xA9rifier.\033[0m\n");
        return 0;
    }
    return _pe_truthy(cond);
}

/* ============================================================
 * Chaos #3: Loop Exhaustion
 * After 20 iterations, 10% chance per cycle of forced break
 * "C'est bon, ça tourne en rond ce truc, j'arrête."
 * ============================================================ */

typedef struct {
    int iteration;
} PeLoopState;

static inline int _pe_loop_check(PeLoopState *state, PeValue cond) {
    if (!_pe_truthy(cond)) return 0;

    state->iteration++;
    if (_pe_chaos_enabled && state->iteration > 20 && _pe_roll() < 10) {
        fprintf(stderr, "\033[33m[CHAOS] C'est bon, \xC3\xA7""a tourne en rond "
                        "ce truc, j'arr\xC3\xAAte. (iteration %d)\033[0m\n",
                state->iteration);
        return 0;
    }
    return 1;
}

/* ============================================================
 * I/O: lache_un_com (print)
 * ============================================================ */

static inline void _pe_print(PeValue v) {
    switch (v.type) {
    case PE_TYPE_INT:    printf("%ld\n", v.val.i); break;
    case PE_TYPE_FLOAT:  printf("%g\n", v.val.f); break;
    case PE_TYPE_STRING: printf("%s\n", v.val.s ? v.val.s : "(null)"); break;
    case PE_TYPE_BOOL:   printf("%s\n", v.val.b ? "ouais" : "non"); break;
    case PE_TYPE_NULL:   printf("nada\n"); break;
    }
}

#endif /* CHAOS_RUNTIME_H */
