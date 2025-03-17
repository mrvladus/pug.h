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
- **Self-rebuild**: If build file `pug.c` changes - it will rebuild itself.

## Getting Started

### Prerequisites

- A C compiler (e.g., GCC or Clang)

### Usage

1. Copy `pug.h` to your project directory.
2. Create a build file named `pug.c`:

Here’s a simplest example of how to use PUG.H in your project:

```c
#include "pug.h"

Target main_exe = {
    .type = TARGET_EXE,
    .name = "main",
    .sources = STRINGS("main.c"),
};

int main(int argc, char **argv) {
  pug_init(argc, argv);
  pug_build_target(&main_exe);
  return 0;
}
```

3. Build `pug` with `gcc -o pug pug.c`
4. Run `./pug` to build your project

For more - see inside `pug.h` file and [examples](examples) directory.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for more details.
