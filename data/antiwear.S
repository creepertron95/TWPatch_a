.3ds
.thumb
.create outfile,0

.align 4

NOP
NOP
NOP
NOP
B payload_end
B payload_end
B payload_end
B payload_end

LDR r0, =0x1ED02204
LDR r1, =0x01FF7F17
STR r1, [r0]

loops:
SUB r1, r1, #1
BNE loops
CMP r1, #2

BX LR


B payload_end
.pool

.align 4
payload_end:
.close