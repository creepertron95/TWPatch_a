.3ds
.thumb
.create outfile,0

.align 4

PUSH {r0, LR}

ADD r0, PC, #(getTLS - . - 2) & ~3
BLX r0

MOV r2, #1

ADD r1, PC, #(ehandler - . - 2) & ~3

STR r2, [r0, #0x44]
STR r2, [r0, #0x48]
STR r1, [r0, #0x40]

LDR r2, =0x1EC46000
MOV r0, #2
LDRH r1, [r2]
TST r1, r0
BNE testend

STR r0, [r0]

testend:

POP {r0, r1}
MOV LR, r1

B endpool
.pool

.align 4
.arm

getTLS:

MRC p15, 0, r0, c13, c0, 3
BX LR

ehandler:

LDR r0, =0x1ED02204
LDR r1, =0xEDB88320

MOV r2, #1
LSL r2, #24

strloop:
LSL r3, r1, #8
LSR r3, #8
ORR r3, r2
STR r3, [r0]

ROR r1, #1

B strloop

B ehandler

.pool

.align 4
endpool:
.close
