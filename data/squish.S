.3ds
.thumb
.create outfile,0

.align 4

LDR r0, =0x1EF00400
ADD r0, #0x54

LDR r1, [r0, #(0x64 - 0x54)]
LSR r1, #16
ADD r1, #1

LDR r2, [r0]
CMP r1, r2
BNE . - 4

ADD r0, PC, #(PDC - . - 2) & ~3
LDR r1, =0x1EF00400

MOV r2, #50h
LDMIA r0!, {r3}
STMIA r1!, {r3}
SUB r2, #4
BNE . - 6

ADD r1, #0xC

MOV r2, #0xC
LDMIA r0!, {r3}
STMIA r1!, {r3}
SUB r2, #4
BNE . - 6

;LDR r3, [r0]
;STR r3, [r1, #8]

B payload_end

.align 4
.pool

.align 4
PDC:

DCD      0x1C2
DCD       0xD1
DCD      0x1C1
DCD      0x1C1
DCD          0
DCD       0xCF
DCD       0xD1
DCD  0x1C501C1
DCD    0x10000
DCD      0x33B
DCD          2
DCD      0x322
DCD      0x322
DCD      0x322
DCD          1
DCD          2
DCD  0x3260322
DCD          0
DCD          0
DCD       0xFF

DCD  0x32000F0
DCD  0x1C100D1
DCD  0x3220002

; PDC + 70h
.dcd 3 << 8 | 0

.align 4
payload_end:
.close
