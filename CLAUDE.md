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
- **Codegen** (`src/codegen_c.c`, `src/codegen_ts.c`): Not yet implemented. Will walk AST and emit target language code with chaos wrappers.
- **Runtime** (`runtime/chaos_runtime.h`, `runtime/chaos_runtime.ts`): Not yet implemented. Headers/modules injected into generated code.
- **Main** (`src/main.c`): CLI argument parsing, dictionary resolution, and dispatch for `--lex`, `--ast`, `--lint`, `--format`, `--generate-tmLanguage`.
- **Util** (`src/util.h`, `src/util.c`): `pe_read_file()` — reads entire file into malloc'd buffer.

## Key Conventions

- All error messages and CLI output are in French, UTF-8 encoded (raw `\xNN` byte sequences in C strings).
- Token types are semantic (e.g., `TOK_IF`, `TOK_WHILE`), not lexical. The French keyword mapping is purely in `dictionnaire.json`.
- AST nodes use `calloc` + manual `free` (no arena yet). `ast_free()` handles recursive cleanup.
- `NodeList` is the dynamic array type for variable-length children (block statements, elif chains, function args).
- The `PeDict` struct in `lexer.h` is the runtime keyword table. It's loaded once and shared across the lexer and main (for tmLanguage generation).

## Testing

```bash
./peut-etre --lex examples/bonjour.pt    # verify lexer tokenization
./peut-etre --ast examples/bonjour.pt    # verify parser AST output
./peut-etre --lint examples/bonjour.pt   # run lazy linter
./peut-etre --format examples/bonjour.pt # run anarchist formatter
./peut-etre --generate-tmLanguage        # generate TextMate grammar JSON
```

No test framework yet — verify by running the above and inspecting output.

## Dictionary

`dictionnaire.json` maps concept names (e.g., `"VAR"`, `"IF"`, `"WHILE"`) to French keywords. Values can be strings or arrays of strings (for synonyms like `["genre", "en_gros"]`). Adding a new keyword requires:
1. Add the concept→keyword mapping in `dictionnaire.json`
2. Add a `TOK_*` enum value in `lexer.h`
3. Add the concept→token mapping in the `concept_map[]` table in `lexer.c`
4. Add the name in `token_type_name()` in `lexer.c`

## Chaos Engine (Design — Not Yet Implemented)

Three probabilistic behaviors injected into transpiled output:
1. **Math Apathy**: 5% chance arithmetic results off by ±1
2. **Conditional Laziness**: 5% chance `if` evaluates false regardless
3. **Loop Exhaustion**: After 20 iterations, 10%/cycle chance of forced break
