#ifndef CPP_ENTRY_H_
#define CPP_ENTRY_H_

// Header file that has entry point for the build system that jumps from C
// code to C++ code.
//
// This avoids having to add extern "C" and avoid bools in all the headers.

#ifdef __cplusplus
extern "C" {
#endif

void cpp_entry();

#ifdef __cplusplus
}
#endif

#endif  // CPP_ENTRY_H_
