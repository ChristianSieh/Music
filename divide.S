	.text
	.align	2
@@@ -----------------------------------------------------------------
	@@ udiv32 takes a 32-bit unsigned dividend in r0 and
	@@ divides it by a 32-bit unsigned divisor in r1.
	@@ Returns the quotient in r0 and remainder in r1
	@@ It calls no other functions and only 
        @@ uses r0-r3. We don't need to use the stack
	.global	udiv32
udiv32:	stmfd	sp!, {lr}
	cmp	r1,#0		@ if divisor == zero
	beq	quitudiv32	@   exit immediately
	mov     r2,r1           @ move divisor to r2
        mov     r1,r0           @ move dividend to r1
        mov     r0,#0           @ clear r0 to accumulate result
        mov     r3,#1           @ set "current" bit in r3
divstrt:cmp	r2,#0           @ WHILE ((msb of r2 != 1)
	blt	divloop
	cmp     r2,r1		@ && (r2 < r1))
        lslls   r2,r2,#1        @  shift dividend left
        lslls   r3,r3,#1        @  shift "current" bit left
        bls     divstrt		@ end WHILE
divloop:cmp     r1,r2           @ if dividend >= divisor
        subge   r1,r1,r2        @   subtract divisor from dividend
        addge   r0,r0,r3        @   set "current" bit in the result
        lsr	r2,r2,#1	@ shift dividend right
        lsrs    r3,r3,#1	@ Shift current bit right into carry
        bcc     divloop         @ If carry not clear, R3 has shifted
                                @ one bit past where it started, and we
                                @ are done.
quitudiv32:	
	mov	r0,r1
	ldmfd	sp!, {lr}
        mov     pc,lr


.end
