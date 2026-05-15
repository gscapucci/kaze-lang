#include <stdbool.h>
#define NOB_IMPLEMENTATION
#define NOB_UNSTRIP_PREFIX
#include "../nob.h"


bool visit_src(Nob_Walk_Entry entry) {
    Nob_Procs *procs = (Nob_Procs *) entry.data;
    
    if(entry.type != NOB_FILE_REGULAR) return true;
    
    Nob_String_View input = nob_sv_from_cstr(entry.path);

    if(!nob_sv_ends_with_cstr(input, ".c")) return true;
    nob_sv_chop_right(&input, 2);

    Nob_String_Builder output = {0};

    nob_sb_append_sv(&output, input);
    nob_sb_append_cstr(&output, ".o");

    Nob_Cmd cmd = {0};
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cmd_append(&cmd, "-c");
    nob_cc_output(&cmd, nob_temp_sv_to_cstr(nob_sb_to_sv(output)));
    nob_cc_inputs(&cmd, entry.path);

    if(!nob_cmd_run(&cmd, .async = procs)) return false;
    nob_sb_free(output);
    return true;
}

bool visit_move_obj(Nob_Walk_Entry entry) {
    Nob_Procs *procs = (Nob_Procs *) entry.data;

    if(entry.type != NOB_FILE_REGULAR) return true;
    Nob_String_View input = nob_sv_from_cstr(entry.path);

    if(!nob_sv_ends_with_cstr(input, ".o")) return true;

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "mv");
    nob_cmd_append(&cmd, entry.path);
    nob_cmd_append(&cmd, "./utils/build");
    if(!nob_cmd_run(&cmd, .async = procs)) return false;
    return true;
}


int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    
    if(!nob_mkdir_if_not_exists("./utils/build")) return 1;
    
    Nob_Procs procs = {0};
    
    if(!nob_walk_dir("./utils/src", visit_src, .data = &procs)) return 1;
    if(!nob_procs_flush(&procs)) return 1;
    
    if(!nob_walk_dir("./utils/src", visit_move_obj, .data = &procs)) return 1;
    if(!nob_procs_flush(&procs)) return 1;

    return 0;
}