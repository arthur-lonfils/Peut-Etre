#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * NodeList helpers
 * ============================================================ */

void nodelist_init(NodeList *list) {
    list->items    = NULL;
    list->count    = 0;
    list->capacity = 0;
}

void nodelist_push(NodeList *list, AstNode *node) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity ? list->capacity * 2 : 8;
        list->items = realloc(list->items,
                              (size_t)list->capacity * sizeof(AstNode *));
    }
    list->items[list->count++] = node;
}

void nodelist_free(NodeList *list) {
    free(list->items);
    list->items    = NULL;
    list->count    = 0;
    list->capacity = 0;
}

/* ============================================================
 * Parser State
 * ============================================================ */

typedef struct {
    Lexer *lexer;
    Token  current;
    Token  previous;
    int    had_error;
    int    panic_mode;
} Parser;

static void parser_init(Parser *p, Lexer *lexer) {
    p->lexer      = lexer;
    p->had_error  = 0;
    p->panic_mode = 0;
    p->current    = lexer_next(lexer);
    p->previous   = p->current;
}

/* ============================================================
 * Error reporting & recovery
 * ============================================================ */

static void error_at(Parser *p, Token *tok, const char *msg) {
    if (p->panic_mode) return;
    p->panic_mode = 1;
    p->had_error  = 1;

    fprintf(stderr, "%s:%d:%d: Erreur",
            p->lexer->filename, tok->line, tok->col);

    if (tok->type == TOK_EOF) {
        fprintf(stderr, " a la fin du fichier");
    } else if (tok->type != TOK_ERROR) {
        fprintf(stderr, " pres de '%.*s'", tok->length, tok->start);
    }

    fprintf(stderr, " \xE2\x80\x94 %s\n", msg);
}

static void error_current(Parser *p, const char *msg) {
    error_at(p, &p->current, msg);
}

/* Synchronize: skip tokens until we find a statement boundary. */
static void synchronize(Parser *p) {
    p->panic_mode = 0;

    while (p->current.type != TOK_EOF) {
        /* A semicolon ends a statement — resume after it. */
        if (p->previous.type == TOK_SEMICOLON) return;

        /* These tokens start new statements. */
        switch (p->current.type) {
        case TOK_VAR:
        case TOK_IF:
        case TOK_WHILE:
        case TOK_FUNC:
        case TOK_RETURN:
        case TOK_PRINT:
        case TOK_BREAK:
        case TOK_CONTINUE:
        case TOK_THROW:
            return;
        default:
            break;
        }

        p->previous = p->current;
        p->current  = lexer_next(p->lexer);
    }
}

/* ============================================================
 * Token consumption
 * ============================================================ */

static Token advance(Parser *p) {
    p->previous = p->current;
    p->current  = lexer_next(p->lexer);
    return p->previous;
}

static int check(Parser *p, TokenType type) {
    return p->current.type == type;
}

static int match(Parser *p, TokenType type) {
    if (!check(p, type)) return 0;
    advance(p);
    return 1;
}

static void consume(Parser *p, TokenType type, const char *msg) {
    if (check(p, type)) { advance(p); return; }
    error_current(p, msg);
}

/* ============================================================
 * AST node constructors
 * ============================================================ */

static AstNode *make_node(NodeType type, int line) {
    AstNode *n = calloc(1, sizeof(AstNode));
    n->type = type;
    n->line = line;
    return n;
}

static char *copy_token_text(Token *tok) {
    char *s = malloc((size_t)tok->length + 1);
    memcpy(s, tok->start, (size_t)tok->length);
    s[tok->length] = '\0';
    return s;
}

/* Copy a string literal token, stripping the surrounding quotes. */
static char *copy_string_literal(Token *tok) {
    /* tok->start includes the quotes: "hello" */
    int inner_len = tok->length - 2;
    if (inner_len < 0) inner_len = 0;
    char *s = malloc((size_t)inner_len + 1);
    memcpy(s, tok->start + 1, (size_t)inner_len);
    s[inner_len] = '\0';
    return s;
}

/* ============================================================
 * Expression Parsing (Pratt / Precedence Climbing)
 * ============================================================ */

typedef enum {
    PREC_NONE,
    PREC_OR,            /* || ou_alors_bref */
    PREC_AND,           /* && et_vas_y_aussi */
    PREC_EQUALITY,      /* == != kif_kif pas_trop_pareil */
    PREC_COMPARISON,    /* < > <= >= plus_gros_que moins_ouf_que */
    PREC_TERM,          /* + - */
    PREC_FACTOR,        /* * / % */
    PREC_UNARY,         /* ! - */
    PREC_CALL,          /* () */
    PREC_PRIMARY,
} Precedence;

/* Forward declarations */
static AstNode *parse_expression(Parser *p);
static AstNode *parse_precedence(Parser *p, Precedence min_prec);
static AstNode *parse_primary(Parser *p);
static AstNode *parse_block(Parser *p);
static AstNode *parse_statement(Parser *p);
static AstNode *parse_declaration(Parser *p);

/* Get the precedence of a binary operator token. */
static Precedence get_precedence(TokenType type) {
    switch (type) {
    case TOK_OR:  case TOK_LOGIC_OR:              return PREC_OR;
    case TOK_AND: case TOK_LOGIC_AND:             return PREC_AND;
    case TOK_EQ:  case TOK_NEQ: case TOK_FUZZY_EQ:
    case TOK_NOT_EQ:                               return PREC_EQUALITY;
    case TOK_LT:  case TOK_GT:  case TOK_LTE:
    case TOK_GTE: case TOK_GREATER: case TOK_LESS: return PREC_COMPARISON;
    case TOK_PLUS:  case TOK_MINUS:                return PREC_TERM;
    case TOK_STAR:  case TOK_SLASH: case TOK_PERCENT: return PREC_FACTOR;
    default:                                       return PREC_NONE;
    }
}

static AstNode *parse_precedence(Parser *p, Precedence min_prec) {
    AstNode *left = parse_primary(p);
    if (!left) return NULL;

    while (1) {
        Precedence prec = get_precedence(p->current.type);
        if (prec < min_prec) break;

        Token op_tok = advance(p);
        /* Right-associate: use prec; left-associate: use prec + 1 */
        AstNode *right = parse_precedence(p, (Precedence)(prec + 1));
        if (!right) { ast_free(left); return NULL; }

        AstNode *bin = make_node(NODE_BINARY_OP, op_tok.line);
        bin->as.binary.op    = op_tok.type;
        bin->as.binary.left  = left;
        bin->as.binary.right = right;
        left = bin;
    }

    return left;
}

static AstNode *parse_expression(Parser *p) {
    return parse_precedence(p, PREC_OR);
}

/* ============================================================
 * Primary expressions
 * ============================================================ */

static AstNode *parse_primary(Parser *p) {
    Token tok = p->current;

    /* Unary operators: ! and - */
    if (check(p, TOK_NOT) || check(p, TOK_MINUS)) {
        advance(p);
        AstNode *operand = parse_precedence(p, PREC_UNARY);
        if (!operand) return NULL;
        AstNode *node = make_node(NODE_UNARY_OP, tok.line);
        node->as.unary.op      = tok.type;
        node->as.unary.operand = operand;
        return node;
    }

    /* Integer literal */
    if (check(p, TOK_INT_LIT)) {
        advance(p);
        AstNode *node = make_node(NODE_INT_LIT, tok.line);
        char *text = copy_token_text(&tok);
        node->as.int_lit.value = strtol(text, NULL, 10);
        free(text);
        return node;
    }

    /* Float literal */
    if (check(p, TOK_FLOAT_LIT)) {
        advance(p);
        AstNode *node = make_node(NODE_FLOAT_LIT, tok.line);
        char *text = copy_token_text(&tok);
        node->as.float_lit.value = strtod(text, NULL);
        free(text);
        return node;
    }

    /* String literal */
    if (check(p, TOK_STRING_LIT)) {
        advance(p);
        AstNode *node = make_node(NODE_STRING_LIT, tok.line);
        node->as.string_lit.value = copy_string_literal(&tok);
        return node;
    }

    /* Boolean literals */
    if (check(p, TOK_TRUE)) {
        advance(p);
        AstNode *node = make_node(NODE_BOOL_LIT, tok.line);
        node->as.bool_lit.probability = 100;
        return node;
    }
    if (check(p, TOK_FALSE)) {
        advance(p);
        AstNode *node = make_node(NODE_BOOL_LIT, tok.line);
        node->as.bool_lit.probability = 0;
        return node;
    }
    if (check(p, TOK_MAYBE_TRUE)) {
        advance(p);
        AstNode *node = make_node(NODE_BOOL_LIT, tok.line);
        node->as.bool_lit.probability = 75;
        return node;
    }
    if (check(p, TOK_MAYBE_FALSE)) {
        advance(p);
        AstNode *node = make_node(NODE_BOOL_LIT, tok.line);
        node->as.bool_lit.probability = 25;
        return node;
    }
    if (check(p, TOK_HALF_TRUE)) {
        advance(p);
        AstNode *node = make_node(NODE_BOOL_LIT, tok.line);
        node->as.bool_lit.probability = 50;
        return node;
    }

    /* Null literal */
    if (check(p, TOK_NULL)) {
        advance(p);
        return make_node(NODE_NULL_LIT, tok.line);
    }

    /* Identifier or function call */
    if (check(p, TOK_IDENT)) {
        advance(p);
        char *name = copy_token_text(&tok);

        /* Check for function call: ident( */
        if (check(p, TOK_LPAREN)) {
            advance(p);   /* consume ( */
            AstNode *node = make_node(NODE_CALL, tok.line);
            node->as.call.func_name = name;
            nodelist_init(&node->as.call.args);

            if (!check(p, TOK_RPAREN)) {
                do {
                    AstNode *arg = parse_expression(p);
                    if (arg) nodelist_push(&node->as.call.args, arg);
                } while (match(p, TOK_COMMA));
            }

            consume(p, TOK_RPAREN, "')' attendu apres les arguments.");
            return node;
        }

        AstNode *node = make_node(NODE_IDENT, tok.line);
        node->as.ident.name = name;
        return node;
    }

    /* Parenthesized expression */
    if (check(p, TOK_LPAREN)) {
        advance(p);
        AstNode *expr = parse_expression(p);
        consume(p, TOK_RPAREN, "')' attendu.");
        return expr;
    }

    error_current(p, "expression attendue.");
    return NULL;
}

/* ============================================================
 * Statement Parsing
 * ============================================================ */

/* Parse a brace-enclosed block: { stmts... } */
static AstNode *parse_block(Parser *p) {
    consume(p, TOK_LBRACE, "'{' attendu.");

    AstNode *block = make_node(NODE_BLOCK, p->previous.line);
    nodelist_init(&block->as.block.stmts);

    while (!check(p, TOK_RBRACE) && !check(p, TOK_EOF)) {
        AstNode *stmt = parse_declaration(p);
        if (stmt) nodelist_push(&block->as.block.stmts, stmt);
        if (p->panic_mode) synchronize(p);
    }

    consume(p, TOK_RBRACE, "'}' attendu pour fermer le bloc.");
    return block;
}

/* genre x = expr; */
static AstNode *parse_var_decl(Parser *p) {
    int line = p->previous.line;  /* the 'genre' / 'en_gros' token */

    consume(p, TOK_IDENT, "nom de variable attendu apres 'genre'.");
    char *name = copy_token_text(&p->previous);

    AstNode *init = NULL;
    if (match(p, TOK_ASSIGN)) {
        init = parse_expression(p);
    }

    consume(p, TOK_SEMICOLON, "';' attendu apres la declaration.");

    AstNode *node = make_node(NODE_VAR_DECL, line);
    node->as.var_decl.name = name;
    node->as.var_decl.init = init;
    return node;
}

/* si_ca_te_tente (cond) { body } sur_un_malentendu ... sinon_tant_pis ... */
static AstNode *parse_if(Parser *p) {
    int line = p->previous.line;

    consume(p, TOK_LPAREN, "'(' attendu apres 'si_ca_te_tente'.");
    AstNode *cond = parse_expression(p);
    consume(p, TOK_RPAREN, "')' attendu apres la condition.");

    AstNode *then_body = parse_block(p);

    AstNode *node = make_node(NODE_IF, line);
    node->as.if_stmt.cond      = cond;
    node->as.if_stmt.then_body = then_body;
    nodelist_init(&node->as.if_stmt.elif_conds);
    nodelist_init(&node->as.if_stmt.elif_bodies);
    node->as.if_stmt.else_body = NULL;

    /* sur_un_malentendu (else-if) chains */
    while (match(p, TOK_ELIF)) {
        consume(p, TOK_LPAREN, "'(' attendu apres 'sur_un_malentendu'.");
        AstNode *elif_cond = parse_expression(p);
        consume(p, TOK_RPAREN, "')' attendu.");

        AstNode *elif_body = parse_block(p);
        nodelist_push(&node->as.if_stmt.elif_conds, elif_cond);
        nodelist_push(&node->as.if_stmt.elif_bodies, elif_body);
    }

    /* sinon_tant_pis (else) */
    if (match(p, TOK_ELSE)) {
        node->as.if_stmt.else_body = parse_block(p);
    }

    return node;
}

/* jusqu_a_ce_qu_on_en_ait_marre (cond) { body } */
static AstNode *parse_while(Parser *p) {
    int line = p->previous.line;

    consume(p, TOK_LPAREN,
            "'(' attendu apres 'jusqu_a_ce_qu_on_en_ait_marre'.");
    AstNode *cond = parse_expression(p);
    consume(p, TOK_RPAREN, "')' attendu.");

    AstNode *body = parse_block(p);

    AstNode *node = make_node(NODE_WHILE, line);
    node->as.while_stmt.cond = cond;
    node->as.while_stmt.body = body;
    return node;
}

/* petit_truc_qui_fait name(params) { body } */
static AstNode *parse_func_def(Parser *p) {
    int line = p->previous.line;

    consume(p, TOK_IDENT, "nom de fonction attendu.");
    char *name = copy_token_text(&p->previous);

    consume(p, TOK_LPAREN, "'(' attendu apres le nom de la fonction.");

    /* Parse parameter list */
    char **params = NULL;
    int param_count = 0;
    int param_cap = 0;

    if (!check(p, TOK_RPAREN)) {
        do {
            consume(p, TOK_IDENT, "nom de parametre attendu.");
            if (param_count >= param_cap) {
                param_cap = param_cap ? param_cap * 2 : 4;
                params = realloc(params, (size_t)param_cap * sizeof(char *));
            }
            params[param_count++] = copy_token_text(&p->previous);
        } while (match(p, TOK_COMMA));
    }

    consume(p, TOK_RPAREN, "')' attendu apres les parametres.");

    AstNode *body = parse_block(p);

    AstNode *node = make_node(NODE_FUNC_DEF, line);
    node->as.func_def.name        = name;
    node->as.func_def.params      = params;
    node->as.func_def.param_count = param_count;
    node->as.func_def.body        = body;
    return node;
}

/* lache_un_com(expr); */
static AstNode *parse_print(Parser *p) {
    int line = p->previous.line;

    consume(p, TOK_LPAREN, "'(' attendu apres 'lache_un_com'.");
    AstNode *expr = parse_expression(p);
    consume(p, TOK_RPAREN, "')' attendu.");
    consume(p, TOK_SEMICOLON, "';' attendu.");

    AstNode *node = make_node(NODE_PRINT, line);
    node->as.single.expr = expr;
    return node;
}

/* on_verra_bien expr; */
static AstNode *parse_return(Parser *p) {
    int line = p->previous.line;

    AstNode *expr = NULL;
    if (!check(p, TOK_SEMICOLON))
        expr = parse_expression(p);

    consume(p, TOK_SEMICOLON, "';' attendu apres 'on_verra_bien'.");

    AstNode *node = make_node(NODE_RETURN, line);
    node->as.single.expr = expr;
    return node;
}

/* je_me_casse expr; */
static AstNode *parse_throw(Parser *p) {
    int line = p->previous.line;

    AstNode *expr = NULL;
    if (!check(p, TOK_SEMICOLON))
        expr = parse_expression(p);

    consume(p, TOK_SEMICOLON, "';' attendu apres 'je_me_casse'.");

    AstNode *node = make_node(NODE_THROW, line);
    node->as.single.expr = expr;
    return node;
}

static AstNode *parse_statement(Parser *p) {
    if (match(p, TOK_IF))       return parse_if(p);
    if (match(p, TOK_WHILE))    return parse_while(p);
    if (match(p, TOK_PRINT))    return parse_print(p);
    if (match(p, TOK_RETURN))   return parse_return(p);
    if (match(p, TOK_THROW))    return parse_throw(p);

    if (match(p, TOK_BREAK)) {
        int line = p->previous.line;
        consume(p, TOK_SEMICOLON, "';' attendu apres 'c_est_bon_laisse_tomber'.");
        return make_node(NODE_BREAK, line);
    }

    if (match(p, TOK_CONTINUE)) {
        int line = p->previous.line;
        consume(p, TOK_SEMICOLON, "';' attendu apres 'flemme_passe_au_suivant'.");
        return make_node(NODE_CONTINUE, line);
    }

    /* Assignment: ident = expr;  or expression statement */
    if (check(p, TOK_IDENT)) {
        Token ident = p->current;
        advance(p);

        /* Assignment */
        if (match(p, TOK_ASSIGN)) {
            char *name = copy_token_text(&ident);
            AstNode *value = parse_expression(p);
            consume(p, TOK_SEMICOLON, "';' attendu apres l'assignation.");

            AstNode *node = make_node(NODE_ASSIGN, ident.line);
            node->as.assign.name  = name;
            node->as.assign.value = value;
            return node;
        }

        /* Function call as statement: ident(args); */
        if (check(p, TOK_LPAREN)) {
            advance(p);
            char *name = copy_token_text(&ident);
            AstNode *call = make_node(NODE_CALL, ident.line);
            call->as.call.func_name = name;
            nodelist_init(&call->as.call.args);

            if (!check(p, TOK_RPAREN)) {
                do {
                    AstNode *arg = parse_expression(p);
                    if (arg) nodelist_push(&call->as.call.args, arg);
                } while (match(p, TOK_COMMA));
            }
            consume(p, TOK_RPAREN, "')' attendu.");
            consume(p, TOK_SEMICOLON, "';' attendu.");

            AstNode *node = make_node(NODE_EXPR_STMT, ident.line);
            node->as.single.expr = call;
            return node;
        }

        /* General expression statement starting with identifier —
           back up and re-parse as expression.
           We already consumed the ident, so we construct it manually
           and let binary parsing continue. */
        {
            char *name = copy_token_text(&ident);
            AstNode *left = make_node(NODE_IDENT, ident.line);
            left->as.ident.name = name;

            /* If there's a binary operator, continue parsing */
            Precedence prec = get_precedence(p->current.type);
            while (prec >= PREC_OR) {
                Token op_tok = advance(p);
                AstNode *right = parse_precedence(p, (Precedence)(prec + 1));
                AstNode *bin = make_node(NODE_BINARY_OP, op_tok.line);
                bin->as.binary.op    = op_tok.type;
                bin->as.binary.left  = left;
                bin->as.binary.right = right;
                left = bin;
                prec = get_precedence(p->current.type);
            }

            consume(p, TOK_SEMICOLON, "';' attendu.");
            AstNode *node = make_node(NODE_EXPR_STMT, ident.line);
            node->as.single.expr = left;
            return node;
        }
    }

    /* Bare expression statement (rare but possible) */
    {
        AstNode *expr = parse_expression(p);
        if (!expr) return NULL;
        consume(p, TOK_SEMICOLON, "';' attendu.");
        AstNode *node = make_node(NODE_EXPR_STMT, expr->line);
        node->as.single.expr = expr;
        return node;
    }
}

static AstNode *parse_declaration(Parser *p) {
    if (match(p, TOK_VAR))  return parse_var_decl(p);
    if (match(p, TOK_FUNC)) return parse_func_def(p);
    return parse_statement(p);
}

/* ============================================================
 * Public API: parse
 * ============================================================ */

AstNode *parse(Lexer *lexer) {
    Parser p;
    parser_init(&p, lexer);

    AstNode *program = make_node(NODE_PROGRAM, 1);
    nodelist_init(&program->as.block.stmts);

    while (!check(&p, TOK_EOF)) {
        AstNode *decl = parse_declaration(&p);
        if (decl) nodelist_push(&program->as.block.stmts, decl);
        if (p.panic_mode) synchronize(&p);
    }

    if (p.had_error) {
        ast_free(program);
        return NULL;
    }

    return program;
}

/* ============================================================
 * AST cleanup
 * ============================================================ */

void ast_free(AstNode *node) {
    if (!node) return;

    switch (node->type) {
    case NODE_PROGRAM:
    case NODE_BLOCK:
        for (int i = 0; i < node->as.block.stmts.count; i++)
            ast_free(node->as.block.stmts.items[i]);
        nodelist_free(&node->as.block.stmts);
        break;

    case NODE_VAR_DECL:
        free(node->as.var_decl.name);
        ast_free(node->as.var_decl.init);
        break;

    case NODE_ASSIGN:
        free(node->as.assign.name);
        ast_free(node->as.assign.value);
        break;

    case NODE_IF:
        ast_free(node->as.if_stmt.cond);
        ast_free(node->as.if_stmt.then_body);
        for (int i = 0; i < node->as.if_stmt.elif_conds.count; i++) {
            ast_free(node->as.if_stmt.elif_conds.items[i]);
            ast_free(node->as.if_stmt.elif_bodies.items[i]);
        }
        nodelist_free(&node->as.if_stmt.elif_conds);
        nodelist_free(&node->as.if_stmt.elif_bodies);
        ast_free(node->as.if_stmt.else_body);
        break;

    case NODE_WHILE:
        ast_free(node->as.while_stmt.cond);
        ast_free(node->as.while_stmt.body);
        break;

    case NODE_RETURN:
    case NODE_THROW:
    case NODE_PRINT:
    case NODE_EXPR_STMT:
        ast_free(node->as.single.expr);
        break;

    case NODE_FUNC_DEF:
        free(node->as.func_def.name);
        for (int i = 0; i < node->as.func_def.param_count; i++)
            free(node->as.func_def.params[i]);
        free(node->as.func_def.params);
        ast_free(node->as.func_def.body);
        break;

    case NODE_INT_LIT:
    case NODE_FLOAT_LIT:
    case NODE_BOOL_LIT:
    case NODE_NULL_LIT:
    case NODE_BREAK:
    case NODE_CONTINUE:
        break;

    case NODE_STRING_LIT:
        free(node->as.string_lit.value);
        break;

    case NODE_IDENT:
        free(node->as.ident.name);
        break;

    case NODE_BINARY_OP:
        ast_free(node->as.binary.left);
        ast_free(node->as.binary.right);
        break;

    case NODE_UNARY_OP:
        ast_free(node->as.unary.operand);
        break;

    case NODE_CALL:
        free(node->as.call.func_name);
        for (int i = 0; i < node->as.call.args.count; i++)
            ast_free(node->as.call.args.items[i]);
        nodelist_free(&node->as.call.args);
        break;
    }

    free(node);
}

/* ============================================================
 * AST dump (debug)
 * ============================================================ */

static const char *node_type_name(NodeType type) {
    switch (type) {
    case NODE_PROGRAM:    return "PROGRAM";
    case NODE_VAR_DECL:   return "VAR_DECL";
    case NODE_ASSIGN:     return "ASSIGN";
    case NODE_IF:         return "IF";
    case NODE_WHILE:      return "WHILE";
    case NODE_BREAK:      return "BREAK";
    case NODE_CONTINUE:   return "CONTINUE";
    case NODE_RETURN:     return "RETURN";
    case NODE_THROW:      return "THROW";
    case NODE_PRINT:      return "PRINT";
    case NODE_EXPR_STMT:  return "EXPR_STMT";
    case NODE_BLOCK:      return "BLOCK";
    case NODE_FUNC_DEF:   return "FUNC_DEF";
    case NODE_INT_LIT:    return "INT_LIT";
    case NODE_FLOAT_LIT:  return "FLOAT_LIT";
    case NODE_STRING_LIT: return "STRING_LIT";
    case NODE_BOOL_LIT:   return "BOOL_LIT";
    case NODE_NULL_LIT:   return "NULL_LIT";
    case NODE_IDENT:      return "IDENT";
    case NODE_BINARY_OP:  return "BINARY_OP";
    case NODE_UNARY_OP:   return "UNARY_OP";
    case NODE_CALL:       return "CALL";
    }
    return "???";
}

static void indent(int n) {
    for (int i = 0; i < n; i++) fprintf(stderr, "  ");
}

void ast_dump(AstNode *node, int depth) {
    if (!node) { indent(depth); fprintf(stderr, "(null)\n"); return; }

    indent(depth);

    switch (node->type) {
    case NODE_PROGRAM:
    case NODE_BLOCK:
        fprintf(stderr, "%s\n", node_type_name(node->type));
        for (int i = 0; i < node->as.block.stmts.count; i++)
            ast_dump(node->as.block.stmts.items[i], depth + 1);
        break;

    case NODE_VAR_DECL:
        fprintf(stderr, "VAR_DECL '%s'\n", node->as.var_decl.name);
        if (node->as.var_decl.init)
            ast_dump(node->as.var_decl.init, depth + 1);
        break;

    case NODE_ASSIGN:
        fprintf(stderr, "ASSIGN '%s'\n", node->as.assign.name);
        ast_dump(node->as.assign.value, depth + 1);
        break;

    case NODE_IF:
        fprintf(stderr, "IF\n");
        indent(depth + 1); fprintf(stderr, "cond:\n");
        ast_dump(node->as.if_stmt.cond, depth + 2);
        indent(depth + 1); fprintf(stderr, "then:\n");
        ast_dump(node->as.if_stmt.then_body, depth + 2);
        for (int i = 0; i < node->as.if_stmt.elif_conds.count; i++) {
            indent(depth + 1); fprintf(stderr, "elif cond:\n");
            ast_dump(node->as.if_stmt.elif_conds.items[i], depth + 2);
            indent(depth + 1); fprintf(stderr, "elif body:\n");
            ast_dump(node->as.if_stmt.elif_bodies.items[i], depth + 2);
        }
        if (node->as.if_stmt.else_body) {
            indent(depth + 1); fprintf(stderr, "else:\n");
            ast_dump(node->as.if_stmt.else_body, depth + 2);
        }
        break;

    case NODE_WHILE:
        fprintf(stderr, "WHILE\n");
        indent(depth + 1); fprintf(stderr, "cond:\n");
        ast_dump(node->as.while_stmt.cond, depth + 2);
        indent(depth + 1); fprintf(stderr, "body:\n");
        ast_dump(node->as.while_stmt.body, depth + 2);
        break;

    case NODE_BREAK:
        fprintf(stderr, "BREAK\n");
        break;

    case NODE_CONTINUE:
        fprintf(stderr, "CONTINUE\n");
        break;

    case NODE_RETURN:
        fprintf(stderr, "RETURN\n");
        if (node->as.single.expr)
            ast_dump(node->as.single.expr, depth + 1);
        break;

    case NODE_THROW:
        fprintf(stderr, "THROW\n");
        if (node->as.single.expr)
            ast_dump(node->as.single.expr, depth + 1);
        break;

    case NODE_PRINT:
        fprintf(stderr, "PRINT\n");
        ast_dump(node->as.single.expr, depth + 1);
        break;

    case NODE_EXPR_STMT:
        fprintf(stderr, "EXPR_STMT\n");
        ast_dump(node->as.single.expr, depth + 1);
        break;

    case NODE_FUNC_DEF:
        fprintf(stderr, "FUNC_DEF '%s'(", node->as.func_def.name);
        for (int i = 0; i < node->as.func_def.param_count; i++) {
            if (i > 0) fprintf(stderr, ", ");
            fprintf(stderr, "%s", node->as.func_def.params[i]);
        }
        fprintf(stderr, ")\n");
        ast_dump(node->as.func_def.body, depth + 1);
        break;

    case NODE_INT_LIT:
        fprintf(stderr, "INT %ld\n", node->as.int_lit.value);
        break;

    case NODE_FLOAT_LIT:
        fprintf(stderr, "FLOAT %g\n", node->as.float_lit.value);
        break;

    case NODE_STRING_LIT:
        fprintf(stderr, "STRING \"%s\"\n", node->as.string_lit.value);
        break;

    case NODE_BOOL_LIT:
        fprintf(stderr, "BOOL(%d%%)\n", node->as.bool_lit.probability);
        break;

    case NODE_NULL_LIT:
        fprintf(stderr, "NULL\n");
        break;

    case NODE_IDENT:
        fprintf(stderr, "IDENT '%s'\n", node->as.ident.name);
        break;

    case NODE_BINARY_OP:
        fprintf(stderr, "BINARY '%s'\n", token_type_name(node->as.binary.op));
        ast_dump(node->as.binary.left, depth + 1);
        ast_dump(node->as.binary.right, depth + 1);
        break;

    case NODE_UNARY_OP:
        fprintf(stderr, "UNARY '%s'\n", token_type_name(node->as.unary.op));
        ast_dump(node->as.unary.operand, depth + 1);
        break;

    case NODE_CALL:
        fprintf(stderr, "CALL '%s'\n", node->as.call.func_name);
        for (int i = 0; i < node->as.call.args.count; i++)
            ast_dump(node->as.call.args.items[i], depth + 1);
        break;
    }
}
