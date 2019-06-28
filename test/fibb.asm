
    .orig x3000
    ld r6, STACK
    and r0, r0, #0
    add r0, r0, #4
    add r6, r6, #-1
    str r0, r6, #0

    jsr fibonacci

    ldr r0, r6, #0
    add r6, r6, #2
    lea r0, MSG
    trap x22
    and r0, r0, #0
    add r0, r0, #10
    trap x21
    halt

fibonacci

    add r6, r6, #-1

    add r6, r6, #-1
    str r7, r6, #0

    add r6, r6, #-1
    str r5, r6, #0

    add r6, r6, #-1
    add r5, r6, #0

    add r6, r5, #-5

    str r0, r5, #-1
    str r1, r5, #-2
    str r2, r5, #-3
    str r3, r5, #-4
    str r4, r5, #-5

    ldr r0, r5, #4
    add r1, r0, #-1
    brp eif
    str r0, r5, #3

    ldr r0, r5, #-1
    ldr r1, r5, #-2
    ldr r2, r5, #-3
    ldr r3, r5, #-4
    ldr r4, r5, #-5
    ldr r7, r5, #2

    add r6, r5, #3
    ldr r5, r5, #1
    ret
eif
    add r6, r6, #-1
    str r1, r6, #0

    jsr fibonacci

    ldr r2, r6, #0
    add r6, r6, #2

    str r2, r5, #0

    ldr r0, r5, #4
    add r0, r0, #-2
    add r6, r6, #-1
    str r0, r6, #0

    jsr fibonacci

    ldr r2, r6, #0
    add r6, r6, #2

    ldr r3, r5, #0
    add r2, r2, r3
    str r2, r5, #3
    ldr r0, r5, #-1
    ldr r1, r5, #-2
    ldr r2, r5, #-3
    ldr r3, r5, #-4
    ldr r4, r5, #-5
    ldr r7, r5, #2
    add r6, r5, #3
    ldr r5, r5, #1
    ld r0, MSG
    ret
MSG .stringz "this is a string"
STACK .fill xF000
    .end
