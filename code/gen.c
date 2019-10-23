char *gen_buf = NULL;
size_t indent_newline;

#define genf(...)                                                                                  \
    (buf_printf(gen_buf, "%.*s", 2 * indent_newline, "                                  ")),       \
        (buf_printf(gen_buf, __VA_ARGS__))

const char *type_to_cdecl(Type *t, const char *gen);
const char *gen_expr(Expr *expr);
void gen_stmt(Stmt *stmt);

const char *aggregate_typedecl_to_cdecl(Type *t) {

    if (t->kind != TYPE_STRUCT) {
        assert(0);
    }
    // todo type_union

    genf("struct %s {\n", t->aggregate.name);
    indent_newline++;
    for (int i = 0; i < t->aggregate.num_fields; i++) {
        genf("%s;\n", type_to_cdecl(t->aggregate.fields[i].type, t->aggregate.fields[i].name));
    }
    indent_newline--;
    genf("};\n\n");
}

const char *funcdef_to_cdef(Decl *decl) {
    const char *args;

    if (decl->func_decl.num_func_args == 0) {
        args = strf("void");
    } else {
        args =
            type_to_cdecl(create_type(decl->func_decl.args[0].type), decl->func_decl.args[0].name);
    }

    for (int i = 1; i < decl->func_decl.num_func_args; i++) {
        args = strf(
            "%s, %s", args,
            type_to_cdecl(create_type(decl->func_decl.args[i].type), decl->func_decl.args[i].name));
    }

    return type_to_cdecl(create_type(decl->func_decl.type), strf("%s(%s)", decl->name, args));
}

const char *type_to_cdecl(Type *t, const char *gen) {
    switch (t->kind) {
    case TYPE_VOID:
        return strf("void %s", gen);
    case TYPE_INT:
        return strf("int %s", gen);
    case TYPE_PTR:
        return type_to_cdecl(t->ptr.elem, strf("*(%s)", gen));
    case TYPE_ARRAY:
        // (var foo (array (ptr int) 10)) --> int *(foo[10])
        if (gen == "") {
            return type_to_cdecl(t->array.elem, strf("[%d]", t->array.size));
        } else {
            return type_to_cdecl(t->array.elem, strf("(%s)[%d]", gen, t->array.size));
        }
    case TYPE_STRUCT: {
        // (var foo S*)
        return strf("struct %s %s", t->aggregate.name, gen);
    }
        // (var f (func () int)) --> int (f)()
    case TYPE_FUNC: {
        const char *args;
        if (t->func.num_args == 0) {
            args = strf("void");
        } else {
            args = type_to_cdecl(t->func.args[0], "");
        }

        for (int i = 1; i < t->func.num_args; i++) {
            args = strf("%s, %s", args, type_to_cdecl(t->func.args[i], ""));
        }

        return type_to_cdecl(t->func.ret_type, strf("(%s)(%s)", gen, args));
    }

    default:
        assert(0);
    }
}

void gen_decl(Sym *sym) {
    Decl *decl = sym->decl;

    if (!decl) {
        return;
    }

    switch (decl->kind) {
    case DECL_AGGREGATE: {
        if (decl->aggregate_decl.kind != AGGREGATE_STRUCT) {
            assert(0);
        }
        aggregate_typedecl_to_cdecl(sym->type);
        break;
    }
    default:
        assert(0);
    }
}

const char *gen_compound_val(CompoundVal *val) {
    switch (val->kind) {
    case SIMPLE_EXPR:
        return strf("%s", gen_expr(val->expr));
    case INDEX_EXPR:
        return strf("[%s] = %s", gen_expr(val->index.index), gen_expr(val->index.val));
    case NAME_EXPR:
        return strf(".%s = %s", val->name.name, gen_expr(val->name.val));
    default:
        assert(0);
    }
}

const char *gen_expr(Expr *expr) {

    if (!expr) {
        return "";
    }

    switch (expr->kind) {
    case EXPR_INT:
        return strf("%d", expr->int_val);
    case EXPR_UNARY:
        return strf("%s(%s)", token_kind_name(expr->unary_expr.op),
                    gen_expr(expr->unary_expr.expr));
    case EXPR_BINARY:
        return strf("(%s)%s(%s)", gen_expr(expr->binary_expr.left),
                    token_kind_name(expr->binary_expr.op), gen_expr(expr->binary_expr.right));
    case EXPR_NAME:
        return strf("%s", expr->name);
    case EXPR_STR:
        return strf("\"%s\"", expr->str_val);
    case EXPR_FIELD:
        return strf("(%s).%s", gen_expr(expr->field_expr.expr), expr->field_expr.name);
    case EXPR_INDEX:
        return strf("(%s)[%s]", gen_expr(expr->index_expr.expr), gen_expr(expr->index_expr.index));
    case EXPR_CALL: {
        if (!expr->call_expr.num_args) {
            return strf("(%s)()", gen_expr(expr->call_expr.expr));
        }

        const char *args = strf("%s", gen_expr(expr->call_expr.args[0]));
        for (int i = 1; i < expr->call_expr.num_args; i++) {
            args = strf("%s, %s", args, gen_expr(expr->call_expr.args[i]));
        }
        return strf("%s(%s)", gen_expr(expr->call_expr.expr), args);
    }
    case EXPR_CAST:
        return strf("(%s)%s", type_to_cdecl(create_type(expr->cast_expr.type), ""),
                    gen_expr(expr->cast_expr.expr));
    case EXPR_COMPOUND: {
        Type *type = create_type(expr->compound_expr.type);
        if (!expr->compound_expr.num_args) {
            if (type == type_void) {
                return strf("{}");
            } else {
                return strf("(%s){}", type_to_cdecl(type, ""));
            }
        }

        const char *args = gen_compound_val(expr->compound_expr.args[0]);

        for (int i = 1; i < expr->compound_expr.num_args; i++) {
            args = strf("%s, %s", args, gen_compound_val(expr->compound_expr.args[i]));
        }

        if (type == type_void) {
            return strf("{%s}", args);
        } else {
            return strf("(%s){%s}", type_to_cdecl(type, ""), args);
        }
    }
    case EXPR_SIZEOF_TYPE:
        return strf("sizeof(%s)", type_to_cdecl(create_type(expr->sizeof_type), ""));
    case EXPR_SIZEOF_EXPR:
        return strf("sizeof(%s)", gen_expr(expr->sizeof_expr));
    case EXPR_TERNARY:
        return strf("(%s)?(%s):(%s)", gen_expr(expr->ternary_expr.eval),
                    gen_expr(expr->ternary_expr.then_expr), gen_expr(expr->ternary_expr.else_expr));
        break;
    default:
        assert(0);
    }
}

void gen_stmtblock(StmtBlock block) {
    genf("{\n");
    indent_newline++;
    for (int i = 0; i < block.num_stmts; i++) {
        gen_stmt(block.stmts[i]);
    }
    indent_newline--;
    genf("}\n");
}

void gen_stmt(Stmt *stmt) {
    switch (stmt->kind) {
    case STMT_NONE:
        assert(0);

    case STMT_DECL:
        break;

    case STMT_INIT: {
        ResolvedExpr expr = resolve_expr(stmt->stmt_init.expr);
        genf("%s;\n", type_to_cdecl(expr.type, stmt->stmt_init.name));
        break;
    }
    case STMT_ASSIGN:
        genf("%s %s %s", gen_expr(stmt->stmt_assign.left_expr),
             token_kind_name(stmt->stmt_assign.op), gen_expr(stmt->stmt_assign.right_expr));
        break;
    case STMT_RETURN:
        if (stmt->stmt_return.expr) {
            genf("return %s;\n", gen_expr(stmt->stmt_return.expr));
        } else {
            genf("return;\n");
        }
        break;
    case STMT_BREAK:
        genf("break;\n");
        break;
    case STMT_CONTINUE:
        genf("continue;\n");
        break;
    case STMT_BLOCK: {
        gen_stmtblock(stmt->block);
        break;
    }
    case STMT_IF: {
        genf("if (%s)", gen_expr(stmt->stmt_if.expr));
        gen_stmtblock(stmt->stmt_if.if_block);
        for (int i = 0; i < stmt->stmt_if.num_elseifs; i++) {
            genf("else if(%s) ", stmt->stmt_if.else_ifs[i].expr);
            gen_stmtblock(stmt->stmt_if.else_ifs[i].block);
        }

        if (stmt->stmt_if.else_block.num_stmts) {
            genf("else {\n");
            gen_stmtblock(stmt->stmt_if.else_block);
            genf("}");
        }

        genf("\n");
        break;
    }
    case STMT_DO_WHILE:
        genf("do {\n");
        gen_stmtblock(stmt->stmt_while.block);
        genf("while (%s);\n", stmt->stmt_while.expr);
        break;
    case STMT_WHILE:
        genf("while (%s)", stmt->stmt_while.expr);
        gen_stmtblock(stmt->stmt_while.block);
        break;
    case STMT_FOR: {
        genf("for (");
        if (stmt->stmt_for.init) {
            Stmt *init = stmt->stmt_for.init;
            if (init->kind == STMT_INIT) {
                ResolvedExpr expr = resolve_expr(init->stmt_init.expr);
                genf("%s; ", type_to_cdecl(expr.type, init->stmt_init.name));
            } else if (init->kind == STMT_ASSIGN) {
                genf("%s %s %s;", gen_expr(init->stmt_assign.left_expr),
                     token_kind_name(init->stmt_assign.op), gen_expr(init->stmt_assign.right_expr));
            } else {
                assert(0);
            }
        } else {
            genf("; ");
        }

        genf("%s; ", gen_expr(stmt->stmt_for.cond));

        if (stmt->stmt_for.next) {
            Stmt *next = stmt->stmt_for.next;
            assert(next->kind == STMT_ASSIGN);
            genf("%s %s %s;", gen_expr(next->stmt_assign.left_expr),
                 token_kind_name(next->stmt_assign.op), gen_expr(next->stmt_assign.right_expr));
        } else {
            genf(";");
        }
        genf("}");
        gen_stmtblock(stmt->stmt_for.block);
        break;
    }
    case STMT_SWITCH: {
        /*         typedef struct SwitchCase { */
        /*     Expr **expr; */
        /*     size_t num_exprs; */
        /*     bool is_default; */
        /*     StmtBlock block; */
        /* }SwitchCase; */

        /* typedef struct StmtSwitch { */
        /*     Expr *expr; */
        /*     SwitchCase *cases; */
        /*     size_t num_cases; */
        /* } StmtSwitch; */

        genf("switch (%s) {", stmt->stmt_switch.expr);
        indent_newline++;
        for (int i = 0; i < stmt->stmt_switch.num_cases; i++) {
            for (int j = 0; j < stmt->stmt_switch.cases[i].num_exprs; j++) {
                genf("case %s: \n", gen_expr(stmt->stmt_switch.cases[i].expr[j]));
            }
            if (stmt->stmt_switch.cases[i].is_default) {
                genf("default:\n");
            }

            gen_stmtblock(stmt->stmt_switch.cases[i].block);
        }
        indent_newline--;
        genf("}");
        break;
    }
    default:
        assert(0);
        break;
    }
}

void gen_code(Sym *sym) {
    // global symbols which are essentialy base types like int,void, char, float
    // are not required.
    if (!sym->decl) {
        return;
    }

    switch (sym->kind) {
    case SYM_VAR: {
        const char *str = sym->name;
        if (sym->type->kind == TYPE_FUNC) {
            str = strf("*%s", str);
        }

        genf("%s", type_to_cdecl(sym->type, str));

        if (sym->decl->var_decl.expr) {
            genf(" = %s;\n", gen_expr(sym->decl->var_decl.expr));
        } else {
            genf(";\n");
        }

        break;
    }
    case SYM_CONST: {
        genf("const %s = %d;\n", type_to_cdecl(sym->type, sym->name), sym->val);
        break;
    }
    case SYM_TYPE: {
        gen_decl(sym);
        break;
    }
    case SYM_FUNC: {
        genf("%s{\n", funcdef_to_cdef(sym->decl));

        indent_newline++;
        for (int i = 0; i < sym->decl->func_decl.block.num_stmts; i++) {
            gen_stmt(sym->decl->func_decl.block.stmts[i]);
        }
        indent_newline--;
        genf("}\n\n");
        break;
    }
    }
}

void gen_test() {
    const char *decl[] = {
        "struct T {t :S;}",

        "struct S {a,b:int; c,d: int[4];}",

        "var testptr :int *",

        "var testval = -10", "var t :S",

        "var p :T",

        "var q :S[10]",

        "var ppp = 10",

        "var llll = S{a=10}",

        "var mmmm :int[4] = {[0] = 1}"

        "var xxx = cast(int, q)",

        "struct Vector{x,y : int;}",

        "var v = Vector{1,2}",

        "var vs = (:int[2]){[0]=1}",

        "var gs = (:int[2]){}",

        "const r = 10",

        "var f :func():int",

        "func foo():int {i := 0;}", "func bar() {j:=0;}",

        "var m = sizeof(:int)", "var qq = sizeof(m)", "var mm = (m==0)? qq : m",

        // test stmts
        "func test1(i:int, j: int, t :T*) :int {return i;}",

        "func test2(i:int, j: int, t :T*) :int {{break;return i;}}",

        "func test3(i:int, t :T) { if (i) {break;} else if(i) {mm := 1;} else{return;}}",
        // todo
        //"typedef cmplx = int***[16]",

    };

    create_base_types();

    for (int i = 0; i < sizeof(decl) / sizeof(*decl); i++) {
        init_stream(decl[i]);
        Decl *d = parse_decl_opt();
        create_global_decl(d);
    }

    for (Sym *sym = global_syms; sym != buf_end(global_syms); sym++) {
        if (sym->decl) {
            resolve_global_sym(sym);
        }
    }

    for (Sym *sym = global_syms; sym != buf_end(global_syms); sym++) {
        if (sym->type) {
            complete_type(sym->type);
        }

        if (sym->decl && sym->decl->kind == DECL_FUNC) {
            resolve_func_body(sym);
        }
    }

    for (Decl **decl = ordered_decls; decl != buf_end(ordered_decls); decl++) {
        gen_code((*decl)->sym);
    }

    for (Sym *sym = global_syms; sym != buf_end(global_syms); sym++) {
        if (sym->decl && sym->decl->kind == DECL_FUNC) {
            gen_code(sym);
        }
    }

    printf("%s", gen_buf);
}

void gen_unit_test() {
    printf("/*****codegen tests *****/\n\n");
    printf("%s\n", type_to_cdecl(type_ptr(type_int), "foo"));
    printf("%s\n", type_to_cdecl(type_array(type_int, 10), "foo"));
    printf("%s\n", type_to_cdecl(type_func((Type *[2]){type_int, type_int}, 2, type_void), "foo"));
    // declare foo as a pointer to func returning array 10 of func pointers returning int
    Type *pointer_to_func_return_int = type_func(NULL, 0, type_void);
    Type *array_10_of_pointers = type_array(pointer_to_func_return_int, 10);
    Type *func = type_func(NULL, 0, array_10_of_pointers);
    Type *func_ptr = func;

    printf("%s\n", type_to_cdecl(func_ptr, "foo"));
}
