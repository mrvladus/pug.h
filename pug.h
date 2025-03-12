// MIT Licence
//
// Copyright 2025 Vlad Krupinskii <mrvladus@yandex.ru>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef __PUG_H__
#define __PUG_H__

#define PUG_VERSION "1.0"

// --------------------------- CONFIGURATION START ------------------------- //

#ifndef CC
#define CC "cc"
#endif // CC

// --------------------------- CONFIGURATION END --------------------------- //

// --------------------------- DECLARATIONS START -------------------------- //

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize PUG with command-line arguments from main(int argc, char **argv)
void pug_init(int argc, char **argv);

// Executable build target
typedef struct {
  // Executable name. (Required)
  const char *name;
  // NULL-terminated array of source file strings. (Required)
  const char **sources;
  // Compile flags. (Optional)
  const char *cflags;
  // Linker flags. (Optional)
  const char *ldflags;
  // Install directory. Default is "/usr/local/bin". (Optional)
  const char *install_dir;
} Exe;

// Library build target
typedef struct {
  // Library name. (Required)
  const char *name;
  // NULL-terminated array of source file strings. (Required)
  const char **sources;
  // NULL-terminated array of header file strings. (Required)
  const char **headers;
  // Compile flags. (Optional)
  const char *cflags;
  // Build static library in addition to shared. Default is false. (Optional)
  bool build_static;
  // Install directory for shared and static libraries. Default is "/usr/local/lib". (Optional)
  const char *lib_install_dir;
  // Install directory for headers. Default is "/usr/local/include". (Optional)
  const char *headers_install_dir;
} Lib;

// Shortcut for setting target sources or headers
#define FILES(...) ((const char *[]){__VA_ARGS__, NULL})

// Build executable
void exe_build(Exe *exe);
// Cleanup executable build files
void exe_clean(Exe *exe);
// Install executable to its install_dir. Compile if needed.
void exe_install(Exe *exe);
// Run compiled executable. Compile if needed.
void exe_run(Exe *exe);

// Build library
void lib_build(Lib *lib);
// Cleanup library build files
void lib_clean(Lib *lib);
// Install library to its install_dir and headers to their install directory
void lib_install(Lib *lib);

// Check for library using its pkg-config name
// If required is true - exits with code 1 if library is not found.
void check_library(const char *pkg_config_name, bool required);

// Check command-line arguments for existing argument.
// e. g. "clean", "--build-static", "-h".
// 'arg' must be the same as it will be provided to pug executable:
// "clean" for "./pug clean" or "--build-static" for "./pug --build-static".
bool get_arg_bool(const char *arg);
// Get value of "--myarg=value" type argument. e. g. "--prefix=/usr".
// 'arg' must be in format "--myarg" without "=value" part.
const char *get_arg_value(const char *arg);

// Get environment variable or default if not defined
const char *env_or(const char *env, const char *default_val);

#ifdef __cplusplus
}
#endif

// ---------------------------- DECLARATIONS END ---------------------------- //

// ---------------------------- IMPLEMENTATION START ------------------------ //

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <sys/stat.h>
#include <sys/wait.h>

// --- MACROS --- //

#define TEXT_GREEN(text) "\033[0;32m" text "\033[0m"
#define TEXT_RED(text) "\033[0;31m" text "\033[0m"

#define PUG_LOG(format, ...) printf(TEXT_GREEN("[PUG] ") format "\n", ##__VA_ARGS__);
#define PUG_ERROR(format, ...) printf(TEXT_RED("[PUG ERROR] ") format "\n", ##__VA_ARGS__);
#define PUG_MALLOC(type, var, size)                                                                                    \
  type var = (type)malloc(size);                                                                                       \
  if (!var) {                                                                                                          \
    PUG_ERROR("Memory allocation error");                                                                              \
    exit(EXIT_FAILURE);                                                                                                \
  }
#define PUG_FREE(...)                                                                                                  \
  do {                                                                                                                 \
    void *ptrs[] = {__VA_ARGS__};                                                                                      \
    size_t count = sizeof(ptrs) / sizeof(ptrs[0]);                                                                     \
    for (size_t i = 0; i < count; ++i) {                                                                               \
      if (ptrs[i]) {                                                                                                   \
        free(ptrs[i]);                                                                                                 \
      }                                                                                                                \
    }                                                                                                                  \
  } while (0)

// --- GLOBAL VARIABLES --- //

int pug_argc;
char **pug_argv;

// --- PRIVATE UTIL FUNCTIONS --- //

// Get length of NULL-terminated array
static inline size_t _get_array_len(const char **arr) {
  size_t len = 0;
  if (!arr)
    return len;
  while (arr[len])
    len++;
  return len;
}

// Allocate formatted string
static char *_strdup_printf(const char *format, ...) {
  if (!format)
    return NULL;
  va_list args;
  va_start(args, format);
  int needed = vsnprintf(NULL, 0, format, args);
  va_end(args);
  if (needed < 0)
    return NULL;
  PUG_MALLOC(char *, buffer, needed + 1);
  va_start(args, format);
  int written = vsnprintf(buffer, needed + 1, format, args);
  va_end(args);
  if (written < 0) {
    PUG_FREE(buffer);
    return NULL;
  }
  return buffer;
}

// Replace file extension: returns a new allocated filename with new_ext appended
static inline char *_replace_file_extension(const char *filename, const char *new_ext) {
  const char *dot = strrchr(filename, '.');
  size_t basename_len = (dot && dot != filename) ? (size_t)(dot - filename) : strlen(filename);
  size_t new_len = basename_len + 1 + strlen(new_ext) + 1;
  PUG_MALLOC(char *, new_filename, new_len);
  strncpy(new_filename, filename, basename_len);
  new_filename[basename_len] = '\0';
  snprintf(new_filename + basename_len, new_len - basename_len, ".%s", new_ext);
  return new_filename;
}

// Get file extension (returns pointer inside filename)
static const char *_get_file_extension(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if (!dot || dot == filename)
    return NULL;
  return dot + 1;
}

// Check if file exists
static bool _file_exists(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (file) {
    fclose(file);
    return true;
  }
  return false;
}

// Check if file1 was modified after file2
static bool _file_changed_after(const char *file1, const char *file2) {
  struct stat stat1, stat2;
  if (stat(file1, &stat1) != 0 || stat(file2, &stat2) != 0)
    return false;
  return stat1.st_mtime > stat2.st_mtime;
}

// Returns a string representing the command-line arguments
static char *_get_args_as_string(int argc, char **argv) {
  size_t total_length = 0;
  for (int i = 0; i < argc; i++)
    total_length += strlen(argv[i]) + 1;
  PUG_MALLOC(char *, full_cmdline, total_length);
  full_cmdline[0] = '\0';
  for (int i = 0; i < argc; i++) {
    strcat(full_cmdline, argv[i]);
    if (i != argc - 1)
      strcat(full_cmdline, " ");
  }
  return full_cmdline;
}

// Concatenate object file names from an array of source files. Returns a newly allocated string.
static char *_build_obj_files_string(const char **sources) {
  size_t sources_len = _get_array_len(sources);
  size_t total_len = 0;
  PUG_MALLOC(char **, objs, sources_len * sizeof(char *));
  // Build each object file string and compute total length.
  for (size_t i = 0; i < sources_len; i++) {
    objs[i] = _replace_file_extension(sources[i], "o");
    total_len += strlen(objs[i]) + 1; // plus space
  }
  PUG_MALLOC(char *, result, total_len + 1);
  result[0] = '\0';
  for (size_t i = 0; i < sources_len; i++) {
    strcat(result, objs[i]);
    strcat(result, " ");
    PUG_FREE(objs[i]);
  }
  PUG_FREE(objs);
  return result;
}

// Clean all files. if is_exe true then remove the executable itself
static void _clean_files(const char *name, bool is_exe, const char **files) {
  PUG_LOG("Cleanup");
  if (is_exe)
    remove(name);
  else {
    char *shared_so = _strdup_printf("%s.so", name);
    char *shared_dll = _strdup_printf("%s.dll", name);
    char *static_a = _strdup_printf("%s.a", name);
    remove(shared_so);
    remove(shared_dll);
    remove(static_a);
    PUG_FREE(shared_so, shared_dll, static_a);
  }
  size_t len = _get_array_len(files);
  for (size_t i = 0; i < len; i++) {
    char *obj_file = _replace_file_extension(files[i], "o");
    if (obj_file) {
      remove(obj_file);
      PUG_FREE(obj_file);
    }
  }
}

// Copy file from src to dest using the cp command.
static void _copy_file(const char *src, const char *dest) {
  char *mkdir_cmd = _strdup_printf("mkdir -p \"$(dirname '%s')\"", dest);
  system(mkdir_cmd);
  free(mkdir_cmd);
  char *cp_cmd = _strdup_printf("cp \"%s\" \"%s\"", src, dest);
  PUG_LOG("%s", cp_cmd);
  int ret = system(cp_cmd);
  PUG_FREE(cp_cmd);
  if (ret != 0) {
    PUG_LOG("Failed to copy '%s' to '%s'", src, dest);
    exit(EXIT_FAILURE);
  }
}

static bool _program_exists(const char *name) {
  size_t len = strlen(name);
  char cmd[len + 7];
  snprintf(cmd, len, "which %s", name);
  return system(cmd) == 0;
}

void _auto_rebuild() {
  if (!_file_exists("pug.c")) {
    PUG_ERROR("'pug.c' file not found. Auto-rebuild is not possible.");
    return;
  }
  if (!_file_changed_after("pug.c", "pug"))
    return;
  PUG_LOG("Rebuilding pug");
  int ret = system(CC " -o pug pug.c");
  if (ret != 0) {
    PUG_ERROR("Error rebuilding 'pug.c'");
    exit(EXIT_FAILURE);
  }
  char *cmd = _get_args_as_string(pug_argc, pug_argv);
  system(cmd);
  PUG_FREE(cmd);
  exit(EXIT_SUCCESS);
}

// Compile sources.
// Retuns true if compiled, false if no files was changed.
bool _compile_sources(const char **sources, const char *cflags, bool is_library) {
  bool out = false;
  size_t sources_len = _get_array_len(sources);

  // Get the number of available processors (CPU cores)
  int num_processors = 1; // Default to 1 if there is an error getting the processor count.

  // For Linux, use sysconf to get the number of processors
  num_processors = sysconf(_SC_NPROCESSORS_ONLN);

  if (num_processors <= 0) {
    PUG_ERROR("Error getting the number of processors. Defaulting to 1.");
    num_processors = 1; // Default to 1 if sysctl, sysconf, or GetSystemInfo fails
  }

  // Track the number of currently running child processes
  int active_processes = 0;

  for (size_t i = 0; i < sources_len; ++i) {
    // Wait for a child process to finish if we have hit the max number of allowed processes
    if (active_processes >= num_processors) {
      int status;
      waitpid(-1, &status, 0); // Wait for any child process to finish
      active_processes--;
    }

    pid_t pid = fork(); // Create a new child process

    if (pid == -1) {
      // Error while forking
      PUG_ERROR("Error creating process for '%s'", sources[i]);
      exit(EXIT_FAILURE);
    } else if (pid == 0) {
      // Child process - compile source
      char *source_obj = _replace_file_extension(sources[i], "o");
      const char *cmd_fmt = "%s%s-c %s -o %s %s";
      char *cmd = NULL;

      if (_file_exists(source_obj)) {
        if (_file_changed_after(sources[i], source_obj)) {
          cmd = _strdup_printf(cmd_fmt, CC, is_library ? " -fPIC " : " ", cflags ? cflags : "", source_obj, sources[i]);
        }
      } else {
        cmd = _strdup_printf(cmd_fmt, CC, is_library ? " -fPIC " : " ", cflags ? cflags : "", source_obj, sources[i]);
      }

      if (cmd) {
        PUG_LOG("%s", cmd);
        int ret = system(cmd);
        PUG_FREE(cmd);
        if (ret != 0) {
          PUG_ERROR("Error while compiling '%s'", sources[i]);
          exit(EXIT_FAILURE);
        }
      }

      PUG_FREE(source_obj);
      exit(0); // Exit the child process after completion
    } else {
      active_processes++; // Increment the active processes count
    }
  }

  // Parent process waits for all remaining child processes to finish
  for (size_t i = 0; i < sources_len; ++i) {
    int status;
    waitpid(-1, &status, 0); // Wait for any child process to finish
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
      out = false;
    } else {
      out = true;
    }
  }

  return out;
}

// --- FUNCTIONS --- //

// Initialize PUG with command-line arguments from main(int argc, char **argv)
void pug_init(int argc, char **argv) {
  pug_argc = argc;
  pug_argv = argv;
  _auto_rebuild();
}

void exe_build(Exe *exe) {
  if (!exe->name) {
    PUG_LOG("Executable name is NULL");
    exit(EXIT_FAILURE);
  }
  PUG_LOG("Compiling executable '%s'", exe->name);
  bool need_linking = _compile_sources(exe->sources, exe->cflags, false);
  if (!need_linking) {
    PUG_LOG("Nothing to compile for executable '%s'", exe->name);
    return;
  }
  PUG_LOG("Linking executable '%s'", exe->name);
  char *obj_files = _build_obj_files_string(exe->sources);
  char *cmd = _strdup_printf("%s %s %s -o %s", CC, obj_files, exe->ldflags ? exe->ldflags : "", exe->name);
  PUG_LOG("%s", cmd);
  int ret = system(cmd);
  PUG_FREE(cmd, obj_files);
  if (ret != 0) {
    PUG_LOG("Error while linking '%s'", exe->name);
    exit(EXIT_FAILURE);
  }
}

void exe_clean(Exe *exe) { _clean_files(exe->name, true, exe->sources); }

// Install executable to its install_dir. Compile if needed.
void exe_install(Exe *exe) {
  if (!_file_exists(exe->name))
    exe_build(exe);
  const char *install_dir = exe->install_dir ? exe->install_dir : "/usr/local/bin";
  PUG_LOG("Installing executable '%s' to '%s", exe->name, install_dir);
  _copy_file(exe->name, install_dir);
}

// Run compiled executable. Compile if needed.
void exe_run(Exe *exe) {
  if (!_file_exists(exe->name))
    exe_build(exe);
  PUG_LOG("Running executable '%s'", exe->name);
  char *cmd = _strdup_printf("./%s", exe->name);
  system(cmd);
  PUG_FREE(cmd);
}

void lib_build(Lib *lib) {
  if (!lib->name) {
    PUG_LOG("Library name is NULL");
    exit(EXIT_FAILURE);
  }
  PUG_LOG("Compiling library '%s'", lib->name);
  bool lib_rebuilt = _compile_sources(lib->sources, lib->cflags, true);
  if (!lib_rebuilt) {
    PUG_LOG("Nothing to compile for library '%s'", lib->name);
    return;
  }
  // Build shared library
  PUG_LOG("Building shared library '%s'", lib->name);
  char *obj_files = _build_obj_files_string(lib->sources);
  char *cmd = _strdup_printf("%s -shared -o %s.so %s", CC, lib->name, obj_files);
  PUG_LOG("%s", cmd);
  int ret = system(cmd);
  PUG_FREE(cmd);
  if (ret != 0) {
    PUG_ERROR("Failed while building shared library '%s'", lib->name);
    exit(EXIT_FAILURE);
  }
  // Build static library if requested
  if (lib->build_static) {
    PUG_LOG("Building static library '%s'", lib->name);
    cmd = _strdup_printf("ar rcs %s.a %s", lib->name, obj_files);
    PUG_LOG("%s", cmd);
    ret = system(cmd);
    free(cmd);
    if (ret != 0) {
      PUG_ERROR("Failed while building static library '%s'", lib->name);
      exit(EXIT_FAILURE);
    }
  }
  PUG_FREE(obj_files);
}

void lib_clean(Lib *lib) { _clean_files(lib->name, false, lib->sources); }

void lib_install(Lib *lib) {
  PUG_LOG("Installing library '%s'", lib->name);
  const char *lib_dir = lib->lib_install_dir ? lib->lib_install_dir : "/usr/local/lib";
  char *shared = _strdup_printf("%s.so", lib->name);
  char *shared_dest = _strdup_printf("%s/%s.so", lib_dir, lib->name);
  _copy_file(shared, shared_dest);
  PUG_FREE(shared, shared_dest);
  if (lib->build_static) {
    char *static_lib = _strdup_printf("%s.a", lib->name);
    char *static_dest = _strdup_printf("%s/%s.a", lib_dir, lib->name);
    _copy_file(static_lib, static_dest);
    PUG_FREE(static_lib, static_dest);
  }
  // Install headers if a destination directory is provided.
  if (lib->headers_install_dir) {
    size_t headers_len = _get_array_len(lib->headers);
    for (size_t i = 0; i < headers_len; i++) {
      const char *header = lib->headers[i];
      // Create destination path: headers_install_dir + PATH_SEP + basename(header)
      const char *basename = strrchr(header, '/');
      basename = basename ? basename + 1 : header;
      char *header_dest = _strdup_printf("%s"
                                         "/"
                                         "%s",
                                         lib->headers_install_dir, basename);
      _copy_file(header, header_dest);
      PUG_FREE(header_dest);
    }
  }
}

const char *env_or(const char *env, const char *default_val) {
  const char *val = getenv(env);
  return val ? val : default_val;
}

// Check command-line arguments for existing argument.
// e. g. "clean", "--build-static", "-h".
// 'arg' must be the same as it will be provided to pug executable:
// "clean" for "./pug clean" or "--build-static" for "./pug --build-static".
bool get_arg_bool(const char *arg) {
  if (pug_argc == 1)
    return false;
  for (size_t i = 0; i < pug_argc; ++i)
    if (!strcmp(pug_argv[i], arg))
      return true;
  return false;
}

// Get value of "--myarg=value" type argument. e. g. "--prefix=/usr".
// 'arg' must be in format "--myarg" without "=value" part.
const char *get_arg_value(const char *arg) {
  if (pug_argc == 1)
    return NULL;
  for (size_t i = 0; i < pug_argc; ++i) {
    const char *argument = strstr(argument, pug_argv[i]);
    if (argument) {
      const char *eq = strstr(argument, "=");
      if (eq)
        return ++eq;
    }
  }
  return NULL;
}

// Check for library using its pkg-config name
// If required is true - exits with code 1 if library is not found.
void check_library(const char *pkg_config_name, bool required) {
  if (!_program_exists("pkg-config")) {
    PUG_ERROR("Program 'pkg-config' is not found. Skipping check for library %s", pkg_config_name);
    return;
  }
  printf(TEXT_GREEN("[PUG]") " Checking for installed library using pkg-config: %s ... ", pkg_config_name);
  char *cmd = _strdup_printf("pkg-config --exists %s", pkg_config_name);
  int res = system(cmd);
  PUG_FREE(cmd);
  if (res == 0) {
    printf(TEXT_GREEN("YES") "\n");
  } else {
    printf(TEXT_RED("NO") "\n");
    if (required)
      exit(EXIT_FAILURE);
  }
}

// ---------------------------- IMPLEMENTATION END -------------------------- //

#endif // __PUG_H__
