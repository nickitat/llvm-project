define <16 x i32> @masked_count(<16 x i32> %acc, <16 x i8> %m) {
  %c    = icmp ne <16 x i8> %m, zeroinitializer
  %inc  = add <16 x i32> %acc, splat (i32 1)
  %r    = select <16 x i1> %c, <16 x i32> %inc, <16 x i32> %acc
  ret <16 x i32> %r
}
