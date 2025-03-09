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

#include <stdbool.h>

// --------------------------- CONFIGURATION START ------------------------- //

#ifndef CC
#define CC "gcc"
#endif // CC

// --------------------------- CONFIGURATION END --------------------------- //

// --------------------------- DECLARATIONS START -------------------------- //

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
// Install executable to its install_dir
void exe_install(Exe *exe);

// Build library
void lib_build(Lib *lib);
// Cleanup library build files
void lib_clean(Lib *lib);
// Install library to its install_dir and headers to their install directory
void lib_install(Lib *lib);

// Get environment variable or default if not defined
const char *env_or(const char *env, const char *default_val);

// Checks if pug.c was changed and recompiles itself if needed
void auto_rebuild_pug(int argc, const char **argv);

// ---------------------------- DECLARATIONS END ---------------------------- //

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------- IMPLEMENTATION START ------------------------ //

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#define STAT _stat
#define PATH_SEP "\\"
#else
#define STAT stat
#define PATH_SEP "/"
#endif

// --- PRIVATE UTIL FUNCTIONS --- //

// Log formatted message
static inline void _log(const char *format, ...) {
  va_list args;
  printf("[PUG] ");
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
  printf("\n");
}

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
  char *buffer = (char *)malloc(needed + 1);
  if (!buffer)
    return NULL;
  va_start(args, format);
  int written = vsnprintf(buffer, needed + 1, format, args);
  va_end(args);
  if (written < 0) {
    free(buffer);
    return NULL;
  }
  return buffer;
}

// Replace file extension: returns a new allocated filename with new_ext appended
static inline char *_replace_file_extension(const char *filename, const char *new_ext) {
  const char *dot = strrchr(filename, '.');
  size_t basename_len = (dot && dot != filename) ? (size_t)(dot - filename) : strlen(filename);
  size_t new_len = basename_len + 1 + strlen(new_ext) + 1;
  char *new_filename = (char *)malloc(new_len);
  if (!new_filename)
    return NULL;
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
  if (STAT(file1, &stat1) != 0 || STAT(file2, &stat2) != 0)
    return false;
  return stat1.st_mtime > stat2.st_mtime;
}

// Returns a string representing the command-line arguments
static char *_get_args_as_string(int argc, const char **argv) {
  size_t total_length = 0;
  for (int i = 0; i < argc; i++)
    total_length += strlen(argv[i]) + 1;
  char *full_cmdline = malloc(total_length);
  if (!full_cmdline) {
    _log("Can't allocate memory");
    exit(EXIT_FAILURE);
  }
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
  char **objs = malloc(sources_len * sizeof(char *));
  if (!objs) {
    _log("Memory allocation failure");
    exit(EXIT_FAILURE);
  }
  // Build each object file string and compute total length.
  for (size_t i = 0; i < sources_len; i++) {
    objs[i] = _replace_file_extension(sources[i], "o");
    if (!objs[i]) {
      _log("Error creating object file name from: %s", sources[i]);
      exit(EXIT_FAILURE);
    }
    total_len += strlen(objs[i]) + 1; // plus space
  }
  char *result = malloc(total_len + 1);
  if (!result) {
    _log("Memory allocation failure");
    exit(EXIT_FAILURE);
  }
  result[0] = '\0';
  for (size_t i = 0; i < sources_len; i++) {
    strcat(result, objs[i]);
    strcat(result, " ");
    free(objs[i]);
  }
  free(objs);
  return result;
}

// Clean all files. if is_exe true then remove the executable itself
static void _clean_files(const char *name, bool is_exe, const char **files) {
  _log("Cleanup");
  if (is_exe)
    remove(name);
  else {
    char *shared_so = _strdup_printf("%s.so", name);
    char *shared_dll = _strdup_printf("%s.dll", name);
    char *static_a = _strdup_printf("%s.a", name);
    remove(shared_so);
    remove(shared_dll);
    remove(static_a);
    free(shared_so);
    free(shared_dll);
    free(static_a);
  }
  size_t len = _get_array_len(files);
  for (size_t i = 0; i < len; i++) {
    char *obj_file = _replace_file_extension(files[i], "o");
    if (obj_file) {
      remove(obj_file);
      free(obj_file);
    }
  }
}

// Copy file from src to dest using the cp command.
static void _copy_file(const char *src, const char *dest) {
  char *mkdir_cmd = _strdup_printf("mkdir -p \"$(dirname '%s')\"", dest);
  system(mkdir_cmd);
  free(mkdir_cmd);
  char *cp_cmd = _strdup_printf("cp \"%s\" \"%s\"", src, dest);
  _log("%s", cp_cmd);
  int ret = system(cp_cmd);
  free(cp_cmd);
  if (ret != 0) {
    _log("Failed to copy '%s' to '%s'", src, dest);
    exit(EXIT_FAILURE);
  }
}

// --- FUNCTIONS --- //

void exe_build(Exe *exe) {
  if (!exe->name) {
    _log("Executable name is NULL");
    exit(EXIT_FAILURE);
  }
  _log("Compiling executable '%s'", exe->name);
  size_t sources_len = _get_array_len(exe->sources);
  bool need_linking = false;
  for (size_t i = 0; i < sources_len; i++) {
    const char *source = exe->sources[i];
    char *source_obj = _replace_file_extension(source, "o");
    char *cmd = NULL;
    // Recompile if object file is missing or source is newer than object
    if (_file_exists(source_obj)) {
      if (_file_changed_after(source, source_obj))
        cmd = _strdup_printf("%s -c %s -o %s %s", CC, exe->cflags ? exe->cflags : "", source_obj, source);
    } else {
      cmd = _strdup_printf("%s -c %s -o %s %s", CC, exe->cflags ? exe->cflags : "", source_obj, source);
    }
    if (cmd) {
      _log("%s", cmd);
      int ret = system(cmd);
      free(cmd);
      if (ret != 0) {
        _log("Error while compiling '%s'", source);
        exit(EXIT_FAILURE);
      }
      need_linking = true;
    }
    free(source_obj);
  }
  if (!need_linking) {
    _log("Nothing to compile for executable '%s'", exe->name);
    return;
  }
  _log("Linking executable '%s'", exe->name);
  char *obj_files = _build_obj_files_string(exe->sources);
  char *cmd = _strdup_printf("%s %s %s -o %s", CC, obj_files, exe->ldflags ? exe->ldflags : "", exe->name);
  _log("%s", cmd);
  int ret = system(cmd);
  free(cmd);
  free(obj_files);
  if (ret != 0) {
    _log("Error while linking '%s'", exe->name);
    exit(EXIT_FAILURE);
  }
}

void exe_clean(Exe *exe) { _clean_files(exe->name, true, exe->sources); }

void exe_install(Exe *exe) {
  const char *install_dir = exe->install_dir ? exe->install_dir : "/usr/local/bin";
  _log("Installing executable '%s' to '%s", exe->name, install_dir);
  _copy_file(exe->name, install_dir);
}

void lib_build(Lib *lib) {
  if (!lib->name) {
    _log("Library name is NULL");
    exit(EXIT_FAILURE);
  }
  _log("Compiling library '%s'", lib->name);
  size_t sources_len = _get_array_len(lib->sources);
  bool lib_rebuilt = false;
  for (size_t i = 0; i < sources_len; i++) {
    const char *source = lib->sources[i];
    char *source_obj = _replace_file_extension(source, "o");
    char *cmd = NULL;
    if (_file_exists(source_obj)) {
      if (_file_changed_after(source, source_obj))
        cmd = _strdup_printf("%s -fPIC -c %s -o %s %s", CC, lib->cflags ? lib->cflags : "", source_obj, source);
    } else {
      cmd = _strdup_printf("%s -fPIC -c %s -o %s %s", CC, lib->cflags ? lib->cflags : "", source_obj, source);
    }
    if (cmd) {
      _log("%s", cmd);
      int ret = system(cmd);
      free(cmd);
      if (ret != 0) {
        _log("Error while compiling '%s'", source);
        exit(EXIT_FAILURE);
      }
      lib_rebuilt = true;
    }
    free(source_obj);
  }
  if (!lib_rebuilt) {
    _log("Nothing to compile for library '%s'", lib->name);
    return;
  }
  // Build shared library
  _log("Building shared library '%s'", lib->name);
  char *obj_files = _build_obj_files_string(lib->sources);
  char *cmd = _strdup_printf("%s -shared -o %s.so %s", CC, lib->name, obj_files);
  _log("%s", cmd);
  int ret = system(cmd);
  free(cmd);
  if (ret != 0) {
    _log("Error while building shared library '%s'", lib->name);
    exit(EXIT_FAILURE);
  }
  // Build static library if requested
  if (lib->build_static) {
    _log("Building static library '%s'", lib->name);
    cmd = _strdup_printf("ar rcs %s.a %s", lib->name, obj_files);
    _log("%s", cmd);
    ret = system(cmd);
    free(cmd);
    if (ret != 0) {
      _log("Error while building static library '%s'", lib->name);
      exit(EXIT_FAILURE);
    }
  }
  free(obj_files);
}

void lib_clean(Lib *lib) { _clean_files(lib->name, false, lib->sources); }

void lib_install(Lib *lib) {
  _log("Installing library '%s'", lib->name);
  const char *lib_dir = lib->lib_install_dir ? lib->lib_install_dir : "/usr/local/lib";
  char *shared = _strdup_printf("%s.so", lib->name);
  char *shared_dest = _strdup_printf("%s/%s.so", lib_dir, lib->name);
  _copy_file(shared, shared_dest);
  free(shared);
  free(shared_dest);
  if (lib->build_static) {
    char *static_lib = _strdup_printf("%s.a", lib->name);
    char *static_dest = _strdup_printf("%s/%s.a", lib_dir, lib->name);
    _copy_file(static_lib, static_dest);
    free(static_lib);
    free(static_dest);
  }
  // Install headers if a destination directory is provided.
  if (lib->headers_install_dir) {
    size_t headers_len = _get_array_len(lib->headers);
    for (size_t i = 0; i < headers_len; i++) {
      const char *header = lib->headers[i];
      // Create destination path: headers_install_dir + PATH_SEP + basename(header)
      const char *basename = strrchr(header, '/');
      basename = basename ? basename + 1 : header;
      char *header_dest = _strdup_printf("%s" PATH_SEP "%s", lib->headers_install_dir, basename);
      _copy_file(header, header_dest);
      free(header_dest);
    }
  }
}

void auto_rebuild_pug(int argc, const char **argv) {
  if (!_file_exists("pug.c")) {
    _log("Build file not found. Make sure it's called 'pug.c'.");
    exit(EXIT_FAILURE);
  }
  if (!_file_changed_after("pug.c", "pug"))
    return;
  _log("Rebuilding pug");
  int ret = system(CC " -o pug pug.c");
  if (ret != 0) {
    _log("Error rebuilding pug");
    exit(EXIT_FAILURE);
  }
  char *cmd = _get_args_as_string(argc, argv);
  system(cmd);
  free(cmd);
  exit(EXIT_SUCCESS);
}

const char *env_or(const char *env, const char *default_val) {
  const char *val = getenv(env);
  return val ? val : default_val;
}

// ---------------------------- IMPLEMENTATION END -------------------------- //

#ifdef __cplusplus
}
#endif

#endif // __PUG_H__
