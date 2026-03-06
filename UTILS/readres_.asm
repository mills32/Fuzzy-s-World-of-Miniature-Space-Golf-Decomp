.286
LOCALS

.model large

        PUBLIC  _decode_image_asm0
        ;global	_tile_tempdata

.code

        align   2
_decode_image_asm0 proc	far
ARG   compressed_data:dword,destination_buffer:dword
LOCAL bwidth,scanx,scany,input_size:word=LocalStk
    push bp
    mov  bp, sp         ; caller's stack frame
    sub  sp,LocalStk    ; local space

    pusha
    push ds

    jmp @@_start_decode

    @@DECODE_POINTER_TABLE: ;tabla de saltos: 8 entradas x 2 bytes
        dw offset @@handler_literal
        dw offset @@handler_skip_long
        dw offset @@handler_skip_short
        dw offset @@handler_word_fill
        dw offset @@handler_byte_fill
        dw offset @@handler_scanline_fill
        dw offset @@handler_backref_rel
        dw offset @@handler_backref_abs

    @@_start_decode:
    cld ; DF=0: inc SI/DI
    lds si,compressed_data ; DS:SI = source compressed data
    les di, destination_buffer  ; DI = near offset of buffer
    @@READ_NEXT_CODE:      ;Read code, get 3 upper bits and jump
        xor ah, ah
        mov al, byte ptr ds:[si]        ; leer byte de control (SI no avanza aún)
        mov bx,ax                       ; Store control in BX
        shr bx,4
        and bx,0Eh                      ; BL = opcode (0-7) aislar 3 bits x2 (point to word table)

        cmp di, 0FA00h
        jae @@END_DECODE                ; Near jump, it barely reaches
        jmp word ptr cs:[bx + offset @@DECODE_POINTER_TABLE] ;Jump to decode function

    @@handler_literal:      ; OPCODE 0 — handler_literal
        mov cl,al               ; leer byte de control
        inc si
        and cx,001Fh            ; aislar bits 4-0 (count-1)
        inc cx                  ; count real (1..32)
        REP MOVSB               ; memcpy
        jmp @@READ_NEXT_CODE

    @@handler_skip_long:    ;OPCODE 1 — handler_skip_long
        LODSW
        XCHG AH,AL              ; ah high; al low
        and ax,1FFFh            ; aislar 13 bits (skip-1)
        inc ax
        add di, ax              ; avanzar destino
        jmp @@READ_NEXT_CODE

    @@handler_skip_short:   ;OPCODE 2 — handler_skip_short
        LODSB                   ; leer byte de control
        and ax,001Fh            ; aislar 5 bits — AH ya es 0
        inc ax
        add di, ax
        jmp @@READ_NEXT_CODE

    @@handler_word_fill:    ;OPCODE 3 — handler_word_fill
        mov CX, word ptr ds:[si]
        XCHG CH,CL              ; CH high byte, CL low byte
        add si,2
        and cx,1FFFh            ; aislar 13 bits
        LODSW                   ; leer WORD valor
        REP STOSW               ; escribir WORD
        jmp @@READ_NEXT_CODE

    @@handler_byte_fill:    ;OPCODE 4 — handler_byte_fill
        LODSW
        XCHG  AL,AH             ; ah high; al low
        and AX,1FFFh            ; aislar 13 bits (count-1)
        inc AX                  ; count real
        MOV CX,AX               ; count
        LODSB                   ; leer color
        REP STOSB
        jmp @@READ_NEXT_CODE

    @@handler_scanline_fill:   ;OPCODE 5 — handler_scanline_fill
        LODSW                  ; AH = color, AL = opcode<<5 | count-1
        and  al,1Fh            ; aislar count-1 del primer byte
        inc  al                ; count real en AL
        MOV  CL,AL
        XOR  CH,CH             ; CX count real
        XCHG AL,AH             ; AL = color
        REP STOSB
        jmp @@READ_NEXT_CODE

    @@handler_backref_rel:     ;OPCODE 6 — handler_backref_rel
        LODSW
        XCHG AH,AL                 ; ah offset h al offset l
        and ax,1FFFh               ; aislar 13 bits (offset_back-1)
        inc ax                     ; offset_back real
        mov bx, di                 ; BX = posición destino actual
        sub bx, ax                 ; BX = DI - offset_back (fuente)
        mov cl, byte ptr ds:[si]   ; leer count-1
        inc si
        and cx, 00FFh              ; aislar byte bajo
        inc cx                     ; count real
        @@backref_rel_loop:
            mov al, byte ptr es:[bx]; leer byte del destino
            inc bx
            STOSB                   ; escribir en destino es:[di]
            loop @@backref_rel_loop
        jmp @@READ_NEXT_CODE

    @@handler_backref_abs:      ; OPCODE 7 — handler_backref_abs
        mov cl,al                   ; leer byte de control
        inc si
        and cx,001Fh                ; aislar bits 4-0 (count-1)
        inc cx                      ; count real
        mov bx, word ptr ds:[si]    ; BX = offset absoluto (WORD LE)
        add si, 2
        jmp @@backref_rel_loop

    @@END_DECODE:

    POP DS
    popa

    mov sp, bp
    pop bp

    ret
_decode_image_asm0 endp


end

