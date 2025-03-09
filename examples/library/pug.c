#include "../../pug.h"

int main(int argc, const char **argv) {
  auto_rebuild_pug(argc, argv);
  // Create new library target
  Lib add_lib = {
      // Set library name
      .name = "libadd",
      // Set list of sources to be compiled
      .sources = FILES("libadd.c"),
      // Set list of headers to be installed
      .headers = FILES("libadd.h"),
      // Set install directory for library.
      // Can be customized with enviroment variable like this: "LIBDIR=/usr/lib ./pug"
      .lib_install_dir = env_or("LIBDIR", "/usr/local/lib"),
      // Set install directory for header files.
      // Can be customized with enviroment variable like this: "INCLUDE_DIR=/usr/include ./pug"
      .headers_install_dir = env_or("INCLUDEDIR", "/usr/local/include"),
      .build_static = true,
  };
  // Create test executable
  Exe test_libadd = {
      .name = "testlibadd",
      .sources = FILES("test.c"),
      .ldflags = "-L. -ladd -Wl,-rpath,.",
  };
  // Parse args
  if (argc == 2) {
    // If 1st argument is "clean" - remove all build files for this library
    if (!strcmp(argv[1], "clean")) {
      lib_clean(&add_lib);
      exe_clean(&test_libadd);
    }
    // If 1st argument is "install" - install library to "/usr/local/lib"
    if (!strcmp(argv[1], "install"))
      lib_install(&add_lib);
    return 0;
  }
  // Build library if no args is provided
  lib_build(&add_lib);
  // Build test executable
  exe_build(&test_libadd);
}
