set(LLVM_LINK_COMPONENTS support)
add_clang_executable(autopatch
  AutoPatch.cpp
  )
target_link_libraries(autopatch
  PRIVATE
  libclang
  clangTooling
  clangBasic
  clangASTMatchers
  clangLex
  )
