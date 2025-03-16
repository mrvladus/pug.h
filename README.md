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
#define CC "clang" // You can change the C compiler using this. "cc" is the default.
#include "pug.h"

// TODO
```

3. Build `pug` with `gcc -o pug pug.c`
4. Run `./pug` to build your project
5. Run `./pug clean` to clean up the build files
6. Run `./pug install` to install executable

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for more details.
