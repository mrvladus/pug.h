#include "../../pug.h"

int main(int argc, const char **argv) {
  // Auto-rebuild pug if this file is changed.
  // Must use argc and argv to function.
  auto_rebuild_pug(argc, argv);
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
  // Parse args
  if (argc == 2) {
    // If 1st argument is "clean" - remove all build files for this executable
    if (!strcmp(argv[1], "clean"))
      exe_clean(&test_exe);
    // If 1st argument is "install" - install binary to ~/.local/bin
    if (!strcmp(argv[1], "install"))
      exe_install(&test_exe);
    return 0;
  }
  // Build executable if no args is provided
  exe_build(&test_exe);
}
