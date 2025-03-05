/*

MIT Licence

Copyright 2025 Vlad Krupinskii <mrvladus@yandex.ru>

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the “Software”), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

-------------------------------------------------------------------------------

PUG.H is a stupidly simple build system designed for small C/C++ projects.
It consists of a single header file that you can easily copy into your project.
PUG.H requires only a C compiler to build itself, making it lightweight and
straightforward to use.

Here is how it works:
- Copy pug.h to your project
- Create your build file "pug.c"
- Compile it with "cc -o pug pug.c"
- Run with "./pug"

Pretty simple, right?

-------------------------------------------------------------------------------

Example "pug.c":

#define CC "clang" // You can change the C compiler using this.
                   // "gcc" is the default.
#include "pug.h"

int main(int argc, const char **argv) {
    // Auto-rebuild pug if this file is changed
    pug_self_rebuild();
    // Create new executable target
    Executable exe = {
        .name = "example",                     // Required field
        .sources = SOURCES("main.c", "lib.c"), // Required field
        .cflags = "-Wall -Wextra",             // Optional field
        .ldflags = "-lm",                      // Optional field
    };

    // Clean build files if run with "./pug clean"
    if (argc == 2 && !strcmp(argv[1], "clean")) {
        executable_clean(&exe);
        return 0;
    }

    // Build it
    executable_build(&exe);
}

*/

#ifndef __PUG_H__
#define __PUG_H__

// --------------------------- CONFIGURATION START ------------------------- //

#ifndef CC
#define CC "gcc"
#endif // CC

// --------------------------- CONFIGURATION END --------------------------- //

// --------------------------- DECLARATIONS START -------------------------- //

// Executable build target
typedef struct {
  // Executable name
  const char *name;
  // NULL-terminated array of strings. Use SOURCES() macro for easy creation.
  const char **sources;
  // Compile flags
  const char *cflags;
  // Linker flags
  const char *ldflags;
} Executable;

// Shortcut for setting sources of the Executable struct
#define SOURCES(...) ((const char *[]){__VA_ARGS__, NULL})

// Build executable
void executable_build(Executable *exe);
// Cleanup executable and its .o files
void executable_clean(Executable *exe);
// Checks if pug.c was changed and recompiles itself if needed
void pug_self_rebuild(void);

// ---------------------------- DECLARATIONS END ---------------------------- //

// ---------------------------- IMPLEMENTATION START ------------------------ //

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#define STAT _stat
#else
#define STAT stat
#endif

// --- UTILS --- //

// Log formatted message
static inline void pug_log(const char *format, ...) {
  va_list args;
  printf("[PUG] ");
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
  printf("\n");
}

// Get length of NULL-terminated array
static inline size_t pug_get_array_len(const char **arr) {
  size_t len = 0;
  if (!arr)
    return len;
  while (arr[len])
    len++;
  return len;
}

// Allocate formatted string
static char *pug_strdup_printf(const char *format, ...) {
  if (!format)
    return NULL;
  va_list args;
  va_start(args, format);
  // Determine required size
  int needed = vsnprintf(NULL, 0, format, args);
  va_end(args);
  if (needed < 0)
    return NULL;
  // Allocate buffer: +1 for the null terminator
  char *buffer = (char *)malloc(needed + 1);
  if (!buffer)
    return NULL;
  // Format the string into the buffer
  va_start(args, format);
  int written = vsnprintf(buffer, needed + 1, format, args);
  va_end(args);
  if (written < 0) {
    free(buffer);
    return NULL;
  }
  return buffer;
}

static inline char *pug_replace_file_extension(const char *filename, const char *new_ext) {
  const char *dot = strrchr(filename, '.');
  size_t basename_len = (dot && dot != filename) ? (size_t)(dot - filename) : strlen(filename);
  // Allocate memory for new filename: basename + '.' + new_ext + '\0'
  size_t new_len = basename_len + 1 + strlen(new_ext) + 1;
  char *new_filename = (char *)malloc(new_len);
  if (!new_filename)
    return NULL; // Allocation failed
  // Copy the base filename (without the old extension)
  strncpy(new_filename, filename, basename_len);
  new_filename[basename_len] = '\0';
  // Append the new extension
  snprintf(new_filename + basename_len, new_len - basename_len, ".%s", new_ext);
  return new_filename;
}

static const char *pug_get_file_extension(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if (!dot || dot == filename)
    return NULL;
  return dot + 1;
}

static bool pug_file_exists(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (file) {
    fclose(file);
    return true;
  }
  return false;
}

// Check if file1 was modified after file2
static bool pug_file_changed_after(const char *file1, const char *file2) {
  struct stat stat1, stat2;
  if (STAT(file1, &stat1) != 0 || STAT(file2, &stat2) != 0)
    return false; // If either file is missing, assume no change
  return stat1.st_mtime > stat2.st_mtime;
}

// --- FUNCTIONS --- //

void executable_build(Executable *exe) {
  // Compile source files to .o files
  pug_log("Compiling");
  size_t sources_len = pug_get_array_len(exe->sources);
  size_t sources_str_len = 0;
  bool need_linking = false;
  for (size_t i = 0; i < sources_len; ++i) {
    const char *source = exe->sources[i];
    // Get corresponding .o file
    char *source_obj = pug_replace_file_extension(source, "o");
    // If .o file exists and corresponding source file is newer - recompile it
    char *cmd = NULL;
    const char *cmd_f = "%s -c %s -o %s %s";
    if (pug_file_exists(source_obj)) {
      if (pug_file_changed_after(source, source_obj))
        cmd = pug_strdup_printf(cmd_f, CC, exe->cflags ? exe->cflags : "", source_obj, source);
    }
    // If it not exists - compile it
    else
      cmd = pug_strdup_printf(cmd_f, CC, exe->cflags ? exe->cflags : "", source_obj, source);
    if (cmd) {
      pug_log("%s", cmd);
      const int res = system(cmd);
      free(cmd);
      if (res != 0) {
        pug_log("Error while compiling '%s'", source);
        exit(1);
      }
      need_linking = true;
    }
    sources_str_len += strlen(source_obj) + 1; // Add space after filename
    free(source_obj);
  }

  // No files have changed - return
  if (!need_linking) {
    pug_log("Nothing to compile");
    return;
  }

  // Link .o files to an executable
  pug_log("Linking");
  char obj_files[sources_str_len + 1]; // +1 for the null terminator
  obj_files[0] = '\0';                 // Ensure it starts as an empty string
  // Create string with .c files changed to .o
  for (size_t i = 0; i < sources_len; ++i) {
    const char *source = exe->sources[i];
    char *source_obj = pug_replace_file_extension(source, "o");
    strcat(obj_files, source_obj);
    strcat(obj_files, " ");
    free(source_obj);
  }
  char *cmd = pug_strdup_printf("%s %s -o %s %s", CC, exe->ldflags ? exe->ldflags : "", exe->name, obj_files);
  pug_log("%s", cmd);
  const int res = system(cmd);
  free(cmd);
  if (res != 0) {
    pug_log("Error while linking '%s'", exe->name);
    exit(1);
  }
}

// Cleanup executable and its .o files
void executable_clean(Executable *exe) {
  pug_log("Cleanup");
  remove(exe->name);
  size_t sources_len = pug_get_array_len(exe->sources);
  for (size_t i = 0; i < sources_len; i++) {
    char *obj_file = pug_replace_file_extension(exe->sources[i], "o");
    if (obj_file) {
      remove(obj_file);
      free(obj_file);
    }
  }
}

// Checks if pug.c was changed and recompiles itself if needed
void pug_self_rebuild(void) {
  if (!pug_file_changed_after("pug.c", "pug"))
    return;
  pug_log("Rebuilding pug");
  const int res = system(CC " -o pug pug.c");
  if (res != 0) {
    pug_log("Error rebuilding pug");
    exit(1);
  }
  system("./pug");
  exit(0);
}

// --------------------------- IMPLEMENTATION END -------------------------- //

#endif // __PUG_H__
