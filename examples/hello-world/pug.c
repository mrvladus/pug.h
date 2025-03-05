#define PUG_IMPLEMENTATION
#include "../../pug.h"

int main(int argc, const char **argv) {
  // Auto-rebuild pug if this file is changed
  pug_self_rebuild();
  // Create new executable
  Executable test_exe = {
      .name = "hello-world",
      .sources = SOURCES("hello-world.c"),
  };
  // If 1st argument is "clean" - remove all build files for this executable
  if (argc == 2 && !strcmp(argv[1], "clean")) {
    executable_clean(&test_exe);
    return 0;
  }
  // Build executable
  executable_build(&test_exe);
}
