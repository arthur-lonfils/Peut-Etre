/*
 * Peut-Être — Chaos Runtime (TypeScript Target)
 * This file is prepended to every generated TypeScript file.
 * All types are 'any' because commitment is overrated.
 *
 * "Honnêtement, à un près, on va pas chipoter."
 */

let _pe_chaos_enabled: boolean = true;

function _pe_roll(): number {
    return Math.floor(Math.random() * 100);
}

/* ============================================================
 * Chaos #1: Math Apathy
 * 5% chance arithmetic result is off by ±1
 * ============================================================ */

function _pe_math_fudge(result: any): any {
    if (_pe_chaos_enabled && _pe_roll() < 5) {
        const nudge = Math.random() < 0.5 ? 1 : -1;
        console.error(
            `\x1b[33m[CHAOS] Honnêtement, à un près, on va pas chipoter. (${result} -> ${result + nudge})\x1b[0m`
        );
        return result + nudge;
    }
    return result;
}

/* ============================================================
 * Arithmetic
 * ============================================================ */

function _pe_add(a: any, b: any): any {
    if (typeof a === "string" || typeof b === "string") return String(a) + String(b);
    return _pe_math_fudge(a + b);
}

function _pe_sub(a: any, b: any): any {
    return _pe_math_fudge(a - b);
}

function _pe_mul(a: any, b: any): any {
    return _pe_math_fudge(a * b);
}

function _pe_div(a: any, b: any): any {
    if (b === 0) {
        console.error("\x1b[31mOups, division par zéro. C'est la vie.\x1b[0m");
        return 0;
    }
    return _pe_math_fudge(a / b);
}

function _pe_mod(a: any, b: any): any {
    if (b === 0) {
        console.error("\x1b[31mOups, modulo zéro. C'est la vie.\x1b[0m");
        return 0;
    }
    return _pe_math_fudge(a % b);
}

function _pe_neg(a: any): any {
    return -a;
}

/* ============================================================
 * Comparisons
 * ============================================================ */

function _pe_eq(a: any, b: any): any  { return a === b; }
function _pe_neq(a: any, b: any): any { return a !== b; }
function _pe_lt(a: any, b: any): any  { return a < b; }
function _pe_gt(a: any, b: any): any  { return a > b; }
function _pe_lte(a: any, b: any): any { return a <= b; }
function _pe_gte(a: any, b: any): any { return a >= b; }

function _pe_and(a: any, b: any): any { return a && b; }
function _pe_or(a: any, b: any): any  { return a || b; }
function _pe_not(a: any): any         { return !a; }

/* kif_kif: == with 5% chance of returning true anyway */
function _pe_fuzzy_eq(a: any, b: any): any {
    if (_pe_chaos_enabled && _pe_roll() < 5) {
        console.error("\x1b[33m[CHAOS] Ouais, kif-kif, c'est pareil (à peu près).\x1b[0m");
        return true;
    }
    return a === b;
}

/* ============================================================
 * Chaos #2: Conditional Laziness
 * 5% chance if-condition evaluates to false regardless
 * ============================================================ */

function _pe_lazy_cond(cond: any): boolean {
    if (_pe_chaos_enabled && _pe_roll() < 5) {
        console.error("\x1b[33m[CHAOS] Flemme de vérifier.\x1b[0m");
        return false;
    }
    return !!cond;
}

/* ============================================================
 * Chaos #3: Loop Exhaustion
 * After 20 iterations, 10% chance per cycle of forced break
 * ============================================================ */

class PeLoopState {
    iteration: number = 0;
}

function _pe_loop_check(state: PeLoopState, cond: any): boolean {
    if (!cond) return false;

    state.iteration++;
    if (_pe_chaos_enabled && state.iteration > 20 && _pe_roll() < 10) {
        console.error(
            `\x1b[33m[CHAOS] C'est bon, ça tourne en rond ce truc, j'arrête. (iteration ${state.iteration})\x1b[0m`
        );
        return false;
    }
    return true;
}

/* ============================================================
 * I/O: lache_un_com (print)
 * ============================================================ */

function _pe_print(v: any): void {
    if (typeof v === "boolean") {
        console.log(v ? "ouais" : "non");
    } else if (v === null || v === undefined) {
        console.log("nada");
    } else {
        console.log(v);
    }
}

/* ============================================================
 * Probabilistic boolean
 * ============================================================ */

function _pe_prob_bool(probability: number): any {
    return _pe_roll() < probability;
}
