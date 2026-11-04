/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "Globals.h"

#ifdef XP_WIN
#  include <processenv.h>
#endif

// ***************************************************************************
// Constants defining allocator size classes and behavior.

namespace mozilla {

#ifndef MALLOC_STATIC_PAGESIZE
#  define GLOBAL(type, name, value) type name;
#  define GLOBAL_ASSERT(...)
#  include "Globals.inc"
#  undef GLOBAL_ASSERT
#  undef GLOBAL

size_t gRealPageSize = 0;
size_t gPageSize = 0;

void DefineGlobals() {
#  define GLOBAL(type, name, value) name = value;
#  define GLOBAL_LOG2 mozilla::FloorLog2
#  define GLOBAL_ASSERT MOZ_RELEASE_ASSERT
#  define GLOBAL_CONSTEXPR
#  include "Globals.inc"
#  undef GLOBAL_CONSTEXPR
#  undef GLOBAL_ASSERT
#  undef GLOBAL_LOG2
#  undef GLOBAL
}
#endif

#ifdef XP_WIN

// Implement getenv without using malloc.
static char mozillaMallocOptionsBuf[64];

#  define getenv xgetenv
static char* getenv(const char* name) {
  if (GetEnvironmentVariableA(name, mozillaMallocOptionsBuf,
                              sizeof(mozillaMallocOptionsBuf)) > 0) {
    return mozillaMallocOptionsBuf;
  }

  return nullptr;
}
#else

// Firefox-Legacy: on macOS 10.13 getenv() takes an os_unfair_lock on the
// environment table. During PGO instrumented startup, LLVM's compiler-rt
// profile runtime calls setenv() while malloc_init() wants to call
// getenv() — recursive-lock abort. Walk `environ` directly to bypass the
// libsystem_c lock. The returned pointer is into the live environ array
// and does not need to be freed. This replaces the regular getenv() for
// every call in Globals.cpp and (via the unified-build include order)
// mozjemalloc.cpp.
#  include <string.h>
extern "C" char** environ;
#  define getenv xgetenv
static char* xgetenv(const char* name) {
  if (!environ) {
    return nullptr;
  }
  size_t nameLen = strlen(name);
  for (char** e = environ; *e; e++) {
    if (strncmp(*e, name, nameLen) == 0 && (*e)[nameLen] == '=') {
      return (*e) + nameLen + 1;
    }
  }
  return nullptr;
}
#endif

// *****************************
// Runtime configuration options.

// Set in malloc_init_hard() once the page size is known.
size_t opt_dirty_max;

#ifdef MALLOC_RUNTIME_CONFIG
bool opt_junk = OPT_JUNK_DEFAULT;
bool opt_zero = OPT_ZERO_DEFAULT;
PoisonType opt_poison = OPT_POISON_DEFAULT;
size_t opt_poison_size = OPT_POISON_SIZE_DEFAULT;
#endif

bool opt_randomize_small = true;

}  // namespace mozilla
