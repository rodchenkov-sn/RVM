mov r3, 3
mov r4, 1
print_loop:
    mov            r0,  72  ; H
    mov     byte [sp],  r0
    mov            r0,  69  ; E
    mov byte [sp + 1],  r0
    mov            r0,  76  ; L
    mov byte [sp + 2],  r0
    mov            r0,  76  ; L
    mov byte [sp + 3],  r0
    mov            r0,  79  ; O
    mov byte [sp + 4],  r0
    mov            r0,  10  ; \n
    mov byte [sp + 5],  r0
    mov            r0,  0   ; \0
    mov byte [sp + 6],  r0
    mov            ir,  sp
    int             1       ; PUTS
    sub            r3,  r4
    jnz print_loop
int 2 ; GETC
