#include "codegen.h"
#include <string.h>

/* ============================================================
 * C Code Generator
 * Walks the AST and emits C source with chaos runtime calls.
 * ============================================================ */

typedef struct {
    FILE *out;
    int   indent;
    int   loop_id;       /* unique counter for loop state variables */
} CGen;

static void emit_indent(CGen *g) {
    for (int i = 0; i < g->indent; i++)
        fprintf(g->out, "    ");
}

static void emit_expr(CGen *g, AstNode *node);
static void emit_stmt(CGen *g, AstNode *node);
static void emit_block_body(CGen *g, AstNode *block);

/* ============================================================
 * Expression emission
 * ============================================================ */

static void emit_expr(CGen *g, AstNode *node) {
    if (!node) { fprintf(g->out, "_pe_null()"); return; }

    switch (node->type) {
    case NODE_INT_LIT:
        fprintf(g->out, "_pe_int(%ld)", node->as.int_lit.value);
        break;

    case NODE_FLOAT_LIT:
        fprintf(g->out, "_pe_float(%g)", node->as.float_lit.value);
        break;

    case NODE_STRING_LIT:
        fprintf(g->out, "_pe_string(\"%s\")", node->as.string_lit.value);
        break;

    case NODE_BOOL_LIT:
        if (node->as.bool_lit.probability == 100)
            fprintf(g->out, "_pe_bool(1)");
        else if (node->as.bool_lit.probability == 0)
            fprintf(g->out, "_pe_bool(0)");
        else
            fprintf(g->out, "_pe_prob_bool(%d)", node->as.bool_lit.probability);
        break;

    case NODE_NULL_LIT:
        fprintf(g->out, "_pe_null()");
        break;

    case NODE_IDENT:
        fprintf(g->out, "%s", node->as.ident.name);
        break;

    case NODE_UNARY_OP:
        switch (node->as.unary.op) {
        case TOK_MINUS:
            fprintf(g->out, "_pe_neg(");
            emit_expr(g, node->as.unary.operand);
            fprintf(g->out, ")");
            break;
        case TOK_NOT:
            fprintf(g->out, "_pe_not(");
            emit_expr(g, node->as.unary.operand);
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
        emit_expr(g, node->as.binary.left);
        fprintf(g->out, ", ");
        emit_expr(g, node->as.binary.right);
        fprintf(g->out, ")");
        break;
    }

    case NODE_CALL:
        fprintf(g->out, "%s(", node->as.call.func_name);
        for (int i = 0; i < node->as.call.args.count; i++) {
            if (i > 0) fprintf(g->out, ", ");
            emit_expr(g, node->as.call.args.items[i]);
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

static void emit_block_body(CGen *g, AstNode *block) {
    if (!block) return;
    for (int i = 0; i < block->as.block.stmts.count; i++)
        emit_stmt(g, block->as.block.stmts.items[i]);
}

static void emit_stmt(CGen *g, AstNode *node) {
    if (!node) return;

    switch (node->type) {
    case NODE_VAR_DECL:
        emit_indent(g);
        fprintf(g->out, "PeValue %s = ", node->as.var_decl.name);
        if (node->as.var_decl.init)
            emit_expr(g, node->as.var_decl.init);
        else
            fprintf(g->out, "_pe_null()");
        fprintf(g->out, ";\n");
        break;

    case NODE_ASSIGN:
        emit_indent(g);
        fprintf(g->out, "%s = ", node->as.assign.name);
        emit_expr(g, node->as.assign.value);
        fprintf(g->out, ";\n");
        break;

    case NODE_PRINT:
        emit_indent(g);
        fprintf(g->out, "_pe_print(");
        emit_expr(g, node->as.single.expr);
        fprintf(g->out, ");\n");
        break;

    case NODE_RETURN:
        emit_indent(g);
        fprintf(g->out, "return ");
        if (node->as.single.expr)
            emit_expr(g, node->as.single.expr);
        else
            fprintf(g->out, "_pe_null()");
        fprintf(g->out, ";\n");
        break;

    case NODE_THROW:
        emit_indent(g);
        fprintf(g->out, "fprintf(stderr, \"je_me_casse: \");\n");
        emit_indent(g);
        fprintf(g->out, "_pe_print(");
        if (node->as.single.expr)
            emit_expr(g, node->as.single.expr);
        else
            fprintf(g->out, "_pe_string(\"panic\")");
        fprintf(g->out, ");\n");
        emit_indent(g);
        fprintf(g->out, "exit(1);\n");
        break;

    case NODE_BREAK:
        emit_indent(g);
        fprintf(g->out, "break;\n");
        break;

    case NODE_CONTINUE:
        emit_indent(g);
        fprintf(g->out, "continue;\n");
        break;

    case NODE_EXPR_STMT:
        emit_indent(g);
        emit_expr(g, node->as.single.expr);
        fprintf(g->out, ";\n");
        break;

    case NODE_IF:
        emit_indent(g);
        fprintf(g->out, "if (_pe_lazy_cond(");
        emit_expr(g, node->as.if_stmt.cond);
        fprintf(g->out, ")) {\n");
        g->indent++;
        emit_block_body(g, node->as.if_stmt.then_body);
        g->indent--;
        emit_indent(g);
        fprintf(g->out, "}");

        for (int i = 0; i < node->as.if_stmt.elif_conds.count; i++) {
            fprintf(g->out, " else if (_pe_lazy_cond(");
            emit_expr(g, node->as.if_stmt.elif_conds.items[i]);
            fprintf(g->out, ")) {\n");
            g->indent++;
            emit_block_body(g, node->as.if_stmt.elif_bodies.items[i]);
            g->indent--;
            emit_indent(g);
            fprintf(g->out, "}");
        }

        if (node->as.if_stmt.else_body) {
            fprintf(g->out, " else {\n");
            g->indent++;
            emit_block_body(g, node->as.if_stmt.else_body);
            g->indent--;
            emit_indent(g);
            fprintf(g->out, "}");
        }
        fprintf(g->out, "\n");
        break;

    case NODE_WHILE: {
        int lid = g->loop_id++;
        emit_indent(g);
        fprintf(g->out, "{\n");
        g->indent++;
        emit_indent(g);
        fprintf(g->out, "PeLoopState _pe_loop_%d = {0};\n", lid);
        emit_indent(g);
        fprintf(g->out, "while (_pe_loop_check(&_pe_loop_%d, ", lid);
        emit_expr(g, node->as.while_stmt.cond);
        fprintf(g->out, ")) {\n");
        g->indent++;
        emit_block_body(g, node->as.while_stmt.body);
        g->indent--;
        emit_indent(g);
        fprintf(g->out, "}\n");
        g->indent--;
        emit_indent(g);
        fprintf(g->out, "}\n");
        break;
    }

    case NODE_FUNC_DEF:
        /* Function definitions are emitted in a separate pass */
        break;

    case NODE_BLOCK:
        emit_indent(g);
        fprintf(g->out, "{\n");
        g->indent++;
        emit_block_body(g, node);
        g->indent--;
        emit_indent(g);
        fprintf(g->out, "}\n");
        break;

    default:
        emit_indent(g);
        fprintf(g->out, "/* ??? stmt node %d */\n", node->type);
        break;
    }
}

/* ============================================================
 * Top-level emission
 * ============================================================ */

void codegen_c_emit(FILE *out, AstNode *program, const char *runtime_src,
                    int no_chaos, unsigned int seed, int has_seed) {
    CGen g = { .out = out, .indent = 0, .loop_id = 0 };

    fprintf(out, "/* Generated by Peut-\xC3\x8Atre \xE2\x80\x94 bonne chance. */\n\n");

    /* Embed the runtime */
    fprintf(out, "%s\n\n", runtime_src);

    /* Forward declarations for user functions */
    for (int i = 0; i < program->as.block.stmts.count; i++) {
        AstNode *s = program->as.block.stmts.items[i];
        if (s->type == NODE_FUNC_DEF) {
            fprintf(out, "PeValue %s(", s->as.func_def.name);
            for (int j = 0; j < s->as.func_def.param_count; j++) {
                if (j > 0) fprintf(out, ", ");
                fprintf(out, "PeValue %s", s->as.func_def.params[j]);
            }
            fprintf(out, ");\n");
        }
    }
    fprintf(out, "\n");

    /* Function definitions */
    for (int i = 0; i < program->as.block.stmts.count; i++) {
        AstNode *s = program->as.block.stmts.items[i];
        if (s->type == NODE_FUNC_DEF) {
            fprintf(out, "PeValue %s(", s->as.func_def.name);
            for (int j = 0; j < s->as.func_def.param_count; j++) {
                if (j > 0) fprintf(out, ", ");
                fprintf(out, "PeValue %s", s->as.func_def.params[j]);
            }
            fprintf(out, ") {\n");
            g.indent = 1;
            emit_block_body(&g, s->as.func_def.body);
            g.indent = 0;
            fprintf(out, "}\n\n");
        }
    }

    /* main() with top-level statements */
    fprintf(out, "int main(void) {\n");
    g.indent = 1;

    /* Chaos init */
    emit_indent(&g);
    if (has_seed)
        fprintf(out, "_pe_chaos_init_seed(%u);\n", seed);
    else
        fprintf(out, "_pe_chaos_init();\n");

    if (no_chaos) {
        emit_indent(&g);
        fprintf(out, "_pe_chaos_enabled = 0;\n");
    }

    /* Emit non-function top-level statements */
    for (int i = 0; i < program->as.block.stmts.count; i++) {
        AstNode *s = program->as.block.stmts.items[i];
        if (s->type != NODE_FUNC_DEF)
            emit_stmt(&g, s);
    }

    emit_indent(&g);
    fprintf(out, "return 0;\n");
    fprintf(out, "}\n");
}
