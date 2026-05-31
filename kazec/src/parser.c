#include "../include/parser.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Internal dynamic arrays (arena-backed)
// ============================================================================

typedef Node *NodePtr;
VECTOR_DEFINITION(NodePtr, Node)
VECTOR_IMPLEMENTATION(NodePtr, Node)

VECTOR_DEFINITION(Param, Param)
VECTOR_IMPLEMENTATION(Param, Param)

VECTOR_DEFINITION(StringView, Sv)
VECTOR_IMPLEMENTATION(StringView, Sv)

VECTOR_DEFINITION(EnumVariant, EVar)
VECTOR_IMPLEMENTATION(EnumVariant, EVar)

VECTOR_DEFINITION(MatchArm, MArm)
VECTOR_IMPLEMENTATION(MatchArm, MArm)

VECTOR_DEFINITION(WhenBranch, WBranch)
VECTOR_IMPLEMENTATION(WhenBranch, WBranch)

// ============================================================================
// Token cursor helpers
// ============================================================================

static Token eof_token(Parser *p) {
    Token t = {0};
    t.type = TOKEN_EOF;
    t.lexeme = string_view_create("", 0);
    if (p->tokens.len > 0) {
        Token last = TokenVec_get(&p->tokens, p->tokens.len - 1);
        t.line = last.line;
        t.col  = last.col;
    }
    return t;
}

static Token peek(Parser *p) {
    if (p->pos >= p->tokens.len) return eof_token(p);
    return TokenVec_get(&p->tokens, p->pos);
}

static Token peek2(Parser *p) {
    if (p->pos + 1 >= p->tokens.len) return eof_token(p);
    return TokenVec_get(&p->tokens, p->pos + 1);
}

static bool at_end(Parser *p) {
    return p->pos >= p->tokens.len || peek(p).type == TOKEN_EOF;
}

static Token advance(Parser *p) {
    Token t = peek(p);
    if (!at_end(p)) p->pos++;
    return t;
}

static bool check(Parser *p, TokenType type) {
    if (at_end(p)) return type == TOKEN_EOF;
    return peek(p).type == type;
}

static bool match_tok(Parser *p, TokenType type) {
    if (check(p, type)) { advance(p); return true; }
    return false;
}

static SourceLoc loc_of(Parser *p, Token t) {
    return (SourceLoc){ .filename = p->filename, .line = (uint32_t)t.line, .col = (uint32_t)t.col };
}

// ============================================================================
// Errors
// ============================================================================

static void parser_error(Parser *p, Token at, const char *fmt, ...) {
    p->had_error = true;

    va_list args;
    va_start(args, fmt);
    va_list copy;
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0) { va_end(args); return; }

    // prefix with location, then the message
    char *body = arena_alloc(p->arena, (size_t)needed + 1);
    vsnprintf(body, (size_t)needed + 1, fmt, args);
    va_end(args);

    int total = snprintf(NULL, 0, "%s:%zu:%zu: %s",
                         p->filename ? p->filename : "<input>", at.line, at.col, body);
    if (total < 0) return;
    char *msg = arena_alloc(p->arena, (size_t)total + 1);
    snprintf(msg, (size_t)total + 1, "%s:%zu:%zu: %s",
             p->filename ? p->filename : "<input>", at.line, at.col, body);

    ErrorVec_push(&p->errors, msg);
}

static Token expect(Parser *p, TokenType type, const char *what) {
    if (check(p, type)) return advance(p);
    Token t = peek(p);
    parser_error(p, t, "expected %s, got '%.*s'", what, (int)t.lexeme.len, t.lexeme.data);
    return t;
}

// Skip tokens until a likely statement / declaration boundary.
static void synchronize(Parser *p) {
    while (!at_end(p)) {
        Token t = peek(p);
        if (t.type == TOKEN_SEMICOLON) { advance(p); return; }
        switch (t.type) {
            case TOKEN_FN:
            case TOKEN_CONST:
            case TOKEN_VAR:
            case TOKEN_LET:
            case TOKEN_IMPORT:
            case TOKEN_RBRACE:
                return;
            default:
                advance(p);
        }
    }
}

// ============================================================================
// Forward declarations
// ============================================================================

static Node *parse_expr(Parser *p);
static Node *parse_type(Parser *p);
static Node *parse_statement(Parser *p);
static Node *parse_pattern(Parser *p);
static Node *parse_when(Parser *p);
static bool  parse_block(Parser *p, NodeVec *out);

// ============================================================================
// Types
// ============================================================================

static bool is_primitive_type_tok(TokenType t) {
    switch (t) {
        case TOKEN_I8: case TOKEN_I16: case TOKEN_I32: case TOKEN_I64:
        case TOKEN_U8: case TOKEN_U16: case TOKEN_U32: case TOKEN_U64:
        case TOKEN_F32: case TOKEN_F64:
        case TOKEN_BOOL: case TOKEN_VOID: case TOKEN_TYPE:
            return true;
        default: return false;
    }
}

// Does the current token begin a type?
static bool at_type_start(Parser *p) {
    Token t = peek(p);
    if (is_primitive_type_tok(t.type)) return true;
    switch (t.type) {
        case TOKEN_STAR: case TOKEN_LBRACKET: case TOKEN_FN:
            return true;
        default: return false;
    }
}

static Node *parse_type(Parser *p) {
    Token t = peek(p);
    SourceLoc loc = loc_of(p, t);

    if (is_primitive_type_tok(t.type)) {
        advance(p);
        TypePrimitive *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_TYPE_PRIMITIVE, loc), TypePrimitive);
        n->name = t.lexeme;
        return &n->base;
    }

    if (t.type == TOKEN_STAR) {
        advance(p);
        bool is_const = match_tok(p, TOKEN_CONST);
        TypePointer *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_TYPE_POINTER, loc), TypePointer);
        n->is_mutable = !is_const;
        n->pointee = parse_type(p);
        return &n->base;
    }

    if (t.type == TOKEN_LBRACKET) {
        advance(p);
        TypeArray *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_TYPE_ARRAY, loc), TypeArray);
        if (match_tok(p, TOKEN_RBRACKET)) {
            n->is_slice = true;
            n->length = NULL;
        } else {
            n->is_slice = false;
            n->length = parse_expr(p);
            expect(p, TOKEN_RBRACKET, "']'");
        }
        n->element_type = parse_type(p);
        return &n->base;
    }

    if (t.type == TOKEN_FN) {
        advance(p);
        TypeFunction *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_TYPE_FUNCTION, loc), TypeFunction);
        expect(p, TOKEN_LPAREN, "'('");
        NodeVec params = NodeVec_create(p->arena, 4);
        if (!check(p, TOKEN_RPAREN)) {
            do {
                NodeVec_push(&params, parse_type(p));
            } while (match_tok(p, TOKEN_COMMA) && !check(p, TOKEN_RPAREN));
        }
        expect(p, TOKEN_RPAREN, "')'");
        n->param_types = params.data;
        n->param_count = params.len;
        n->return_type = parse_type(p);
        return &n->base;
    }

    if (t.type == TOKEN_IDENT) {
        advance(p);
        // Generic instance:  Name(T, ...)
        if (check(p, TOKEN_LPAREN)) {
            advance(p);
            TypeGeneric *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_TYPE_GENERIC, loc), TypeGeneric);
            n->base_name = t.lexeme;
            NodeVec args = NodeVec_create(p->arena, 4);
            if (!check(p, TOKEN_RPAREN)) {
                do {
                    NodeVec_push(&args, parse_type(p));
                } while (match_tok(p, TOKEN_COMMA) && !check(p, TOKEN_RPAREN));
            }
            expect(p, TOKEN_RPAREN, "')'");
            n->type_args = args.data;
            n->type_arg_count = args.len;
            return &n->base;
        }
        // Named type (reuses TypePrimitive's name slot).
        TypePrimitive *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_TYPE_PRIMITIVE, loc), TypePrimitive);
        n->name = t.lexeme;
        return &n->base;
    }

    parser_error(p, t, "expected a type, got '%.*s'", (int)t.lexeme.len, t.lexeme.data);
    advance(p);
    return NULL;
}

// ============================================================================
// Expressions
// ============================================================================

static Node *make_binary(Parser *p, Node *left, StringView op, Node *right, SourceLoc loc) {
    ExprBinaryOp *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_BINARY_OP, loc), ExprBinaryOp);
    n->left = left;
    n->right = right;
    n->op = op;
    return &n->base;
}

static Node *parse_arg_list_call(Parser *p, Node *callee, SourceLoc loc, bool is_comptime) {
    ExprCall *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_CALL, loc), ExprCall);
    n->callee = callee;
    n->is_comptime = is_comptime;
    NodeVec args = NodeVec_create(p->arena, 4);
    if (!check(p, TOKEN_RPAREN)) {
        do {
            NodeVec_push(&args, parse_expr(p));
        } while (match_tok(p, TOKEN_COMMA) && !check(p, TOKEN_RPAREN));
    }
    expect(p, TOKEN_RPAREN, "')'");
    n->args = args.data;
    n->arg_count = args.len;
    return &n->base;
}

// @cimport ( STRING [ , .{ .key = {"s", ...} ... } ] )
// Only the `defines` option is captured; other keys are parsed and ignored.
static Node *parse_cimport(Parser *p, SourceLoc loc) {
    expect(p, TOKEN_LPAREN, "'('");
    Token path = expect(p, TOKEN_STRING_LIT, "header path string");

    SvVec defines = SvVec_create(p->arena, 4);
    if (match_tok(p, TOKEN_COMMA)) {
        expect(p, TOKEN_DOT, "'.' before options '{'");
        expect(p, TOKEN_LBRACE, "'{'");
        while (!check(p, TOKEN_RBRACE) && !at_end(p)) {
            expect(p, TOKEN_DOT, "'.' before option name");
            Token key = expect(p, TOKEN_IDENT, "option name");
            expect(p, TOKEN_EQ, "'='");
            expect(p, TOKEN_LBRACE, "'{'");
            bool is_defines = (key.lexeme.len == 7 && strncmp(key.lexeme.data, "defines", 7) == 0);
            while (!check(p, TOKEN_RBRACE) && !at_end(p)) {
                Token s = expect(p, TOKEN_STRING_LIT, "string literal");
                if (is_defines) SvVec_push(&defines, s.lexeme);
                match_tok(p, TOKEN_COMMA);
            }
            expect(p, TOKEN_RBRACE, "'}'");
            match_tok(p, TOKEN_COMMA);
        }
        expect(p, TOKEN_RBRACE, "'}'");
    }
    expect(p, TOKEN_RPAREN, "')'");

    ExprCImport *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_CIMPORT, loc), ExprCImport);
    n->path = path.lexeme;
    n->defines = defines.data;
    n->define_count = defines.len;
    return &n->base;
}

// Struct literal:  Name{ .field = expr, ... }   (Name may be empty → anonymous).
static Node *parse_struct_literal(Parser *p, StringView type_name, SourceLoc loc) {
    expect(p, TOKEN_LBRACE, "'{'");
    SvVec names = SvVec_create(p->arena, 4);
    NodeVec values = NodeVec_create(p->arena, 4);
    while (!check(p, TOKEN_RBRACE) && !at_end(p)) {
        expect(p, TOKEN_DOT, "'.' before field name");
        Token fname = expect(p, TOKEN_IDENT, "field name");
        expect(p, TOKEN_EQ, "'='");
        Node *val = parse_expr(p);
        SvVec_push(&names, fname.lexeme);
        NodeVec_push(&values, val);
        match_tok(p, TOKEN_COMMA);
    }
    expect(p, TOKEN_RBRACE, "'}'");

    ExprStructLit *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_STRUCT_LIT, loc), ExprStructLit);
    n->type_name = type_name;
    n->is_anonymous = (type_name.len == 0);
    n->field_names = names.data;
    n->field_values = values.data;
    n->field_count = names.len;
    return &n->base;
}

static Node *parse_primary(Parser *p) {
    Token t = peek(p);
    SourceLoc loc = loc_of(p, t);

    switch (t.type) {
        case TOKEN_INT_LIT: {
            advance(p);
            ExprIntLit *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_INT_LIT, loc), ExprIntLit);
            n->value = strtoll(t.lexeme.data, NULL, 10);
            return &n->base;
        }
        case TOKEN_FLOAT_LIT: {
            advance(p);
            ExprFloatLit *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_FLOAT_LIT, loc), ExprFloatLit);
            n->value = strtod(t.lexeme.data, NULL);
            return &n->base;
        }
        case TOKEN_STRING_LIT: {
            advance(p);
            ExprStringLit *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_STRING_LIT, loc), ExprStringLit);
            n->value = t.lexeme;
            n->is_raw = false;
            return &n->base;
        }
        case TOKEN_TRUE:
        case TOKEN_FALSE: {
            advance(p);
            ExprBoolLit *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_BOOL_LIT, loc), ExprBoolLit);
            n->value = (t.type == TOKEN_TRUE);
            return &n->base;
        }
        case TOKEN_NULL: {
            advance(p);
            return ast_alloc_node(p->arena, NODE_EXPR_NULL_LIT, loc);
        }
        case TOKEN_IDENT: {
            advance(p);
            // Struct literal  Name{ .x = ... }  — disambiguated from a block by
            // requiring the brace to be followed by '.' (a field initializer).
            if (check(p, TOKEN_LBRACE) && peek2(p).type == TOKEN_DOT) {
                return parse_struct_literal(p, t.lexeme, loc);
            }
            ExprIdent *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_IDENT, loc), ExprIdent);
            n->name = t.lexeme;
            return &n->base;
        }
        case TOKEN_PANIC: {
            advance(p);
            ExprIdent *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_IDENT, loc), ExprIdent);
            n->name = t.lexeme;
            return &n->base;
        }
        case TOKEN_AT: {
            // Builtin reference: @name — fold '@' and the identifier into one ident.
            advance(p);
            Token name = expect(p, TOKEN_IDENT, "builtin name after '@'");
            if (name.lexeme.len == 7 && strncmp(name.lexeme.data, "cimport", 7) == 0 && check(p, TOKEN_LPAREN)) {
                return parse_cimport(p, loc);
            }
            ExprIdent *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_IDENT, loc), ExprIdent);
            // '@' and the identifier are contiguous in the source buffer.
            size_t len = (size_t)((name.lexeme.data + name.lexeme.len) - t.lexeme.data);
            n->name = string_view_create(t.lexeme.data, len);
            return &n->base;
        }
        case TOKEN_LPAREN: {
            // Cast  "(" type ")" expr   vs   grouping "(" expr ")".
            // Treat as a cast only when the inner clearly starts a type.
            Token inner = peek2(p);
            if (is_primitive_type_tok(inner.type) ||
                inner.type == TOKEN_STAR || inner.type == TOKEN_LBRACKET || inner.type == TOKEN_FN) {
                advance(p); // (
                Node *target = parse_type(p);
                expect(p, TOKEN_RPAREN, "')'");
                Node *operand = parse_expr(p);
                ExprCast *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_CAST, loc), ExprCast);
                n->target_type = target;
                n->expr = operand;
                return &n->base;
            }
            advance(p); // (
            Node *e = parse_expr(p);
            expect(p, TOKEN_RPAREN, "')'");
            return e;
        }
        default:
            parser_error(p, t, "expected an expression, got '%.*s'", (int)t.lexeme.len, t.lexeme.data);
            advance(p);
            return NULL;
    }
}

static Node *parse_postfix(Parser *p) {
    Node *e = parse_primary(p);

    for (;;) {
        Token t = peek(p);
        SourceLoc loc = loc_of(p, t);
        switch (t.type) {
            case TOKEN_DOT: {
                advance(p);
                Token field = expect(p, TOKEN_IDENT, "field name after '.'");
                ExprFieldAccess *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_FIELD_ACCESS, loc), ExprFieldAccess);
                n->object = e; n->field = field.lexeme; n->is_pointer = false;
                e = &n->base;
                break;
            }
            case TOKEN_ARROW: {
                advance(p);
                Token field = expect(p, TOKEN_IDENT, "field name after '->'");
                ExprFieldAccess *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_FIELD_ACCESS, loc), ExprFieldAccess);
                n->object = e; n->field = field.lexeme; n->is_pointer = true;
                e = &n->base;
                break;
            }
            case TOKEN_COLON_COLON: {
                advance(p);
                Token name = expect(p, TOKEN_IDENT, "name after '::'");
                ExprPath *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_PATH, loc), ExprPath);
                n->scope = e; n->name = name.lexeme;
                e = &n->base;
                break;
            }
            case TOKEN_LBRACKET: {
                advance(p);
                Node *idx = parse_expr(p);
                expect(p, TOKEN_RBRACKET, "']'");
                ExprIndex *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_INDEX, loc), ExprIndex);
                n->object = e; n->index = idx;
                e = &n->base;
                break;
            }
            case TOKEN_LPAREN: {
                advance(p);
                e = parse_arg_list_call(p, e, loc, false);
                break;
            }
            case TOKEN_PLUS_PLUS:
            case TOKEN_MINUS_MINUS:
            case TOKEN_BANG: {
                advance(p);
                ExprPostfix *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_POSTFIX, loc), ExprPostfix);
                n->expr = e; n->op = t.lexeme;
                e = &n->base;
                break;
            }
            default:
                return e;
        }
    }
}

static Node *parse_unary(Parser *p) {
    Token t = peek(p);
    switch (t.type) {
        case TOKEN_MINUS:
        case TOKEN_BANG:
        case TOKEN_STAR:
        case TOKEN_AMP:
        case TOKEN_TILDE: {
            advance(p);
            SourceLoc loc = loc_of(p, t);
            ExprUnaryOp *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_UNARY_OP, loc), ExprUnaryOp);
            n->op = t.lexeme;
            n->is_prefix = true;
            n->operand = parse_unary(p);
            return &n->base;
        }
        default:
            return parse_postfix(p);
    }
}

// Binary precedence levels, lowest first. Each entry: the token types at that
// level. We climb explicitly to mirror the grammar.
static Node *parse_mul(Parser *p) {
    Node *left = parse_unary(p);
    while (check(p, TOKEN_STAR) || check(p, TOKEN_SLASH) || check(p, TOKEN_MOD)) {
        Token op = advance(p);
        left = make_binary(p, left, op.lexeme, parse_unary(p), loc_of(p, op));
    }
    return left;
}
static Node *parse_add(Parser *p) {
    Node *left = parse_mul(p);
    while (check(p, TOKEN_PLUS) || check(p, TOKEN_MINUS)) {
        Token op = advance(p);
        left = make_binary(p, left, op.lexeme, parse_mul(p), loc_of(p, op));
    }
    return left;
}
static Node *parse_shift(Parser *p) {
    Node *left = parse_add(p);
    while (check(p, TOKEN_LESS_LESS) || check(p, TOKEN_GREATER_GREATER)) {
        Token op = advance(p);
        left = make_binary(p, left, op.lexeme, parse_add(p), loc_of(p, op));
    }
    return left;
}
static Node *parse_cmp(Parser *p) {
    Node *left = parse_shift(p);
    while (check(p, TOKEN_LESS) || check(p, TOKEN_GREATER) ||
           check(p, TOKEN_LESS_EQ) || check(p, TOKEN_GREATER_EQ)) {
        Token op = advance(p);
        left = make_binary(p, left, op.lexeme, parse_shift(p), loc_of(p, op));
    }
    return left;
}
static Node *parse_eq(Parser *p) {
    Node *left = parse_cmp(p);
    while (check(p, TOKEN_EQ_EQ) || check(p, TOKEN_BANG_EQ)) {
        Token op = advance(p);
        left = make_binary(p, left, op.lexeme, parse_cmp(p), loc_of(p, op));
    }
    return left;
}
static Node *parse_band(Parser *p) {
    Node *left = parse_eq(p);
    while (check(p, TOKEN_AMP)) {
        Token op = advance(p);
        left = make_binary(p, left, op.lexeme, parse_eq(p), loc_of(p, op));
    }
    return left;
}
static Node *parse_bxor(Parser *p) {
    Node *left = parse_band(p);
    while (check(p, TOKEN_CARET)) {
        Token op = advance(p);
        left = make_binary(p, left, op.lexeme, parse_band(p), loc_of(p, op));
    }
    return left;
}
static Node *parse_bor(Parser *p) {
    Node *left = parse_bxor(p);
    while (check(p, TOKEN_PIPE)) {
        Token op = advance(p);
        left = make_binary(p, left, op.lexeme, parse_bxor(p), loc_of(p, op));
    }
    return left;
}
static Node *parse_and(Parser *p) {
    Node *left = parse_bor(p);
    while (check(p, TOKEN_AMP_AMP)) {
        Token op = advance(p);
        left = make_binary(p, left, op.lexeme, parse_bor(p), loc_of(p, op));
    }
    return left;
}
static Node *parse_or(Parser *p) {
    Node *left = parse_and(p);
    while (check(p, TOKEN_PIPE_PIPE)) {
        Token op = advance(p);
        left = make_binary(p, left, op.lexeme, parse_and(p), loc_of(p, op));
    }
    return left;
}

static Node *parse_expr(Parser *p) {
    if (check(p, TOKEN_COMPTIME)) {
        advance(p);
        Node *inner = parse_expr(p);
        if (inner && NODE_IS(inner, NODE_EXPR_CALL))
            NODE_CAST_UNSAFE(inner, ExprCall)->is_comptime = true;
        return inner;
    }
    if (check(p, TOKEN_TRY)) {
        Token kw = advance(p);
        ExprTry *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_EXPR_TRY, loc_of(p, kw)), ExprTry);
        n->expr = parse_expr(p);
        return &n->base;
    }
    return parse_or(p);
}

// ============================================================================
// Statements
// ============================================================================

static bool is_assign_op(TokenType t) {
    switch (t) {
        case TOKEN_EQ: case TOKEN_PLUS_EQ: case TOKEN_MINUS_EQ:
        case TOKEN_STAR_EQ: case TOKEN_SLASH_EQ: case TOKEN_MOD_EQ:
        case TOKEN_LESS_LESS_EQ: case TOKEN_GREATER_GREATER_EQ:
            return true;
        default: return false;
    }
}

// Binary operator string for a compound assignment, or NULL for plain '='.
static const char *compound_op(TokenType t) {
    switch (t) {
        case TOKEN_PLUS_EQ: return "+";
        case TOKEN_MINUS_EQ: return "-";
        case TOKEN_STAR_EQ: return "*";
        case TOKEN_SLASH_EQ: return "/";
        case TOKEN_MOD_EQ: return "%";
        case TOKEN_LESS_LESS_EQ: return "<<";
        case TOKEN_GREATER_GREATER_EQ: return ">>";
        default: return NULL;
    }
}

static Node *parse_var_decl(Parser *p) {
    Token kw = advance(p); // var | let | const
    SourceLoc loc = loc_of(p, kw);
    Token name = expect(p, TOKEN_IDENT, "variable name");

    StmtVarDecl *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_STMT_VAR_DECL, loc), StmtVarDecl);
    n->name = name.lexeme;
    n->is_const = (kw.type != TOKEN_VAR);
    n->type_annotation = match_tok(p, TOKEN_COLON) ? parse_type(p) : NULL;
    n->initializer = match_tok(p, TOKEN_EQ) ? parse_expr(p) : NULL;
    expect(p, TOKEN_SEMICOLON, "';'");
    return &n->base;
}

static Node *parse_if(Parser *p) {
    Token kw = advance(p); // if
    SourceLoc loc = loc_of(p, kw);
    StmtIf *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_STMT_IF, loc), StmtIf);
    n->condition = parse_expr(p);

    NodeVec then_body = NodeVec_create(p->arena, 4);
    parse_block(p, &then_body);
    n->then_body = then_body.data;
    n->then_len = then_body.len;

    if (match_tok(p, TOKEN_ELSE)) {
        NodeVec else_body = NodeVec_create(p->arena, 4);
        if (check(p, TOKEN_IF)) {
            NodeVec_push(&else_body, parse_if(p));
        } else {
            parse_block(p, &else_body);
        }
        n->else_body = else_body.data;
        n->else_len = else_body.len;
    }
    return &n->base;
}

static Node *parse_while(Parser *p) {
    Token kw = advance(p); // while
    SourceLoc loc = loc_of(p, kw);
    StmtWhile *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_STMT_WHILE, loc), StmtWhile);
    n->condition = parse_expr(p);
    NodeVec body = NodeVec_create(p->arena, 4);
    parse_block(p, &body);
    n->body = body.data;
    n->body_len = body.len;
    return &n->base;
}

static Node *parse_for(Parser *p) {
    Token kw = advance(p); // for
    SourceLoc loc = loc_of(p, kw);
    StmtFor *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_STMT_FOR, loc), StmtFor);
    Token var = expect(p, TOKEN_IDENT, "loop variable");
    n->var_name = var.lexeme;
    expect(p, TOKEN_IN, "'in'");
    n->iter_expr = parse_expr(p);
    NodeVec body = NodeVec_create(p->arena, 4);
    parse_block(p, &body);
    n->body = body.data;
    n->body_len = body.len;
    return &n->base;
}

static Node *parse_return(Parser *p) {
    Token kw = advance(p); // return
    SourceLoc loc = loc_of(p, kw);
    StmtReturn *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_STMT_RETURN, loc), StmtReturn);
    n->value = check(p, TOKEN_SEMICOLON) ? NULL : parse_expr(p);
    expect(p, TOKEN_SEMICOLON, "';'");
    return &n->base;
}

static Node *parse_break_continue(Parser *p, bool is_break) {
    Token kw = advance(p);
    SourceLoc loc = loc_of(p, kw);
    NodeKind kind = is_break ? NODE_STMT_BREAK : NODE_STMT_CONTINUE;
    StmtBreakContinue *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, kind, loc), StmtBreakContinue);
    n->label = check(p, TOKEN_IDENT) ? advance(p).lexeme : string_view_create("", 0);
    expect(p, TOKEN_SEMICOLON, "';'");
    return &n->base;
}

static Node *parse_block_stmt(Parser *p) {
    Token brace = peek(p);
    SourceLoc loc = loc_of(p, brace);
    NodeVec body = NodeVec_create(p->arena, 8);
    parse_block(p, &body);
    StmtBlock *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_STMT_BLOCK, loc), StmtBlock);
    n->statements = body.data;
    n->stmt_count = body.len;
    return &n->base;
}

static Node *parse_expr_statement(Parser *p) {
    Token start = peek(p);
    SourceLoc loc = loc_of(p, start);
    Node *expr = parse_expr(p);

    if (is_assign_op(peek(p).type)) {
        Token op = advance(p);
        Node *rhs = parse_expr(p);
        Node *value = rhs;
        const char *cop = compound_op(op.type);
        if (cop) {
            // desugar  x += y  →  x = x + y  (no dedicated op field on StmtAssignment)
            value = make_binary(p, expr, string_view_create(cop, strlen(cop)), rhs, loc_of(p, op));
        }
        expect(p, TOKEN_SEMICOLON, "';'");
        StmtAssignment *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_STMT_ASSIGNMENT, loc), StmtAssignment);
        n->target = expr;
        n->value = value;
        return &n->base;
    }

    expect(p, TOKEN_SEMICOLON, "';'");
    StmtExprStmt *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_STMT_EXPR, loc), StmtExprStmt);
    n->expr = expr;
    return &n->base;
}

static Node *parse_pattern(Parser *p) {
    Token t = peek(p);
    SourceLoc loc = loc_of(p, t);

    switch (t.type) {
        case TOKEN_LPAREN: {                       // tuple pattern
            advance(p);
            NodeVec elems = NodeVec_create(p->arena, 4);
            if (!check(p, TOKEN_RPAREN)) {
                do { NodeVec_push(&elems, parse_pattern(p)); }
                while (match_tok(p, TOKEN_COMMA) && !check(p, TOKEN_RPAREN));
            }
            expect(p, TOKEN_RPAREN, "')'");
            PatternTuple *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_PATTERN_TUPLE, loc), PatternTuple);
            n->elements = elems.data; n->count = elems.len;
            return &n->base;
        }
        case TOKEN_INT_LIT: case TOKEN_FLOAT_LIT: case TOKEN_STRING_LIT:
        case TOKEN_TRUE: case TOKEN_FALSE: case TOKEN_NULL: {
            Node *lit = parse_primary(p);          // literal node
            PatternLiteral *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_PATTERN_LITERAL, loc), PatternLiteral);
            n->literal = lit;
            return &n->base;
        }
        case TOKEN_IDENT: {
            if (t.lexeme.len == 1 && t.lexeme.data[0] == '_') {   // wildcard
                advance(p);
                return ast_alloc_node(p->arena, NODE_PATTERN_WILDCARD, loc);
            }
            advance(p);
            if (match_tok(p, TOKEN_COLON_COLON)) {                // Enum::Variant
                Token var = expect(p, TOKEN_IDENT, "variant after '::'");
                PatternPath *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_PATTERN_PATH, loc), PatternPath);
                n->type_name = t.lexeme; n->variant = var.lexeme;
                return &n->base;
            }
            if (match_tok(p, TOKEN_LBRACE)) {                     // Type{ a, b }
                SvVec fields = SvVec_create(p->arena, 4);
                while (!check(p, TOKEN_RBRACE) && !at_end(p)) {
                    Token f = expect(p, TOKEN_IDENT, "field name");
                    SvVec_push(&fields, f.lexeme);
                    match_tok(p, TOKEN_COMMA);
                }
                expect(p, TOKEN_RBRACE, "'}'");
                PatternStruct *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_PATTERN_STRUCT, loc), PatternStruct);
                n->type_name = t.lexeme; n->fields = fields.data; n->field_count = fields.len;
                return &n->base;
            }
            PatternIdent *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_PATTERN_IDENT, loc), PatternIdent);
            n->name = t.lexeme;                                   // variable bind
            return &n->base;
        }
        default:
            parser_error(p, t, "expected a pattern, got '%.*s'", (int)t.lexeme.len, t.lexeme.data);
            advance(p);
            return NULL;
    }
}

static Node *parse_match(Parser *p) {
    Token kw = advance(p); // match
    SourceLoc loc = loc_of(p, kw);
    StmtMatch *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_STMT_MATCH, loc), StmtMatch);
    n->subject = parse_expr(p);
    expect(p, TOKEN_LBRACE, "'{'");

    MArmVec arms = MArmVec_create(p->arena, 4);
    while (!check(p, TOKEN_RBRACE) && !at_end(p)) {
        size_t before = p->pos;
        MatchArm arm = {0};
        arm.pattern = parse_pattern(p);
        expect(p, TOKEN_FAT_ARROW, "'=>'");
        NodeVec body = NodeVec_create(p->arena, 4);
        parse_block(p, &body);
        arm.body = body.data;
        arm.body_len = body.len;
        MArmVec_push(&arms, arm);
        match_tok(p, TOKEN_COMMA);
        if (p->pos == before) synchronize(p);
    }
    expect(p, TOKEN_RBRACE, "'}'");
    n->arms = arms.data;
    n->arm_count = arms.len;
    return &n->base;
}

static Node *parse_block_stmt(Parser *p);  // fwd

static Node *parse_defer(Parser *p) {
    Token kw = advance(p); // defer
    SourceLoc loc = loc_of(p, kw);
    StmtDefer *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_STMT_DEFER, loc), StmtDefer);
    n->body = check(p, TOKEN_LBRACE) ? parse_block_stmt(p) : parse_statement(p);
    return &n->base;
}

static Node *parse_when(Parser *p) {
    Token kw = advance(p); // when
    SourceLoc loc = loc_of(p, kw);
    WBranchVec branches = WBranchVec_create(p->arena, 4);

    WhenBranch first = {0};
    first.condition = parse_expr(p);
    NodeVec body = NodeVec_create(p->arena, 4);
    parse_block(p, &body);
    first.body = body.data; first.body_len = body.len;
    WBranchVec_push(&branches, first);

    while (match_tok(p, TOKEN_ELSE)) {
        WhenBranch b = {0};
        bool is_else_when = match_tok(p, TOKEN_WHEN);
        if (is_else_when) b.condition = parse_expr(p);
        NodeVec b_body = NodeVec_create(p->arena, 4);
        parse_block(p, &b_body);
        b.body = b_body.data; b.body_len = b_body.len;
        WBranchVec_push(&branches, b);
        if (!is_else_when) break;  // trailing `else` terminates the chain
    }

    StmtWhen *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_STMT_WHEN, loc), StmtWhen);
    n->branches = branches.data;
    n->branch_count = branches.len;
    return &n->base;
}

static Node *parse_statement(Parser *p) {
    switch (peek(p).type) {
        case TOKEN_VAR:
        case TOKEN_LET:
        case TOKEN_CONST:   return parse_var_decl(p);
        case TOKEN_IF:      return parse_if(p);
        case TOKEN_WHILE:   return parse_while(p);
        case TOKEN_FOR:     return parse_for(p);
        case TOKEN_RETURN:  return parse_return(p);
        case TOKEN_BREAK:   return parse_break_continue(p, true);
        case TOKEN_CONTINUE:return parse_break_continue(p, false);
        case TOKEN_MATCH:   return parse_match(p);
        case TOKEN_DEFER:   return parse_defer(p);
        case TOKEN_WHEN:    return parse_when(p);
        case TOKEN_LBRACE:  return parse_block_stmt(p);
        default:            return parse_expr_statement(p);
    }
}

// Parse '{' { statement } '}' into `out`. Returns false if the opening brace
// was missing.
static bool parse_block(Parser *p, NodeVec *out) {
    if (!check(p, TOKEN_LBRACE)) {
        expect(p, TOKEN_LBRACE, "'{'");
        return false;
    }
    advance(p); // {
    while (!check(p, TOKEN_RBRACE) && !at_end(p)) {
        size_t before = p->pos;
        Node *s = parse_statement(p);
        if (s) NodeVec_push(out, s);
        if (p->pos == before) { synchronize(p); }  // guard against no-progress
    }
    expect(p, TOKEN_RBRACE, "'}'");
    return true;
}

// ============================================================================
// Declarations
// ============================================================================

static Node *parse_function(Parser *p, bool is_extern, bool is_inline, SourceLoc loc) {
    expect(p, TOKEN_FN, "'fn'");
    Token name = expect(p, TOKEN_IDENT, "function name");

    DeclFunction *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_DECL_FUNCTION, loc), DeclFunction);
    n->name = name.lexeme;
    n->is_extern = is_extern;
    n->is_inline = is_inline;

    expect(p, TOKEN_LPAREN, "'('");
    ParamVec params = ParamVec_create(p->arena, 4);
    if (!check(p, TOKEN_RPAREN)) {
        do {
            Param param = {0};
            param.is_comptime = match_tok(p, TOKEN_COMPTIME);
            Token pname = expect(p, TOKEN_IDENT, "parameter name");
            param.name = pname.lexeme;
            expect(p, TOKEN_COLON, "':'");
            param.is_variadic = match_tok(p, TOKEN_ELLIPSIS);
            param.type = parse_type(p);
            ParamVec_push(&params, param);
        } while (match_tok(p, TOKEN_COMMA) && !check(p, TOKEN_RPAREN));
    }
    expect(p, TOKEN_RPAREN, "')'");
    n->params = params.data;
    n->param_count = params.len;

    n->return_type = parse_type(p);

    NodeVec body = NodeVec_create(p->arena, 8);
    parse_block(p, &body);
    n->body = body.data;
    n->body_len = body.len;
    return &n->base;
}

static Node *parse_struct(Parser *p, StringView name, SourceLoc loc) {
    advance(p); // struct
    expect(p, TOKEN_LBRACE, "'{'");
    SvVec names = SvVec_create(p->arena, 8);
    NodeVec types = NodeVec_create(p->arena, 8);
    NodeVec methods = NodeVec_create(p->arena, 4);

    while (!check(p, TOKEN_RBRACE) && !at_end(p)) {
        if (check(p, TOKEN_FN)) {
            NodeVec_push(&methods, parse_function(p, false, false, loc_of(p, peek(p))));
            continue;
        }
        Token fname = expect(p, TOKEN_IDENT, "field name");
        expect(p, TOKEN_COLON, "':'");
        Node *ftype = parse_type(p);
        SvVec_push(&names, fname.lexeme);
        NodeVec_push(&types, ftype);
        match_tok(p, TOKEN_COMMA);
    }
    expect(p, TOKEN_RBRACE, "'}'");
    expect(p, TOKEN_SEMICOLON, "';'");

    DeclStruct *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_DECL_STRUCT, loc), DeclStruct);
    n->name = name;
    n->field_names = names.data;
    n->field_types = types.data;
    n->field_count = names.len;
    n->methods = methods.data;
    n->method_count = methods.len;
    return &n->base;
}

static Node *parse_enum(Parser *p, StringView name, SourceLoc loc) {
    advance(p); // enum
    expect(p, TOKEN_LBRACE, "'{'");
    EVarVec variants = EVarVec_create(p->arena, 8);

    while (!check(p, TOKEN_RBRACE) && !at_end(p)) {
        Token vname = expect(p, TOKEN_IDENT, "variant name");
        EnumVariant v = {0};
        v.name = vname.lexeme;
        v.kind = ENUM_VARIANT_PLAIN;

        if (match_tok(p, TOKEN_LPAREN)) {            // tuple-like: Name(type)
            v.kind = ENUM_VARIANT_TUPLE;
            v.tuple_type = parse_type(p);
            expect(p, TOKEN_RPAREN, "')'");
        } else if (match_tok(p, TOKEN_LBRACE)) {     // algebraic: Name{ a: T, ... }
            v.kind = ENUM_VARIANT_STRUCT;
            SvVec fnames = SvVec_create(p->arena, 4);
            NodeVec ftypes = NodeVec_create(p->arena, 4);
            while (!check(p, TOKEN_RBRACE) && !at_end(p)) {
                Token fn = expect(p, TOKEN_IDENT, "field name");
                expect(p, TOKEN_COLON, "':'");
                NodeVec_push(&ftypes, parse_type(p));
                SvVec_push(&fnames, fn.lexeme);
                match_tok(p, TOKEN_COMMA);
            }
            expect(p, TOKEN_RBRACE, "'}'");
            v.field_names = fnames.data;
            v.field_types = ftypes.data;
            v.field_count = fnames.len;
        } else if (match_tok(p, TOKEN_EQ)) {         // discriminant: Name = expr
            v.kind = ENUM_VARIANT_DISCRIMINANT;
            v.discriminant = parse_expr(p);
        }

        EVarVec_push(&variants, v);
        match_tok(p, TOKEN_COMMA);
    }
    expect(p, TOKEN_RBRACE, "'}'");
    expect(p, TOKEN_SEMICOLON, "';'");

    DeclEnum *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_DECL_ENUM, loc), DeclEnum);
    n->name = name;
    n->variants = variants.data;
    n->variant_count = variants.len;
    return &n->base;
}

static Node *parse_union(Parser *p, StringView name, SourceLoc loc) {
    advance(p); // union
    expect(p, TOKEN_LBRACE, "'{'");
    SvVec names = SvVec_create(p->arena, 8);
    NodeVec types = NodeVec_create(p->arena, 8);

    while (!check(p, TOKEN_RBRACE) && !at_end(p)) {
        Token fname = expect(p, TOKEN_IDENT, "field name");
        expect(p, TOKEN_COLON, "':'");
        NodeVec_push(&types, parse_type(p));
        SvVec_push(&names, fname.lexeme);
        match_tok(p, TOKEN_COMMA);
    }
    expect(p, TOKEN_RBRACE, "'}'");
    expect(p, TOKEN_SEMICOLON, "';'");

    DeclUnion *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_DECL_UNION, loc), DeclUnion);
    n->name = name;
    n->field_names = names.data;
    n->field_types = types.data;
    n->field_count = names.len;
    return &n->base;
}

// const NAME ...   — disambiguates struct / enum / type-alias / const var.
static Node *parse_const_decl(Parser *p) {
    Token kw = advance(p); // const
    SourceLoc loc = loc_of(p, kw);
    Token name = expect(p, TOKEN_IDENT, "name after 'const'");

    if (match_tok(p, TOKEN_COLON)) {
        // const NAME : type = expr ;   (typed constant)
        Node *type_ann = parse_type(p);
        expect(p, TOKEN_EQ, "'='");
        Node *init = parse_expr(p);
        expect(p, TOKEN_SEMICOLON, "';'");
        StmtVarDecl *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_STMT_VAR_DECL, loc), StmtVarDecl);
        n->name = name.lexeme; n->is_const = true;
        n->type_annotation = type_ann; n->initializer = init;
        return &n->base;
    }

    expect(p, TOKEN_EQ, "'='");

    if (check(p, TOKEN_STRUCT)) return parse_struct(p, name.lexeme, loc);
    if (check(p, TOKEN_ENUM))   return parse_enum(p, name.lexeme, loc);
    if (check(p, TOKEN_UNION))  return parse_union(p, name.lexeme, loc);

    if (at_type_start(p)) {
        // type alias:  const NAME = <type> ;
        Node *aliased = parse_type(p);
        expect(p, TOKEN_SEMICOLON, "';'");
        DeclTypeAlias *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_DECL_TYPE_ALIAS, loc), DeclTypeAlias);
        n->name = name.lexeme;
        n->aliased_type = aliased;
        return &n->base;
    }

    // const NAME = expr ;   (constant binding)
    Node *init = parse_expr(p);
    expect(p, TOKEN_SEMICOLON, "';'");
    StmtVarDecl *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_STMT_VAR_DECL, loc), StmtVarDecl);
    n->name = name.lexeme; n->is_const = true;
    n->initializer = init;
    return &n->base;
}

static Node *parse_import(Parser *p) {
    Token kw = advance(p); // import
    SourceLoc loc = loc_of(p, kw);
    Token path = expect(p, TOKEN_STRING_LIT, "module path string");

    DeclImport *n = NODE_CAST_UNSAFE(ast_alloc_node(p->arena, NODE_DECL_IMPORT, loc), DeclImport);
    n->path = path.lexeme;
    n->is_external = false;

    if (match_tok(p, TOKEN_LBRACE)) {
        SvVec sel = SvVec_create(p->arena, 4);
        if (!check(p, TOKEN_RBRACE)) {
            do {
                Token id = expect(p, TOKEN_IDENT, "imported name");
                SvVec_push(&sel, id.lexeme);
            } while (match_tok(p, TOKEN_COMMA) && !check(p, TOKEN_RBRACE));
        }
        expect(p, TOKEN_RBRACE, "'}'");
        n->selective = sel.data;
        n->selective_count = sel.len;
    }
    expect(p, TOKEN_SEMICOLON, "';'");
    return &n->base;
}

static Node *parse_top_decl(Parser *p) {
    Token t = peek(p);
    SourceLoc loc = loc_of(p, t);

    // Attributes before a function:  @cdecl @inline fn ...
    if (t.type == TOKEN_AT) {
        bool is_extern = false, is_inline = false;
        while (check(p, TOKEN_AT)) {
            advance(p);
            Token attr = expect(p, TOKEN_IDENT, "attribute name");
            if (attr.lexeme.len == 6 && strncmp(attr.lexeme.data, "extern", 6) == 0) is_extern = true;
            else if (attr.lexeme.len == 6 && strncmp(attr.lexeme.data, "inline", 6) == 0) is_inline = true;
            // other attributes (@cdecl, @stdcall, ...) accepted but not modelled
        }
        return parse_function(p, is_extern, is_inline, loc);
    }

    switch (t.type) {
        case TOKEN_FN:     return parse_function(p, false, false, loc);
        case TOKEN_CONST:  return parse_const_decl(p);
        case TOKEN_VAR:
        case TOKEN_LET:    return parse_var_decl(p);
        case TOKEN_IMPORT: return parse_import(p);
        case TOKEN_WHEN:   return parse_when(p);
        default:
            parser_error(p, t, "expected a top-level declaration, got '%.*s'",
                         (int)t.lexeme.len, t.lexeme.data);
            advance(p);
            return NULL;
    }
}

// ============================================================================
// Public API
// ============================================================================

void parser_init(Parser *p, TokenVec tokens, Arena *arena, const char *filename) {
    if (!p) return;
    p->tokens = tokens;
    p->pos = 0;
    p->arena = arena;
    p->filename = filename;
    p->errors = ErrorVec_create(arena, 8);
    p->had_error = false;
}

Program parse_program(Parser *p) {
    Program prog = {0};
    prog.filename = p->filename;

    NodeVec decls = NodeVec_create(p->arena, 16);
    while (!at_end(p)) {
        size_t before = p->pos;
        Node *d = parse_top_decl(p);
        if (d) NodeVec_push(&decls, d);
        if (p->pos == before) synchronize(p);  // guarantee forward progress
    }
    prog.decls = decls.data;
    prog.decl_count = decls.len;
    return prog;
}
