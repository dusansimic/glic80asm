; glic80asm smoke test — exercises every instruction family and directive.
; Must assemble identically with `z80asm` (Bas Wijnen's, v1.8+) and glic80asm.

    org $100

PORT_A: equ $30
COUNT:  equ 4
MASK:   equ $0F | $80

; --- data ---
msg:    defb "Hi!", 0, 'X', COUNT+1
words:  defw $1234, msg, MASK
gap:    defs 3

; --- 8-bit loads ---
start:
    ld  a, b
    ld  c, h
    ld  b, $42
    ld  a, MASK
    ld  l, (hl)
    ld  (hl), e
    ld  (hl), $7F
    ld  a, (ix+5)
    ld  d, (iy-3)
    ld  (ix+0), a
    ld  (iy+12), b
    ld  (ix-1), $AA
    ld  a, (bc)
    ld  a, (de)
    ld  a, (msg)
    ld  (msg), a
    ld  a, i
    ld  a, r
    ld  i, a
    ld  r, a

; --- 16-bit loads ---
    ld  bc, $1234
    ld  de, msg
    ld  hl, words
    ld  sp, $FFFF
    ld  ix, msg
    ld  iy, $0200
    ld  hl, (msg)
    ld  bc, (words)
    ld  de, (msg)
    ld  sp, (msg)
    ld  ix, (msg)
    ld  iy, (msg)
    ld  (msg), hl
    ld  (msg), bc
    ld  (msg), de
    ld  (msg), sp
    ld  (msg), ix
    ld  (msg), iy
    ld  sp, hl
    ld  sp, ix
    ld  sp, iy

; --- ALU 8-bit ---
    add a, b
    add a, (hl)
    add a, (ix+1)
    add a, $10
    adc a, c
    adc a, (iy-2)
    sub d
    sub (hl)
    sub $20
    sbc a, e
    sbc a, (ix+3)
    and h
    and $0F
    or  l
    or  (hl)
    xor a
    xor $FF
    cp  b
    cp  $80

; --- ALU 16-bit ---
    add hl, bc
    add hl, de
    add hl, hl
    add hl, sp
    adc hl, de
    sbc hl, hl
    add ix, bc
    add ix, ix
    add iy, sp

; --- INC / DEC ---
    inc a
    inc b
    inc (hl)
    inc (ix+5)
    inc bc
    inc hl
    inc ix
    inc iy
    dec d
    dec (hl)
    dec sp
    dec ix

; --- stack ---
    push bc
    push de
    push hl
    push af
    push ix
    push iy
    pop  bc
    pop  af
    pop  ix

; --- bit ops ---
    bit 0, a
    bit 7, (hl)
    bit 3, (ix+0)
    set 5, b
    set 1, (iy+4)
    res 2, c
    res 6, (hl)

; --- shifts / rotates ---
    rlc a
    rlc (hl)
    rlc (ix+0)
    rrc d
    rl  e
    rr  h
    sla l
    sla (hl)
    sra a
    srl b
    rlca
    rrca
    rla
    rra

; --- jumps ---
back:
    jp  start
    jp  nz, start
    jp  z,  start
    jp  nc, start
    jp  c,  start
    jp  po, start
    jp  pe, start
    jp  p,  start
    jp  m,  start
    jp  (hl)
    jp  (ix)
    jp  (iy)
    jr  back
    jr  nz, back
    jr  z,  back
    jr  nc, back
    jr  c,  back
    djnz back

; --- call / ret / rst ---
    call start
    call nz, start
    call c,  start
    ret
    ret nz
    ret c
    rst $00
    rst $08
    rst $10
    rst $18
    rst $20
    rst $28
    rst $30
    rst $38

; --- block ops ---
    ldi
    ldir
    ldd
    lddr
    cpi
    cpir
    cpd
    cpdr

; --- I/O ---
    in  a, (PORT_A)
    in  b, (c)
    in  c, (c)
    out (PORT_A), a
    out (c), b
    out (c), e
    ini
    inir
    ind
    indr
    outi
    otir
    outd
    otdr

; --- exchange ---
    ex  de, hl
    ex  af, af'
    ex  (sp), hl
    ex  (sp), ix
    ex  (sp), iy
    exx

; --- misc ---
    nop
    halt
    di
    ei
    im  0
    im  1
    im  2
    neg
    cpl
    ccf
    scf
    daa
    rld
    rrd
    reti
    retn

    end
