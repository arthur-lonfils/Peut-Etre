#include "codegen.h"
#include <string.h>

/* ============================================================
 * TypeScript Code Generator
 * Walks the AST and emits TS with chaos runtime calls.
 * Everything is 'any' because commitment is overrated.
 * ============================================================ */

typedef struct {
    FILE *out;
    int   indent;
    int   loop_id;
} TsGen;

static void ts_indent(TsGen *g) {
    for (int i = 0; i < g->indent; i++)
        fprintf(g->out, "    ");
}

static void ts_emit_expr(TsGen *g, AstNode *node);
static void ts_emit_stmt(TsGen *g, AstNode *node);
static void ts_emit_block_body(TsGen *g, AstNode *block);

/* ============================================================
 * Expression emission
 * ============================================================ */

static void ts_emit_expr(TsGen *g, AstNode *node) {
    if (!node) { fprintf(g->out, "null"); return; }

    switch (node->type) {
    case NODE_INT_LIT:
        fprintf(g->out, "%ld", node->as.int_lit.value);
        break;

    case NODE_FLOAT_LIT:
        fprintf(g->out, "%g", node->as.float_lit.value);
        break;

    case NODE_STRING_LIT:
        fprintf(g->out, "\"%s\"", node->as.string_lit.value);
        break;

    case NODE_BOOL_LIT:
        if (node->as.bool_lit.probability == 100)
            fprintf(g->out, "true");
        else if (node->as.bool_lit.probability == 0)
            fprintf(g->out, "false");
        else
            fprintf(g->out, "_pe_prob_bool(%d)", node->as.bool_lit.probability);
        break;

    case NODE_NULL_LIT:
        fprintf(g->out, "null");
        break;

    case NODE_IDENT:
        fprintf(g->out, "%s", node->as.ident.name);
        break;

    case NODE_UNARY_OP:
        switch (node->as.unary.op) {
        case TOK_MINUS:
            fprintf(g->out, "_pe_neg(");
            ts_emit_expr(g, node->as.unary.operand);
            fprintf(g->out, ")");
            break;
        case TOK_NOT:
            fprintf(g->out, "_pe_not(");
            ts_emit_expr(g, node->as.unary.operand);
            fprintf(g->out, ")");
            break;
        default:
            fprintf(g->out, "/* ??? unary */");
            break;
        }
        break;

    case NODE_BINARY_OP: {
        const char *fn = NULL;
        switch (node->as.binary.op) {
        case TOK_PLUS:      fn = "_pe_add"; break;
        case TOK_MINUS:     fn = "_pe_sub"; break;
        case TOK_STAR:      fn = "_pe_mul"; break;
        case TOK_SLASH:     fn = "_pe_div"; break;
        case TOK_PERCENT:   fn = "_pe_mod"; break;
        case TOK_EQ:        fn = "_pe_eq";  break;
        case TOK_NEQ:       fn = "_pe_neq"; break;
        case TOK_LT:  case TOK_LESS:    fn = "_pe_lt";  break;
        case TOK_GT:  case TOK_GREATER: fn = "_pe_gt";  break;
        case TOK_LTE:       fn = "_pe_lte"; break;
        case TOK_GTE:       fn = "_pe_gte"; break;
        case TOK_AND: case TOK_LOGIC_AND: fn = "_pe_and"; break;
        case TOK_OR:  case TOK_LOGIC_OR:  fn = "_pe_or";  break;
        case TOK_FUZZY_EQ:  fn = "_pe_fuzzy_eq"; break;
        case TOK_NOT_EQ:    fn = "_pe_neq"; break;
        default:            fn = "/* ??? op */"; break;
        }
        fprintf(g->out, "%s(", fn);
        ts_emit_expr(g, node->as.binary.left);
        fprintf(g->out, ", ");
        ts_emit_expr(g, node->as.binary.right);
        fprintf(g->out, ")");
        break;
    }

    case NODE_CALL:
        fprintf(g->out, "%s(", node->as.call.func_name);
        for (int i = 0; i < node->as.call.args.count; i++) {
            if (i > 0) fprintf(g->out, ", ");
            ts_emit_expr(g, node->as.call.args.items[i]);
        }
        fprintf(g->out, ")");
        break;

    default:
        fprintf(g->out, "/* ??? expr node %d */", node->type);
        break;
    }
}

/* ============================================================
 * Statement emission
 * ============================================================ */

static void ts_emit_block_body(TsGen *g, AstNode *block) {
    if (!block) return;
    for (int i = 0; i < block->as.block.stmts.count; i++)
        ts_emit_stmt(g, block->as.block.stmts.items[i]);
}

static void ts_emit_stmt(TsGen *g, AstNode *node) {
    if (!node) return;

    switch (node->type) {
    case NODE_VAR_DECL:
        ts_indent(g);
        fprintf(g->out, "let %s: any = ", node->as.var_decl.name);
        if (node->as.var_decl.init)
            ts_emit_expr(g, node->as.var_decl.init);
        else
            fprintf(g->out, "null");
        fprintf(g->out, ";\n");
        break;

    case NODE_ASSIGN:
        ts_indent(g);
        fprintf(g->out, "%s = ", node->as.assign.name);
        ts_emit_expr(g, node->as.assign.value);
        fprintf(g->out, ";\n");
        break;

    case NODE_PRINT:
        ts_indent(g);
        fprintf(g->out, "_pe_print(");
        ts_emit_expr(g, node->as.single.expr);
        fprintf(g->out, ");\n");
        break;

    case NODE_RETURN:
        ts_indent(g);
        fprintf(g->out, "return ");
        if (node->as.single.expr)
            ts_emit_expr(g, node->as.single.expr);
        else
            fprintf(g->out, "null");
        fprintf(g->out, ";\n");
        break;

    case NODE_THROW:
        ts_indent(g);
        fprintf(g->out, "throw new Error(String(");
        if (node->as.single.expr)
            ts_emit_expr(g, node->as.single.expr);
        else
            fprintf(g->out, "\"je_me_casse\"");
        fprintf(g->out, "));\n");
        break;

    case NODE_BREAK:
        ts_indent(g);
        fprintf(g->out, "break;\n");
        break;

    case NODE_CONTINUE:
        ts_indent(g);
        fprintf(g->out, "continue;\n");
        break;

    case NODE_EXPR_STMT:
        ts_indent(g);
        ts_emit_expr(g, node->as.single.expr);
        fprintf(g->out, ";\n");
        break;

    case NODE_IF:
        ts_indent(g);
        fprintf(g->out, "if (_pe_lazy_cond(");
        ts_emit_expr(g, node->as.if_stmt.cond);
        fprintf(g->out, ")) {\n");
        g->indent++;
        ts_emit_block_body(g, node->as.if_stmt.then_body);
        g->indent--;
        ts_indent(g);
        fprintf(g->out, "}");

        for (int i = 0; i < node->as.if_stmt.elif_conds.count; i++) {
            fprintf(g->out, " else if (_pe_lazy_cond(");
            ts_emit_expr(g, node->as.if_stmt.elif_conds.items[i]);
            fprintf(g->out, ")) {\n");
            g->indent++;
            ts_emit_block_body(g, node->as.if_stmt.elif_bodies.items[i]);
            g->indent--;
            ts_indent(g);
            fprintf(g->out, "}");
        }

        if (node->as.if_stmt.else_body) {
            fprintf(g->out, " else {\n");
            g->indent++;
            ts_emit_block_body(g, node->as.if_stmt.else_body);
            g->indent--;
            ts_indent(g);
            fprintf(g->out, "}");
        }
        fprintf(g->out, "\n");
        break;

    case NODE_WHILE: {
        int lid = g->loop_id++;
        ts_indent(g);
        fprintf(g->out, "{\n");
        g->indent++;
        ts_indent(g);
        fprintf(g->out, "const _pe_loop_%d = new PeLoopState();\n", lid);
        ts_indent(g);
        fprintf(g->out, "while (_pe_loop_check(_pe_loop_%d, ", lid);
        ts_emit_expr(g, node->as.while_stmt.cond);
        fprintf(g->out, ")) {\n");
        g->indent++;
        ts_emit_block_body(g, node->as.while_stmt.body);
        g->indent--;
        ts_indent(g);
        fprintf(g->out, "}\n");
        g->indent--;
        ts_indent(g);
        fprintf(g->out, "}\n");
        break;
    }

    case NODE_FUNC_DEF:
        break;

    case NODE_BLOCK:
        ts_indent(g);
        fprintf(g->out, "{\n");
        g->indent++;
        ts_emit_block_body(g, node);
        g->indent--;
        ts_indent(g);
        fprintf(g->out, "}\n");
        break;

    default:
        ts_indent(g);
        fprintf(g->out, "/* ??? stmt node %d */\n", node->type);
        break;
    }
}

/* ============================================================
 * Top-level emission
 * ============================================================ */

void codegen_ts_emit(FILE *out, AstNode *program, const char *runtime_src,
                     int no_chaos, unsigned int seed, int has_seed) {
    TsGen g = { .out = out, .indent = 0, .loop_id = 0 };

    fprintf(out, "/* Generated by Peut-\xC3\x8Atre \xE2\x80\x94 bonne chance. */\n\n");

    /* Embed the runtime */
    fprintf(out, "%s\n\n", runtime_src);

    if (no_chaos) {
        fprintf(out, "_pe_chaos_enabled = false;\n\n");
    }

    /* Seed is handled differently in TS — no seeded random without a library,
       so we just note it as a comment */
    if (has_seed) {
        fprintf(out, "/* --seed %u (note: TS Math.random() cannot be seeded) */\n\n",
                seed);
    }

    /* Function definitions */
    for (int i = 0; i < program->as.block.stmts.count; i++) {
        AstNode *s = program->as.block.stmts.items[i];
        if (s->type == NODE_FUNC_DEF) {
            fprintf(out, "function %s(", s->as.func_def.name);
            for (int j = 0; j < s->as.func_def.param_count; j++) {
                if (j > 0) fprintf(out, ", ");
                fprintf(out, "%s: any", s->as.func_def.params[j]);
            }
            fprintf(out, "): any {\n");
            g.indent = 1;
            ts_emit_block_body(&g, s->as.func_def.body);
            g.indent = 0;
            fprintf(out, "}\n\n");
        }
    }

    /* Top-level statements (wrapped in an IIFE for scoping) */
    fprintf(out, "/* --- main --- */\n");
    for (int i = 0; i < program->as.block.stmts.count; i++) {
        AstNode *s = program->as.block.stmts.items[i];
        if (s->type != NODE_FUNC_DEF) {
            g.indent = 0;
            ts_emit_stmt(&g, s);
        }
    }
}
