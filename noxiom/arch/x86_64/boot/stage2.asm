; Noxiom OS - Stage 2 Bootloader
; Executes at 0x7E00 in 16-bit real mode.
; 1. Loads the kernel (128 sectors = 64KB) to 0x10000
; 2. Enables the A20 line
; 3. Switches to 32-bit protected mode
; 4. Copies kernel to 0x100000 (1MB)
; 5. Sets up page tables for 64-bit long mode (identity map, 2MB pages)
; 6. Switches to 64-bit long mode
; 7. Jumps to kernel entry at 0x100000

[BITS 16]
[ORG 0x7E00]

stage2_start:
    mov [boot_drive], dl

    mov si, msg_loading
    call print16

    ; Load kernel to 0x10000 (segment 0x1000, offset 0)
    mov si, kernel_dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc .disk_error

    call enable_a20

    lgdt [gdt32_ptr]

    cli
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pmode32_start      ; CS = 32-bit code segment (index 1)

.disk_error:
    mov si, msg_disk_err
    call print16
    cli
    hlt

; --- 16-bit helpers ---

print16:
    push ax
    push bx
.loop:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp .loop
.done:
    pop bx
    pop ax
    ret

enable_a20:
    call .wait_in
    mov al, 0xAD            ; disable keyboard
    out 0x64, al
    call .wait_in
    mov al, 0xD0            ; read controller output port
    out 0x64, al
    call .wait_out
    in al, 0x60
    push ax
    call .wait_in
    mov al, 0xD1            ; write controller output port
    out 0x64, al
    call .wait_in
    pop ax
    or al, 2                ; set A20 bit
    out 0x60, al
    call .wait_in
    mov al, 0xAE            ; re-enable keyboard
    out 0x64, al
    call .wait_in
    ret
.wait_in:
    in al, 0x64
    test al, 2
    jnz .wait_in
    ret
.wait_out:
    in al, 0x64
    test al, 1
    jz .wait_out
    ret

; --- Data ---

msg_loading  db "Noxiom: loading kernel...", 13, 10, 0
msg_disk_err db "Stage2: kernel load failed!", 0
boot_drive   db 0

; Disk Address Packet: load 128 sectors (64KB) to 0x10000
align 4
kernel_dap:
    db 0x10
    db 0x00
    dw 128              ; sectors (128 * 512 = 64KB)
    dw 0x0000           ; offset within segment
    dw 0x1000           ; segment (0x1000 * 16 = 0x10000)
    dq 17               ; starting LBA (after stage1 + stage2)

; Minimal GDT for 32/64-bit mode transition
align 8
gdt32:
    dq 0                                    ; 0x00: null
    dw 0xFFFF, 0x0000, 0x9A00, 0x00CF      ; 0x08: 32-bit code
    dw 0xFFFF, 0x0000, 0x9200, 0x00CF      ; 0x10: 32-bit data
    dw 0xFFFF, 0x0000, 0x9A00, 0x00AF      ; 0x18: 64-bit code
    dw 0xFFFF, 0x0000, 0x9200, 0x00AF      ; 0x20: 64-bit data
gdt32_end:

gdt32_ptr:
    dw gdt32_end - gdt32 - 1
    dd gdt32

; --- 32-bit protected mode ---

[BITS 32]
align 16
pmode32_start:
    mov ax, 0x10            ; 32-bit data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000        ; temporary 32-bit stack

    ; Copy kernel: 0x10000 -> 0x100000 (64KB = 16384 dwords)
    mov esi, 0x10000
    mov edi, 0x100000
    mov ecx, 0x4000
    rep movsd

    ; Zero page table area at 0x1000-0x3FFF (3 tables x 4KB)
    mov edi, 0x1000
    xor eax, eax
    mov ecx, 3072           ; 3 * 1024 dwords = 12KB
    rep stosd

    ; PML4[0] -> PDPT at 0x2000
    mov dword [0x1000], 0x2003
    mov dword [0x1004], 0

    ; PDPT[0] -> PD at 0x3000
    mov dword [0x2000], 0x3003
    mov dword [0x2004], 0

    ; PD: 512 x 2MB identity-mapped pages (covers 1GB)
    mov edi, 0x3000
    mov eax, 0x0083         ; present + writable + 2MB (PS bit)
    mov ecx, 512
.fill_pd:
    mov [edi], eax
    mov dword [edi + 4], 0
    add eax, 0x200000
    add edi, 8
    loop .fill_pd

    ; Enable PAE
    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    ; CR3 = PML4 at 0x1000
    mov eax, 0x1000
    mov cr3, eax

    ; Enable long mode (LME) via EFER MSR
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    ; Enable paging (activates long mode since LME is set)
    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax

    ; Far jump into 64-bit code segment
    jmp 0x18:lmode64_start

; --- 64-bit long mode ---

[BITS 64]
align 16
lmode64_start:
    mov ax, 0x20            ; 64-bit data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Temporary stack at 2MB (kernel will set up its own)
    mov rsp, 0x1FF000

    ; Jump to kernel entry
    mov rax, 0x100000
    jmp rax
