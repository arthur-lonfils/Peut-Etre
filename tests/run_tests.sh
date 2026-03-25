#!/bin/bash
# Peut-Être — Test Suite
# "On va tester, mais on promet rien."

set -e
cd "$(dirname "$0")/.."

BINARY=./peut-etre
PASS=0
FAIL=0

# Colors
GREEN='\033[32m'
RED='\033[31m'
YELLOW='\033[33m'
RESET='\033[0m'

pass() { echo -e "  ${GREEN}✓${RESET} $1"; PASS=$((PASS + 1)); }
fail() { echo -e "  ${RED}✗${RESET} $1: $2"; FAIL=$((FAIL + 1)); }

echo "Peut-Être — Test Suite"
echo "======================"
echo ""

# --- Build ---
echo "Build:"
if make -s 2>&1; then
    pass "make compiles without errors"
else
    fail "make" "compilation failed"
    exit 1
fi
echo ""

# --- Lexer ---
echo "Lexer (--lex):"
for f in examples/*.pt; do
    name=$(basename "$f")
    if $BINARY --lex "$f" > /dev/null 2>&1; then
        pass "--lex $name"
    else
        fail "--lex $name" "lexer failed"
    fi
done
echo ""

# --- Parser ---
echo "Parser (--ast):"
for f in examples/*.pt; do
    name=$(basename "$f")
    if $BINARY --ast "$f" > /dev/null 2>&1; then
        pass "--ast $name"
    else
        fail "--ast $name" "parser failed"
    fi
done
echo ""

# --- Code Generation ---
echo "Codegen (--emit):"
for f in examples/*.pt; do
    name=$(basename "$f")
    if $BINARY --emit "$f" > /dev/null 2>&1; then
        pass "--emit $name (C)"
    else
        fail "--emit $name (C)" "codegen failed"
    fi
    if $BINARY --emit --target ts "$f" > /dev/null 2>&1; then
        pass "--emit --target ts $name"
    else
        fail "--emit --target ts $name" "codegen failed"
    fi
done
echo ""

# --- End-to-End (C target) ---
echo "End-to-End (--no-chaos):"
for f in examples/*.pt; do
    name=$(basename "$f")
    if $BINARY --no-chaos "$f" > /dev/null 2>&1; then
        pass "run $name"
    else
        fail "run $name" "execution failed"
    fi
done
echo ""

# --- Determinism with --seed ---
echo "Determinism (--seed):"
OUT1=$($BINARY --no-chaos --seed 42 examples/bonjour.pt 2>/dev/null)
OUT2=$($BINARY --no-chaos --seed 42 examples/bonjour.pt 2>/dev/null)
if [ "$OUT1" = "$OUT2" ]; then
    pass "--seed 42 produces identical output"
else
    fail "--seed 42" "outputs differ"
fi
echo ""

# --- DevEx Tools ---
echo "DevEx Tools:"
if $BINARY --lint examples/bonjour.pt > /dev/null 2>&1; then
    pass "--lint"
else
    fail "--lint" "linter crashed"
fi
if $BINARY --format examples/bonjour.pt > /dev/null 2>&1; then
    pass "--format"
else
    fail "--format" "formatter crashed"
fi
if $BINARY --generate-tmLanguage > /dev/null 2>&1; then
    pass "--generate-tmLanguage"
else
    fail "--generate-tmLanguage" "grammar generation crashed"
fi
echo ""

# --- Summary ---
TOTAL=$((PASS + FAIL))
echo "======================"
if [ "$FAIL" -eq 0 ]; then
    echo -e "${GREEN}$PASS/$TOTAL tests passed. Pas mal, non ?${RESET}"
else
    echo -e "${RED}$FAIL/$TOTAL tests failed. Bof.${RESET}"
fi
exit $FAIL
