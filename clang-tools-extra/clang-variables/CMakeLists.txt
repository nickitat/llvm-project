set(LLVM_LINK_COMPONENTS
  Support
)

add_clang_tool(clang-variables
  clang-variables.cpp

  DEPENDS
  ClangDriverOptions
)

clang_target_link_libraries(clang-variables
  PRIVATE
  clangAST
  clangASTMatchers
  #clangAnalysis
  #clangBasic
  #clangDriver
  clangDynamicASTMatchers
  #clangEdit
  #clangFrontend
  #clangFrontendTool
  #clangLex
  #clangParse
  #clangRewrite
  #clangRewriteFrontend
  #clangSema
  #clangSerialization
  clangTooling
  clangToolingCore
)

include_directories(SYSTEM
  ${CMAKE_CURRENT_SOURCE_DIR}
  clang/include
  llvm/include
)

