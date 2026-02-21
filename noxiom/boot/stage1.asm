; Noxiom OS - Stage 1 Bootloader (MBR)
; Executes at 0x7C00 in 16-bit real mode.
; Loads Stage 2 (16 sectors = 8KB) from disk to 0x7E00, then jumps there.

[BITS 16]
[ORG 0x7C00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; stack grows down from below the bootloader
    sti

    mov [boot_drive], dl    ; BIOS passes boot drive in DL

    ; Load Stage 2 using INT 13h extended read (LBA)
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc .disk_error

    jmp 0x0000:0x7E00       ; jump to Stage 2

.disk_error:
    mov si, err_msg
.print:
    lodsb
    test al, al
    jz .halt
    mov ah, 0x0E
    int 0x10
    jmp .print
.halt:
    cli
    hlt

err_msg    db "Stage1: disk error!", 0
boot_drive db 0

; Disk Address Packet for INT 13h AH=42h
align 4
dap:
    db 0x10         ; DAP size = 16 bytes
    db 0x00         ; reserved
    dw 16           ; sectors to read (16 * 512 = 8KB for Stage 2)
    dw 0x7E00       ; destination offset
    dw 0x0000       ; destination segment
    dq 1            ; starting LBA (sector 1, immediately after MBR)

times 510-($-$$) db 0
dw 0xAA55
