#ifndef PE_LEXER_H
#define PE_LEXER_H

/* ============================================================
 * Token Types — semantic, not lexical.
 * The French keyword that maps to each type is loaded at runtime
 * from dictionnaire.json.
 * ============================================================ */

typedef enum {
    /* Literals */
    TOK_INT_LIT,
    TOK_FLOAT_LIT,
    TOK_STRING_LIT,

    /* Identifiers */
    TOK_IDENT,

    /* Variable declaration */
    TOK_VAR,            /* genre, en_gros */

    /* Control flow */
    TOK_IF,             /* si_ca_te_tente */
    TOK_ELIF,           /* sur_un_malentendu */
    TOK_ELSE,           /* sinon_tant_pis */
    TOK_WHILE,          /* jusqu_a_ce_qu_on_en_ait_marre */
    TOK_BREAK,          /* c_est_bon_laisse_tomber */
    TOK_CONTINUE,       /* flemme_passe_au_suivant */

    /* Functions */
    TOK_FUNC,           /* petit_truc_qui_fait */
    TOK_RETURN,         /* on_verra_bien */

    /* Builtins */
    TOK_PRINT,          /* lache_un_com */
    TOK_MAIN,           /* bon_on_y_va */
    TOK_IMPORT,         /* ramene_le_pote */
    TOK_THROW,          /* je_me_casse */

    /* Booleans & null */
    TOK_TRUE,           /* carrement (100%) */
    TOK_FALSE,          /* mort (0%) */
    TOK_MAYBE_TRUE,     /* ouais_vite_fait (75%) */
    TOK_MAYBE_FALSE,    /* bof (25%) */
    TOK_NULL,           /* nada */
    TOK_HALF_TRUE,      /* c_est_pas_faux (50%) */

    /* Type keywords */
    TOK_STRING_TYPE,    /* blabla */
    TOK_NUMBER_TYPE,    /* un_chiffre */
    TOK_ARRAY_TYPE,     /* un_tas_de_trucs */
    TOK_ANY_TYPE,       /* n_importe_quoi */

    /* Keyword-based operators */
    TOK_FUZZY_EQ,       /* kif_kif (== with 5% fuzzy) */
    TOK_NOT_EQ,         /* pas_trop_pareil */
    TOK_LOGIC_AND,      /* et_vas_y_aussi */
    TOK_LOGIC_OR,       /* ou_alors_bref */
    TOK_GREATER,        /* plus_gros_que */
    TOK_LESS,           /* moins_ouf_que */

    /* TS-specific keywords */
    TOK_ASYNC,          /* quand_j_aurai_le_temps */
    TOK_AWAIT,          /* pause_cafe */
    TOK_INTERFACE,      /* en_theorie */
    TOK_EXPORT,         /* tiens_cadeau */
    TOK_TRY,            /* on_tente_un_truc */
    TOK_CATCH,          /* oups_my_bad */
    TOK_FINALLY,        /* bref_on_avance */

    /* Bonus keywords */
    TOK_CAST,           /* fais_genre */
    TOK_ASSERT,         /* j_te_jure */
    TOK_RETRY,          /* rooh_allez */
    TOK_DELETE,         /* laisse_beton */

    /* Symbol operators */
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_ASSIGN,         /* = */
    TOK_EQ,             /* == */
    TOK_NEQ,            /* != */
    TOK_LT,             /* < */
    TOK_GT,             /* > */
    TOK_LTE,            /* <= */
    TOK_GTE,            /* >= */
    TOK_AND,            /* && */
    TOK_OR,             /* || */
    TOK_NOT,            /* ! */

    /* Delimiters */
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_SEMICOLON,
    TOK_COMMA,
    TOK_DOT,

    /* Special */
    TOK_EOF,
    TOK_ERROR,

    TOK_COUNT
} TokenType;

/* ============================================================
 * Token
 * ============================================================ */

typedef struct {
    TokenType   type;
    const char *start;   /* pointer into source buffer */
    int         length;
    int         line;
    int         col;
} Token;

/* ============================================================
 * Dynamic Dictionary — loaded from dictionnaire.json
 * ============================================================ */

typedef struct {
    char     *keyword;   /* French keyword (heap-allocated) */
    TokenType type;
} PeDictEntry;

typedef struct {
    PeDictEntry *entries;
    int          count;
    int          capacity;
} PeDict;

/* Load / free the keyword dictionary. Returns 0 on success, -1 on error. */
int  pe_dict_load(PeDict *dict, const char *json_path);
void pe_dict_free(PeDict *dict);

/* ============================================================
 * Lexer
 * ============================================================ */

typedef struct {
    const char *source;
    const char *current;
    int         line;
    int         col;
    const char *filename;
    PeDict     *dict;
} Lexer;

void  lexer_init(Lexer *lex, const char *source, const char *filename,
                 PeDict *dict);
Token lexer_next(Lexer *lex);

/* Human-readable name for a token type. */
const char *token_type_name(TokenType type);

#endif
