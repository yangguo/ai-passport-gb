; AIPATTERN — checker border, 4-shade bar, "AIP0" (homebrew / RGBDS).

DEF rLCDC EQU $40
DEF rSCY  EQU $42
DEF rSCX  EQU $43
DEF rLY   EQU $44
DEF rBGP  EQU $47

SECTION "Load", ROM0[$0100]
    di
    jp Start

SECTION "Main", ROM0[$0150]
Start::
    ldh a, [rLCDC]
    bit 7, a
    jr z, .lcd_setup

.wait_vblank:
    ldh a, [rLY]
    cp 144
    jr c, .wait_vblank
    xor a
    ldh [rLCDC], a

.lcd_setup:
    ld de, $8000
    xor a
    call FillSolidTile
    ld de, $8010
    ld a, 3
    call FillSolidTile
    ld de, $8020
    ld a, 2
    call FillSolidTile
    ld de, $8030
    ld a, 1
    call FillSolidTile

    ld hl, LetterA
    ld de, $8040
    ld bc, 16
    call CopyBytes
    ld hl, LetterI
    ld de, $8050
    ld bc, 16
    call CopyBytes
    ld hl, LetterP
    ld de, $8060
    ld bc, 16
    call CopyBytes
    ld hl, Letter0
    ld de, $8070
    ld bc, 16
    call CopyBytes

    ld hl, $9800
    ld bc, 32 * 32
.map_clear:
    xor a
    ld [hl+], a
    dec bc
    ld a, b
    or c
    jr nz, .map_clear

    ld hl, $9800
    ld b, 20
    ld c, 0
.border_top:
    ld a, c
    and 1
    jr z, .top1
    xor a
    jr .topw
.top1:
    ld a, 1
.topw:
    ld [hl+], a
    inc c
    dec b
    jr nz, .border_top

    ld hl, $9A20
    ld b, 20
    ld c, 0
.border_bot:
    ld a, c
    and 1
    jr z, .bot1
    xor a
    jr .botw
.bot1:
    ld a, 1
.botw:
    ld [hl+], a
    inc c
    dec b
    jr nz, .border_bot

    ld hl, $9820
    ld c, 1
    ld de, 32
.border_left:
    ld a, c
    and 1
    jr z, .l0
    ld a, 1
    jr .lw
.l0:
    xor a
.lw:
    ld [hl], a
    add hl, de
    inc c
    ld a, c
    cp 17
    jr nz, .border_left

    ld hl, $9833
    ld c, 1
.border_right:
    ld a, c
    and 1
    jr z, .r0
    ld a, 1
    jr .rw
.r0:
    xor a
.rw:
    ld [hl], a
    add hl, de
    inc c
    ld a, c
    cp 17
    jr nz, .border_right

    ld a, 1
    ld [$9882], a
    ld a, 2
    ld [$9883], a
    ld a, 3
    ld [$9884], a
    xor a
    ld [$9885], a

    ld a, 4
    ld [$9907], a
    ld a, 5
    ld [$9908], a
    ld a, 6
    ld [$9909], a
    ld a, 7
    ld [$990A], a

    xor a
    ldh [rSCX], a
    ldh [rSCY], a

    ld a, $E4
    ldh [rBGP], a

    ld a, $91
    ldh [rLCDC], a

.loop:
    halt
    nop
    jr .loop

; A = color 0..3, DE = tile
FillSolidTile:
    push bc
    push de
    and $03
    cp 0
    jr z, .c0
    cp 1
    jr z, .c1
    cp 2
    jr z, .c2
    ld b, 16
    ld a, $FF
.c3:
    ld [de], a
    inc de
    dec b
    jr nz, .c3
    pop de
    pop bc
    ret
.c0:
    ld b, 16
    xor a
    jr .c3
.c1:
    ld b, 8
.c1r:
    ld a, $FF
    ld [de], a
    inc de
    xor a
    ld [de], a
    inc de
    dec b
    jr nz, .c1r
    pop de
    pop bc
    ret
.c2:
    ld b, 8
.c2r:
    xor a
    ld [de], a
    inc de
    ld a, $FF
    ld [de], a
    inc de
    dec b
    jr nz, .c2r
    pop de
    pop bc
    ret

CopyBytes:
.copy:
    ld a, [hl+]
    ld [de], a
    inc de
    dec bc
    ld a, b
    or c
    jr nz, .copy
    ret

SECTION "TileData", ROM0[$350]
LetterA:
    db $18, $3C, $66, $66, $7E, $66, $66, $00
    db $18, $3C, $66, $66, $7E, $66, $66, $00
LetterI:
    db $3C, $18, $18, $18, $18, $18, $3C, $00
    db $3C, $18, $18, $18, $18, $18, $3C, $00
LetterP:
    db $7C, $66, $66, $7C, $60, $60, $60, $00
    db $7C, $66, $66, $7C, $60, $60, $60, $00
Letter0:
    db $3C, $66, $6E, $76, $66, $66, $3C, $00
    db $3C, $66, $6E, $76, $66, $66, $3C, $00
