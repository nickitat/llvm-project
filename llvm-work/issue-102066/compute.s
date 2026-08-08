	.att_syntax
	.file	"compute.cpp"
	.text
	.globl	_Z7computePKfPKhi               # -- Begin function _Z7computePKfPKhi
	.prefalign	4, .Lfunc_end0, nop
	.type	_Z7computePKfPKhi,@function
_Z7computePKfPKhi:                      # @_Z7computePKfPKhi
	.cfi_startproc
# %bb.0:                                # %entry
	movl	%edx, %eax
	cmpl	$16, %edx
	jae	.LBB0_2
# %bb.1:
	xorl	%ecx, %ecx
	vxorps	%xmm0, %xmm0, %xmm0
	xorl	%edx, %edx
	jmp	.LBB0_5
.LBB0_2:                                # %vector.ph
	movl	%eax, %ecx
	andl	$2147483632, %ecx               # imm = 0x7FFFFFF0
	vpxor	%xmm1, %xmm1, %xmm1
	vxorps	%xmm0, %xmm0, %xmm0
	xorl	%edx, %edx
	.p2align	4
.LBB0_3:                                # %vector.body
                                        # =>This Inner Loop Header: Depth=1
	vmovdqu	(%rsi,%rdx), %xmm2
	vptestmb	%xmm2, %xmm2, %k1
	vmovups	(%rdi,%rdx,4), %zmm2 {%k1} {z}
	vaddps	%zmm2, %zmm0, %zmm0
	vpmovm2d	%k1, %zmm2
	vpsubd	%zmm2, %zmm1, %zmm1
	addq	$16, %rdx
	cmpq	%rdx, %rcx
	jne	.LBB0_3
# %bb.4:                                # %middle.block
	vextractf64x4	$1, %zmm0, %ymm2
	vaddps	%zmm2, %zmm0, %zmm0
	vextractf128	$1, %ymm0, %xmm2
	vaddps	%xmm2, %xmm0, %xmm0
	vshufpd	$1, %xmm0, %xmm0, %xmm2         # xmm2 = xmm0[1,0]
	vaddps	%xmm2, %xmm0, %xmm0
	vmovshdup	%xmm0, %xmm2            # xmm2 = xmm0[1,1,3,3]
	vaddss	%xmm2, %xmm0, %xmm0
	vextracti64x4	$1, %zmm1, %ymm2
	vpaddd	%zmm2, %zmm1, %zmm1
	vextracti128	$1, %ymm1, %xmm2
	vpaddd	%xmm2, %xmm1, %xmm1
	vpshufd	$238, %xmm1, %xmm2              # xmm2 = xmm1[2,3,2,3]
	vpaddd	%xmm2, %xmm1, %xmm1
	vpshufd	$85, %xmm1, %xmm2               # xmm2 = xmm1[1,1,1,1]
	vpaddd	%xmm2, %xmm1, %xmm1
	vmovd	%xmm1, %edx
	cmpl	%eax, %ecx
	jne	.LBB0_5
.LBB0_8:                                # %for.cond.cleanup.loopexit
	vcvtsi2ss	%edx, %xmm15, %xmm1
	vdivss	%xmm1, %xmm0, %xmm0
	vzeroupper
	retq
	.p2align	4
.LBB0_7:                                # %if.end
                                        #   in Loop: Header=BB0_5 Depth=1
	incq	%rcx
	cmpq	%rcx, %rax
	je	.LBB0_8
.LBB0_5:                                # %for.body
                                        # =>This Inner Loop Header: Depth=1
	cmpb	$0, (%rsi,%rcx)
	je	.LBB0_7
# %bb.6:                                # %if.then
                                        #   in Loop: Header=BB0_5 Depth=1
	vaddss	(%rdi,%rcx,4), %xmm0, %xmm0
	incl	%edx
	jmp	.LBB0_7
.Lfunc_end0:
	.size	_Z7computePKfPKhi, .Lfunc_end0-_Z7computePKfPKhi
	.cfi_endproc
                                        # -- End function
	.ident	"clang version 24.0.0git (https://github.com/llvm/llvm-project.git 2d18fbf2a869b3be75f3a7ae3ca61ee85ddf496d)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
