char *gen_buf = NULL;
size_t indent_newline;
size_t ion_line_tracker = 1;

#define genf(...)                                                                                  \
    (buf_printf(gen_buf, "%.*s", 4 * indent_newline, "                                  ")),       \
        (buf_printf(gen_buf, __VA_ARGS__))

#define genln(...)                                                                                 \
    do {                                                                                           \
        ion_line_tracker++;                                                                        \
        (buf_printf(gen_buf, "\n%.*s", 4 * indent_newline, "                                  ")), \
            (buf_printf(gen_buf, __VA_ARGS__));                                                    \
    } while (0)

const char *type_to_cdecl(Type *t, const char *gen);
const char *gen_expr(Expr *expr);
void gen_stmt(Stmt *stmt);
void gen_stmtblock(StmtBlock block);

const char *type_to_cdecl(Type *t, const char *gen) {
    switch (t->kind) {
    case TYPE_VOID:
        return strf("void %s", gen);
    case TYPE_INT:
        return strf("int %s", gen);
    case TYPE_CHAR:
        return strf("char %s", gen);
    case TYPE_UCHAR:
        return strf("unsigned char %s", gen);
    case TYPE_FLOAT:
        return strf("float %s", gen);
    case TYPE_PTR:
        return type_to_cdecl(t->ptr.elem, ((gen == "") ? strf("*") : strf("*(%s)", gen)));
    case TYPE_ARRAY:
        if (gen == "") {
            return type_to_cdecl(t->array.elem, strf("[%d]", t->array.size));
        } else {
            return type_to_cdecl(t->array.elem, strf("(%s)[%d]", gen, t->array.size));
        }
    case TYPE_UNION: {
        return strf("union %s %s", t->aggregate.name, gen);
    }
    case TYPE_STRUCT: {
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

const char *typespec_to_cdecl(Typespec *t, const char *gen) {
    if (!t) {
        return strf("void %s", gen);
    }

    switch (t->kind) {
    case TYPESPEC_NAME:
        return (gen == "") ? strf("%s", t->name) : strf("%s %s", t->name, gen);
    case TYPESPEC_PTR:
        return typespec_to_cdecl(t->ptr.type, ((gen == "") ? strf("*") : strf("*(%s)", gen)));
    case TYPESPEC_ARRAY:
        if (gen == "") {
            return typespec_to_cdecl(t->array.type, strf("[%s]", gen_expr(t->array.expr)));
        } else {
            return typespec_to_cdecl(t->array.type, strf("(%s)[%s]", gen, gen_expr(t->array.expr)));
        }
    case TYPESPEC_FUNC: {
        const char *args;
        if (t->func.num_args == 0) {
            args = strf("void");
        } else {
            args = typespec_to_cdecl(t->func.args[0], "");
        }

        for (int i = 1; i < t->func.num_args; i++) {
            args = strf("%s, %s", args, typespec_to_cdecl(t->func.args[i], ""));
        }

        return typespec_to_cdecl(t->func.ret, strf("(%s)(%s)", gen, args));
    }

    default:
        assert(0);
    }
}

void gen_decl(Decl *decl) {
    Sym *sym = decl->sym;
    const char *name = decl->name;
    Type *type = sym->type;

    if (!decl) {
        return;
    }

    genln("#line %d \"%s\"", decl->pos.line_num, decl->pos.file);
    switch (decl->kind) {
    case DECL_ENUM: {
        genln("enum %s {", name);
        indent_newline++;
        for (int i = 0; i < decl->enum_decl.num_enum_items; i++) {
            genln("%s");
            if (decl->enum_decl.items[i].expr) {
                genf(" = %s,", gen_expr(decl->enum_decl.items[i].expr));
            }
        }
        indent_newline--;
        genln("};");
        break;
    }
    case DECL_TYPEDEF:
        genln("typedef %s;", typespec_to_cdecl(decl->typedef_decl.type, name));
        break;
    case DECL_VAR: {
        if (type->kind == TYPE_FUNC) {
            name = strf("*%s", name);
        }

        if (decl->var_decl.type) {
            genln("%s", typespec_to_cdecl(decl->var_decl.type, name));
        } else {
            genln("%s", type_to_cdecl(type, name));
        }

        if (decl->var_decl.expr) {
            genf(" = %s;", gen_expr(decl->var_decl.expr));
        } else {
            genf(";");
        }

        break;
    }
    case DECL_CONST:
        genln("enum {%s = %s};", name, gen_expr(decl->const_decl.expr));
        break;

    case DECL_AGGREGATE: {
        const char *aggregate =
            (decl->aggregate_decl.kind == AGGREGATE_STRUCT) ? "struct" : "union";
        genln("%s %s {", aggregate, decl->name);
        indent_newline++;
        for (int i = 0; i < decl->aggregate_decl.num_items; i++) {
            for (int j = 0; j < decl->aggregate_decl.items[i].num_items; j++) {
                genln("%s;", typespec_to_cdecl(decl->aggregate_decl.items[i].type,
                                               decl->aggregate_decl.items[i].names[j]));
            }
        }
        indent_newline--;
        genln("};");
        break;
    }

    case DECL_FUNC: {
        const char *args;
        if (!decl->func_decl.is_foreign) {
            if (decl->func_decl.num_func_args == 0) {
                args = strf("void");
            } else {
                args =
                    typespec_to_cdecl(decl->func_decl.args[0].type, decl->func_decl.args[0].name);
            }
            for (int i = 1; i < decl->func_decl.num_func_args; i++) {
                args = strf(
                    "%s, %s", args,
                    typespec_to_cdecl(decl->func_decl.args[i].type, decl->func_decl.args[i].name));
            }
            genln("%s", typespec_to_cdecl(decl->func_decl.type, strf("%s(%s)", decl->name, args)));
            gen_stmtblock(decl->func_decl.block);
        }
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

const char *gen_escaped_str(const unsigned char *str) {
    char *escaped_str = NULL;

    while (*str != '\0') {
        if (char_to_escape[*str]) {
            buf_push(escaped_str, '\\');
            buf_push(escaped_str, char_to_escape[*str]);
        } else {
            buf_push(escaped_str, *str);
        }

        str++;
    }

    buf_push(escaped_str, '\0');
    return strf("\"%s\"", escaped_str);
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
        return gen_escaped_str(expr->str_val);
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
        return strf("(%s)%s", typespec_to_cdecl(expr->cast_expr.type, ""),
                    gen_expr(expr->cast_expr.expr));
    case EXPR_COMPOUND: {
        Type *type = create_type(expr->compound_expr.type);
        if (!expr->compound_expr.num_args) {
            if (type == type_void) {
                return strf("{}");
            } else {
                return strf("(%s){}", typespec_to_cdecl(expr->compound_expr.type, ""));
            }
        }

        const char *args = gen_compound_val(expr->compound_expr.args[0]);

        for (int i = 1; i < expr->compound_expr.num_args; i++) {
            args = strf("%s, %s", args, gen_compound_val(expr->compound_expr.args[i]));
        }

        if (type == type_void) {
            return strf("{%s}", args);
        } else {
            return strf("(%s){%s}", typespec_to_cdecl(expr->compound_expr.type, ""), args);
        }
    }
    case EXPR_SIZEOF_TYPE:
        return strf("sizeof(%s)", typespec_to_cdecl(expr->sizeof_type, ""));
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
    genln("{");
    indent_newline++;
    for (int i = 0; i < block.num_stmts; i++) {
        gen_stmt(block.stmts[i]);
    }
    indent_newline--;
    genln("}");
}

void gen_line_directive(Stmt *stmt) {
    if (stmt->pos.line_num != ion_line_tracker) {
        genln("#line %d \"%s\"", stmt->pos.line_num, stmt->pos.file);
        ion_line_tracker = stmt->pos.line_num;
    }
}

void gen_stmt(Stmt *stmt) {

    gen_line_directive(stmt);

    switch (stmt->kind) {
    case STMT_NONE:
        assert(0);
    case STMT_DECL:
        assert(0);
        break;
    case STMT_EXPR:
        genln("%s;", gen_expr(stmt->expr));
        break;
    case STMT_INIT: {
        genln("%s = %s;", type_to_cdecl(stmt->stmt_init.type, stmt->stmt_init.name),
              gen_expr(stmt->stmt_init.expr));
        break;
    }
    case STMT_ASSIGN:
        genln("%s %s %s;", gen_expr(stmt->stmt_assign.left_expr),
              token_kind_name(stmt->stmt_assign.op), gen_expr(stmt->stmt_assign.right_expr));
        break;
    case STMT_RETURN:
        if (stmt->stmt_return.expr) {
            genln("return %s;", gen_expr(stmt->stmt_return.expr));
        } else {
            genln("return;");
        }
        break;
    case STMT_BREAK:
        genln("break;");
        break;
    case STMT_CONTINUE:
        genln("continue;");
        break;
    case STMT_BLOCK: {
        gen_stmtblock(stmt->block);
        break;
    }
    case STMT_IF: {
        genln("if (%s)", gen_expr(stmt->stmt_if.expr));
        gen_stmtblock(stmt->stmt_if.if_block);
        for (int i = 0; i < stmt->stmt_if.num_elseifs; i++) {
            genln("else if(%s)", gen_expr(stmt->stmt_if.else_ifs[i].expr));
            gen_stmtblock(stmt->stmt_if.else_ifs[i].block);
        }

        if (stmt->stmt_if.else_block.num_stmts) {
            genln("else");
            gen_stmtblock(stmt->stmt_if.else_block);
        }
        break;
    }
    case STMT_DO_WHILE:
        genln("do");
        gen_stmtblock(stmt->stmt_while.block);
        genf(" while (%s);", gen_expr(stmt->stmt_while.expr));
        break;
    case STMT_WHILE:
        genln("while (%s)", gen_expr(stmt->stmt_while.expr));
        gen_stmtblock(stmt->stmt_while.block);
        break;
    case STMT_FOR: {
        const char *str;
        str = strf("for (");
        if (stmt->stmt_for.init) {
            Stmt *init = stmt->stmt_for.init;
            if (init->kind == STMT_INIT) {
                str = strf("%s%s = %s; ", str,
                           type_to_cdecl(init->stmt_init.type, init->stmt_init.name),
                           gen_expr(init->stmt_init.expr));
            } else if (init->kind == STMT_ASSIGN) {
                str = strf("%s%s %s %s;", str, gen_expr(init->stmt_assign.left_expr),
                           token_kind_name(init->stmt_assign.op),
                           gen_expr(init->stmt_assign.right_expr));
            } else {
                assert(0);
            }
        } else {
            str = strf("%s; ", str);
        }

        str = strf("%s%s; ", str, gen_expr(stmt->stmt_for.cond));

        if (stmt->stmt_for.next) {
            Stmt *next = stmt->stmt_for.next;
            assert(next->kind == STMT_ASSIGN);
            str =
                strf("%s%s %s %s", str, gen_expr(next->stmt_assign.left_expr),
                     token_kind_name(next->stmt_assign.op), gen_expr(next->stmt_assign.right_expr));
        }
        str = strf("%s)", str);
        genln("%s", str);
        gen_stmtblock(stmt->stmt_for.block);
        break;
    }
    case STMT_SWITCH: {
        genln("switch (%s) {", gen_expr(stmt->stmt_switch.expr));
        indent_newline++;
        for (int i = 0; i < stmt->stmt_switch.num_cases; i++) {
            for (int j = 0; j < stmt->stmt_switch.cases[i].num_exprs; j++) {
                genln("case %s:", gen_expr(stmt->stmt_switch.cases[i].expr[j]));
            }
            if (stmt->stmt_switch.cases[i].is_default) {
                genln("default:");
            }
            gen_stmtblock(stmt->stmt_switch.cases[i].block);
            genln("break;");
        }
        indent_newline--;
        genln("}");
        break;
    }
    default:
        assert(0);
        break;
    }
}

void resolve_symbols() {
    // Resolve all Global symbols
    for (Sym **sym = global_sym_list; sym != buf_end(global_sym_list); sym++) {
        if ((*sym)->decl) {
            resolve_global_sym(*sym);
        }
    }

    // Complete the types and functions
    for (Sym **sym = global_sym_list; sym != buf_end(global_sym_list); sym++) {
        if ((*sym)->type) {
            complete_type((*sym)->type);
        }

        if ((*sym)->decl && (*sym)->decl->kind == DECL_FUNC) {
            resolve_func_body(*sym);
        }
    }
}

void gen_preamble() {
    const char *preamble = "// preamble\n\
#include <stdio.h>\n";
    genf("%s", preamble);
}

void forward_declare_types() {
    genf("// Forward declared all types //\n\n");
    // Forward declare all types
    for (Decl **decl = ordered_decls; decl != buf_end(ordered_decls); decl++) {
        if ((*decl)->kind == DECL_AGGREGATE) {
            if ((*decl)->aggregate_decl.kind == AGGREGATE_STRUCT) {
                genln("typedef struct %s %s;", (*decl)->name, (*decl)->name);
            } else {
                genln("typedef union %s %s;", (*decl)->name, (*decl)->name);
            }
        }
    }
}

void generate_types() {
    // generate decls other than funcs in resolved order
    for (Decl **decl = ordered_decls; decl != buf_end(ordered_decls); decl++) {
        if ((*decl)->kind != DECL_FUNC) {
            gen_decl(*decl);
        } else if ((*decl)->kind == DECL_FUNC && !(*decl)->func_decl.is_foreign) {
            genln("%s;", type_to_cdecl((*decl)->sym->type, (*decl)->name));
        }
    }
}

void generate_functions() {
    for (Decl **decl = ordered_decls; decl != buf_end(ordered_decls); decl++) {
        if ((*decl)->kind == DECL_FUNC) {
            gen_decl(*decl);
        }
    }
}

void gen_c_code(const char *code) {
    init_stream(NULL, code);

    create_base_types();
    install_global_decls(parse_file());

    resolve_symbols();

    gen_preamble();
    forward_declare_types();
    generate_types();

    generate_functions();

    printf("%s", gen_buf);
}

void gen_test() {

    const char *code =
        "func example_test(): int { return fact_rec(10) == fact_iter(10); }\n"
        "union IntOrPtr { i: int; p: int*; }\n"
        "func f() {\n"
        "    u1 := IntOrPtr{i = 42};\n"
        "    u2 := IntOrPtr{p = cast(int*, 42)};\n"
        "    u1.i = 0;\n"
        "    u2.p = cast(int*, 0);\n"
        "}\n"
        "var i: int\n"
        "struct Vector { x: int; y: int; }\n"
        "func fact_iter(n: int): int { r := 1; for (i := 2; i <= n; i++) { r *= i; } return r; }\n"
        "func fact_rec(n: int): int { if (n == 0) { return 1; } else { return n * fact_rec(n-1); } "
        "}\n"

        "func f1() { v := Vector{1, 2}; j := i; i++; j++; v.x = 2*j; }\n"
        "func f2(n: int): int { return 2*n; }\n"
        "func f3(x: int): int { if (x) { return -x; } else if (x % 2 == 0) { return 42; } else { "
        "return -1; } }\n"
        "func f4(n: int): int { for (i := 0; i < n; i++) { if (i % 3 == 0) { return n; } } return "
        "0; }\n"
        "func f5(x: int): int { switch(x) { case 0: case 1: return 42; case 3: default: return -1; "
        "} }\n"
        "func f6(n: int): int { p := 1; while (n) { p *= 2; n--; } return p; }\n"
        "func f7(n: int): int { p := 1; do { p *= 2; n--; } while (n); return p; }\n"

        "const n = 1+sizeof(p)\n"
        "var p: T*\n"
        "struct T { a: int[n]; }\n"
        "var q:int[sizeof(:IntOrPtr)]";
    gen_c_code(code);
}

void unit_test() {
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
        init_stream(NULL, decl[i]);
        Decl *d = parse_decl_opt();
        install_global_decl(d);
    }

    for (Sym **sym = global_sym_list; sym != buf_end(global_sym_list); sym++) {
        if ((*sym)->decl) {
            resolve_global_sym(*sym);
        }
    }

    for (Sym **sym = global_sym_list; sym != buf_end(global_sym_list); sym++) {
        if ((*sym)->type) {
            complete_type((*sym)->type);
        }

        if ((*sym)->decl && (*sym)->decl->kind == DECL_FUNC) {
            resolve_func_body(*sym);
        }
    }

    printf("%s", gen_buf);
}

void gen_unit_test() {
    printf("/*****codegen tests *****/\n\n");
    printf("%s\n", type_to_cdecl(type_ptr(type_int), "foo"));
    printf("%s\n", type_to_cdecl(type_array(type_int, 10), "foo"));
    printf("%s\n",
           type_to_cdecl(type_func((Type *[2]){type_int, type_int}, 2, type_void, false), "foo"));
    // declare foo as a pointer to func returning array 10 of func pointers returning int
    Type *pointer_to_func_return_int = type_func(NULL, 0, type_void, false);
    Type *array_10_of_pointers = type_array(pointer_to_func_return_int, 10);
    Type *func = type_func(NULL, 0, array_10_of_pointers, false);
    Type *func_ptr = func;

    printf("%s\n", type_to_cdecl(func_ptr, "foo"));
}
