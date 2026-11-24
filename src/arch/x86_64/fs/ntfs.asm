bits 64

global ntfs_mount
global ntfs_read_sectors
global ntfs_find_root_file
global ntfs_read_file

section .text

; int ntfs_read_sectors(uint32_t lba, uint32_t count, void *buffer)
ntfs_read_sectors:
    test rdi, rdi
    jnz .check_lba
    ; LBA zero is valid.
.check_lba:
    cmp rdi, 0x0fffffff
    ja .bad_argument
    test rsi, rsi
    jz .bad_argument
    cmp rsi, 128
    ja .bad_argument
    test rdx, rdx
    jz .bad_argument
    mov rax, rdi
    add rax, rsi
    dec rax
    cmp rax, 0x0fffffff
    ja .bad_argument

    push rbx
    push r12
    push r13
    push r14
    push r15
    mov r12, rdi
    mov r13, rsi
    mov r14, rdx

    mov dx, 0x1f6
    mov eax, r12d
    shr eax, 24
    and al, 0x0f
    or al, 0xe0
    out dx, al
    mov dx, 0x1f2
    mov eax, r13d
    out dx, al
    mov dx, 0x1f3
    mov eax, r12d
    out dx, al
    mov dx, 0x1f4
    mov eax, r12d
    shr eax, 8
    out dx, al
    mov dx, 0x1f5
    mov eax, r12d
    shr eax, 16
    out dx, al
    mov dx, 0x1f7
    mov al, 0x20
    out dx, al

    cld
.sector_loop:
    call ntfs_ata_wait_drq
    test eax, eax
    jnz .ata_error
    mov dx, 0x1f0
    mov rdi, r14
    mov ecx, 256
    rep insw
    add r14, 512
    inc r12
    dec r13
    jnz .sector_loop
    xor eax, eax
    jmp .done

.ata_error:
    mov eax, -2
.done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
.bad_argument:
    mov eax, -1
    ret

ntfs_ata_wait_drq:
    mov ecx, 1000000
.wait:
    mov dx, 0x1f7
    in al, dx
    test al, 0x01
    jnz .device_error
    test al, 0x80
    jz .not_busy
    loop .wait
    mov eax, -2
    ret
.not_busy:
    test al, 0x08
    jnz .ready
    loop .wait
    mov eax, -2
    ret
.ready:
    xor eax, eax
    ret
.device_error:
    mov eax, -3
    ret

; int ntfs_mount(void): returns 0 when an NTFS volume is ready.
ntfs_mount:
    push rbx
    push r12
    push r13
    push r14
    push r15

    xor edi, edi
    mov esi, 1
    lea rdx, [rel ntfs_sector_buffer]
    call ntfs_read_sectors
    test eax, eax
    jnz .mount_error
    cmp word [rel ntfs_sector_buffer + 510], 0xaa55
    jne .mount_invalid

    ; Prefer a direct volume boot sector, otherwise scan the MBR for type 0x07.
    lea rsi, [rel ntfs_sector_buffer + 3]
    mov rdi, ntfs_oem_id
    mov ecx, 8
    repe cmpsb
    je .direct_volume

    mov r12d, 0
.partition_loop:
    cmp r12d, 4
    jae .mount_invalid
    mov eax, r12d
    imul eax, 16
    lea rbx, [rel ntfs_sector_buffer + 446]
    add rbx, rax
    cmp byte [rbx + 4], 0x07
    je .partition_found
    inc r12d
    jmp .partition_loop
.partition_found:
    mov eax, [rbx + 8]
    mov [rel ntfs_volume_lba], eax
    jmp .read_volume
.direct_volume:
    mov qword [rel ntfs_volume_lba], 0

.read_volume:
    mov edi, [rel ntfs_volume_lba]
    mov esi, 1
    lea rdx, [rel ntfs_sector_buffer]
    call ntfs_read_sectors
    test eax, eax
    jnz .mount_error

    lea rsi, [rel ntfs_sector_buffer + 3]
    mov rdi, ntfs_oem_id
    mov ecx, 8
    repe cmpsb
    jne .mount_invalid
    cmp word [rel ntfs_sector_buffer + 11], 512
    jne .mount_invalid

    movzx eax, byte [rel ntfs_sector_buffer + 13]
    test eax, eax
    jz .mount_invalid
    cmp eax, 128
    ja .mount_invalid
    lea ecx, [rax - 1]
    test eax, ecx
    jnz .mount_invalid
    mov [rel ntfs_sectors_per_cluster], eax
    shl eax, 9
    mov [rel ntfs_cluster_bytes], eax

    mov rax, [rel ntfs_sector_buffer + 40]
    test rax, rax
    jz .mount_invalid
    mov [rel ntfs_volume_sectors], rax
    mov rax, [rel ntfs_sector_buffer + 48]
    mov [rel ntfs_mft_lcn], rax

    movsx eax, byte [rel ntfs_sector_buffer + 64]
    test eax, eax
    jz .mount_invalid
    js .record_size_power
    imul eax, [rel ntfs_cluster_bytes]
    jmp .record_size_validated
.record_size_power:
    neg eax
    cmp eax, 12
    ja .mount_invalid
    mov ecx, eax
    mov eax, 1
    shl eax, cl
.record_size_validated:
    cmp eax, 512
    jb .mount_invalid
    cmp eax, 4096
    ja .mount_invalid
    test eax, 511
    jnz .mount_invalid
    mov [rel ntfs_record_bytes], eax
    shr eax, 9
    mov [rel ntfs_record_sectors], eax

    ; Record zero begins at the LCN from the boot sector.
    mov rax, [rel ntfs_mft_lcn]
    imul rax, [rel ntfs_sectors_per_cluster]
    add rax, [rel ntfs_volume_lba]
    mov edi, eax
    mov esi, [rel ntfs_record_sectors]
    lea rdx, [rel ntfs_record_buffer]
    call ntfs_read_sectors
    test eax, eax
    jnz .mount_error
    lea rdi, [rel ntfs_record_buffer]
    call ntfs_fixup_record
    test eax, eax
    jnz .mount_invalid

    ; Parse the unnamed $DATA attribute of $MFT into its run list.
    lea rdi, [rel ntfs_record_buffer]
    call ntfs_find_mft_data
    test eax, eax
    jnz .mount_invalid
    mov byte [rel ntfs_is_mounted], 1
    xor eax, eax
    jmp .mount_done

.mount_error:
    mov dword [rel ntfs_is_mounted], 0
    mov eax, -2
    jmp .mount_done
.mount_invalid:
    mov dword [rel ntfs_is_mounted], 0
    mov eax, -3
.mount_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

; Validate and apply the update-sequence array in an in-memory FILE record.
; int ntfs_fixup_record(void *record)
ntfs_fixup_record:
    cmp dword [rdi], 0x454c4946
    jne .fixup_invalid
    movzx ecx, word [rdi + 6]
    mov eax, [rel ntfs_record_sectors]
    inc eax
    cmp ecx, eax
    jne .fixup_invalid
    movzx eax, word [rdi + 4]
    mov edx, eax
    lea edx, [edx + ecx * 2]
    cmp edx, [rel ntfs_record_bytes]
    ja .fixup_invalid
    lea rsi, [rdi + rax]
    movzx r8d, word [rsi]
    lea r9, [rsi + 2]
    mov r10d, 1
.fixup_loop:
    cmp r10d, ecx
    jae .fixup_success
    mov eax, r10d
    shl eax, 9
    sub eax, 2
    cmp word [rdi + rax], r8w
    jne .fixup_invalid
    mov dx, [r9]
    mov [rdi + rax], dx
    add r9, 2
    inc r10d
    jmp .fixup_loop
.fixup_success:
    xor eax, eax
    ret
.fixup_invalid:
    mov eax, -1
    ret

; Parse $MFT's unnamed non-resident DATA attribute.
; int ntfs_find_mft_data(void *record)
ntfs_find_mft_data:
    push rbx
    movzx r8d, word [rdi + 20]
    lea r9, [rdi + r8]
    mov ecx, [rdi + 24]
    add rdi, rcx
.mft_attr_loop:
    cmp r9, rdi
    jae .mft_attr_invalid
    mov eax, [r9]
    cmp eax, 0xffffffff
    je .mft_attr_invalid
    cmp eax, 0x80
    jne .mft_attr_next
    cmp byte [r9 + 8], 1
    jne .mft_attr_next
    cmp byte [r9 + 9], 0
    jne .mft_attr_next
    cmp qword [r9 + 16], 0
    jne .mft_attr_invalid
    mov rax, [r9 + 48]
    mov [rel ntfs_mft_size], rax
    movzx eax, word [r9 + 32]
    lea rdi, [r9 + rax]
    lea rsi, [rel ntfs_mft_runs]
    mov edx, 64
    call ntfs_parse_runs
    test eax, eax
    jle .mft_attr_invalid
    mov [rel ntfs_mft_run_count], eax
    xor eax, eax
    pop rbx
    ret
.mft_attr_next:
    mov eax, [r9 + 4]
    test eax, eax
    jz .mft_attr_invalid
    add r9, rax
    jmp .mft_attr_loop
.mft_attr_invalid:
    mov eax, -1
    pop rbx
    ret

; Decode mapping pairs. Inputs: rdi=bytes, rsi=run array, rdx=capacity.
; Each run is {starting VCN, starting LCN, cluster count}, all qwords.
; Returns the number of runs, or a negative error.
ntfs_parse_runs:
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rbp
    sub rsp, 8
    mov r12, rdi
    mov r13, rsi
    mov r14, rdx
    xor r15d, r15d
    xor ebx, ebx
    xor ebp, ebp
.run_loop:
    movzx eax, byte [r12]
    inc r12
    test eax, eax
    jz .run_success
    mov ecx, eax
    and ecx, 0x0f
    shr eax, 4
    mov r8d, eax
    test ecx, ecx
    jz .run_invalid
    cmp ecx, 8
    ja .run_invalid
    test r8d, r8d
    jz .run_invalid
    cmp r8d, 8
    ja .run_invalid
    cmp r15, r14
    jae .run_invalid
    mov r10d, ecx
    xor r9d, r9d
    xor r11d, r11d
.length_bytes:
    cmp r11d, r10d
    jae .length_done
    movzx eax, byte [r12 + r11]
    mov ecx, r11d
    shl ecx, 3
    shl rax, cl
    or r9, rax
    inc r11d
    jmp .length_bytes
.length_done:
    add r12, r10
    xor r11d, r11d
    xor r10d, r10d
.offset_bytes:
    cmp r10d, r8d
    jae .offset_done
    movzx eax, byte [r12 + r10]
    mov ecx, r10d
    shl ecx, 3
    shl rax, cl
    or r11, rax
    inc r10d
    jmp .offset_bytes
.offset_done:
    add r12, r8
    mov ecx, r8d
    shl ecx, 3
    cmp ecx, 64
    je .offset_signed
    dec ecx
    bt r11, rcx
    jnc .offset_signed
    inc ecx
    mov eax, 1
    shl rax, cl
    sub r11, rax
.offset_signed:
    mov [r13 + r15 * 24], rbx
    mov [r13 + r15 * 24 + 8], rbp
    mov [r13 + r15 * 24 + 16], r9
    add rbx, r9
    add rbp, r11
    inc r15
    jmp .run_loop
.run_success:
    mov eax, r15d
    jmp .run_done
.run_invalid:
    mov eax, -1
.run_done:
    add rsp, 8
    pop rbp
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

; Map a logical cluster to an LCN. Inputs: rdi=run array, rsi=count, rdx=VCN.
ntfs_map_cluster:
    xor ecx, ecx
.map_loop:
    cmp rcx, rsi
    jae .map_missing
    mov rax, [rdi + rcx * 24]
    cmp rdx, rax
    jb .map_next
    mov r8, [rdi + rcx * 24 + 16]
    add rax, r8
    cmp rdx, rax
    jae .map_next
    mov rax, [rdi + rcx * 24 + 8]
    test rax, rax
    js .map_missing
    sub rdx, [rdi + rcx * 24]
    add rax, rdx
    ret
.map_next:
    inc rcx
    jmp .map_loop
.map_missing:
    mov rax, -1
    ret

; Read an MFT record by its record number into rsi.
ntfs_read_mft_record:
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov r12, rdi
    mov r15, rsi
    mov eax, [rel ntfs_record_sectors]
    imul r12, rax
    xor r14d, r14d
.mft_sector_loop:
    cmp r14d, [rel ntfs_record_sectors]
    jae .mft_record_fixup
    mov rax, r12
    xor edx, edx
    div qword [rel ntfs_sectors_per_cluster]
    mov rbx, rdx
    mov rdi, [rel ntfs_mft_runs]
    mov esi, [rel ntfs_mft_run_count]
    mov rdx, rax
    call ntfs_map_cluster
    cmp rax, -1
    je .mft_read_error
    imul rax, [rel ntfs_sectors_per_cluster]
    add rax, rbx
    add rax, [rel ntfs_volume_lba]
    mov edi, eax
    mov esi, 1
    mov rdx, r15
    call ntfs_read_sectors
    test eax, eax
    jnz .mft_read_error
    add r15, 512
    inc r12
    inc r14d
    jmp .mft_sector_loop
.mft_record_fixup:
    mov rdi, r15
    mov rax, rdi
    mov eax, [rel ntfs_record_sectors]
    shl eax, 9
    sub rdi, rax
    call ntfs_fixup_record
    jmp .mft_read_done
.mft_read_error:
    mov eax, -1
.mft_read_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

; int64_t ntfs_find_root_file(const char *ascii_name)
; Finds an ASCII filename directly under the root directory (record 5).
ntfs_find_root_file:
    test rdi, rdi
    jz .find_bad_argument
    cmp byte [rdi], 0
    je .find_bad_argument
    cmp byte [rel ntfs_is_mounted], 1
    jne .find_unmounted
    cmp byte [rdi], '/'
    je .find_not_found
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov r12, rdi
    xor r13d, r13d
    mov rax, [rel ntfs_mft_size]
    xor edx, edx
    div qword [rel ntfs_record_bytes]
    cmp rax, 65536
    jbe .find_count_ready
    mov eax, 65536
.find_count_ready:
    mov r14, rax
.find_record_loop:
    cmp r13, r14
    jae .find_missing
    mov rdi, r13
    lea rsi, [rel ntfs_record_buffer]
    call ntfs_read_mft_record
    test eax, eax
    jnz .find_next_record
    movzx eax, word [rel ntfs_record_buffer + 22]
    test eax, 1
    jz .find_next_record
    movzx eax, word [rel ntfs_record_buffer + 20]
    lea r15, [rel ntfs_record_buffer + rax]
    mov ebx, [rel ntfs_record_buffer + 24]
    add rbx, ntfs_record_buffer
.find_attr_loop:
    cmp r15, rbx
    jae .find_next_record
    mov eax, [r15]
    cmp eax, 0xffffffff
    je .find_next_record
    cmp eax, 0x30
    jne .find_attr_next
    cmp byte [r15 + 8], 0
    jne .find_attr_next
    movzx eax, word [r15 + 20]
    lea rsi, [r15 + rax]
    cmp ax, 5
    jne .find_attr_next
    movzx ecx, byte [rsi + 64]
    test ecx, ecx
    jz .find_attr_next
    mov rdi, r12
    lea rdx, [rsi + 66]
    xor r8d, r8d
.name_compare:
    cmp r8d, ecx
    jae .name_end
    mov al, [rdi + r8]
    test al, al
    jz .find_attr_next
    cmp byte [rdx + r8 * 2 + 1], 0
    jne .find_attr_next
    mov r11b, [rdx + r8 * 2]
    cmp al, 'A'
    jb .input_lower
    cmp al, 'Z'
    ja .input_lower
    add al, 32
.input_lower:
    cmp r11b, 'A'
    jb .disk_lower
    cmp r11b, 'Z'
    ja .disk_lower
    add r11b, 32
.disk_lower:
    cmp al, r11b
    jne .find_attr_next
    inc r8d
    jmp .name_compare
.name_end:
    cmp byte [rdi + r8], 0
    jne .find_attr_next
    mov rax, r13
    jmp .find_done
.find_attr_next:
    mov eax, [r15 + 4]
    test eax, eax
    jz .find_next_record
    add r15, rax
    jmp .find_attr_loop
.find_next_record:
    inc r13
    jmp .find_record_loop
.find_missing:
    mov rax, -4
.find_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
.find_bad_argument:
    mov rax, -1
    ret
.find_unmounted:
    mov rax, -2
    ret
.find_not_found:
    mov rax, -4
    ret

; int64_t ntfs_read_file(const char *name, void *buffer, uint64_t capacity)
; Returns the file length, or a negative error.
ntfs_read_file:
    test rsi, rsi
    jz .read_file_bad_argument
    test rdx, rdx
    jz .read_file_bad_argument
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov r12, rsi
    mov r13, rdx
    call ntfs_find_root_file
    test rax, rax
    js .read_file_return
    mov rdi, rax
    lea rsi, [rel ntfs_record_buffer]
    call ntfs_read_mft_record
    test eax, eax
    jnz .read_file_io_error

    movzx eax, word [rel ntfs_record_buffer + 20]
    lea r14, [rel ntfs_record_buffer + rax]
    mov ebx, [rel ntfs_record_buffer + 24]
    add rbx, ntfs_record_buffer
.data_attr_loop:
    cmp r14, rbx
    jae .read_file_not_found
    mov eax, [r14]
    cmp eax, 0xffffffff
    je .read_file_not_found
    cmp eax, 0x80
    jne .data_attr_next
    cmp byte [r14 + 9], 0
    jne .data_attr_next
    cmp byte [r14 + 8], 0
    jne .nonresident_data
    mov eax, [r14 + 16]
    mov [rel ntfs_file_size], rax
    cmp rax, r13
    ja .read_file_too_large
    movzx eax, word [r14 + 20]
    lea rsi, [r14 + rax]
    mov rdi, r12
    mov rcx, [rel ntfs_file_size]
    rep movsb
    mov rax, [rel ntfs_file_size]
    jmp .read_file_return
.nonresident_data:
    test word [r14 + 12], 0x4001
    jnz .read_file_unsupported
    mov rax, [r14 + 48]
    mov [rel ntfs_file_size], rax
    cmp rax, r13
    ja .read_file_too_large
    movzx eax, word [r14 + 32]
    lea rdi, [r14 + rax]
    lea rsi, [rel ntfs_file_runs]
    mov edx, 64
    call ntfs_parse_runs
    test eax, eax
    jle .read_file_io_error
    mov [rel ntfs_file_run_count], eax
    call ntfs_copy_file_data
    jmp .read_file_return
.data_attr_next:
    mov eax, [r14 + 4]
    test eax, eax
    jz .read_file_not_found
    add r14, rax
    jmp .data_attr_loop
.read_file_too_large:
    mov rax, -5
    jmp .read_file_return
.read_file_unsupported:
    mov rax, -6
    jmp .read_file_return
.read_file_io_error:
    mov rax, -3
    jmp .read_file_return
.read_file_not_found:
    mov rax, -4
.read_file_return:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
.read_file_bad_argument:
    mov rax, -1
    ret

; Copy an uncompressed non-resident stream using the previously decoded runs.
ntfs_copy_file_data:
    push rbx
    push r12
    push r13
    push r14
    push r15
    mov r13, [rel ntfs_file_size]
    xor r14d, r14d
.file_sector_loop:
    cmp r14, r13
    jae .file_copy_success
    mov eax, [rel ntfs_cluster_bytes]
    mov rax, r14
    xor edx, edx
    div qword [rel ntfs_cluster_bytes]
    mov r15, rdx
    mov rdi, [rel ntfs_file_runs]
    mov esi, [rel ntfs_file_run_count]
    mov rdx, rax
    call ntfs_map_cluster
    cmp rax, -1
    je .file_copy_error
    mov rbx, rax
    ; Convert the byte offset within the cluster into a sector offset.
    mov rax, r15
    shr rax, 9
    imul rbx, [rel ntfs_sectors_per_cluster]
    add rbx, rax
    add rbx, [rel ntfs_volume_lba]
    mov edi, ebx
    mov esi, 1
    lea rdx, [rel ntfs_sector_buffer]
    call ntfs_read_sectors
    test eax, eax
    jnz .file_copy_error
    mov rax, r14
    and eax, 511
    mov rsi, rax
    mov rax, 512
    sub rax, rsi
    mov rcx, r13
    sub rcx, r14
    cmp rax, rcx
    cmova rax, rcx
    mov rcx, rax
    lea rsi, [rel ntfs_sector_buffer + rsi]
    mov rdi, r12
    add rdi, r14
    rep movsb
    add r14, rax
    jmp .file_sector_loop
.file_copy_success:
    mov rax, r13
    jmp .file_copy_done
.file_copy_error:
    mov rax, -3
.file_copy_done:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

section .rodata
ntfs_oem_id: db 'NTFS    '

section .bss
align 16
ntfs_is_mounted: resb 1
align 8
ntfs_volume_lba: resq 1
ntfs_volume_sectors: resq 1
ntfs_mft_lcn: resq 1
ntfs_mft_size: resq 1
ntfs_file_size: resq 1
ntfs_sectors_per_cluster: resq 1
ntfs_cluster_bytes: resq 1
ntfs_record_bytes: resd 1
ntfs_record_sectors: resd 1
ntfs_mft_run_count: resd 1
ntfs_file_run_count: resd 1
align 16
ntfs_sector_buffer: resb 512
ntfs_record_buffer: resb 4096
ntfs_mft_runs: resq 64 * 3
ntfs_file_runs: resq 64 * 3
