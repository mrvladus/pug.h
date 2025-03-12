#include "../../pug.h"

int main(int argc, char **argv) {
  pug_init(argc, argv);

  // Create new executable
  Exe test_exe = {
      // Set executable name
      .name = "hello-world",
      // Set list of sources for executable
      .sources = FILES("hello-world.c"),
      // Set install directory for compiled binary.
      // Can be customized with enviroment variable like this: "BINDIR=/usr/bin ./pug"
      .install_dir = env_or("BINDIR", "/usr/local/bin"),
  };
  if (get_arg_bool("clean")) {
    exe_clean(&test_exe);
    return 0;
  }
  // Run if launched with "./pug run"
  else if (get_arg_bool("run")) {
    exe_run(&test_exe);
    return 0;
  }
  // Build executable
  exe_build(&test_exe);
  // Install if "install" argument provided
  if (get_arg_bool("install"))
    exe_install(&test_exe);
}
