#include "../../pug.h"

// This is example of the incremental build.
// If any of the source files is changed - pug will rebuild only those files.

int main(int argc, const char **argv) {
  // Auto-rebuild pug if this file is changed
  pug_self_rebuild();
  // Create new executable
  Executable incremental_exe = {
      .name = "incremental",
      .sources = SOURCES("main.c", "add.c", "subtract.c"),
  };
  // If 1st argument is "clean" - remove all build files for this executable
  if (argc == 2 && !strcmp(argv[1], "clean")) {
    executable_clean(&incremental_exe);
    return 0;
  }
  // Build executable
  executable_build(&incremental_exe);
}
