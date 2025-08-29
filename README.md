<div align="center">
  <img src="logo.png" width="200">

# pug.h

</div>

**Pug.h** is a simple build system designed for C/C++ projects.
It consists of a single stb-style header file that you can copy into your project.
It requires **only** a C compiler to build itself, making it portable, lightweight and straightforward to use.

## Features

- **Single Header File**: Just copy `pug.h` to your project.
- **Tiny**: Less than 600 lines of code.
- **No Dependencies**: Requires only a C compiler.
- **Easy to Use**: Simple syntax to create your builds.
- **Incremental Builds**: PUG rebuilds only changed source files for fast incremental compilation.
- **Headers Tracking**: If header files change, PUG will rebuild the source files where it included.
- **Self-rebuild**: If build file `pug.c` changes - it will rebuild itself.

## Getting Started

### Prerequisites

- A C compiler (e.g., GCC or Clang)

### Usage

Here’s a simplest example of how to use **pug.h** in your project:

1. Copy `pug.h` to your project directory.
2. Create a build file named `pug.c`:

```c
// Define PUG_IMPLEMENTATION in ONE file to include the functions implementation.
#define PUG_IMPLEMENTATION
#include "pug.h"

int main(int argc, char **argv) {
    // Initialize PUG library.
    // This will auto-rebuild this file if it changes and enable parsing of command line arguments.
    pug_init(argc, argv);

    // Create target for your executable with name, target type and build directory.
    // Build directory will be created if it doesn't exist.
    PugTarget hello_bin = pug_target_new("hello", PUG_TARGET_TYPE_EXECUTABLE, "build");
    // Add sources files.
    pug_target_add_sources(&hello_bin, "src/main.c", NULL);
    // Add compiler flags.
    pug_target_add_cflags(&hello_bin, "-Wall", "-Wextra", NULL);
    // Build target executable.
    if(!pug_target_build(&hello_bin)) return 1;

    return 0;
}
```

3. Build `pug` with `cc -o pug pug.c`
4. Run `./pug` to build your project

For more - see inside `pug.h` file and [examples](examples) directory.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for more details.
