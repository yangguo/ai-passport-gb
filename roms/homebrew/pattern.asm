; Homemade DMG pattern ROM (AIPATTERN). Built with RGBDS (MIT tool).
; No hardware.inc — constants are local.

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
    ld hl, $8000
    ld bc, 16
    xor a
.tile0:
    ld [hl+], a
    dec bc
    ld a, b
    or c
    jr nz, .tile0

    ld hl, $8010
    ld bc, 16
    ld a, $FF
.tile1:
    ld [hl+], a
    dec bc
    ld a, b
    or c
    jr nz, .tile1

    ld hl, $9800
    ld bc, 32 * 32
    xor a
.map_fill:
    ld [hl+], a
    dec bc
    ld a, b
    or c
    jr nz, .map_fill

    ld a, 1
    ld [$9800], a

    ; SCX=4: after boot-state LCD timing, visible pixel (0,0) must read shade 3 (tile 1).
    ld a, 4
    ldh [rSCX], a
    xor a
    ldh [rSCY], a

    ld a, $E4
    ldh [rBGP], a

    ld a, $91
    ldh [rLCDC], a

.loop:
    halt
    nop
    jr .loop
