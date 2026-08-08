define <16 x i32> @masked_count(<16 x i32> %acc, <16 x i8> %m) {
  %c = icmp ne <16 x i8> %m, zeroinitializer
  %sel = select <16 x i1> %c, <16 x i32> splat (i32 1), <16 x i32> zeroinitializer
  %add = add <16 x i32> %acc, %sel
  ret <16 x i32> %add
}
