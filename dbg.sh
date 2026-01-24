#!/usr/bin/sh

# inline,instcombine,gvn,simplifycfg,inline
# -mllvm -stats \
./build_dbg/bin/clang++ -O2 inline.cc -o inline.out \
  -mllvm -debug-only=inline,inline-cost,module-inline,cgscc \
  -mllvm -print-after=inline \
  -mllvm -print-before=inline \
  -Rpass=inline \
  -Rpass-missed=inline \
  -Rpass-analysis=inline \
  -S -emit-llvm \
  2>&1 | c++filt | tee complete_inline_info.txt
