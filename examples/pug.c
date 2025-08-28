#define PUG_IMPLEMENTATION
#include "../pug.h"

// Test library. Shared and static.
Target libtest = {
    .type = TYPE_SHARED_LIB | TYPE_STATIC_LIB,
    .name = "libtest",
    .cflags = "-Wall -Wextra",
    .sources = STRINGS("libtest.c"),
};

// Test executable. With dependency on libtest target.
Target test_exe = {
    .type = TYPE_EXE,
    .name = "test",
    .cflags = "-Wall -Wextra ",
    .ldflags = "-L. -ltest -Wl,-rpath,.",
    .sources = STRINGS("test.c"),
    .dep_targets = TARGETS(&libtest),
};

int main(int argc, char **argv) {
  pug_init(argc, argv);
  if (pug_get_bool_arg("--clean")) {
    pug_target_clean(&test_exe);
    return 0;
  }
  pug_target_build(&test_exe);
  return 0;
}
