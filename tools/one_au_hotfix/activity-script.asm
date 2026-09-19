; Exact 1AU no-wipe candidate ABI only. See candidate-patch.json and README.md.
; Source equivalent: native/omega_activity_script.h and write_activity_script.
EXTERN writer_write:PROC
.code
omega_script PROC FRAME
    push rbx
    .pushreg rbx
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    sub rsp,20h
    .allocstack 20h
    .endprolog
    mov rbx,rcx
    mov rsi,rdx
    cmp BYTE PTR [rsi+0F510h],0
    je legacy_time
    lea rdi,omega_fields
field_loop:
    mov rdx,QWORD PTR [rdi]
    mov r8b,BYTE PTR [rdi+8]
    mov rcx,rbx
    call writer_write
    test al,al
    jz done
    add rdi,16
    cmp BYTE PTR [rdi+8],0
    jne field_loop
    jmp script_flag
legacy_time:
    mov edx,1
    mov r8d,1
    mov rcx,rbx
    call writer_write
    test al,al
    jz done
    mov edi,11
zero_loop:
    xor edx,edx
    mov r8d,32
    mov rcx,rbx
    call writer_write
    test al,al
    jz done
    dec edi
    jnz zero_loop
script_flag:
    xor edx,edx
    cmp BYTE PTR [rsi+0F510h],0
    jne flag_ready
    cmp BYTE PTR [rsi+274D0h],0
    setne dl
flag_ready:
    mov r8d,1
    mov rcx,rbx
    call writer_write
    test al,al
    jz done
    mov edx,DWORD PTR [rsi+274D4h]
    add edx,80000000h
    mov r8d,32
    mov rcx,rbx
    call writer_write
done:
    add rsp,20h
    pop rdi
    pop rsi
    pop rbx
    ret
omega_script ENDP
ALIGN 8
omega_fields LABEL BYTE
    DQ 0,1
    DQ 0,64
    DQ 134F00C00000h,64
    DQ 0,64
    DQ 0,64
    DQ 0FFFFFFFFFFFFFFFFh,64
    DQ 3F800000h,32
    DQ 0,0
END
