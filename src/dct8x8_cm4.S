.syntax unified
.thumb

// Horizontal 1-d DCT of width 8
// r0  - source buffer (8 bytes)
// r1  - destination buffer (8 int32_t)
// r8  - #212
// r9  - #181
// r10 - #100
// r11 - #177
// r12 - #106
// as a result, even rows are not scaled (except the initial scaling from paper)
// but odd ones are scaled by 128
.macro DCT8_HORIZONTAL_CM4 offset
    // r2 - inputs 0-3
    ldr    r2, [r0, #8*\offset]
    // r3 - inputs 4-7
    ldr    r3, [r0, #8*\offset+4]
    // r0 - inputs 0 and 2
    uxtb   r0, r2
    // r2 - inputs 3 and 1
    uxtb   r2, r2, ror #8
    // r4 - inputs 4 and 6
    uxtb   r4, r3
    // r3 - inputs 7 and 5
    uxtb   r3, r3, ror #8

    // Stage 1 - butterfly add/sub
    // r5: 0 + 7 and 2 + 5
    sadd16 r5, r0, r3
    // r0: 0 - 7 and 2 - 5
    ssub16 r0, r0, r3
    // r3: 3 + 4 and 1 + 6
    sadd16 r3, r2, r4
    // r4: 3 - 4 and 1 - 6
    ssub16 r4, r2, r4

    // Stage 2 - even part
    // r2: 0 + 3 and 2 + 1
    sadd16 r2, r5, r3
    // r3: 0 - 3 and 2 - 1
    ssub16 r3, r5, r3

    // Stage 3 - even part
    sasx   r2, r2, r2
    // result[0]
    sxth   r5, r2, ror #16
    str    r5, [r1, #8*\offset]
    //  result[4]
    sxth   r5, r2
    str    r5, [r1, #32*\offset+16]

    // Stage 3 - even-odd part (planar rotation sqrt2*c1)
    // tmp (r7) = r3.l
    sxth   r7, r3
    // tmp (r5) = (r3.l + r3.h)*sqrt(2)*cos(pi/16)*128
    sxtah  r5, r7, r3, ror #16
    mul    r5, r5, r11
    // result[6] = r5 - (r7)*sqrt(2)*(cos(pi/16)+sin(pi/16))*128
    mls    r2, r7, r8, r5
    str    r2, [r1, #32*\offset+24]
    // tmp (r2) = r3.h*(-1*sqrt(2)*(sin(pi/16) - cos(pi/16)))*128
    sxth   r2, r3, ror #16
    mov    r6, #142
    mls    r2, r2, r6, r5
    // result[2] = (r5 - r2)/128
    str    r2, [r1, #32*\offset+8]

    // Stage 2 - odd part 1
    // Rotate (3 - 4) (r4.l) and (0 - 7) (r0.l)
    // tmp (r2) = r0.l
    sxth   r2, r0
    // tmp (r5) = (r0.l + r4.l)*cos(3*pi/16)*128
    sxtah  r5, r2, r4
    mul    r5, r5, r12
    // stage3[7](r7) = r5 - (r4.l)*(sin(3*pi/16) + cos(3*pi/16))
    sxth   r3, r4
    mls    r7, r3, r11, r5
    // stage3[4](r6) = r5 - (r2)*(-1)*(sin(3*pi/16) - cos(3*pi/16))
    mov    r6, #35
    mls    r6, r2, r6, r5

    // stage 2 - odd part 2
    // Rotate (1 - 6) (r4.h) (2 - 5) (r0.h)
    // tmp (r2) = r0.h
    sxth   r2, r0, ror #16
    // tmp (r5) = (r2 + r4.h) * cos(pi/16)*128
    sxtah  r5, r2, r4, ror #16
    mov    r3, #125
    mul    r5, r5, r3
    // stage3[6](r0) = r5 - (r4.l)*(sin(pi/16) + cos(pi/16))
    sxth   r3, r4
    mov    r0, #150
    mls    r0, r3, r0, r5
    // stage3[5](r4) = r5 - (r2)*(-1)*(sin(pi/16) - cos(pi/16))
    mls    r4, r2, r10, r5

    // stage 3,4 - odd part
    // result[3] = (sqrt(2) * (stage3[7]-stage3[5])*128)/128
    sub    r2, r7, r4
    mul    r2, r2, r9
    asr    r2, r2, #7
    str    r2, [r1, #32*\offset+12]
    // result[5] = (sqrt(2) * (stage3[4]-stage3[6])*128)/128
    sub    r2, r6, r0
    mul    r2, r2, r3
    asr    r2, r2, #7
    str    r2, [r1, #32*\offset+20]
    // result[7] = (stage3[7]+stage3[5]-stage3[4]-stage3[6])
    // tmp(r2) = stage3[7]+stage3[5]
    // tmp(r3) = stage3[4]+stage3[6]
    add    r2, r7, r4
    add    r3, r6, r0
    sub    r4, r2, r3
    str    r2, [r1, #32*\offset+28]
    // result[1] = (stage3[7]+stage3[5]+stage3[4]+stage3[6])
    add    r2, r2, r3
    str    r2, [r1, #32*\offset+4]
.endm

// r0 - source/dest 8x8 32-bit integer buffer (no additional scaling)
// r8  - #212
// r9  - #181
// r10 - #100
// r11 - #177
// r12 - #106
.macro DCT8_VERTICAL_EVEN_CM4 offset
    // Stage 1 - 2 - 3 even

    // r4 = stage2[0]
    // r5 = stage2[1]
    // r6 = stage2[2]
    // r7 = stage2[3]
    ldr r2, [r0]
    ldr r3, [r0, #\offset+224]
    add r4, r2, r3

    ldr r2, [r0, #\offset+4]
    ldr r3, [r0, #\offset+192]
    add r5, r2, r3

    ldr r2, [r0, #\offset+8]
    ldr r3, [r0, #\offset+160]
    add r6, r2, r3

    ldr r2, [r0, #\offset+12]
    ldr r3, [r0, #\offset+128]
    add r7, r2, r3

    // r2 = stage3[0]
    // r3 = stage3[1]
    // r5 = stage3[2]
    // r6 = stage3[3]
    // r7 = in[4]
    add r2, r4, r7
    add r3, r5, r5
    add r2, r2, r3
    sub r3, r2, r3
    // result[0]
    str r2, [r1, #\offset]
    // result[4]
    str r3, [r1, #\offset+8]

    // stage3[2]
    sub r4, r4, r7
    // stage3[1]
    sub r5, r5, r6

    // Stage 3 - even-odd part (planar rotation sqrt2*c1)
    // tmp (r2) = (r4 + r5)*sqrt(2)*cos(pi/16)*128
    add    r2, r4, r5
    mul    r2, r2, r11
    // result[6] = r2 - (r4)*sqrt(2)*(cos(pi/16)+sin(pi/16))*128
    mls    r3, r4, r8, r2
    str    r3, [r1, #\offset+192]
    // tmp (r3) = r5*(-1*sqrt(2)*(sin(pi/16) - cos(pi/16)))*128
    mov    r3, #142
    mls    r3, r5, r3, r2
    // result[2] = (r2 - r3)/128
    str    r3, [r1, #\offset+8]

    // r2 = stage2[7]
    ldr r2, [r0, #\offset]
    ldr r3, [r0, #\offset+224]
    sub r2, r2, r3

    // r3 = stage2[4]
    ldr r3, [r0, #\offset+12]
    ldr r4, [r0, #\offset+128]
    sub r3, r2, r4

    // Stage 2 - odd part 1
    // Rotate (3 - 4) (r4.l) and (0 - 7) (r0.l)
    // tmp (r2) = r0.l
    // tmp (r5) = (r2 + r3)*cos(3*pi/16)*128
    add    r5, r2, r3
    mul    r5, r5, r12
    // stage3[7](r7) = r5 - (r3)*(sin(3*pi/16) + cos(3*pi/16))*128
    mls    r7, r3, r11, r5
    // stage3[4](r6) = r5 - (r2)*(-1)*(sin(3*pi/16) - cos(3*pi/16))*128
    mov    r6, #35
    mls    r6, r2, r6, r5

    // r2 = stage2[5]
    ldr r2, [r0, #\offset+8]
    ldr r3, [r0, #\offset+160]
    add r2, r2, r3

    // r3 = stage2[6]
    ldr r3, [r0, #\offset+4]
    ldr r4, [r0, #\offset+192]
    sub r3, r3, r4

    // stage 2 - odd part 2
    // Rotate (1 - 6) (r4.h) (2 - 5) (r0.h)
    // tmp (r5) = (r2 + r3) * cos(pi/16)*128
    add    r5, r2, r3
    mov    r4, #125
    mul    r5, r5, r4
    // stage3[6](r0) = r5 - (r2)*(sin(pi/16) + cos(pi/16))*128
    mov    r0, #150
    mls    r0, r2, r0, r5
    // stage3[5](r4) = r5 - (r3)*(-1)*(sin(pi/16) - cos(pi/16))*128
    mls    r4, r3, r10, r5

    // Summary
    // r6 = stage3[4]
    // r4 = stage3[5]
    // r0 = stage3[6]
    // r7 = stage3[7]

    // stage 3,4 - odd part
    // result[3] = (sqrt(2) * (stage3[7]-stage3[5])*128)/128
    sub    r2, r7, r4
    mul    r2, r2, r9
    asr    r2, r2, #7
    str    r2, [r1, #\offset+96]
    // result[5] = (sqrt(2) * (stage3[4]-stage3[6])*128)/128
    sub    r2, r6, r0
    mul    r2, r2, r3
    asr    r2, r2, #7
    str    r2, [r1, #\offset+160]
    // result[7] = (stage3[7]+stage3[5]-stage3[4]-stage3[6])
    // tmp(r2) = stage3[7]+stage3[5]
    // tmp(r3) = stage3[4]+stage3[6]
    add    r2, r7, r4
    add    r3, r6, r0
    sub    r4, r2, r3
    str    r2, [r1, #\offset+224]
    // result[1] = (stage3[7]+stage3[5]+stage3[4]+stage3[6])
    add    r2, r2, r3
    str    r2, [r1, #\offset+32]
.endm

// r0 - source/dest 8x8 32-bit integer buffer (no additional scaling)
// r8  - #212
// r9  - #181
// r10 - #100
// r11 - #177
// r12 - #106
.macro DCT8_VERTICAL_ODD_CM4 offset
    // Stage 1 - 2 - 3 even

    // r4 = stage2[0]
    // r5 = stage2[1]
    // r6 = stage2[2]
    // r7 = stage2[3]
    ldr r2, [r0, #\offset]
    ldr r3, [r0, #\offset+224]
    add r4, r2, r3

    ldr r2, [r0, #\offset+4]
    ldr r3, [r0, #\offset+192]
    add r5, r2, r3

    ldr r2, [r0, #\offset+8]
    ldr r3, [r0, #\offset+160]
    add r6, r2, r3

    ldr r2, [r0, #\offset+12]
    ldr r3, [r0, #\offset+128]
    add r7, r2, r3

    // r2 = stage3[0]
    // r3 = stage3[1]
    // r5 = stage3[2]
    // r6 = stage3[3]
    // r7 = in[4]
    add r2, r4, r7
    add r3, r5, r5
    add r2, r2, r3
    sub r3, r2, r3
    // result[0]
    str r2, [r1, #\offset]
    // result[4]
    str r3, [r1, #\offset+8]

    // stage3[2]
    sub r4, r4, r7
    // stage3[1]
    sub r5, r5, r6

    // Stage 3 - even-odd part (planar rotation sqrt2*c1)
    // tmp (r2) = (r4 + r5)*sqrt(2)*cos(pi/16)*128
    add    r2, r4, r5
    mul    r2, r2, r11
    // result[6] = r2 - (r4)*sqrt(2)*(cos(pi/16)+sin(pi/16))*128
    mls    r3, r4, r8, r2
    str    r3, [r1, #\offset+192]
    // tmp (r3) = r5*(-1*sqrt(2)*(sin(pi/16) - cos(pi/16)))*128
    mov    r3, #142
    mls    r3, r5, r3, r2
    // result[2] = (r2 - r3)/128
    str    r3, [r1, #\offset+8]

    // r2 = stage2[7]
    ldr r2, [r0, #\offset]
    ldr r3, [r0, #\offset+224]
    sub r2, r2, r3

    // r3 = stage2[4]
    ldr r3, [r0, #\offset+12]
    ldr r4, [r0, #\offset+128]
    sub r3, r2, r4

    // Stage 2 - odd part 1
    // Rotate (3 - 4) (r4.l) and (0 - 7) (r0.l)
    // tmp (r2) = r0.l
    // tmp (r5) = (r2 + r3)*cos(3*pi/16)*128
    add    r5, r2, r3
    mul    r5, r5, r12
    // stage3[7](r7) = r5 - (r3)*(sin(3*pi/16) + cos(3*pi/16))*128
    mls    r7, r3, r11, r5
    // stage3[4](r6) = r5 - (r2)*(-1)*(sin(3*pi/16) - cos(3*pi/16))*128
    mov    r6, #35
    mls    r6, r2, r6, r5

    // r2 = stage2[5]
    ldr r2, [r0, #\offset+8]
    ldr r3, [r0, #\offset+160]
    add r2, r2, r3

    // r3 = stage2[6]
    ldr r3, [r0, #\offset+4]
    ldr r4, [r0, #\offset+192]
    sub r3, r3, r4

    // stage 2 - odd part 2
    // Rotate (1 - 6) (r4.h) (2 - 5) (r0.h)
    // tmp (r5) = (r2 + r3) * cos(pi/16)*128
    add    r5, r2, r3
    mov    r4, #125
    mul    r5, r5, r4
    // stage3[6](r0) = r5 - (r2)*(sin(pi/16) + cos(pi/16))*128
    mov    r0, #150
    mls    r0, r2, r0, r5
    // stage3[5](r4) = r5 - (r3)*(-1)*(sin(pi/16) - cos(pi/16))*128
    mls    r4, r3, r10, r5

    // Summary
    // r6 = stage3[4]
    // r4 = stage3[5]
    // r0 = stage3[6]
    // r7 = stage3[7]

    // stage 3,4 - odd part
    // result[3] = (sqrt(2) * (stage3[7]-stage3[5])*128)/16384
    sub    r2, r7, r4
    mul    r2, r2, r9
    asr    r2, r2, #7
    str    r2, [r1, #\offset+96]
    // result[5] = (sqrt(2) * (stage3[4]-stage3[6])*128)/16384
    sub    r2, r6, r0
    mul    r2, r2, r3
    asr    r2, r2, #7
    str    r2, [r1, #\offset+160]
    // result[7] = (stage3[7]+stage3[5]-stage3[4]-stage3[6])/128
    // tmp(r2) = stage3[7]+stage3[5]
    // tmp(r3) = stage3[4]+stage3[6]
    add    r2, r7, r4
    add    r3, r6, r0
    sub    r4, r2, r3
    str    r2, [r1, #\offset+224]
    // result[1] = (stage3[7]+stage3[5]+stage3[4]+stage3[6])/128
    add    r2, r2, r3
    str    r2, [r1, #\offset+32]
.endm


// r0 - src 8x8 8-bit  buffer
// r1 - temp 8x8 32-bit buffer
// r2 - dst 8x8 32-bit buffer
dct8x8_cm4:
    // Load constant registers
    mov r8,  #212
    mov r9,  #181
    mov r10, #100
    mov r11, #177
    mov r12, #106

    push {r2}

    .set i,0
    .rept 8
    DCT8_HORIZONTAL_CM4 i
    .set i,i+1
    .endr

    mov r0, r1
    pop {r1}

    .set i,0
    .rept 4
    DCT8_VERTICAL_EVEN_CM4 i
    .set i,i+4
    DCT8_VERTICAL_ODD_CM4 i
    .set i,i+4
    .endr
