# Peut-Être — Development Guide

## Project Overview

Peut-Être is a French esoteric programming language transpiler written in C. It reads `.pt` source files, parses French slang keywords, and transpiles to C or TypeScript with injected probabilistic "Chaos Engine" behaviors.

## Build

```bash
make          # builds the 'peut-etre' binary
make clean    # removes build artifacts
```

Requires `gcc` with C11 support. No external dependencies.

## Architecture

```
.pt source → Lexer (lexer.c) → Parser (parser.c) → Codegen → gcc/ts-node → run
```

- **Lexer** (`src/lexer.h`, `src/lexer.c`): Tokenizes `.pt` files. Keywords are loaded dynamically from `dictionnaire.json` at runtime — the lexer contains a minimal JSON parser and a hardcoded concept→TokenType map. Identifier scanning checks against the dynamic keyword table.
- **Parser** (`src/parser.h`, `src/parser.c`): Recursive-descent with Pratt precedence climbing for expressions. Builds an AST using a tagged union (`AstNode`). Panic-mode error recovery at statement boundaries.
- **Codegen** (`src/codegen.h`, `src/codegen_c.c`, `src/codegen_ts.c`): Walks AST and emits C or TypeScript with chaos wrappers (`_pe_add`, `_pe_lazy_cond`, `_pe_loop_check`). Two-pass for C (forward declarations then definitions). All variables are `PeValue` (C) or `any` (TS).
- **Runtime** (`runtime/chaos_runtime.h`, `runtime/chaos_runtime.ts`): Self-contained modules embedded into generated code. Implement `PeValue` tagged union, Math Apathy, Conditional Laziness, Loop Exhaustion, fuzzy equality, probabilistic booleans.
- **Main** (`src/main.c`): CLI argument parsing, dictionary/runtime resolution, full pipeline (lex → parse → codegen → temp file → gcc/ts-node → run → cleanup), plus dispatch for `--lex`, `--ast`, `--emit`, `--lint`, `--format`, `--generate-tmLanguage`.
- **VS Code Extension** (`vscode-extension/`): TextMate grammar (auto-generated from dictionary), language configuration, snippets.
- **Util** (`src/util.h`, `src/util.c`): `pe_read_file()` — reads entire file into malloc'd buffer.

## Key Conventions

- All error messages and CLI output are in French, UTF-8 encoded (raw `\xNN` byte sequences in C strings).
- Token types are semantic (e.g., `TOK_IF`, `TOK_WHILE`), not lexical. The French keyword mapping is purely in `dictionnaire.json`.
- AST nodes use `calloc` + manual `free` (no arena yet). `ast_free()` handles recursive cleanup.
- `NodeList` is the dynamic array type for variable-length children (block statements, elif chains, function args).
- The `PeDict` struct in `lexer.h` is the runtime keyword table. It's loaded once and shared across the lexer and main (for tmLanguage generation).

## Testing

```bash
make test                                 # run full test suite (30 tests)
./peut-etre examples/bonjour.pt           # end-to-end: transpile + compile + run
./peut-etre --no-chaos examples/bonjour.pt # deterministic output (no chaos)
./peut-etre --lex examples/bonjour.pt     # verify lexer tokenization
./peut-etre --ast examples/bonjour.pt     # verify parser AST output
./peut-etre --emit examples/bonjour.pt    # print generated C code
./peut-etre --emit --target ts examples/bonjour.pt # print generated TypeScript
./peut-etre --lint examples/bonjour.pt    # run lazy linter
./peut-etre --format examples/bonjour.pt  # run anarchist formatter
./peut-etre --generate-tmLanguage         # generate TextMate grammar JSON
```

The test suite (`tests/run_tests.sh`) verifies all examples through every pipeline stage (lex, parse, codegen C+TS, end-to-end run), plus determinism with `--seed` and DevEx tools.

## Dictionary

`dictionnaire.json` maps concept names (e.g., `"VAR"`, `"IF"`, `"WHILE"`) to French keywords. Values can be strings or arrays of strings (for synonyms like `["genre", "en_gros"]`). Adding a new keyword requires:
1. Add the concept→keyword mapping in `dictionnaire.json`
2. Add a `TOK_*` enum value in `lexer.h`
3. Add the concept→token mapping in the `concept_map[]` table in `lexer.c`
4. Add the name in `token_type_name()` in `lexer.c`

## Chaos Engine

Three probabilistic behaviors injected into transpiled output via `chaos_runtime.h` / `chaos_runtime.ts`:
1. **Math Apathy**: 5% chance arithmetic results off by ±1 — `_pe_math_fudge_i()` / `_pe_math_fudge_f()`
2. **Conditional Laziness**: 5% chance `if` evaluates false regardless — `_pe_lazy_cond()`
3. **Loop Exhaustion**: After 20 iterations, 10%/cycle chance of forced break — `_pe_loop_check()`

Disable with `--no-chaos`. Seed with `--seed N` for reproducible chaos (C target only — TS `Math.random()` cannot be seeded).
