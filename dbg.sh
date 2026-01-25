#!/usr/bin/sh

# -mllvm -debug \
# -mllvm -print-after-all \
# -mllvm -print-before-all \
./build_dbg/bin/clang++ -g -O2 inline.cc -o inline.out \
  -mllvm -debug \
  -mllvm -debug-only=inline,inline-cost,module-inline,cgscc \
  -mllvm -print-after=gvn \
  -mllvm -print-before=gvn \
  -mllvm -print-after=tbaa \
  -mllvm -print-before=tbaa \
  -Rpass=inline \
  -Rpass-missed=inline \
  -Rpass-analysis=inline \
  -Rpass=devirt \
  -Rpass-missed=devirt \
  -Rpass-analysis=devirt \
  -Rpass=gvn \
  -Rpass=tbaa \
  -S -emit-llvm \
  2>&1 | c++filt | tee complete_inline_devirt_info.txt
