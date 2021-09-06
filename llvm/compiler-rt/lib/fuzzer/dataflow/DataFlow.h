/*===- DataFlow.h - a standalone DataFlow trace                     -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Internal header file to connect DataFlow.cpp and DataFlowCallbacks.cpp.
//===----------------------------------------------------------------------===*/

#ifndef __LIBFUZZER_DATAFLOW_H
#define __LIBFUZZER_DATAFLOW_H

#include <cstddef>
#include <cstdint>
#include <sanitizer/dfsan_interface.h>

#define MAX_DSO 64
// This data is shared between DataFlowCallbacks.cpp and DataFlow.cpp.
struct CallbackData {
  size_t NumDSO;
  size_t NumFuncsTot;
  size_t NumFuncs[MAX_DSO];
  size_t NumGuards[MAX_DSO];
  const uintptr_t *PCsBeg[MAX_DSO], *PCsEnd[MAX_DSO];
  dfsan_label **FuncLabels;  // Array of NumFuncs elements.
  bool *BBExecuted[MAX_DSO];         // Array of NumGuards elements.
};

extern CallbackData __dft;

enum {
  PCFLAG_FUNC_ENTRY = 1,
};

#endif  // __LIBFUZZER_DATAFLOW_H
