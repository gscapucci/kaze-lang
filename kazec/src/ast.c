#include "../include/ast.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// ============================================================================
// Node size table — indexed by NodeKind
// ============================================================================

static const size_t node_sizes[NODE_KIND_COUNT] = {
    [NODE_EXPR_INT_LIT]      = sizeof(ExprIntLit),
    [NODE_EXPR_FLOAT_LIT]    = sizeof(ExprFloatLit),
    [NODE_EXPR_STRING_LIT]   = sizeof(ExprStringLit),
    [NODE_EXPR_BOOL_LIT]     = sizeof(ExprBoolLit),
    [NODE_EXPR_NULL_LIT]     = sizeof(ExprNullLit),
    [NODE_EXPR_IDENT]        = sizeof(ExprIdent),
    [NODE_EXPR_BINARY_OP]    = sizeof(ExprBinaryOp),
    [NODE_EXPR_UNARY_OP]     = sizeof(ExprUnaryOp),
    [NODE_EXPR_CALL]         = sizeof(ExprCall),
    [NODE_EXPR_INDEX]        = sizeof(ExprIndex),
    [NODE_EXPR_FIELD_ACCESS] = sizeof(ExprFieldAccess),
    [NODE_EXPR_PATH]         = sizeof(ExprPath),
    [NODE_EXPR_CAST]         = sizeof(ExprCast),
    [NODE_EXPR_POSTFIX]      = sizeof(ExprPostfix),
    [NODE_EXPR_CIMPORT]      = sizeof(ExprCImport),
    [NODE_EXPR_STRUCT_LIT]   = sizeof(ExprStructLit),
    [NODE_EXPR_TRY]          = sizeof(ExprTry),

    [NODE_STMT_VAR_DECL]     = sizeof(StmtVarDecl),
    [NODE_STMT_ASSIGNMENT]   = sizeof(StmtAssignment),
    [NODE_STMT_IF]           = sizeof(StmtIf),
    [NODE_STMT_WHILE]        = sizeof(StmtWhile),
    [NODE_STMT_FOR]          = sizeof(StmtFor),
    [NODE_STMT_RETURN]       = sizeof(StmtReturn),
    [NODE_STMT_BREAK]        = sizeof(StmtBreakContinue),
    [NODE_STMT_CONTINUE]     = sizeof(StmtBreakContinue),
    [NODE_STMT_BLOCK]        = sizeof(StmtBlock),
    [NODE_STMT_EXPR]         = sizeof(StmtExprStmt),
    [NODE_STMT_MATCH]        = sizeof(StmtMatch),
    [NODE_STMT_DEFER]        = sizeof(StmtDefer),
    [NODE_STMT_WHEN]         = sizeof(StmtWhen),

    [NODE_DECL_FUNCTION]     = sizeof(DeclFunction),
    [NODE_DECL_STRUCT]       = sizeof(DeclStruct),
    [NODE_DECL_ENUM]         = sizeof(DeclEnum),
    [NODE_DECL_UNION]        = sizeof(DeclUnion),
    [NODE_DECL_TYPE_ALIAS]   = sizeof(DeclTypeAlias),
    [NODE_DECL_IMPORT]       = sizeof(DeclImport),

    [NODE_TYPE_PRIMITIVE]    = sizeof(TypePrimitive),
    [NODE_TYPE_POINTER]      = sizeof(TypePointer),
    [NODE_TYPE_ARRAY]        = sizeof(TypeArray),
    [NODE_TYPE_FUNCTION]     = sizeof(TypeFunction),
    [NODE_TYPE_GENERIC]      = sizeof(TypeGeneric),

    [NODE_PATTERN_WILDCARD]  = sizeof(PatternWildcard),
    [NODE_PATTERN_IDENT]     = sizeof(PatternIdent),
    [NODE_PATTERN_LITERAL]   = sizeof(PatternLiteral),
    [NODE_PATTERN_PATH]      = sizeof(PatternPath),
    [NODE_PATTERN_TUPLE]     = sizeof(PatternTuple),
    [NODE_PATTERN_STRUCT]    = sizeof(PatternStruct),
};

// ============================================================================
// ast_alloc_node
// ============================================================================

Node *ast_alloc_node(Arena *arena, NodeKind kind, SourceLoc loc) {
    assert(kind < NODE_KIND_COUNT);
    size_t size = node_sizes[kind];
    Node *node = (Node *)arena_alloc_zeroed(arena, size);
    node->kind = kind;
    node->loc  = loc;
    return node;
}

// ============================================================================
// _node_cast_any
// ============================================================================

Node *_node_cast_any(Node *node, NodeKind *kinds, size_t count) {
    if (!node) return NULL;
    for (size_t i = 0; i < count; i++) {
        if (node->kind == kinds[i]) return node;
    }
    return NULL;
}

// ============================================================================
// node_kind_to_string
// ============================================================================

const char *node_kind_to_string(NodeKind kind) {
    switch (kind) {
        case NODE_EXPR_INT_LIT:      return "ExprIntLit";
        case NODE_EXPR_FLOAT_LIT:    return "ExprFloatLit";
        case NODE_EXPR_STRING_LIT:   return "ExprStringLit";
        case NODE_EXPR_BOOL_LIT:     return "ExprBoolLit";
        case NODE_EXPR_NULL_LIT:     return "ExprNullLit";
        case NODE_EXPR_IDENT:        return "ExprIdent";
        case NODE_EXPR_BINARY_OP:    return "ExprBinaryOp";
        case NODE_EXPR_UNARY_OP:     return "ExprUnaryOp";
        case NODE_EXPR_CALL:         return "ExprCall";
        case NODE_EXPR_INDEX:        return "ExprIndex";
        case NODE_EXPR_FIELD_ACCESS: return "ExprFieldAccess";
        case NODE_EXPR_PATH:         return "ExprPath";
        case NODE_EXPR_CAST:         return "ExprCast";
        case NODE_EXPR_POSTFIX:      return "ExprPostfix";
        case NODE_EXPR_CIMPORT:      return "ExprCImport";
        case NODE_EXPR_STRUCT_LIT:   return "ExprStructLit";
        case NODE_EXPR_TRY:          return "ExprTry";
        case NODE_STMT_VAR_DECL:     return "StmtVarDecl";
        case NODE_STMT_ASSIGNMENT:   return "StmtAssignment";
        case NODE_STMT_IF:           return "StmtIf";
        case NODE_STMT_WHILE:        return "StmtWhile";
        case NODE_STMT_FOR:          return "StmtFor";
        case NODE_STMT_RETURN:       return "StmtReturn";
        case NODE_STMT_BREAK:        return "StmtBreak";
        case NODE_STMT_CONTINUE:     return "StmtContinue";
        case NODE_STMT_BLOCK:        return "StmtBlock";
        case NODE_STMT_EXPR:         return "StmtExprStmt";
        case NODE_STMT_MATCH:        return "StmtMatch";
        case NODE_STMT_DEFER:        return "StmtDefer";
        case NODE_STMT_WHEN:         return "StmtWhen";
        case NODE_DECL_FUNCTION:     return "DeclFunction";
        case NODE_DECL_STRUCT:       return "DeclStruct";
        case NODE_DECL_ENUM:         return "DeclEnum";
        case NODE_DECL_UNION:        return "DeclUnion";
        case NODE_DECL_TYPE_ALIAS:   return "DeclTypeAlias";
        case NODE_DECL_IMPORT:       return "DeclImport";
        case NODE_TYPE_PRIMITIVE:    return "TypePrimitive";
        case NODE_TYPE_POINTER:      return "TypePointer";
        case NODE_TYPE_ARRAY:        return "TypeArray";
        case NODE_TYPE_FUNCTION:     return "TypeFunction";
        case NODE_TYPE_GENERIC:      return "TypeGeneric";
        case NODE_PATTERN_WILDCARD:  return "PatternWildcard";
        case NODE_PATTERN_IDENT:     return "PatternIdent";
        case NODE_PATTERN_LITERAL:   return "PatternLiteral";
        case NODE_PATTERN_PATH:      return "PatternPath";
        case NODE_PATTERN_TUPLE:     return "PatternTuple";
        case NODE_PATTERN_STRUCT:    return "PatternStruct";
        default:                     return "<unknown>";
    }
}

// ============================================================================
// ast_print_tree
// ============================================================================

static void print_indent(int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
}

static void print_sv(StringView sv) {
    printf("%.*s", (int)sv.len, sv.data);
}

void ast_print_tree(Node *root, int depth) {
    if (!root) return;

    print_indent(depth);
    printf("[%s] (%u:%u)", node_kind_to_string(root->kind), root->loc.line, root->loc.col);

    switch (root->kind) {
        case NODE_EXPR_INT_LIT: {
            ExprIntLit *n = NODE_CAST_UNSAFE(root, ExprIntLit);
            printf(" value=%lld\n", (long long)n->value);
            break;
        }
        case NODE_EXPR_FLOAT_LIT: {
            ExprFloatLit *n = NODE_CAST_UNSAFE(root, ExprFloatLit);
            printf(" value=%g\n", n->value);
            break;
        }
        case NODE_EXPR_STRING_LIT: {
            ExprStringLit *n = NODE_CAST_UNSAFE(root, ExprStringLit);
            printf(" value=\"");
            print_sv(n->value);
            printf("\"%s\n", n->is_raw ? " raw" : "");
            break;
        }
        case NODE_EXPR_BOOL_LIT: {
            ExprBoolLit *n = NODE_CAST_UNSAFE(root, ExprBoolLit);
            printf(" value=%s\n", n->value ? "true" : "false");
            break;
        }
        case NODE_EXPR_NULL_LIT:
            printf("\n");
            break;
        case NODE_EXPR_IDENT: {
            ExprIdent *n = NODE_CAST_UNSAFE(root, ExprIdent);
            printf(" name=");
            print_sv(n->name);
            printf("\n");
            break;
        }
        case NODE_EXPR_BINARY_OP: {
            ExprBinaryOp *n = NODE_CAST_UNSAFE(root, ExprBinaryOp);
            printf(" op=");
            print_sv(n->op);
            printf("\n");
            ast_print_tree(n->left,  depth + 1);
            ast_print_tree(n->right, depth + 1);
            break;
        }
        case NODE_EXPR_UNARY_OP: {
            ExprUnaryOp *n = NODE_CAST_UNSAFE(root, ExprUnaryOp);
            printf(" op=");
            print_sv(n->op);
            printf(" %s\n", n->is_prefix ? "prefix" : "postfix");
            ast_print_tree(n->operand, depth + 1);
            break;
        }
        case NODE_EXPR_CALL: {
            ExprCall *n = NODE_CAST_UNSAFE(root, ExprCall);
            printf("%s\n", n->is_comptime ? " comptime" : "");
            ast_print_tree(n->callee, depth + 1);
            for (size_t i = 0; i < n->arg_count; i++)
                ast_print_tree(n->args[i], depth + 1);
            break;
        }
        case NODE_EXPR_INDEX: {
            ExprIndex *n = NODE_CAST_UNSAFE(root, ExprIndex);
            printf("\n");
            ast_print_tree(n->object, depth + 1);
            ast_print_tree(n->index,  depth + 1);
            break;
        }
        case NODE_EXPR_FIELD_ACCESS: {
            ExprFieldAccess *n = NODE_CAST_UNSAFE(root, ExprFieldAccess);
            printf(" field=");
            print_sv(n->field);
            printf("%s\n", n->is_pointer ? " (->)" : " (.)");
            ast_print_tree(n->object, depth + 1);
            break;
        }
        case NODE_EXPR_PATH: {
            ExprPath *n = NODE_CAST_UNSAFE(root, ExprPath);
            printf(" name=");
            print_sv(n->name);
            printf("\n");
            ast_print_tree(n->scope, depth + 1);
            break;
        }
        case NODE_EXPR_CAST: {
            ExprCast *n = NODE_CAST_UNSAFE(root, ExprCast);
            printf("\n");
            ast_print_tree(n->expr,        depth + 1);
            ast_print_tree(n->target_type, depth + 1);
            break;
        }
        case NODE_EXPR_POSTFIX: {
            ExprPostfix *n = NODE_CAST_UNSAFE(root, ExprPostfix);
            printf(" op=");
            print_sv(n->op);
            printf("\n");
            ast_print_tree(n->expr, depth + 1);
            break;
        }
        case NODE_EXPR_CIMPORT: {
            ExprCImport *n = NODE_CAST_UNSAFE(root, ExprCImport);
            printf(" @cimport \"");
            print_sv(n->path);
            printf("\"\n");
            for (size_t i = 0; i < n->define_count; i++) {
                print_indent(depth + 1);
                printf("define: \"");
                print_sv(n->defines[i]);
                printf("\"\n");
            }
            break;
        }
        case NODE_EXPR_STRUCT_LIT: {
            ExprStructLit *n = NODE_CAST_UNSAFE(root, ExprStructLit);
            printf(" ");
            if (n->is_anonymous) printf(".{ }"); else print_sv(n->type_name);
            printf("\n");
            for (size_t i = 0; i < n->field_count; i++) {
                print_indent(depth + 1);
                printf(".");
                print_sv(n->field_names[i]);
                printf(" =\n");
                ast_print_tree(n->field_values[i], depth + 2);
            }
            break;
        }
        case NODE_EXPR_TRY: {
            ExprTry *n = NODE_CAST_UNSAFE(root, ExprTry);
            printf("\n");
            ast_print_tree(n->expr, depth + 1);
            break;
        }
        case NODE_STMT_VAR_DECL: {
            StmtVarDecl *n = NODE_CAST_UNSAFE(root, StmtVarDecl);
            printf(" %s ", n->is_const ? "const" : "var");
            print_sv(n->name);
            if (n->is_comptime) printf(" comptime");
            printf("\n");
            ast_print_tree(n->type_annotation, depth + 1);
            ast_print_tree(n->initializer,     depth + 1);
            break;
        }
        case NODE_STMT_ASSIGNMENT: {
            StmtAssignment *n = NODE_CAST_UNSAFE(root, StmtAssignment);
            printf("\n");
            ast_print_tree(n->target, depth + 1);
            ast_print_tree(n->value,  depth + 1);
            break;
        }
        case NODE_STMT_IF: {
            StmtIf *n = NODE_CAST_UNSAFE(root, StmtIf);
            printf("\n");
            ast_print_tree(n->condition, depth + 1);
            print_indent(depth + 1); printf("then:\n");
            for (size_t i = 0; i < n->then_len; i++)
                ast_print_tree(n->then_body[i], depth + 2);
            if (n->else_len > 0) {
                print_indent(depth + 1); printf("else:\n");
                for (size_t i = 0; i < n->else_len; i++)
                    ast_print_tree(n->else_body[i], depth + 2);
            }
            break;
        }
        case NODE_STMT_WHILE: {
            StmtWhile *n = NODE_CAST_UNSAFE(root, StmtWhile);
            printf("\n");
            ast_print_tree(n->condition, depth + 1);
            for (size_t i = 0; i < n->body_len; i++)
                ast_print_tree(n->body[i], depth + 1);
            break;
        }
        case NODE_STMT_FOR: {
            StmtFor *n = NODE_CAST_UNSAFE(root, StmtFor);
            printf(" var=");
            print_sv(n->var_name);
            printf("\n");
            ast_print_tree(n->iter_expr, depth + 1);
            for (size_t i = 0; i < n->body_len; i++)
                ast_print_tree(n->body[i], depth + 1);
            break;
        }
        case NODE_STMT_RETURN: {
            StmtReturn *n = NODE_CAST_UNSAFE(root, StmtReturn);
            printf("\n");
            ast_print_tree(n->value, depth + 1);
            break;
        }
        case NODE_STMT_BREAK:
        case NODE_STMT_CONTINUE: {
            StmtBreakContinue *n = NODE_CAST_UNSAFE(root, StmtBreakContinue);
            if (n->label.len > 0) { printf(" label="); print_sv(n->label); }
            printf("\n");
            break;
        }
        case NODE_STMT_BLOCK: {
            StmtBlock *n = NODE_CAST_UNSAFE(root, StmtBlock);
            printf("\n");
            for (size_t i = 0; i < n->stmt_count; i++)
                ast_print_tree(n->statements[i], depth + 1);
            break;
        }
        case NODE_STMT_EXPR: {
            StmtExprStmt *n = NODE_CAST_UNSAFE(root, StmtExprStmt);
            printf("\n");
            ast_print_tree(n->expr, depth + 1);
            break;
        }
        case NODE_DECL_FUNCTION: {
            DeclFunction *n = NODE_CAST_UNSAFE(root, DeclFunction);
            printf(" fn ");
            print_sv(n->name);
            if (n->is_extern)   printf(" extern");
            if (n->is_inline)   printf(" inline");
            if (n->is_comptime) printf(" comptime");
            printf("\n");
            for (size_t i = 0; i < n->param_count; i++) {
                print_indent(depth + 1);
                printf("param: ");
                print_sv(n->params[i].name);
                if (n->params[i].is_comptime) printf(" comptime");
                if (n->params[i].is_variadic) printf(" ...");
                printf("\n");
                ast_print_tree(n->params[i].type, depth + 2);
            }
            if (n->return_type) {
                print_indent(depth + 1); printf("return_type:\n");
                ast_print_tree(n->return_type, depth + 2);
            }
            for (size_t i = 0; i < n->body_len; i++)
                ast_print_tree(n->body[i], depth + 1);
            break;
        }
        case NODE_DECL_STRUCT: {
            DeclStruct *n = NODE_CAST_UNSAFE(root, DeclStruct);
            printf(" struct ");
            print_sv(n->name);
            printf("\n");
            for (size_t i = 0; i < n->field_count; i++) {
                print_indent(depth + 1);
                printf("field: ");
                print_sv(n->field_names[i]);
                printf("\n");
                ast_print_tree(n->field_types[i], depth + 2);
            }
            for (size_t i = 0; i < n->method_count; i++)
                ast_print_tree(n->methods[i], depth + 1);
            break;
        }
        case NODE_DECL_ENUM: {
            DeclEnum *n = NODE_CAST_UNSAFE(root, DeclEnum);
            printf(" enum ");
            print_sv(n->name);
            printf("\n");
            for (size_t i = 0; i < n->variant_count; i++) {
                EnumVariant *v = &n->variants[i];
                print_indent(depth + 1);
                printf("variant: ");
                print_sv(v->name);
                printf("\n");
                if (v->kind == ENUM_VARIANT_DISCRIMINANT) {
                    ast_print_tree(v->discriminant, depth + 2);
                } else if (v->kind == ENUM_VARIANT_TUPLE) {
                    ast_print_tree(v->tuple_type, depth + 2);
                } else if (v->kind == ENUM_VARIANT_STRUCT) {
                    for (size_t j = 0; j < v->field_count; j++) {
                        print_indent(depth + 2);
                        printf("field: ");
                        print_sv(v->field_names[j]);
                        printf("\n");
                        ast_print_tree(v->field_types[j], depth + 3);
                    }
                }
            }
            break;
        }
        case NODE_DECL_UNION: {
            DeclUnion *n = NODE_CAST_UNSAFE(root, DeclUnion);
            printf(" union ");
            print_sv(n->name);
            printf("\n");
            for (size_t i = 0; i < n->field_count; i++) {
                print_indent(depth + 1);
                printf("field: ");
                print_sv(n->field_names[i]);
                printf("\n");
                ast_print_tree(n->field_types[i], depth + 2);
            }
            break;
        }
        case NODE_DECL_TYPE_ALIAS: {
            DeclTypeAlias *n = NODE_CAST_UNSAFE(root, DeclTypeAlias);
            printf(" type ");
            print_sv(n->name);
            printf("\n");
            ast_print_tree(n->aliased_type, depth + 1);
            break;
        }
        case NODE_DECL_IMPORT: {
            DeclImport *n = NODE_CAST_UNSAFE(root, DeclImport);
            printf(" import \"");
            print_sv(n->path);
            printf("\"%s\n", n->is_external ? " @cimport" : "");
            break;
        }
        case NODE_TYPE_PRIMITIVE: {
            TypePrimitive *n = NODE_CAST_UNSAFE(root, TypePrimitive);
            printf(" ");
            print_sv(n->name);
            printf("\n");
            break;
        }
        case NODE_TYPE_POINTER: {
            TypePointer *n = NODE_CAST_UNSAFE(root, TypePointer);
            printf("%s\n", n->is_mutable ? " mut" : "");
            ast_print_tree(n->pointee, depth + 1);
            break;
        }
        case NODE_TYPE_ARRAY: {
            TypeArray *n = NODE_CAST_UNSAFE(root, TypeArray);
            printf("%s\n", n->is_slice ? " slice" : "");
            ast_print_tree(n->length,       depth + 1);
            ast_print_tree(n->element_type, depth + 1);
            break;
        }
        case NODE_TYPE_FUNCTION: {
            TypeFunction *n = NODE_CAST_UNSAFE(root, TypeFunction);
            printf("\n");
            for (size_t i = 0; i < n->param_count; i++)
                ast_print_tree(n->param_types[i], depth + 1);
            ast_print_tree(n->return_type, depth + 1);
            break;
        }
        case NODE_TYPE_GENERIC: {
            TypeGeneric *n = NODE_CAST_UNSAFE(root, TypeGeneric);
            printf(" ");
            print_sv(n->base_name);
            printf("\n");
            for (size_t i = 0; i < n->type_arg_count; i++)
                ast_print_tree(n->type_args[i], depth + 1);
            break;
        }
        case NODE_STMT_MATCH: {
            StmtMatch *n = NODE_CAST_UNSAFE(root, StmtMatch);
            printf("\n");
            ast_print_tree(n->subject, depth + 1);
            for (size_t i = 0; i < n->arm_count; i++) {
                print_indent(depth + 1); printf("arm:\n");
                ast_print_tree(n->arms[i].pattern, depth + 2);
                for (size_t j = 0; j < n->arms[i].body_len; j++)
                    ast_print_tree(n->arms[i].body[j], depth + 2);
            }
            break;
        }
        case NODE_STMT_DEFER: {
            StmtDefer *n = NODE_CAST_UNSAFE(root, StmtDefer);
            printf("\n");
            ast_print_tree(n->body, depth + 1);
            break;
        }
        case NODE_STMT_WHEN: {
            StmtWhen *n = NODE_CAST_UNSAFE(root, StmtWhen);
            printf("\n");
            for (size_t i = 0; i < n->branch_count; i++) {
                print_indent(depth + 1);
                printf(n->branches[i].condition ? "when:\n" : "else:\n");
                if (n->branches[i].condition)
                    ast_print_tree(n->branches[i].condition, depth + 2);
                for (size_t j = 0; j < n->branches[i].body_len; j++)
                    ast_print_tree(n->branches[i].body[j], depth + 2);
            }
            break;
        }
        case NODE_PATTERN_WILDCARD:
            printf(" _\n");
            break;
        case NODE_PATTERN_IDENT: {
            PatternIdent *n = NODE_CAST_UNSAFE(root, PatternIdent);
            printf(" ");
            print_sv(n->name);
            printf("\n");
            break;
        }
        case NODE_PATTERN_LITERAL: {
            PatternLiteral *n = NODE_CAST_UNSAFE(root, PatternLiteral);
            printf("\n");
            ast_print_tree(n->literal, depth + 1);
            break;
        }
        case NODE_PATTERN_PATH: {
            PatternPath *n = NODE_CAST_UNSAFE(root, PatternPath);
            printf(" ");
            print_sv(n->type_name);
            printf("::");
            print_sv(n->variant);
            printf("\n");
            break;
        }
        case NODE_PATTERN_TUPLE: {
            PatternTuple *n = NODE_CAST_UNSAFE(root, PatternTuple);
            printf("\n");
            for (size_t i = 0; i < n->count; i++)
                ast_print_tree(n->elements[i], depth + 1);
            break;
        }
        case NODE_PATTERN_STRUCT: {
            PatternStruct *n = NODE_CAST_UNSAFE(root, PatternStruct);
            printf(" ");
            print_sv(n->type_name);
            printf("\n");
            for (size_t i = 0; i < n->field_count; i++) {
                print_indent(depth + 1);
                print_sv(n->fields[i]);
                printf("\n");
            }
            break;
        }
        default:
            printf("\n");
            break;
    }
}
