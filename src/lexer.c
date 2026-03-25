#include "lexer.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Concept Map — hardcoded concept name → TokenType
 * The dictionary JSON maps concepts to French keywords.
 * This table resolves concepts to their semantic token type.
 * ============================================================ */

typedef struct {
    const char *concept;
    TokenType   type;
} ConceptEntry;

static const ConceptEntry concept_map[] = {
    {"ANY_TYPE",     TOK_ANY_TYPE},
    {"ARRAY_TYPE",   TOK_ARRAY_TYPE},
    {"ASSERT",       TOK_ASSERT},
    {"ASYNC",        TOK_ASYNC},
    {"AWAIT",        TOK_AWAIT},
    {"BREAK",        TOK_BREAK},
    {"CAST",         TOK_CAST},
    {"CATCH",        TOK_CATCH},
    {"CONTINUE",     TOK_CONTINUE},
    {"DELETE",       TOK_DELETE},
    {"ELIF",         TOK_ELIF},
    {"ELSE",         TOK_ELSE},
    {"EXPORT",       TOK_EXPORT},
    {"FALSE",        TOK_FALSE},
    {"FINALLY",      TOK_FINALLY},
    {"FUNC",         TOK_FUNC},
    {"FUZZY_EQ",     TOK_FUZZY_EQ},
    {"GREATER",      TOK_GREATER},
    {"HALF_TRUE",    TOK_HALF_TRUE},
    {"IF",           TOK_IF},
    {"IMPORT",       TOK_IMPORT},
    {"INTERFACE",    TOK_INTERFACE},
    {"LESS",         TOK_LESS},
    {"LOGIC_AND",    TOK_LOGIC_AND},
    {"LOGIC_OR",     TOK_LOGIC_OR},
    {"MAIN",         TOK_MAIN},
    {"MAYBE_FALSE",  TOK_MAYBE_FALSE},
    {"MAYBE_TRUE",   TOK_MAYBE_TRUE},
    {"NOT_EQ",       TOK_NOT_EQ},
    {"NULL",         TOK_NULL},
    {"NUMBER_TYPE",  TOK_NUMBER_TYPE},
    {"PRINT",        TOK_PRINT},
    {"RETRY",        TOK_RETRY},
    {"RETURN",       TOK_RETURN},
    {"STRING_TYPE",  TOK_STRING_TYPE},
    {"THROW",        TOK_THROW},
    {"TRUE",         TOK_TRUE},
    {"TRY",          TOK_TRY},
    {"VAR",          TOK_VAR},
    {"WHILE",        TOK_WHILE},
};

#define CONCEPT_MAP_SIZE (sizeof(concept_map) / sizeof(concept_map[0]))

static TokenType concept_to_token(const char *concept) {
    for (size_t i = 0; i < CONCEPT_MAP_SIZE; i++) {
        if (strcmp(concept_map[i].concept, concept) == 0)
            return concept_map[i].type;
    }
    return TOK_ERROR;
}

/* ============================================================
 * Minimal JSON Parser
 * Only handles: flat objects with string or array-of-string values.
 * Unknown value types (nested objects, numbers, etc.) are skipped.
 * ============================================================ */

typedef struct { const char *p; } JsonCtx;

static void json_ws(JsonCtx *j) {
    while (*j->p == ' ' || *j->p == '\t' || *j->p == '\n' || *j->p == '\r')
        j->p++;
}

/* Parse a JSON string. Returns a malloc'd copy without quotes. */
static char *json_string(JsonCtx *j) {
    if (*j->p != '"') return NULL;
    j->p++;
    const char *start = j->p;
    while (*j->p && *j->p != '"') {
        if (*j->p == '\\') j->p++;   /* skip escaped char */
        j->p++;
    }
    size_t len = (size_t)(j->p - start);
    if (*j->p == '"') j->p++;
    char *s = malloc(len + 1);
    if (!s) return NULL;
    memcpy(s, start, len);
    s[len] = '\0';
    return s;
}

/* Skip any JSON value (string, array, object, number, literal). */
static void json_skip(JsonCtx *j) {
    json_ws(j);
    if (*j->p == '"') {
        /* Skip string */
        j->p++;
        while (*j->p && *j->p != '"') {
            if (*j->p == '\\') j->p++;
            j->p++;
        }
        if (*j->p) j->p++;
    } else if (*j->p == '[' || *j->p == '{') {
        /* Skip balanced brackets/braces */
        char open = *j->p;
        char close = (open == '[') ? ']' : '}';
        int depth = 1;
        j->p++;
        while (*j->p && depth > 0) {
            if (*j->p == '"') {
                j->p++;
                while (*j->p && *j->p != '"') {
                    if (*j->p == '\\') j->p++;
                    j->p++;
                }
                if (*j->p) j->p++;
            } else {
                if (*j->p == open)  depth++;
                if (*j->p == close) depth--;
                j->p++;
            }
        }
    } else {
        /* Skip number / true / false / null */
        while (*j->p && *j->p != ',' && *j->p != '}' && *j->p != ']')
            j->p++;
    }
}

/* ============================================================
 * Dictionary Loading
 * ============================================================ */

static void dict_add(PeDict *dict, char *keyword, TokenType type) {
    if (dict->count >= dict->capacity) {
        dict->capacity = dict->capacity ? dict->capacity * 2 : 64;
        dict->entries = realloc(dict->entries,
                                (size_t)dict->capacity * sizeof(PeDictEntry));
    }
    dict->entries[dict->count].keyword = keyword;   /* takes ownership */
    dict->entries[dict->count].type    = type;
    dict->count++;
}

int pe_dict_load(PeDict *dict, const char *json_path) {
    char *json = pe_read_file(json_path);
    if (!json) return -1;

    dict->entries  = NULL;
    dict->count    = 0;
    dict->capacity = 0;

    JsonCtx j = { json };
    json_ws(&j);
    if (*j.p != '{') { free(json); return -1; }
    j.p++;

    while (1) {
        json_ws(&j);
        if (*j.p == '}' || *j.p == '\0') break;
        if (*j.p == ',') { j.p++; continue; }

        /* Parse concept key */
        char *concept = json_string(&j);
        if (!concept) break;

        json_ws(&j);
        if (*j.p == ':') j.p++;
        json_ws(&j);

        /* Resolve concept → token type */
        TokenType type = concept_to_token(concept);
        if (type == TOK_ERROR) {
            /* Unknown concept — skip its value */
            json_skip(&j);
            free(concept);
            continue;
        }

        /* Parse value: string or array of strings */
        if (*j.p == '"') {
            char *kw = json_string(&j);
            if (kw) dict_add(dict, kw, type);
        } else if (*j.p == '[') {
            j.p++;
            while (1) {
                json_ws(&j);
                if (*j.p == ']') { j.p++; break; }
                if (*j.p == ',') { j.p++; continue; }
                char *kw = json_string(&j);
                if (kw) dict_add(dict, kw, type);
                else break;
            }
        } else {
            json_skip(&j);
        }

        free(concept);
    }

    free(json);
    return 0;
}

void pe_dict_free(PeDict *dict) {
    for (int i = 0; i < dict->count; i++)
        free(dict->entries[i].keyword);
    free(dict->entries);
    dict->entries  = NULL;
    dict->count    = 0;
    dict->capacity = 0;
}

/* ============================================================
 * Lexer — Scanner
 * ============================================================ */

static int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

static char advance(Lexer *lex) {
    char c = *lex->current;
    lex->current++;
    if (c == '\n') { lex->line++; lex->col = 1; }
    else            { lex->col++; }
    return c;
}

static char peek(Lexer *lex) {
    return *lex->current;
}

static char peek_next(Lexer *lex) {
    if (*lex->current == '\0') return '\0';
    return lex->current[1];
}

static void skip_whitespace(Lexer *lex) {
    while (1) {
        char c = peek(lex);
        switch (c) {
        case ' ': case '\t': case '\r': case '\n':
            advance(lex);
            break;
        case '/':
            if (peek_next(lex) == '/') {
                /* Line comment: skip to end of line */
                while (peek(lex) != '\0' && peek(lex) != '\n')
                    advance(lex);
            } else if (peek_next(lex) == '*') {
                /* Block comment: skip to closing delimiter */
                advance(lex); advance(lex);
                while (peek(lex) != '\0') {
                    if (peek(lex) == '*' && peek_next(lex) == '/') {
                        advance(lex); advance(lex);
                        break;
                    }
                    advance(lex);
                }
            } else {
                return;
            }
            break;
        default:
            return;
        }
    }
}

static TokenType dict_lookup(PeDict *dict, const char *text, int length) {
    for (int i = 0; i < dict->count; i++) {
        if ((int)strlen(dict->entries[i].keyword) == length &&
            memcmp(dict->entries[i].keyword, text, (size_t)length) == 0)
            return dict->entries[i].type;
    }
    return TOK_IDENT;
}

/* ============================================================
 * Public API
 * ============================================================ */

void lexer_init(Lexer *lex, const char *source, const char *filename,
                PeDict *dict) {
    lex->source   = source;
    lex->current  = source;
    lex->line     = 1;
    lex->col      = 1;
    lex->filename = filename;
    lex->dict     = dict;
}

Token lexer_next(Lexer *lex) {
    skip_whitespace(lex);

    int start_line = lex->line;
    int start_col  = lex->col;
    const char *start = lex->current;

    if (*lex->current == '\0')
        return (Token){ TOK_EOF, start, 0, start_line, start_col };

    char c = advance(lex);

    /* ---- Identifiers & keywords ---- */
    if (is_alpha(c)) {
        while (is_alnum(peek(lex)))
            advance(lex);
        int length = (int)(lex->current - start);
        TokenType type = dict_lookup(lex->dict, start, length);
        return (Token){ type, start, length, start_line, start_col };
    }

    /* ---- Number literals ---- */
    if (is_digit(c)) {
        while (is_digit(peek(lex)))
            advance(lex);
        TokenType type = TOK_INT_LIT;
        if (peek(lex) == '.' && is_digit(peek_next(lex))) {
            advance(lex);   /* consume '.' */
            while (is_digit(peek(lex)))
                advance(lex);
            type = TOK_FLOAT_LIT;
        }
        int length = (int)(lex->current - start);
        return (Token){ type, start, length, start_line, start_col };
    }

    /* ---- String literals ---- */
    if (c == '"') {
        while (peek(lex) != '\0' && peek(lex) != '"') {
            if (peek(lex) == '\\') advance(lex);
            advance(lex);
        }
        if (peek(lex) == '"') advance(lex);
        int length = (int)(lex->current - start);
        return (Token){ TOK_STRING_LIT, start, length, start_line, start_col };
    }

    /* ---- Operators & delimiters ---- */
    switch (c) {
    case '+': return (Token){ TOK_PLUS,      start, 1, start_line, start_col };
    case '-': return (Token){ TOK_MINUS,     start, 1, start_line, start_col };
    case '*': return (Token){ TOK_STAR,      start, 1, start_line, start_col };
    case '/': return (Token){ TOK_SLASH,     start, 1, start_line, start_col };
    case '%': return (Token){ TOK_PERCENT,   start, 1, start_line, start_col };
    case '(': return (Token){ TOK_LPAREN,    start, 1, start_line, start_col };
    case ')': return (Token){ TOK_RPAREN,    start, 1, start_line, start_col };
    case '{': return (Token){ TOK_LBRACE,    start, 1, start_line, start_col };
    case '}': return (Token){ TOK_RBRACE,    start, 1, start_line, start_col };
    case '[': return (Token){ TOK_LBRACKET,  start, 1, start_line, start_col };
    case ']': return (Token){ TOK_RBRACKET,  start, 1, start_line, start_col };
    case ';': return (Token){ TOK_SEMICOLON, start, 1, start_line, start_col };
    case ',': return (Token){ TOK_COMMA,     start, 1, start_line, start_col };
    case '.': return (Token){ TOK_DOT,       start, 1, start_line, start_col };
    case '!':
        if (peek(lex) == '=') {
            advance(lex);
            return (Token){ TOK_NEQ, start, 2, start_line, start_col };
        }
        return (Token){ TOK_NOT, start, 1, start_line, start_col };
    case '=':
        if (peek(lex) == '=') {
            advance(lex);
            return (Token){ TOK_EQ, start, 2, start_line, start_col };
        }
        return (Token){ TOK_ASSIGN, start, 1, start_line, start_col };
    case '<':
        if (peek(lex) == '=') {
            advance(lex);
            return (Token){ TOK_LTE, start, 2, start_line, start_col };
        }
        return (Token){ TOK_LT, start, 1, start_line, start_col };
    case '>':
        if (peek(lex) == '=') {
            advance(lex);
            return (Token){ TOK_GTE, start, 2, start_line, start_col };
        }
        return (Token){ TOK_GT, start, 1, start_line, start_col };
    case '&':
        if (peek(lex) == '&') {
            advance(lex);
            return (Token){ TOK_AND, start, 2, start_line, start_col };
        }
        break;
    case '|':
        if (peek(lex) == '|') {
            advance(lex);
            return (Token){ TOK_OR, start, 2, start_line, start_col };
        }
        break;
    }

    /* Unknown character */
    fprintf(stderr, "%s:%d:%d: Euh, c'est quoi '%c' ? Connais pas.\n",
            lex->filename, start_line, start_col, c);
    return (Token){ TOK_ERROR, start, 1, start_line, start_col };
}

/* ============================================================
 * Token type name (for debug output)
 * ============================================================ */

const char *token_type_name(TokenType type) {
    static const char *names[TOK_COUNT] = {
        [TOK_INT_LIT]     = "INT_LIT",
        [TOK_FLOAT_LIT]   = "FLOAT_LIT",
        [TOK_STRING_LIT]  = "STRING_LIT",
        [TOK_IDENT]       = "IDENT",
        [TOK_VAR]         = "VAR",
        [TOK_IF]          = "IF",
        [TOK_ELIF]        = "ELIF",
        [TOK_ELSE]        = "ELSE",
        [TOK_WHILE]       = "WHILE",
        [TOK_BREAK]       = "BREAK",
        [TOK_CONTINUE]    = "CONTINUE",
        [TOK_FUNC]        = "FUNC",
        [TOK_RETURN]      = "RETURN",
        [TOK_PRINT]       = "PRINT",
        [TOK_MAIN]        = "MAIN",
        [TOK_IMPORT]      = "IMPORT",
        [TOK_THROW]       = "THROW",
        [TOK_TRUE]        = "TRUE",
        [TOK_FALSE]       = "FALSE",
        [TOK_MAYBE_TRUE]  = "MAYBE_TRUE",
        [TOK_MAYBE_FALSE] = "MAYBE_FALSE",
        [TOK_NULL]        = "NULL",
        [TOK_HALF_TRUE]   = "HALF_TRUE",
        [TOK_STRING_TYPE] = "STRING_TYPE",
        [TOK_NUMBER_TYPE] = "NUMBER_TYPE",
        [TOK_ARRAY_TYPE]  = "ARRAY_TYPE",
        [TOK_ANY_TYPE]    = "ANY_TYPE",
        [TOK_FUZZY_EQ]    = "FUZZY_EQ",
        [TOK_NOT_EQ]      = "NOT_EQ",
        [TOK_LOGIC_AND]   = "LOGIC_AND",
        [TOK_LOGIC_OR]    = "LOGIC_OR",
        [TOK_GREATER]     = "GREATER",
        [TOK_LESS]        = "LESS",
        [TOK_ASYNC]       = "ASYNC",
        [TOK_AWAIT]       = "AWAIT",
        [TOK_INTERFACE]   = "INTERFACE",
        [TOK_EXPORT]      = "EXPORT",
        [TOK_TRY]         = "TRY",
        [TOK_CATCH]       = "CATCH",
        [TOK_FINALLY]     = "FINALLY",
        [TOK_CAST]        = "CAST",
        [TOK_ASSERT]      = "ASSERT",
        [TOK_RETRY]       = "RETRY",
        [TOK_DELETE]       = "DELETE",
        [TOK_PLUS]        = "PLUS",
        [TOK_MINUS]       = "MINUS",
        [TOK_STAR]        = "STAR",
        [TOK_SLASH]       = "SLASH",
        [TOK_PERCENT]     = "PERCENT",
        [TOK_ASSIGN]      = "ASSIGN",
        [TOK_EQ]          = "EQ",
        [TOK_NEQ]         = "NEQ",
        [TOK_LT]          = "LT",
        [TOK_GT]          = "GT",
        [TOK_LTE]         = "LTE",
        [TOK_GTE]         = "GTE",
        [TOK_AND]         = "AND",
        [TOK_OR]          = "OR",
        [TOK_NOT]         = "NOT",
        [TOK_LPAREN]      = "LPAREN",
        [TOK_RPAREN]      = "RPAREN",
        [TOK_LBRACE]      = "LBRACE",
        [TOK_RBRACE]      = "RBRACE",
        [TOK_LBRACKET]    = "LBRACKET",
        [TOK_RBRACKET]    = "RBRACKET",
        [TOK_SEMICOLON]   = "SEMICOLON",
        [TOK_COMMA]       = "COMMA",
        [TOK_DOT]         = "DOT",
        [TOK_EOF]         = "EOF",
        [TOK_ERROR]       = "ERROR",
    };
    if (type >= 0 && type < TOK_COUNT && names[type])
        return names[type];
    return "???";
}
