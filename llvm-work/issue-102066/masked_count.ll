; Minimal repro of llvm/llvm-project#102066 — pure X86, no OpenMP/clang.
; Masked "count += 1": zext<i1->i32> + add should lower to a masked vpaddd,
; but the backend emits vpmovm2d (mask->0/-1) + vpsubd.
define <16 x i32> @masked_count(<16 x i32> %acc, <16 x i8> %m) {
  %c = icmp ne <16 x i8> %m, zeroinitializer
  %z = zext <16 x i1> %c to <16 x i32>
  %add = add <16 x i32> %acc, %z
  ret <16 x i32> %add
}
