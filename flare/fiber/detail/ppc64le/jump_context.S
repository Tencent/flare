/*
    A slightly modified version of boost.context 1.69.

    Only ppc64le is supported. Unfortunately at the time I don't have access to
    any ppc64be machine.

    // `self` is updated prior to `to` starts executing.
    //
    // R3: self
    // R4: to
    // R5: context
    void jump_context(void** self, void* to, void* context);
*/

/*
            Copyright Oliver Kowalke 2009.
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENSE_1_0.txt or copy at
          http://www.boost.org/LICENSE_1_0.txt)
*/

/*******************************************************
 *                                                     *
 *  -------------------------------------------------  *
 *  |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  *
 *  -------------------------------------------------  *
 *  |  0  |  4  |  8  |  12 |  16 |  20 |  24 |  28 |  *
 *  -------------------------------------------------  *
 *  | Not Used  |    R14    |    R15    |    R16    |  *
 *  -------------------------------------------------  *
 *  -------------------------------------------------  *
 *  |  8  |  9  |  10 |  11 |  12 |  13 |  14 |  15 |  *
 *  -------------------------------------------------  *
 *  |  32 |  36 |  40 |  44 |  48 |  52 |  56 |  60 |  *
 *  -------------------------------------------------  *
 *  |    R17    |    R18    |     R19   |    R20    |  *
 *  -------------------------------------------------  *
 *  -------------------------------------------------  *
 *  |  16 |  17 |  18 |  19 |  20 |  21 |  22 |  23 |  *
 *  -------------------------------------------------  *
 *  |  64 |  68 |  72 |  76 |  80 |  84 |  88 |  92 |  *
 *  -------------------------------------------------  *
 *  |    R21    |    R22    |    R23    |    R24    |  *
 *  -------------------------------------------------  *
 *  -------------------------------------------------  *
 *  |  24 |  25 |  26 |  27 |  28 |  29 |  30 |  31 |  *
 *  -------------------------------------------------  *
 *  |  96 | 100 | 104 | 108 | 112 | 116 | 120 | 124 |  *
 *  -------------------------------------------------  *
 *  |    R25    |    R26    |    R27    |    R28    |  *
 *  -------------------------------------------------  *
 *  -------------------------------------------------  *
 *  |  32 |  33 |  34 |  35 |  36 |  37 |  38 |  39 |  *
 *  -------------------------------------------------  *
 *  | 128 | 132 | 136 | 140 | 144 | 148 | 152 | 156 |  *
 *  -------------------------------------------------  *
 *  |    R29    |    R30    |    R31    | Not used  |  *
 *  -------------------------------------------------  *
 *  -------------------------------------------------  *
 *  |  40 |  41 |  42 |  43 |  44 |  45 |  46 |  47 |  *
 *  -------------------------------------------------  *
 *  | 160 | 164 | 168 | 172 | 176 | 180 | 184 | 188 |  *
 *  -------------------------------------------------  *
 *  |     CR    |     LR    |     PC    | back-chain|  *
 *  -------------------------------------------------  *
 *                                                     *
 *******************************************************/

/*
    According to Power 64-bit ELF ABI V2, f14 ~ f31 are non-volatile.

    +---------------------------------------------------+
    |  200: f14  |  208: f15  |  216: f16  |  224: f17  |
    |  232: f18  |  240: f19  |  248: f20  |  256: f21  |
    |  264: f22  |  272: f23  |  280: f24  |  288: f25  |
    |  296: f26  |  304: f27  |  312: f28  |  320: f29  |
    |  328: f30  |  336: f31  |  344:      |            |
    +---------------------------------------------------+

    v20 ~ v31 need to be preserved as well.

    +---------------------------------------------------+
    |  400: v20  |  416: v21  |  432: v22  |  448: v23  |
    |  464: v24  |  480: v25  |  496: v26  |  512: v27  |
    |  528: v28  |  544: v29  |  560: v30  |  576: v31  |
    +---------------------------------------------------+
*/

/*
    ... and 64-byte region for linkage + parameter ...

    For a total of 640 bytes.
*/

.file "jump_context.S"
.globl jump_context
	.text
	.align 2
jump_context:
        addis   %r2, %r12, .TOC.-jump_context@ha
        addi    %r2, %r2, .TOC.-jump_context@l
        .localentry jump_context, . - jump_context
    # reserve space on stack
    subi  %r1, %r1, 576  # 640 - 64 bytes linkage + parameter area.

    # FIXME: What about TOC pointer? boost.context seems to explicitly not
    # saving it when using v2 ABI.

    std  %r14, 8(%r1)  # save R14
    std  %r15, 16(%r1)  # save R15
    std  %r16, 24(%r1)  # save R16
    std  %r17, 32(%r1)  # save R17
    std  %r18, 40(%r1)  # save R18
    std  %r19, 48(%r1)  # save R19
    std  %r20, 56(%r1)  # save R20
    std  %r21, 64(%r1)  # save R21
    std  %r22, 72(%r1)  # save R22
    std  %r23, 80(%r1)  # save R23
    std  %r24, 88(%r1)  # save R24
    std  %r25, 96(%r1)  # save R25
    std  %r26, 104(%r1)  # save R26
    std  %r27, 112(%r1)  # save R27
    std  %r28, 120(%r1)  # save R28
    std  %r29, 128(%r1)  # save R29
    std  %r30, 136(%r1)  # save R30
    std  %r31, 144(%r1)  # save R31

    # save CR
    mfcr  %r0   # But see also: https://stackoverflow.com/a/41132999
    std   %r0, 160(%r1)
    # save LR
    mflr  %r0
    std   %r0, 168(%r1)
    # save LR as PC
    std   %r0, 176(%r1)

    # According to spec. some of vector registers / floating point registers are
    # non-volatile. Unfortunately boost.context does not save them here, so
    # let's roll our own.
    #
    # @sa: glibc/sysdeps/unix/sysv/linux/powerpc/powerpc64/swapcontext.S
    stfd  %f14, 200(%r1)
    stfd  %f15, 208(%r1)
    stfd  %f16, 216(%r1)
    stfd  %f17, 224(%r1)
    stfd  %f18, 232(%r1)
    stfd  %f19, 240(%r1)
    stfd  %f20, 248(%r1)
    stfd  %f21, 256(%r1)
    stfd  %f22, 264(%r1)
    stfd  %f23, 272(%r1)
    stfd  %f24, 280(%r1)
    stfd  %f25, 288(%r1)
    stfd  %f26, 296(%r1)
    stfd  %f27, 304(%r1)
    stfd  %f28, 312(%r1)
    stfd  %f29, 320(%r1)
    stfd  %f30, 328(%r1)
    stfd  %f31, 336(%r1)

    # Vector registers.
    la    %r7, 400(%r1)
    la    %r8, 416(%r1)
    la    %r9, 432(%r1)
    la    %r10, 448(%r1)

    stvx  %v20, 0, %r7
    stvx  %v21, 0, %r8
    stvx  %v22, 0, %r9
    stvx  %v23, 0, %r10
    addi  %r7, %r7, 64
    addi  %r8, %r8, 64
    addi  %r9, %r9, 64
    addi  %r10, %r10, 64
    stvx  %v24, 0, %r7
    stvx  %v25, 0, %r8
    stvx  %v26, 0, %r9
    stvx  %v27, 0, %r10
    addi  %r7, %r7, 64
    addi  %r8, %r8, 64
    addi  %r9, %r9, 64
    addi  %r10, %r10, 64
    stvx  %v28, 0, %r7
    stvx  %v29, 0, %r8
    stvx  %v30, 0, %r9
    stvx  %v31, 0, %r10
    addi  %r7, %r7, 64
    addi  %r8, %r8, 64
    addi  %r9, %r9, 64
    addi  %r10, %r10, 64

    # store RSP (pointing to context-data) in variable pointed to by R4
    std  %r1, 0(%r3)

    # restore RSP (pointing to context-data) from R4
    mr  %r1, %r4

    # Vector registers.
    la    %r7, 400(%r1)
    la    %r8, 416(%r1)
    la    %r9, 432(%r1)
    la    %r10, 448(%r1)

    lvx   %v20, 0, %r7
    lvx   %v21, 0, %r8
    lvx   %v22, 0, %r9
    lvx   %v23, 0, %r10
    addi  %r7, %r7, 64
    addi  %r8, %r8, 64
    addi  %r9, %r9, 64
    addi  %r10, %r10, 64
    lvx   %v24, 0, %r7
    lvx   %v25, 0, %r8
    lvx   %v26, 0, %r9
    lvx   %v27, 0, %r10
    addi  %r7, %r7, 64
    addi  %r8, %r8, 64
    addi  %r9, %r9, 64
    addi  %r10, %r10, 64
    lvx   %v28, 0, %r7
    lvx   %v29, 0, %r8
    lvx   %v30, 0, %r9
    lvx   %v31, 0, %r10
    addi  %r7, %r7, 64
    addi  %r8, %r8, 64
    addi  %r9, %r9, 64
    addi  %r10, %r10, 64

    # Floating-point registers.
    lfd  %f14, 200(%r1)
    lfd  %f15, 208(%r1)
    lfd  %f16, 216(%r1)
    lfd  %f17, 224(%r1)
    lfd  %f18, 232(%r1)
    lfd  %f19, 240(%r1)
    lfd  %f20, 248(%r1)
    lfd  %f21, 256(%r1)
    lfd  %f22, 264(%r1)
    lfd  %f23, 272(%r1)
    lfd  %f24, 280(%r1)
    lfd  %f25, 288(%r1)
    lfd  %f26, 296(%r1)
    lfd  %f27, 304(%r1)
    lfd  %f28, 312(%r1)
    lfd  %f29, 320(%r1)
    lfd  %f30, 328(%r1)
    lfd  %f31, 336(%r1)

    ld  %r14, 8(%r1)  # restore R14
    ld  %r15, 16(%r1)  # restore R15
    ld  %r16, 24(%r1)  # restore R16
    ld  %r17, 32(%r1)  # restore R17
    ld  %r18, 40(%r1)  # restore R18
    ld  %r19, 48(%r1)  # restore R19
    ld  %r20, 56(%r1)  # restore R20
    ld  %r21, 64(%r1)  # restore R21
    ld  %r22, 72(%r1)  # restore R22
    ld  %r23, 80(%r1)  # restore R23
    ld  %r24, 88(%r1)  # restore R24
    ld  %r25, 96(%r1)  # restore R25
    ld  %r26, 104(%r1)  # restore R26
    ld  %r27, 112(%r1)  # restore R27
    ld  %r28, 120(%r1)  # restore R28
    ld  %r29, 128(%r1)  # restore R29
    ld  %r30, 136(%r1)  # restore R30
    ld  %r31, 144(%r1)  # restore R31

    # restore CR
    ld  %r0, 160(%r1)
    mtcr  %r0
    # restore LR
    ld  %r0, 168(%r1)
    mtlr  %r0

    # load PC
    ld  %r12, 176(%r1)
    # restore CTR
    mtctr  %r12

    # adjust stack
    addi  %r1, %r1, 576  # 640 - 64 bytes linkage + parameter area.

    # Pass `context` as first argument to `start_proc` on first entry.
    mr  %r3, %r5
    # arg pointer already in %r4

    # jump to context
    bctr
	.size jump_context, .-jump_context


/* Mark that we don't need executable stack.  */
.section .note.GNU-stack,"",%progbits
