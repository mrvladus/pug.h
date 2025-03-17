#ifndef _PUG_H_
#define _PUG_H_

#include <stdbool.h>

#ifndef CC
#define CC "cc"
#endif // CC

// Build target type
typedef enum {
  TARGET_EXE = 1 << 0,        // 1
  TARGET_SHARED_LIB = 1 << 1, // 2
  TARGET_STATIC_LIB = 1 << 2  // 4
} TargetType;
// Build target
typedef struct Target Target;
struct Target {
  const TargetType type;           // Type of build target
  const char *name;                // Name of executable or library
  const char **sources;            // List of source files. Use STRINGS() macro.
  const char **headers;            // List of header files. Use STRINGS() macro.
  const char *cflags;              // Compiler flags
  const char *ldflags;             // Linker flags
  const char *install_bin_dir;     // Directory to install executable. e. g /usr/bin
  const char *install_lib_dir;     // Directory to install shared library. e. g /usr/lib
  const char *install_headers_dir; // Directory to install headers. e. g /usr/include
  Target **dep_targets;            // List of Targets that this target depends on. These will be built first.
};
// Macro for setting list of files e. g. sources and headers
#define STRINGS(...) ((const char *[]){__VA_ARGS__, NULL})
// Macro for setting list of Targets
#define TARGETS(...) ((Target *[]){__VA_ARGS__, NULL})
// Initialize PUG
void pug_init(int argc, char **argv);
// Build target
void pug_build_target(Target *tgt);
// Install target
void pug_install_target(Target *tgt);
// Run target. Runs only if tgt->type is TARGET_EXE.
void pug_run_target(Target *tgt);
// Clean target build files
void pug_clean_target(Target *tgt);
// Check command-line arguments for existing argument. If arg not exists - use default_val.
// e. g. "clean", "--build-static", "-h".
// 'arg' must be the same as it will be provided to pug executable:
// "clean" for "./pug clean" or "--build-static" for "./pug --build-static".
bool pug_arg_bool_or(const char *arg, bool default_val);
// Get value from command-line argument of type "--arg=value" or default if argument is not provided.
// 'arg' must be in format "--myarg" without "=value" part.
const char *pug_value_or(const char *argument, const char *default_val);
// Get environment variable or default_val if variable is not set.
const char *pug_env_or(const char *env, const char *default_val);
// Copy file from src_file to dest_dir, creating directories if needed.
void install_file(const char *src_file, const char *dest_dir);
// Copy directory and all files in it src_dir to dest_dir, creating directories if needed.
void install_dir(const char *src_dir, const char *dest_dir);
// Check if libraries is installed using pkg-config.
// 'libs' is NULL-terminated array of strings. Use STRINGS() macro.
bool check_libs(const char **libs);

#ifdef PUG_IMPLEMENTATION

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

// ---------- MACROS ---------- //

#define TEXT_GREEN(text) "\033[0;32m" text "\033[0m"
#define TEXT_YELLOW(text) "\033[0;33m" text "\033[0m"
#define TEXT_RED(text) "\033[0;31m" text "\033[0m"
#define TEXT_CYAN(text) "\033[0;36m" text "\033[0m"
#define PUG_LOG(format, ...) printf(TEXT_GREEN("[PUG] ") format "\n", ##__VA_ARGS__)
#define PUG_ERROR(format, ...)                                                                                         \
  {                                                                                                                    \
    printf(TEXT_RED("[PUG ERROR] ") format "\n", ##__VA_ARGS__);                                                       \
    exit(EXIT_FAILURE);                                                                                                \
  }
#define PUG_MALLOC(type, var, size)                                                                                    \
  type var = (type)malloc(size);                                                                                       \
  if (!var)                                                                                                            \
    PUG_ERROR("Memory allocation error");
#define FILE_EXISTS(path) (access(path, F_OK) != -1)
#define PUG_ASSERT(expr)                                                                                               \
  if (!(expr)) {                                                                                                       \
    printf(TEXT_YELLOW("[PUG ASSERT FAILED] ") "%s\n", #expr);                                                         \
    exit(EXIT_FAILURE);                                                                                                \
  }
#define COMMAND(format, ...)                                                                                           \
  do {                                                                                                                 \
    char *_cmd = _strdup_printf(format, __VA_ARGS__);                                                                  \
    printf(TEXT_CYAN("[PUG COMMAND] ") "%s\n", _cmd);                                                                  \
    if (system(_cmd) != 0)                                                                                             \
      exit(EXIT_FAILURE);                                                                                              \
    free(_cmd);                                                                                                        \
  } while (0);
#define LIB_INSTALLED(lib) (system("pkg-config --check " lib) == 0)
#define PROGRAM_EXISTS(program) (system("which " program " > /dev/null") == 0)

// ---------- GLOBAL VARIABLES ---------- //

int pug_argc;    // Global arguments count. Set by pug_init.
char **pug_argv; // Global arguments array. Set by pug_init.

// ---------- UTILS ---------- //

// Get length of NULL-terminated array
static inline size_t _get_array_len(const char **arr) {
  size_t len = 0;
  if (!arr)
    return len;
  while (arr[len])
    len++;
  return len;
}
// Check if file1 was modified after file2
static bool _file_changed_after(const char *file1, const char *file2) {
  struct stat stat1, stat2;
  if (stat(file1, &stat1) != 0 || stat(file2, &stat2) != 0)
    return false;
  return stat1.st_mtime > stat2.st_mtime;
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
    free(buffer);
    return NULL;
  }
  return buffer;
}
// Concatenate object file names from an array of source files. Returns a newly allocated string.
static char *_build_obj_files_string(const char **sources) {
  size_t sources_len = _get_array_len(sources);
  size_t total_len = 0;
  PUG_MALLOC(char **, objs, sources_len * sizeof(char *));
  for (size_t i = 0; sources[i]; ++i) {
    objs[i] = _replace_file_extension(sources[i], "o");
    total_len += strlen(objs[i]) + 1;
  }
  PUG_MALLOC(char *, result, total_len + 1);
  result[0] = '\0';
  for (size_t i = 0; sources[i]; ++i) {
    strcat(result, objs[i]);
    strcat(result, " ");
    free(objs[i]);
  }
  free(objs);
  return result;
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

// ---------- FUNCTIONS ---------- //

// Initialize PUG
void pug_init(int argc, char **argv) {
  pug_argc = argc;
  pug_argv = argv;
  // Rebuild PUG is pug.c is changed
  if (!FILE_EXISTS("pug.c"))
    PUG_ERROR("'pug.c' file is not found. Auto-rebuild is not possible.");
  if (!_file_changed_after("pug.c", "pug"))
    return;
  PUG_LOG("Rebuilding pug");
  if (system(CC " -o pug pug.c") != 0)
    PUG_ERROR("Error rebuilding 'pug.c'");
  char *cmd = _get_args_as_string(pug_argc, pug_argv);
  system(cmd);
  exit(EXIT_SUCCESS);
}
// Build target
void pug_build_target(Target *tgt) {
  if (tgt->dep_targets)
    for (size_t i = 0; tgt->dep_targets[i]; ++i)
      pug_build_target(tgt->dep_targets[i]);
  PUG_ASSERT(tgt->name);
  PUG_ASSERT(tgt->sources);
  PUG_LOG("Building target '%s'", tgt->name);
  bool compiled = false;
  for (size_t i = 0; tgt->sources[i]; ++i) {
    const char *source = tgt->sources[i];
    char *source_obj = _replace_file_extension(source, "o");
    if (!FILE_EXISTS(source_obj) || _file_changed_after(source, source_obj)) {
      if (tgt->type & TARGET_SHARED_LIB || tgt->type & TARGET_STATIC_LIB) {
        COMMAND(CC " -fPIC -c %s -o %s %s", tgt->cflags ? tgt->cflags : "", source_obj, source);
      } else {
        COMMAND(CC " -c %s -o %s %s", tgt->cflags ? tgt->cflags : "", source_obj, source);
      }
      compiled = true;
    }
    free(source_obj);
  }
  if (!compiled)
    return;
  char *obj_files = _build_obj_files_string(tgt->sources);
  // Link executable
  if (tgt->type & TARGET_EXE) {
    PUG_LOG("Linking executable '%s'", tgt->name);
    COMMAND(CC " %s %s -o %s", obj_files, tgt->ldflags ? tgt->ldflags : "", tgt->name);
  }
  // Build shared library
  if (tgt->type & TARGET_SHARED_LIB) {
    PUG_LOG("Building shared library '%s'", tgt->name);
    COMMAND(CC " -shared -o %s.so %s", tgt->name, obj_files);
  }
  // Build static library
  if (tgt->type & TARGET_STATIC_LIB) {
    PUG_LOG("Building static library '%s'", tgt->name);
    COMMAND("ar rcs %s.a %s", tgt->name, obj_files);
  }
  free(obj_files);
}
// Install target
void pug_install_target(Target *tgt) {
  if (tgt->dep_targets)
    for (size_t i = 0; tgt->dep_targets[i]; ++i)
      pug_install_target(tgt->dep_targets[i]);
  PUG_ASSERT(tgt->name);
  if (tgt->type & TARGET_EXE) {
    PUG_ASSERT(tgt->install_bin_dir);
    COMMAND("cp -f %s %s", tgt->name, tgt->install_lib_dir);
  }
  if (tgt->type & TARGET_SHARED_LIB) {
    PUG_ASSERT(tgt->install_lib_dir);
    COMMAND("cp -f %s.{so,dll} %s", tgt->name, tgt->install_lib_dir);
  }
  if (tgt->type & TARGET_STATIC_LIB) {
    PUG_ASSERT(tgt->install_lib_dir);
    COMMAND("cp -f %s.{a} %s", tgt->name, tgt->install_lib_dir);
  }
}
// Run target. Runs only if tgt->type is TARGET_EXE.
void pug_run_target(Target *tgt) {
  PUG_ASSERT(tgt->name);
  COMMAND("./%s", tgt->name);
  exit(EXIT_SUCCESS);
}
// Clean target build files
void pug_clean_target(Target *tgt) {
  if (tgt->dep_targets)
    for (size_t i = 0; tgt->dep_targets[i]; ++i)
      pug_clean_target(tgt->dep_targets[i]);
  PUG_ASSERT(tgt->name);
  PUG_ASSERT(tgt->sources);
  PUG_LOG("Clean build files for '%s'", tgt->name);
  char *obj_files = _build_obj_files_string(tgt->sources);
  COMMAND("rm -f %s %s.so %s.a %s.dll %s", tgt->name, tgt->name, tgt->name, tgt->name, obj_files);
  free(obj_files);
}
// Check command-line arguments for existing argument. If arg not exists - use default_val.
// e. g. "clean", "--build-static", "-h".
// 'arg' must be the same as it will be provided to pug executable:
// "clean" for "./pug clean" or "--build-static" for "./pug --build-static".
bool pug_arg_bool_or(const char *arg, bool default_val) {
  if (pug_argc == 1)
    return false;
  for (size_t i = 0; i < pug_argc; ++i)
    if (!strcmp(pug_argv[i], arg))
      return true;
  return false;
}
// Get value from command-line argument of type "--arg=value" or default if argument is not provided.
// 'arg' must be in format "--myarg" without "=value" part.
const char *pug_arg_value_or(const char *argument, const char *default_val) {
  if (pug_argc == 1)
    return default_val;
  for (size_t i = 0; i < pug_argc; ++i) {
    const char *argument = strstr(argument, pug_argv[i]);
    if (argument) {
      const char *eq = strstr(argument, "=");
      if (eq)
        return ++eq;
    }
  }
  return default_val;
}
// Get environment variable or default if not defined
const char *pug_env_or(const char *env, const char *default_val) {
  const char *val = getenv(env);
  return val ? val : default_val;
}
// Copy file from src_file to dest_dir, creating directories if needed.
void install_file(const char *src_file, const char *dest_dir) {
  PUG_LOG("Install file '%s' to '%s'", src_file, dest_dir);
  COMMAND("mkdir -p %s", dest_dir);
  COMMAND("cp -f %s %s", src_file, dest_dir);
}
// Copy directory and all files in it src_dir to dest_dir, creating directories if needed.
void install_dir(const char *src_dir, const char *dest_dir) {
  PUG_LOG("Install dir '%s' to '%s'", src_dir, dest_dir);
  COMMAND("cp -rf %s %s", src_dir, dest_dir);
}
// Check if libraries is installed using pkg-config.
// 'libs' is NULL-terminated array of strings. Use STRINGS() macro.
bool check_libs(const char **libs) {
  PUG_ASSERT(libs);
  if (!PROGRAM_EXISTS("pkg-config")) {
    PUG_LOG("Program " TEXT_CYAN("pkg-config") " is not found. Can't check installed libs.");
    return false;
  }
  bool out = true;
  for (size_t i = 0; libs[i]; ++i) {
    printf(TEXT_GREEN("[PUG]") " Checking for " TEXT_CYAN("%s") " ... ", libs[i]);
    char *cmd = _strdup_printf("pkg-config --exists %s", libs[i]);
    if (system(cmd) == 0) {
      printf(TEXT_GREEN("YES") "\n");
    } else {
      printf(TEXT_RED("NO") "\n");
      out = false;
    }
    free(cmd);
  }
  return out;
}

#endif // PUG_IMPLEMENTATION

#endif // _PUG_H_
