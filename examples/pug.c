#define PUG_IMPLEMENTATION
#include "../pug.h"

// Test library. Shared and static.
Target libtest = {
    .type = TARGET_SHARED_LIB | TARGET_STATIC_LIB,
    .name = "libtest",
    .cflags = "-Wall -Wextra",
    .sources = STRINGS("libtest.c"),
    .headers = STRINGS("libtest.h"),
};

// Test executable. With dependency on libtest target.
Target test_exe = {
    .type = TARGET_EXE,
    .name = "test",
    .cflags = "-Wall -Wextra ",
    .ldflags = "-L. -ltest -Wl,-rpath,.",
    .sources = STRINGS("test.c"),
    .dep_targets = TARGETS(&libtest),
};

int main(int argc, char **argv) {
  // Test auto-rebuild
  pug_init(argc, argv);
  // Test cmd-line arguments
  // Test cleaning target files
  if (pug_arg_bool_or("clean", false)) {
    pug_clean_target(&test_exe);
    return 0;
  }
  // Test checking for installed libraries
  if (pug_arg_bool_or("check", false)) {
    if (!check_libs(STRINGS("libcurl", "gtk4")))
      return 1;
  }
  // Test building target
  pug_build_target(&test_exe);
  // Test running target executable
  if (pug_arg_bool_or("run", false)) {
    pug_run_target(&test_exe);
  }
  return 0;
}
