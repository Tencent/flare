/*
    A slightly modified version of boost.context 1.69.

    // DO NOT RETURN FROM `start_proc`, THIS LEADS TO CRASH (IN AN UNFRIENDLY
    // WAY.).
    //
    // Returns: Jump target (same as `sp`) for **first** call to `jump_context`.
    //
    // R3: sp
    // R4: size (not used)
    // R5: start_proc (jump target is passed as parameter)
    void* make_context(void* sp, std::size_t size, void (*start_proc)(void*));
*/

/*
            Copyright Oliver Kowalke 2009.
   Distributed under the Boost Software License, Version 1.0.
      (See accompanying file LICENSE_1_0.txt or copy at
          http://www.boost.org/LICENSE_1_0.txt)
*/

/* @sa: `jump_context.S` for stack usage. */

.file "make_context.S"
.globl make_context
	.text
	.align 2
make_context:
	addis	%r2, %r12, .TOC.-make_context@ha
	addi	%r2, %r2, .TOC.-make_context@l
	.localentry make_context, . - make_context

    # first arg of make_context() == top address of context-stack
    # shift address in R3 to lower 16 byte boundary
    clrrdi  %r3, %r3, 4

    # reserve space for context-data on context-stack
    # including 64 byte of linkage + parameter area (R1 % 16 == 0)
    subi  %r3, %r3, 640

    # third arg of make_context() == address of context-function
    # entry point (ELFv2)
    # save address of context-function entry point
    std  %r5, 176(%r3)

    # set back-chain to zero
    li   %r0, 0
    std  %r0, 184(%r3)

    # save address 0 as return-address for context-function
    # will be entered after context-function returns
    std  0, 168(%r3)

    blr  # return pointer to context-data
	.size make_context, .-make_context

/* Mark that we don't need executable stack.  */
.section .note.GNU-stack,"",%progbits
