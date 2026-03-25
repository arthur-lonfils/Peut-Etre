#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "util.h"
#include <unistd.h>
#include <sys/wait.h>

#define PE_VERSION "0.1.2"

/* ============================================================
 * CLI Options
 * ============================================================ */

typedef struct {
    const char *input_file;
    const char *dict_path;
    const char *target;     /* "c" or "ts" */
    int do_lint;
    int do_format;
    int do_tmlanguage;
    int do_lex;
    int do_ast;
    int do_emit;
    unsigned int seed;
    int has_seed;
    int no_chaos;
} PeOptions;

static void print_banner(void) {
    fprintf(stderr,
        "Peut-\xC3\x8Atre v%s \xE2\x80\x94 On va essayer, on promet rien.\n",
        PE_VERSION);
}

static void print_usage(const char *prog) {
    print_banner();
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: %s [options] <fichier.pt>\n", prog);
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  --target c|ts            Langage cible (defaut: c)\n");
    fprintf(stderr, "  --dict CHEMIN            Chemin vers dictionnaire.json\n");
    fprintf(stderr, "  --lex                    Afficher les tokens (debug)\n");
    fprintf(stderr, "  --ast                    Afficher l'AST (debug)\n");
    fprintf(stderr, "  --emit                   Afficher le code genere\n");
    fprintf(stderr, "  --lint                   Lancer le linter feignant\n");
    fprintf(stderr, "  --format                 Lancer le formateur anarchiste\n");
    fprintf(stderr, "  --generate-tmLanguage    Generer la grammaire TextMate\n");
    fprintf(stderr, "  --seed N                 Graine du chaos (reproductible)\n");
    fprintf(stderr, "  --no-chaos               Desactiver le moteur de chaos\n");
    fprintf(stderr, "\nExemple:\n");
    fprintf(stderr, "  %s --lex examples/bonjour.pt\n", prog);
    fprintf(stderr, "\n");
}

static int parse_args(int argc, char *argv[], PeOptions *opts) {
    memset(opts, 0, sizeof(*opts));
    opts->target = "c";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--target") == 0) {
            if (++i >= argc) { fprintf(stderr, "Erreur: --target veut un argument.\n"); return -1; }
            opts->target = argv[i];
            if (strcmp(opts->target, "c") != 0 && strcmp(opts->target, "ts") != 0) {
                fprintf(stderr, "Erreur: --target accepte 'c' ou 'ts', pas '%s'.\n", opts->target);
                return -1;
            }
        } else if (strcmp(argv[i], "--dict") == 0) {
            if (++i >= argc) { fprintf(stderr, "Erreur: --dict veut un chemin.\n"); return -1; }
            opts->dict_path = argv[i];
        } else if (strcmp(argv[i], "--lint") == 0) {
            opts->do_lint = 1;
        } else if (strcmp(argv[i], "--format") == 0) {
            opts->do_format = 1;
        } else if (strcmp(argv[i], "--generate-tmLanguage") == 0) {
            opts->do_tmlanguage = 1;
        } else if (strcmp(argv[i], "--lex") == 0) {
            opts->do_lex = 1;
        } else if (strcmp(argv[i], "--ast") == 0) {
            opts->do_ast = 1;
        } else if (strcmp(argv[i], "--emit") == 0) {
            opts->do_emit = 1;
        } else if (strcmp(argv[i], "--seed") == 0) {
            if (++i >= argc) { fprintf(stderr, "Erreur: --seed veut un nombre.\n"); return -1; }
            opts->seed = (unsigned int)atoi(argv[i]);
            opts->has_seed = 1;
        } else if (strcmp(argv[i], "--no-chaos") == 0) {
            opts->no_chaos = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Option inconnue: %s. Essaie --help, si t'as le courage.\n", argv[i]);
            return -1;
        } else {
            if (opts->input_file) {
                fprintf(stderr, "Erreur: un seul fichier a la fois, on est pas des machines.\n");
                return -1;
            }
            opts->input_file = argv[i];
        }
    }
    return 0;
}

/* ============================================================
 * Dictionary path resolution
 * Priority: --dict flag > next to binary > current directory
 * ============================================================ */

static const char *resolve_dict_path(const char *argv0, const char *flag_path,
                                     char *buf, size_t buf_size) {
    FILE *f;

    /* 1. Explicit --dict flag */
    if (flag_path) {
        f = fopen(flag_path, "r");
        if (f) { fclose(f); return flag_path; }
        return flag_path;   /* let pe_dict_load report the error */
    }

    /* 2. Next to the binary */
    const char *last_slash = strrchr(argv0, '/');
    if (last_slash) {
        int dir_len = (int)(last_slash - argv0 + 1);
        snprintf(buf, buf_size, "%.*sdictionnaire.json", dir_len, argv0);
        f = fopen(buf, "r");
        if (f) { fclose(f); return buf; }
    }

    /* 3. Current working directory */
    snprintf(buf, buf_size, "dictionnaire.json");
    f = fopen(buf, "r");
    if (f) { fclose(f); return buf; }

    return buf;  /* let pe_dict_load report the error */
}

/* Resolve path to a runtime file (chaos_runtime.h or chaos_runtime.ts).
 * Looks in: runtime/ next to binary, then runtime/ in cwd. */
static void resolve_runtime_path(const char *argv0, const char *filename,
                                 char *buf, size_t buf_size) {
    FILE *f;
    const char *last_slash = strrchr(argv0, '/');

    if (last_slash) {
        int dir_len = (int)(last_slash - argv0 + 1);
        snprintf(buf, buf_size, "%.*sruntime/%s", dir_len, argv0, filename);
        f = fopen(buf, "r");
        if (f) { fclose(f); return; }
    }

    snprintf(buf, buf_size, "runtime/%s", filename);
    f = fopen(buf, "r");
    if (f) { fclose(f); return; }

    /* Fallback: just the filename */
    snprintf(buf, buf_size, "%s", filename);
}

/* ============================================================
 * --lint : The Lazy Linter
 * ============================================================ */

static void pe_lint(const char *source, const char *filename, PeDict *dict) {
    /* Count lines */
    int lines = 1;
    for (const char *p = source; *p; p++) {
        if (*p == '\n') lines++;
    }

    /* Check for chaos keywords */
    int has_chaos = 0;
    for (int i = 0; i < dict->count; i++) {
        TokenType t = dict->entries[i].type;
        if (t == TOK_MAYBE_TRUE  || t == TOK_MAYBE_FALSE ||
            t == TOK_HALF_TRUE   || t == TOK_FUZZY_EQ    ||
            t == TOK_AWAIT       || t == TOK_RETRY) {
            if (strstr(source, dict->entries[i].keyword)) {
                has_chaos = 1;
                break;
            }
        }
    }

    int warnings = 0;

    if (lines > 50) {
        printf("[LINT] %s:%d lignes \xE2\x80\x94 Warning: Code trop long. "
               "Flemme de lire.\n", filename, lines);
        warnings++;
    }

    if (!has_chaos) {
        printf("[LINT] %s \xE2\x80\x94 Warning: Code trop propre. "
               "Pas un seul truc chaotique ? C'est suspect.\n", filename);
        warnings++;
    }

    if (warnings == 0) {
        printf("[LINT] %s \xE2\x80\x94 Bon, \xC3\xA7""a a l'air suffisamment "
               "n'importe quoi. \xC3\x87""a passe.\n", filename);
    }
}

/* ============================================================
 * --format : The Anarchist Formatter
 * ============================================================ */

static void pe_format(const char *source) {
    const char *p = source;

    while (*p) {
        /* Find end of line */
        const char *eol = strchr(p, '\n');
        if (!eol) eol = p + strlen(p);

        /* Skip original leading whitespace to get content */
        const char *content = p;
        while (content < eol && (*content == ' ' || *content == '\t'))
            content++;
        int content_len = (int)(eol - content);

        /* Random indentation: 0-12 units, 33% chance of tabs */
        int indent = rand() % 13;
        int use_tabs = rand() % 3 == 0;

        for (int i = 0; i < indent; i++)
            putchar(use_tabs ? '\t' : ' ');

        /* Print content */
        printf("%.*s\n", content_len, content);

        p = (*eol) ? eol + 1 : eol;
    }

    printf("\n/* Voil\xC3\xA0, c'est 'artistique' maintenant. */\n");
}

/* ============================================================
 * --generate-tmLanguage : TextMate Grammar Generator
 * ============================================================ */

/* Map token types to TextMate scope categories. */
static const char *tm_scope_for(TokenType type) {
    switch (type) {
    case TOK_IF: case TOK_ELIF: case TOK_ELSE:
    case TOK_WHILE: case TOK_BREAK: case TOK_CONTINUE:
    case TOK_RETURN: case TOK_THROW:
    case TOK_TRY: case TOK_CATCH: case TOK_FINALLY:
    case TOK_AWAIT:
        return "keyword.control.pt";

    case TOK_VAR: case TOK_FUNC: case TOK_MAIN:
    case TOK_IMPORT: case TOK_EXPORT:
    case TOK_INTERFACE: case TOK_ASYNC:
        return "keyword.declaration.pt";

    case TOK_TRUE: case TOK_FALSE:
    case TOK_MAYBE_TRUE: case TOK_MAYBE_FALSE:
    case TOK_HALF_TRUE: case TOK_NULL:
        return "constant.language.pt";

    case TOK_STRING_TYPE: case TOK_NUMBER_TYPE:
    case TOK_ARRAY_TYPE: case TOK_ANY_TYPE:
        return "storage.type.pt";

    case TOK_FUZZY_EQ: case TOK_NOT_EQ:
    case TOK_LOGIC_AND: case TOK_LOGIC_OR:
    case TOK_GREATER: case TOK_LESS:
        return "keyword.operator.pt";

    case TOK_PRINT:
        return "support.function.pt";

    case TOK_CAST: case TOK_ASSERT:
    case TOK_RETRY: case TOK_DELETE:
        return "keyword.other.pt";

    default:
        return NULL;
    }
}

/* Write a JSON-escaped string (the content between quotes). */
static void json_write_escaped(FILE *out, const char *s) {
    for (; *s; s++) {
        if (*s == '"')       fprintf(out, "\\\"");
        else if (*s == '\\') fprintf(out, "\\\\");
        else                 fputc(*s, out);
    }
}

static void pe_generate_tmlanguage(PeDict *dict, FILE *out) {
    /* Collect unique scope names */
    const char *scopes[32];
    int scope_count = 0;

    for (int i = 0; i < dict->count; i++) {
        const char *scope = tm_scope_for(dict->entries[i].type);
        if (!scope) continue;
        int found = 0;
        for (int s = 0; s < scope_count; s++) {
            if (strcmp(scopes[s], scope) == 0) { found = 1; break; }
        }
        if (!found && scope_count < 32)
            scopes[scope_count++] = scope;
    }

    fprintf(out, "{\n");
    fprintf(out, "  \"name\": \"Peut-\xC3\x8Atre\",\n");
    fprintf(out, "  \"scopeName\": \"source.pt\",\n");
    fprintf(out, "  \"fileTypes\": [\"pt\"],\n");
    fprintf(out, "  \"patterns\": [\n");

    /* Keyword groups */
    for (int s = 0; s < scope_count; s++) {
        fprintf(out, "    {\n");
        fprintf(out, "      \"name\": \"");
        json_write_escaped(out, scopes[s]);
        fprintf(out, "\",\n");
        fprintf(out, "      \"match\": \"\\\\b(");

        int first = 1;
        for (int i = 0; i < dict->count; i++) {
            if (tm_scope_for(dict->entries[i].type) != NULL &&
                strcmp(tm_scope_for(dict->entries[i].type), scopes[s]) == 0) {
                if (!first) fputc('|', out);
                json_write_escaped(out, dict->entries[i].keyword);
                first = 0;
            }
        }

        fprintf(out, ")\\\\b\"\n");
        fprintf(out, "    },\n");
    }

    /* Comments */
    fprintf(out, "    {\n");
    fprintf(out, "      \"name\": \"comment.line.double-slash.pt\",\n");
    fprintf(out, "      \"match\": \"//.*$\"\n");
    fprintf(out, "    },\n");

    /* Strings */
    fprintf(out, "    {\n");
    fprintf(out, "      \"name\": \"string.quoted.double.pt\",\n");
    fprintf(out, "      \"begin\": \"\\\"\",\n");
    fprintf(out, "      \"end\": \"\\\"\"\n");
    fprintf(out, "    },\n");

    /* Numbers */
    fprintf(out, "    {\n");
    fprintf(out, "      \"name\": \"constant.numeric.pt\",\n");
    fprintf(out, "      \"match\": \"\\\\b[0-9]+(\\\\.[0-9]+)?\\\\b\"\n");
    fprintf(out, "    }\n");

    fprintf(out, "  ]\n");
    fprintf(out, "}\n");
}

/* ============================================================
 * Main
 * ============================================================ */

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    PeOptions opts;
    if (parse_args(argc, argv, &opts) < 0) {
        return 1;
    }

    /* Seed randomness */
    if (opts.has_seed)
        srand(opts.seed);
    else
        srand((unsigned int)time(NULL));

    /* Resolve dictionary path */
    char dict_buf[1024];
    const char *dict_path = resolve_dict_path(
        argv[0], opts.dict_path, dict_buf, sizeof(dict_buf));

    /* Load dictionary */
    PeDict dict;
    if (pe_dict_load(&dict, dict_path) < 0) {
        fprintf(stderr, "Erreur: impossible de charger le dictionnaire '%s'.\n"
                        "Tu l'as bien mis quelque part ?\n", dict_path);
        return 1;
    }

    /* --generate-tmLanguage (no input file needed) */
    if (opts.do_tmlanguage) {
        pe_generate_tmlanguage(&dict, stdout);
        pe_dict_free(&dict);
        return 0;
    }

    /* All other modes need an input file */
    if (!opts.input_file) {
        fprintf(stderr, "Erreur: aucun fichier .pt specifie.\n");
        print_usage(argv[0]);
        pe_dict_free(&dict);
        return 1;
    }

    /* Read source */
    char *source = pe_read_file(opts.input_file);
    if (!source) {
        fprintf(stderr, "Erreur: impossible de lire '%s'.\n"
                        "T'es sur que ce fichier existe ?\n", opts.input_file);
        pe_dict_free(&dict);
        return 1;
    }

    /* --lint */
    if (opts.do_lint) {
        pe_lint(source, opts.input_file, &dict);
        free(source);
        pe_dict_free(&dict);
        return 0;
    }

    /* --format */
    if (opts.do_format) {
        pe_format(source);
        free(source);
        pe_dict_free(&dict);
        return 0;
    }

    /* --lex (debug: print token stream) */
    if (opts.do_lex) {
        Lexer lex;
        lexer_init(&lex, source, opts.input_file, &dict);
        Token tok;
        printf("%-6s %-4s  %-15s  %s\n", "LINE", "COL", "TYPE", "TEXT");
        printf("%-6s %-4s  %-15s  %s\n", "----", "---", "----", "----");
        do {
            tok = lexer_next(&lex);
            printf("%-6d %-4d  %-15s  '%.*s'\n",
                   tok.line, tok.col,
                   token_type_name(tok.type),
                   tok.length, tok.start);
        } while (tok.type != TOK_EOF && tok.type != TOK_ERROR);
        free(source);
        pe_dict_free(&dict);
        return 0;
    }

    /* --ast (debug: print AST) */
    if (opts.do_ast) {
        Lexer lex;
        lexer_init(&lex, source, opts.input_file, &dict);
        AstNode *program = parse(&lex);
        if (program) {
            ast_dump(program, 0);
            ast_free(program);
        } else {
            fprintf(stderr, "Bon, le parsing a plante. Etonnant.\n");
        }
        free(source);
        pe_dict_free(&dict);
        return program ? 0 : 1;
    }

    /* ============================================================
     * Full transpilation pipeline: lex -> parse -> codegen -> compile -> run
     * ============================================================ */

    /* Parse */
    Lexer lex;
    lexer_init(&lex, source, opts.input_file, &dict);
    AstNode *program = parse(&lex);
    if (!program) {
        fprintf(stderr, "Bon, le parsing a plant\xC3\xA9. \xC3\x89tonnant.\n");
        free(source);
        pe_dict_free(&dict);
        return 1;
    }

    /* Load the appropriate runtime */
    char runtime_path_buf[1024];
    const char *rt_filename = (strcmp(opts.target, "ts") == 0)
                              ? "chaos_runtime.ts" : "chaos_runtime.h";
    resolve_runtime_path(argv[0], rt_filename, runtime_path_buf,
                         sizeof(runtime_path_buf));
    char *runtime_src = pe_read_file(runtime_path_buf);
    if (!runtime_src) {
        fprintf(stderr, "Erreur: impossible de charger le runtime '%s'.\n",
                runtime_path_buf);
        ast_free(program);
        free(source);
        pe_dict_free(&dict);
        return 1;
    }

    /* --emit: just print the generated code */
    if (opts.do_emit) {
        if (strcmp(opts.target, "ts") == 0)
            codegen_ts_emit(stdout, program, runtime_src, opts.no_chaos,
                            opts.seed, opts.has_seed);
        else
            codegen_c_emit(stdout, program, runtime_src, opts.no_chaos,
                           opts.seed, opts.has_seed);
        free(runtime_src);
        ast_free(program);
        free(source);
        pe_dict_free(&dict);
        return 0;
    }

    /* Generate to temp file, compile, and run */
    print_banner();

    int is_ts = (strcmp(opts.target, "ts") == 0);
    char tmp_src[256], tmp_bin[256];
    pid_t pid = getpid();

    if (is_ts) {
        snprintf(tmp_src, sizeof(tmp_src), "/tmp/pe_%d.ts", pid);
    } else {
        snprintf(tmp_src, sizeof(tmp_src), "/tmp/pe_%d.c", pid);
        snprintf(tmp_bin, sizeof(tmp_bin), "/tmp/pe_%d", pid);
    }

    FILE *tmp = fopen(tmp_src, "w");
    if (!tmp) {
        fprintf(stderr, "Erreur: impossible de cr\xC3\xA9""er '%s'.\n", tmp_src);
        free(runtime_src);
        ast_free(program);
        free(source);
        pe_dict_free(&dict);
        return 1;
    }

    if (is_ts)
        codegen_ts_emit(tmp, program, runtime_src, opts.no_chaos,
                        opts.seed, opts.has_seed);
    else
        codegen_c_emit(tmp, program, runtime_src, opts.no_chaos,
                       opts.seed, opts.has_seed);
    fclose(tmp);

    int rc;
    if (is_ts) {
        /* TypeScript: run with npx ts-node */
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "npx ts-node \"%s\"", tmp_src);
        rc = system(cmd);
    } else {
        /* C: compile with gcc, then run */
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
                 "gcc -std=c11 -O1 -w -o \"%s\" \"%s\" -lm", tmp_bin, tmp_src);
        rc = system(cmd);
        if (rc != 0) {
            fprintf(stderr, "Bon, \xC3\xA7""a compile pas. \xC3\x89tonnant.\n");
        } else {
            rc = system(tmp_bin);
        }
        remove(tmp_bin);
    }
    remove(tmp_src);

    free(runtime_src);
    ast_free(program);
    free(source);
    pe_dict_free(&dict);
    return WIFEXITED(rc) ? WEXITSTATUS(rc) : 1;
}
