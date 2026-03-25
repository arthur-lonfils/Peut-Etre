#ifndef PE_PARSER_H
#define PE_PARSER_H

#include "lexer.h"

/* ============================================================
 * AST Node Types
 * ============================================================ */

typedef enum {
    NODE_PROGRAM,       /* root: list of top-level statements/functions */

    /* Statements */
    NODE_VAR_DECL,      /* genre x = expr; */
    NODE_ASSIGN,        /* x = expr; */
    NODE_IF,            /* si_ca_te_tente / sur_un_malentendu / sinon_tant_pis */
    NODE_WHILE,         /* jusqu_a_ce_qu_on_en_ait_marre */
    NODE_BREAK,         /* c_est_bon_laisse_tomber; */
    NODE_CONTINUE,      /* flemme_passe_au_suivant; */
    NODE_RETURN,        /* on_verra_bien expr; */
    NODE_THROW,         /* je_me_casse expr; */
    NODE_PRINT,         /* lache_un_com(expr); */
    NODE_EXPR_STMT,     /* expression as statement (e.g., function call) */
    NODE_BLOCK,         /* { ... } */
    NODE_FUNC_DEF,      /* petit_truc_qui_fait name(params) { body } */

    /* Expressions */
    NODE_INT_LIT,
    NODE_FLOAT_LIT,
    NODE_STRING_LIT,
    NODE_BOOL_LIT,      /* stores probability: 100, 75, 50, 25, 0 */
    NODE_NULL_LIT,
    NODE_IDENT,
    NODE_BINARY_OP,     /* left op right */
    NODE_UNARY_OP,      /* op operand */
    NODE_CALL,          /* func_name(args) */
} NodeType;

/* Forward declaration */
typedef struct AstNode AstNode;

/* Dynamic list of child nodes */
typedef struct {
    AstNode **items;
    int       count;
    int       capacity;
} NodeList;

void nodelist_init(NodeList *list);
void nodelist_push(NodeList *list, AstNode *node);
void nodelist_free(NodeList *list);

/* ============================================================
 * AST Node (tagged union)
 * ============================================================ */

struct AstNode {
    NodeType type;
    int      line;

    union {
        /* NODE_PROGRAM, NODE_BLOCK */
        struct { NodeList stmts; } block;

        /* NODE_VAR_DECL */
        struct {
            char    *name;
            AstNode *init;       /* NULL if no initializer */
        } var_decl;

        /* NODE_ASSIGN */
        struct {
            char    *name;
            AstNode *value;
        } assign;

        /* NODE_IF */
        struct {
            AstNode  *cond;
            AstNode  *then_body;       /* NODE_BLOCK */
            NodeList  elif_conds;      /* parallel with elif_bodies */
            NodeList  elif_bodies;
            AstNode  *else_body;       /* NODE_BLOCK or NULL */
        } if_stmt;

        /* NODE_WHILE */
        struct {
            AstNode *cond;
            AstNode *body;             /* NODE_BLOCK */
        } while_stmt;

        /* NODE_RETURN, NODE_THROW, NODE_PRINT, NODE_EXPR_STMT */
        struct { AstNode *expr; } single;   /* expr may be NULL for bare return */

        /* NODE_FUNC_DEF */
        struct {
            char    *name;
            char   **params;
            int      param_count;
            AstNode *body;             /* NODE_BLOCK */
        } func_def;

        /* NODE_INT_LIT */
        struct { long value; } int_lit;

        /* NODE_FLOAT_LIT */
        struct { double value; } float_lit;

        /* NODE_STRING_LIT */
        struct { char *value; } string_lit;

        /* NODE_BOOL_LIT */
        struct { int probability; } bool_lit;   /* 0, 25, 50, 75, 100 */

        /* NODE_IDENT */
        struct { char *name; } ident;

        /* NODE_BINARY_OP */
        struct {
            TokenType  op;
            AstNode   *left;
            AstNode   *right;
        } binary;

        /* NODE_UNARY_OP */
        struct {
            TokenType  op;
            AstNode   *operand;
        } unary;

        /* NODE_CALL */
        struct {
            char     *func_name;
            NodeList  args;
        } call;
    } as;
};

/* ============================================================
 * Parser API
 * ============================================================ */

/* Parse a token stream into an AST. Returns NULL on fatal error. */
AstNode *parse(Lexer *lexer);

/* Free an AST tree recursively. */
void ast_free(AstNode *node);

/* Print an AST tree for debugging. */
void ast_dump(AstNode *node, int indent);

#endif
