
const char *ion_compile_str(const char *str) {
    init_stream(str);

    create_base_types();
    install_global_decls(parse_file());

    resolve_symbols();

    forward_declare_types();
    generate_types();
    forward_declare_functions();
    generate_functions();

    const char *result = gen_buf;
    gen_buf = NULL;
    return result;
}

const char *ion_compile_file(const char *path) {
    const char *result = ion_compile_str(read_file(path));
    const char *c_path = replace_ext(path, "c");

    write_file(c_path, result, buf_len(result));
}

int ion_main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage %s <file_name>", argv[0]);
        exit(1);
    }

    ion_compile_file(argv[1]);
}