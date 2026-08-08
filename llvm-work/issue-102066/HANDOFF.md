# Session handoff — LLVM issue #102066 (AVX-512 masked-accumulator codegen)

> Purpose: restore full working context on a remote/cloud dev box. Read this top to
> bottom, then continue from "Open TODOs". Nothing here needs re-deriving.
> Branch: `masked-avx`. Do NOT confuse this with the LEA-reuse work (#51707).

## TL;DR of current state

- Issue: https://github.com/llvm/llvm-project/issues/102066 — "Sub-optimal bool/masked
  vector-based operations (mainly on AVX-512)".
- A masked counter (`count += 1` under `if(mask)`) emits `vpmovm2d` (mask→0/-1 vector)
  + unmasked `vpsubd` **every loop iteration**, instead of one masked `vpsubd {k1}`
  with a loop-invariant addend.
- Root cause: an X86-specific fold in `combineAdd` rewrites
  `add(Y, zext(vXi1 M)) -> sub(Y, sext(vXi1 M))`, which commits to the "materialize
  mask as vector" path and throws away the chance to keep the mask in a `k` register.
- Fix (prototype, WORKING): a second fold in `combineSub` that re-predicates the
  masked increment **only for loop-carried accumulators**:
  `sub(Y, sext(vXi1 M)) -> vselect(M, add(Y,1), Y)`, gated on the result being
  copied out to a **virtual** register (loop-carried / live-out), not a physical
  (ABI/return) register.
- Result: fixes the target loop. Across 5545 X86 CodeGen tests: **1 diff (`pr53842`,
  neutral), 0 regressions.**

## Where the change lives

- The +38-line `combineSub` fold is **already committed** on `masked-avx` in commit
  `b9e5fe057` (message: "stash"). Working tree is otherwise clean.
  File: `llvm/lib/Target/X86/X86ISelLowering.cpp`, in `combineSub`, right after the
  `IsNonOpaqueConstant` lambda (~line 60037). Verbatim:

```cpp
  // Predicate a masked increment so it stays in the mask (k) register:
  //   sub(Y, sext(vXi1 M))  ->  vselect(M, add(Y, 1), Y)
  // ...only for loop-carried accumulators (result copied to a virtual register).
  if (VT.isVector()) {
    EVT BoolVT = EVT::getVectorVT(*DAG.getContext(), MVT::i1,
                                  VT.getVectorElementCount());
    if (DAG.getTargetLoweringInfo().isTypeLegal(BoolVT) &&
        Op1.getOpcode() == ISD::SIGN_EXTEND && Op1.hasOneUse() &&
        Op1.getOperand(0).getValueType() == BoolVT) {
      bool LiveOutToVReg = false;
      for (SDNode *U : N->users()) {
        if (U->getOpcode() != ISD::CopyToReg)
          continue;
        if (auto *R = dyn_cast<RegisterSDNode>(U->getOperand(1)))
          if (R->getReg().isVirtual()) {
            LiveOutToVReg = true;
            break;
          }
      }
      if (LiveOutToVReg) {
        SDValue Inc =
            DAG.getNode(ISD::ADD, DL, VT, Op0, DAG.getConstant(1, DL, VT));
        return DAG.getNode(ISD::VSELECT, DL, VT, Op1.getOperand(0), Inc, Op0);
      }
    }
  }
```

- `combineAdd` (the original fold at ~X86ISelLowering.cpp:59808) is **pristine /
  unmodified** — Direction 1 was fully reverted. Do not re-touch it.

## Why this is the right shape (do not re-litigate)

- The masked form is a win **only** when (a) `Y` is a loop-carried accumulator (so
  the `+1`/`-1` addend hoists out via MachineLICM and the masked op's two-address
  passthru is free) and (b) the mask is loop-variant (recomputed each iteration).
- Straight-line / horizontal-reduction / function-argument cases look similar in a
  per-block DAG but get **worse** if predicated (extra blend/copy at ABI boundary).
- Distinguishing signal that cleanly separates the two: loop accumulators copy their
  result to a **virtual** register; ABI boundaries (return/args) use **physical**
  registers. Gating on `R->getReg().isVirtual()` is what took Direction 1's 1
  regression down to 0.
- This is still a DAG-level **heuristic**, not a profitability proof. The fully-general
  non-regressing version would be a pre-RA loop peephole — much more machinery for the
  same practical win. There's a `FIXME` near the `combineAdd` fold noting prior
  attempts in this area caused regressions.

## Reproducers (untracked, in this dir `llvm-work/issue-102066/`)

- `compute.cpp` — plain C++ (no OpenMP), uses `unsigned char` (avoids missing
  `<cstdint>` in cross-compile). The masked-sum loop from the issue.
- `compute.ll` / `compute.s` — clang `-O3 -march=icelake-server` optimized IR / asm.
  Reduction is `%z = zext <16 x i1> %mask to <16 x i32>` then `add <16 x i32> %acc,%z`.
- `masked_count.ll` — 4-line minimal repro (function-arg case → result to physical
  `$zmm0`; correctly stays `vpmovm2d`+`vpsubd`, proving the gate works).
- `ideal.ll`, `vsel.ll` — prove `select(mask,1,0)+add` and `vselect(mask,acc+1,acc)`
  both lower to a single masked `vpsubd {k1}`.

Note: these repro files are **untracked**. If you want them on the remote box, either
`git add -f llvm-work/issue-102066/` before pushing, or copy them over separately.

## How to rebuild & verify on the box

```bash
# configure (if not already) — Release + assertions, X86 only is enough
cmake -G Ninja -S llvm -B build \
  -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_ENABLE_PROJECTS=clang
ninja -C build llc

# reproduce the fix on the minimal case (loop version -> masked vpsubd {k1})
build/bin/llc -mtriple=x86_64-- -mattr=+avx512f,+avx512dq,+avx512bw \
  llvm-work/issue-102066/compute.ll -o -

# the function-arg case must STAY vpmovm2d+vpsubd (gate working)
build/bin/llc -mtriple=x86_64-- -mattr=+avx512f,+avx512bw \
  llvm-work/issue-102066/masked_count.ll -o -

# full regression sweep (expect exactly 1 diff: pr53842, neutral)
ninja -C build check-llvm-codegen-x86
```

Gotchas learned the hard way:
- `llc` defaults to the **host** triple — on this Apple M1 laptop it emitted arm64
  (`cinc`, `x8`). Always pass `-mtriple=x86_64--`. (Non-issue on an x86 box, but keep
  the flag.)
- After reverting/editing source, **`ninja llc` before inspecting** — a stale binary
  produced a very confusing DAG dump.
- Debug traces used: `-debug-only=dagcombine` and `-debug-only=isel` (needs an
  assertions build). The fold shows as `add t2, zext(t8)` -> `sub t2, sext(t8)`.

## The one test diff (pr53842)

`llvm/test/CodeGen/X86/pr53842.ll`: AVX512F unchanged; AVX512DQ goes `vpsubq` ->
`vpsubq {k1}` — both one loop instruction, equal cost (its mask happens to be
loop-invariant, so it's neutral, not a regression). The golden file was reverted, so
this run currently reports it as a diff.

## Open TODOs (pick up here)

Previously offered, no selection made yet:
- (a) **Broaden liveout detection** to chase through one hop — catch
  `sub -> op -> CopyToReg` chains where the accumulator isn't copied out directly.
- (b) **Land the tests**: write a dedicated `llvm/test/CodeGen/X86/pr102066.ll`
  regression test, and update the `pr53842.ll` CHECK lines for the neutral diff.
- (c) Stop / write up the RFC.

Recommended next step if resuming cold: do (b) first (locks in the win with a test),
then decide on (a) vs sending an RFC/PR upstream referencing the `FIXME` and the
per-block-heuristic caveat.

## Repo facts

- Branch `masked-avx`, based on upstream around commit `2f5771c8e`.
- The `combineSub` change is committed as `b9e5fe057` ("stash") — rename this commit
  to something real before any PR.
- Reminder from user memory: PR-review fixes go in NEW commits — never amend/force-push
  reviewed commits.
