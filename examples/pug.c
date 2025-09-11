#define PUG_IMPLEMENTATION
#include "../pug.h"

#define BUILD_DIR "build"

int main(int argc, char **argv) {
  pug_init(argc, argv);

  // Test library. Static.
  PugTarget libtest = pug_target_new("libtest", PUG_TARGET_TYPE_STATIC_LIBRARY, BUILD_DIR);
  pug_target_add_source(&libtest, "libtest.c");
  pug_target_add_cflags(&libtest, "-Wall", "-Wextra");
  if (!pug_target_build(&libtest)) return 1;

  // Test executable. With dependency on libtest.
  PugTarget test = pug_target_new("test", PUG_TARGET_TYPE_EXECUTABLE, BUILD_DIR);
  pug_target_add_source(&test, "test.c");
  pug_target_add_cflags(&test, "-Wall", "-Wextra");
  pug_target_add_ldflags(&test, "-L" BUILD_DIR, "-ltest");
  if (!pug_target_build(&test)) return 1;

  return 0;
}
