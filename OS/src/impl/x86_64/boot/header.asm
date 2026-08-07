header_start:
    ;MAGIC
    dd 0xe85250d6

    dd 0

    dd header_end - header_start

    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start))

    ; framebuffer request tag
    dw 5
    dw 0
    dd 20
    dd 1024
    dd 768
    dd 32

    ; end tag
    dw 0
    dw 0
    dd 8
header_end: