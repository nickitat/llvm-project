; ModuleID = 'compute.cpp'
source_filename = "compute.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: read) uwtable
define dso_local noundef nofpclass(nan inf) float @_Z7computePKfPKhi(ptr nofree noundef readonly captures(none) %v, ptr nofree noundef readonly captures(none) %m, i32 noundef %size) local_unnamed_addr #0 {
entry:
  %cmp9 = icmp sgt i32 %size, 0
  tail call void @llvm.assume(i1 %cmp9)
  %wide.trip.count = zext nneg i32 %size to i64
  %min.iters.check = icmp samesign ult i32 %size, 16
  br i1 %min.iters.check, label %for.body.preheader, label %vector.ph

vector.ph:                                        ; preds = %entry
  %n.vec = and i64 %wide.trip.count, 2147483632
  br label %vector.body

vector.body:                                      ; preds = %vector.body, %vector.ph
  %index = phi i64 [ 0, %vector.ph ], [ %index.next, %vector.body ]
  %vec.phi = phi <16 x float> [ zeroinitializer, %vector.ph ], [ %predphi16, %vector.body ]
  %vec.phi15 = phi <16 x i32> [ zeroinitializer, %vector.ph ], [ %predphi, %vector.body ]
  %0 = getelementptr inbounds nuw i8, ptr %m, i64 %index
  %wide.load = load <16 x i8>, ptr %0, align 1, !tbaa !9
  %1 = icmp ne <16 x i8> %wide.load, zeroinitializer
  %2 = getelementptr [4 x i8], ptr %v, i64 %index
  %wide.masked.load = tail call <16 x float> @llvm.masked.load.v16f32.p0(ptr align 4 %2, <16 x i1> %1, <16 x float> zeroinitializer), !tbaa !10
  %3 = zext <16 x i1> %1 to <16 x i32>
  %predphi = add <16 x i32> %vec.phi15, %3
  %predphi16 = fadd reassoc nsz arcp contract afn <16 x float> %vec.phi, %wide.masked.load
  %index.next = add nuw i64 %index, 16
  %4 = icmp eq i64 %index.next, %n.vec
  br i1 %4, label %middle.block, label %vector.body, !llvm.loop !12

middle.block:                                     ; preds = %vector.body
  %5 = tail call fast float @llvm.vector.reduce.fadd.v16f32(float 0.000000e+00, <16 x float> %predphi16)
  %6 = tail call i32 @llvm.vector.reduce.add.v16i32(<16 x i32> %predphi)
  %cmp.n = icmp eq i64 %n.vec, %wide.trip.count
  br i1 %cmp.n, label %for.cond.cleanup.loopexit, label %for.body.preheader

for.body.preheader:                               ; preds = %entry, %middle.block
  %indvars.iv.ph = phi i64 [ 0, %entry ], [ %n.vec, %middle.block ]
  %vsum.012.ph = phi float [ 0.000000e+00, %entry ], [ %5, %middle.block ]
  %vcount.010.ph = phi i32 [ 0, %entry ], [ %6, %middle.block ]
  br label %for.body

for.cond.cleanup.loopexit:                        ; preds = %if.end, %middle.block
  %vcount.1.lcssa = phi i32 [ %6, %middle.block ], [ %vcount.1, %if.end ]
  %vsum.1.lcssa = phi float [ %5, %middle.block ], [ %vsum.1, %if.end ]
  %7 = sitofp fast i32 %vcount.1.lcssa to float
  %8 = fdiv fast float %vsum.1.lcssa, %7
  ret float %8

for.body:                                         ; preds = %for.body.preheader, %if.end
  %indvars.iv = phi i64 [ %indvars.iv.next, %if.end ], [ %indvars.iv.ph, %for.body.preheader ]
  %vsum.012 = phi float [ %vsum.1, %if.end ], [ %vsum.012.ph, %for.body.preheader ]
  %vcount.010 = phi i32 [ %vcount.1, %if.end ], [ %vcount.010.ph, %for.body.preheader ]
  %arrayidx2 = getelementptr inbounds nuw i8, ptr %m, i64 %indvars.iv
  %9 = load i8, ptr %arrayidx2, align 1, !tbaa !9
  %tobool.not = icmp eq i8 %9, 0
  br i1 %tobool.not, label %if.end, label %if.then

if.then:                                          ; preds = %for.body
  %arrayidx = getelementptr inbounds nuw [4 x i8], ptr %v, i64 %indvars.iv
  %10 = load float, ptr %arrayidx, align 4, !tbaa !10
  %add = fadd fast float %10, %vsum.012
  %add3 = add nsw i32 %vcount.010, 1
  br label %if.end

if.end:                                           ; preds = %if.then, %for.body
  %vcount.1 = phi i32 [ %add3, %if.then ], [ %vcount.010, %for.body ]
  %vsum.1 = phi nsz float [ %add, %if.then ], [ %vsum.012, %for.body ]
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond.not = icmp eq i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond.not, label %for.cond.cleanup.loopexit, label %for.body, !llvm.loop !17
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: write)
declare void @llvm.assume(i1 noundef) #1

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(argmem: read)
declare <16 x float> @llvm.masked.load.v16f32.p0(ptr captures(none), <16 x i1>, <16 x float>) #2

; Function Attrs: nocallback nocreateundeforpoison nofree nosync nounwind speculatable willreturn memory(none)
declare float @llvm.vector.reduce.fadd.v16f32(float, <16 x float>) #3

; Function Attrs: nocallback nocreateundeforpoison nofree nosync nounwind speculatable willreturn memory(none)
declare i32 @llvm.vector.reduce.add.v16i32(<16 x i32>) #3

attributes #0 = { mustprogress nofree norecurse nosync nounwind willreturn memory(argmem: read) uwtable "min-legal-vector-width"="0" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "prefer-vector-width"="512" "stack-protector-buffer-size"="8" "target-cpu"="icelake-server" "target-features"="+adx,+aes,+avx,+avx2,+avx512bitalg,+avx512bw,+avx512cd,+avx512dq,+avx512f,+avx512ifma,+avx512vbmi,+avx512vbmi2,+avx512vl,+avx512vnni,+avx512vpopcntdq,+bmi,+bmi2,+clflushopt,+clwb,+cmov,+crc32,+cx16,+cx8,+f16c,+fma,+fsgsbase,+fxsr,+gfni,+invpcid,+lzcnt,+mmx,+movbe,+pclmul,+pconfig,+pku,+popcnt,+prfchw,+rdpid,+rdrnd,+rdseed,+sahf,+sgx,+sha,+sse,+sse2,+sse3,+sse4.1,+sse4.2,+ssse3,+vaes,+vpclmulqdq,+wbnoinvd,+x87,+xsave,+xsavec,+xsaveopt,+xsaves" }
attributes #1 = { nocallback nofree nosync nounwind willreturn memory(inaccessiblemem: write) }
attributes #2 = { nocallback nofree nosync nounwind willreturn memory(argmem: read) }
attributes #3 = { nocallback nocreateundeforpoison nofree nosync nounwind speculatable willreturn memory(none) }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}
!llvm.errno.tbaa = !{!4}

!0 = !{i32 8, !"PIC Level", i32 2}
!1 = !{i32 7, !"PIE Level", i32 2}
!2 = !{i32 7, !"uwtable", i32 2}
!3 = !{!"clang version 24.0.0git (https://github.com/llvm/llvm-project.git 2d18fbf2a869b3be75f3a7ae3ca61ee85ddf496d)"}
!4 = !{!5, !6, i64 0}
!5 = !{!"__libc_errno", !6, i64 0}
!6 = !{!"int", !7, i64 0}
!7 = !{!"omnipotent char", !8, i64 0}
!8 = !{!"Simple C++ TBAA"}
!9 = !{!7, !7, i64 0}
!10 = !{!11, !11, i64 0}
!11 = !{!"float", !7, i64 0}
!12 = distinct !{!12, !13, !14, !15, !16}
!13 = !{!"llvm.loop.mustprogress"}
!14 = !{!"llvm.loop.unroll.disable"}
!15 = !{!"llvm.loop.isvectorized", i32 1}
!16 = !{!"llvm.loop.unroll.runtime.disable"}
!17 = distinct !{!17, !13, !14, !15}
