# PUG.H

PUG.H is a stupidly simple build system designed for small C/C++ projects.
It consists of a single header file that you can easily copy into your project.
PUG.H requires only a C compiler to build itself, making it lightweight and straightforward to use.

## Features

- **Single Header File**: Just copy `pug.h` to your project.
- **Tiny**: Less than 500 lines of code.
- **No Dependencies**: Requires only a C compiler.
- **Easy to Use**: Simple syntax to create your builds.
- **Incremental Builds**: PUG rebuilds only changed source files for fast incremental compilation.

## Getting Started

### Prerequisites

- A C compiler (e.g., GCC or Clang)

### Usage

1. Copy `pug.h` to your project directory.
2. Create a build file named `pug.c`:

Here’s a simple example of how to use PUG.H in your project:

```c
#define CC "clang" // You can change the C compiler using this. "gcc" is the default.
#include "pug.h"

int main(int argc, const char **argv) {
  // Auto-rebuild pug if this file is changed.
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
    // If 1st argument is "install" - install binary to install_dir
    if (!strcmp(argv[1], "install"))
      exe_install(&test_exe);
    return 0;
  }
  // Build executable if no args is provided
  exe_build(&test_exe);
}

```

3. Build `pug` with `gcc -o pug pug.c`
4. Run `./pug` to build your project
5. Run `./pug clean` to clean up the build files
6. Run `./pug install` to install executable

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for more details.
