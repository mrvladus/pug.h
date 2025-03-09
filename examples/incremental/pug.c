#include "../../pug.h"

// This is example of the incremental build.
// If any of the source files is changed - pug will rebuild only those files.

int main(int argc, const char **argv) {
  auto_rebuild_pug(argc, argv);
  Exe incremental_exe = {
      .name = "incremental",
      // Here we have multiple source files that will be tracked for changes
      .sources = FILES("main.c", "add.c", "subtract.c"),
      .install_dir = env_or("BINDIR", "/usr/bin"),
  };
  if (argc == 2) {
    if (!strcmp(argv[1], "clean"))
      exe_clean(&incremental_exe);
    if (!strcmp(argv[1], "install"))
      exe_install(&incremental_exe);
    return 0;
  }
  exe_build(&incremental_exe);
}
