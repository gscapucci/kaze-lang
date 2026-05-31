#define NOB_IMPLEMENTATION
#define NOB_UNSTRIP_PREFIX
#include "nob.h"
#include <glob.h>

static int help() {
    printf("Commands: \n");
    printf("\ttest\t\trun tests\n");
    printf("\tbuild\t\trun build\n");
    printf("\tclean\t\tremove build objs\n");
    return 0;
}

static int run_nob_script(const char *folder_path) {
    
    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    
    char helper1[256] = {0};
    snprintf(helper1, sizeof(helper1), "./%s/nob.c", folder_path);
    nob_cc_inputs(&cmd, helper1);
    
    char helper2[256] = {0};
    snprintf(helper2, sizeof(helper2), "./%s/nob", folder_path);
    nob_cc_output(&cmd, helper2);
    
    
    if(!nob_cmd_run(&cmd)) return 1;
    
    
    char helper3[256] = {0};
    snprintf(helper3, sizeof(helper3), "./%s/nob", folder_path);
    nob_cmd_append(&cmd, helper3);

    if(!nob_cmd_run(&cmd)) return 1;
    return 0;
}

static bool append_objects_from_dir(Nob_Cmd *cmd, const char *dirpath) {
    Nob_File_Paths files = {0};

    if (!nob_read_entire_dir(dirpath, &files)) {
        return false;
    }

    for (size_t i = 0; i < files.count; i++) {
        const char *filename = files.items[i];

        if (nob_sv_end_with(nob_sv_from_cstr(filename), ".o")) {

            Nob_String_Builder sb = {0};

            nob_sb_append_cstr(&sb, dirpath);
            nob_sb_append_cstr(&sb, "/");
            nob_sb_append_cstr(&sb, filename);

            nob_cmd_append(cmd, nob_temp_sv_to_cstr(nob_sb_to_sv(sb)));
        }
    }

    return true;
}

static int link_kazec() {
    Nob_Cmd cmd = {0};
    nob_cc(&cmd);

    append_objects_from_dir(&cmd, "./utils/build");
    append_objects_from_dir(&cmd, "./kazec/build");

    nob_cc_output(&cmd, "./build/kazec");
    if(!nob_cmd_run(&cmd)) return 1;
    return 0;
}

static int build();

static bool compile_c_file(const char *src, const char *obj) {
    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cmd_append(&cmd, "-c");
    nob_cc_inputs(&cmd, src);
    nob_cc_output(&cmd, obj);
    return nob_cmd_run(&cmd);
}

static int build_tests() {
    int ret = build();
    if (ret != 0) return ret;

    // Compile test framework + test sources
    const char *test_srcs[] = {
        "./test/test.c",
        "./test/main.c",
        "./test/utils/hashmap_test.c",
        "./test/kazec/lexer_test.c",
        "./test/kazec/ast_test.c",
        "./test/kazec/parser_test.c",
        "./test/kazec/type_test.c",
        "./test/kazec/scope_test.c",
        "./test/kazec/sema_test.c",
    };
    const char *test_objs[] = {
        "./build/test_framework.o",
        "./build/test_main.o",
        "./build/test_hashmap.o",
        "./build/test_lexer.o",
        "./build/test_ast.o",
        "./build/test_parser.o",
        "./build/test_type.o",
        "./build/test_scope.o",
        "./build/test_sema.o",
    };
    size_t n = sizeof(test_srcs) / sizeof(test_srcs[0]);
    for (size_t i = 0; i < n; i++) {
        if (!compile_c_file(test_srcs[i], test_objs[i])) return 1;
    }

    // Link: utils + kazec (excluding main.o) + test objects
    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    append_objects_from_dir(&cmd, "./utils/build");

    // kazec objects except main.o
    Nob_File_Paths files = {0};
    nob_read_entire_dir("./kazec/build", &files);
    for (size_t i = 0; i < files.count; i++) {
        if (!nob_sv_end_with(nob_sv_from_cstr(files.items[i]), ".o")) continue;
        if (strcmp(files.items[i], "main.o") == 0) continue;
        Nob_String_Builder sb = {0};
        nob_sb_append_cstr(&sb, "./kazec/build/");
        nob_sb_append_cstr(&sb, files.items[i]);
        nob_cmd_append(&cmd, nob_temp_sv_to_cstr(nob_sb_to_sv(sb)));
    }

    for (size_t i = 0; i < n; i++) nob_cmd_append(&cmd, test_objs[i]);
    nob_cc_output(&cmd, "./build/test");
    if (!nob_cmd_run(&cmd)) return 1;
    return 0;
}

static int build() {
    if(!nob_mkdir_if_not_exists("./build")) return 1;
    int ret = 0;
    ret = run_nob_script("./utils/");
    if(ret != 0) return ret;
    ret = run_nob_script("./kazec/");
    if(ret != 0) return ret;
    ret = link_kazec();
    if(ret != 0) return ret;
    return 0;
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    if(argc == 1) {
        return build();
    }


    if(argc == 2) {
        const char *arg = argv[1];
        if(!strcmp(arg, "test")) {
            int tret = build_tests();
            if(tret != 0) return tret;
            Nob_Cmd cmd = {0};
            nob_cmd_append(&cmd, "./build/test");
            if(!nob_cmd_run(&cmd)) return 1;
            return 0;
        }
        if(!strcmp(arg, "build")) {
            return build();
        }
        if(!strcmp(arg, "clean")) {
            system("rm -rf ./kazec/build");
            system("rm -rf ./utils/build");
            system("rm -rf ./build");
            return 0;
        }
        
    }
    if(argc == 3) {
        const char *arg = argv[1];
        const char *input = argv[2];
        if(!strcmp(arg, "run")) {
            int ret = build();
            if(ret != 0) return ret;
            Nob_Cmd cmd = {0};
            nob_cmd_append(&cmd, "./build/kazec", "--dump-tokens", input);
            if(!nob_cmd_run(&cmd)) return 1;
            return 0;
        }
    }
    return help();
}