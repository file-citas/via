/*===- DataFlowCallbacks.cpp - a standalone DataFlow trace          -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Instrumentation callbacks for DataFlow.cpp.
// These functions should not be instrumented by DFSan, so we
// keep them in a separate file and compile it w/o DFSan.
//===----------------------------------------------------------------------===*/
#include "DataFlow.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

static size_t currentDSO;
static __thread size_t CurrentFunc;
static __thread size_t CurrentDso;
static uint32_t *GuardsBeg[MAX_DSO], *GuardsEnd[MAX_DSO];
static inline bool BlockIsEntry(size_t BlockIdx, size_t dso) {
  return __dft.PCsBeg[dso][BlockIdx * 2 + 1] & PCFLAG_FUNC_ENTRY;
}

extern "C" {

size_t get_dso(uint32_t *guard) {
  for(size_t i=0; i<__dft.NumDSO; i++) {
    if(guard >= GuardsBeg[i] && guard < GuardsEnd[i]) {
      return i;
    }
  }
  assert(false);
}

void __sanitizer_cov_trace_pc_guard_init(uint32_t *start,
                                         uint32_t *stop) {
  //assert(__dft.NumFuncs == 0 && "This tool does not support DSOs");
  __dft.NumDSO++;
  size_t dso = __dft.NumDSO-1;
  assert(start < stop && "The code is not instrumented for coverage");
  if (start == stop || *start) return;  // Initialize only once.
  GuardsBeg[dso] = start;
  GuardsEnd[dso] = stop;
}

void __sanitizer_cov_pcs_init(const uintptr_t *pcs_beg,
                              const uintptr_t *pcs_end) {
  //if (__dft.NumGuards) return;  // Initialize only once.
  size_t dso = __dft.NumDSO-1;
  __dft.NumGuards[dso] = GuardsEnd[dso] - GuardsBeg[dso];
  __dft.PCsBeg[dso] = pcs_beg;
  __dft.PCsEnd[dso] = pcs_end;
  assert(__dft.NumGuards[dso] == (__dft.PCsEnd[dso] - __dft.PCsBeg[dso]) / 2);
  for (size_t i = 0; i < __dft.NumGuards[dso]; i++) {
    if (BlockIsEntry(i, dso)) {
      __dft.NumFuncs[dso]++;
      __dft.NumFuncsTot++;
      GuardsBeg[dso][i] = __dft.NumFuncs[dso];
    }
  }
  __dft.BBExecuted[dso] = (bool*)calloc(__dft.NumGuards[dso], sizeof(bool));
  fprintf(stderr, "INFO: %zd instrumented function(s) observed "
          "and %zd basic blocks (DSO %ld)\n", __dft.NumFuncs[dso], __dft.NumGuards[dso], dso);
}

void __sanitizer_cov_trace_pc_indir(uint64_t x){}  // unused.

void __sanitizer_cov_trace_pc_guard(uint32_t *guard) {
  size_t dso = get_dso(guard);
  CurrentDso = dso;
  size_t GuardIdx = guard - GuardsBeg[dso];
  // assert(GuardIdx < __dft.NumGuards);
  __dft.BBExecuted[dso][GuardIdx] = true;
  if (!*guard) return;  // not a function entry.
  uint32_t FuncNum = *guard - 1;  // Guards start from 1.
  // assert(FuncNum < __dft.NumFuncs);
  CurrentFunc = FuncNum;
}

void __dfsw___sanitizer_cov_trace_switch(uint64_t Val, uint64_t *Cases,
                                         dfsan_label L1, dfsan_label UnusedL) {
  if(__dft.FuncLabels != NULL) {
    assert(CurrentFunc < __dft.NumFuncs[CurrentDso]);
    __dft.FuncLabels[CurrentDso][CurrentFunc] |= L1;
  }
}

#define HOOK(Name, Type)                                                       \
  void Name(Type Arg1, Type Arg2, dfsan_label L1, dfsan_label L2) {            \
    if(__dft.FuncLabels != NULL) { \
      __dft.FuncLabels[CurrentDso][CurrentFunc] |= L1 | L2;                                  \
    }\
  }
    //assert(CurrentFunc < __dft.NumFuncs);
      //if(L1|L2) printf("R %p\n", __builtin_return_address(0)); \

HOOK(__dfsw___sanitizer_cov_trace_const_cmp1, uint8_t)
HOOK(__dfsw___sanitizer_cov_trace_const_cmp2, uint16_t)
HOOK(__dfsw___sanitizer_cov_trace_const_cmp4, uint32_t)
HOOK(__dfsw___sanitizer_cov_trace_const_cmp8, uint64_t)
HOOK(__dfsw___sanitizer_cov_trace_cmp1, uint8_t)
HOOK(__dfsw___sanitizer_cov_trace_cmp2, uint16_t)
HOOK(__dfsw___sanitizer_cov_trace_cmp4, uint32_t)
HOOK(__dfsw___sanitizer_cov_trace_cmp8, uint64_t)

} // extern "C"
