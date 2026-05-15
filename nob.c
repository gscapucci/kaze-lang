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
            // return test();
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
    return help();
}