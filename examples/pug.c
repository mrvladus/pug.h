#define PUG_IMPLEMENTATION
#include "../pug.h"

int main(int argc, char **argv) {
  pug_init(argc, argv);

  // Test library. Static.
  PugTarget libtest = pug_target_new("libtest", PUG_TARGET_TYPE_STATIC_LIBRARY, "build");
  pug_target_add_sources(&libtest, "libtest.c", NULL);
  pug_target_add_cflags(&libtest, "-Wall", "-Wextra", NULL);
  if (!pug_target_build(&libtest)) return 1;

  // Test executable. With dependency on libtest.
  PugTarget test = pug_target_new("test", PUG_TARGET_TYPE_EXECUTABLE, "build");
  pug_target_add_sources(&test, "test.c", NULL);
  pug_target_add_cflags(&test, "-Wall", "-Wextra", NULL);
  pug_target_add_ldflags(&test, "-Lbuild", "-ltest", NULL);
  if (!pug_target_build(&test)) return 1;

  return 0;
}
