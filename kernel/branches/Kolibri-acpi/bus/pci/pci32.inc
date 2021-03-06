;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;                                                              ;;
;; Copyright (C) KolibriOS team 2004-2011. All rights reserved. ;;
;; Distributed under terms of the GNU General Public License    ;;
;;                                                              ;;
;;                                                              ;;
;;  PCI32.INC                                                   ;;
;;                                                              ;;
;;  32 bit PCI driver code                                      ;;
;;                                                              ;;
;;  Version 0.3  April 9, 2007                                  ;;
;;  Version 0.2  December 21st, 2002                            ;;
;;                                                              ;;
;;  Author: Victor Prodan, victorprodan@yahoo.com               ;;
;;          Mihailov Ilia, ghost.nsk@gmail.com                  ;;
;;    Credits:                                                  ;;
;;          Ralf Brown                                          ;;
;;          Mike Hibbett, mikeh@oceanfree.net                   ;;
;;                                                              ;;
;;  See file COPYING for details                                ;;
;;                                                              ;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

$Revision: 3910 $

;***************************************************************************
;   Function
;      pci_api:
;
;   Description
;       entry point for system PCI calls
;***************************************************************************
;mmio_pci_addr  equ  0x400               ; set actual PCI address here to activate user-MMIO

iglobal
align 4
f62call:
        dd      pci_fn_0
        dd      pci_fn_1
        dd      pci_fn_2
        dd      pci_service_not_supported       ;3
        dd      pci_read_reg            ;4 byte
        dd      pci_read_reg            ;5 word
        dd      pci_read_reg            ;6 dword
        dd      pci_service_not_supported   ;7
        dd      pci_write_reg           ;8 byte
        dd      pci_write_reg           ;9 word
        dd      pci_write_reg           ;10 dword
if defined mmio_pci_addr
        dd      pci_mmio_init           ;11
        dd      pci_mmio_map            ;12
        dd      pci_mmio_unmap          ;13
end if

endg

align 4

pci_api:

;cross
        mov     eax, ebx
        mov     ebx, ecx
        mov     ecx, edx

        cmp     [pci_access_enabled], 1
        jne     pci_service_not_supported

        movzx   edx, al

if defined mmio_pci_addr
        cmp     al, 13
        ja      pci_service_not_supported
else
        cmp     al, 10
        ja      pci_service_not_supported
end if

        call    dword [f62call+edx*4]
        mov     dword [esp+32], eax
        ret


align 4
pci_api_drv:

        cmp     [pci_access_enabled], 1
        jne     .fail

        cmp     eax, 2
        ja      .fail

        jmp     dword [f62call+eax*4]

.fail:
        or      eax, -1
        ret


;; ============================================

pci_fn_0:
; PCI function 0: get pci version (AH.AL)
        movzx   eax, word [BOOT_VARS+0x9022]
        ret

pci_fn_1:
; PCI function 1: get last bus in AL
        mov     al, [BOOT_VARS+0x9021]
        ret

pci_fn_2:
; PCI function 2: get pci access mechanism
        mov     al, [BOOT_VARS+0x9020]
        ret

pci_service_not_supported:
        or      eax, -1
        mov     dword [esp+32], eax
        ret

;***************************************************************************
;   Function
;      pci_make_config_cmd
;
;   Description
;       creates a command dword  for use with the PCI bus
;       bus # in ah
;       device+func in bh (dddddfff)
;       register in bl
;
;      command dword returned in eax ( 10000000 bbbbbbbb dddddfff rrrrrr00 )
;***************************************************************************

align 4

pci_make_config_cmd:
        shl     eax, 8     ; move bus to bits 16-23
        mov     ax, bx     ; combine all
        and     eax, 0xffffff
        or      eax, 0x80000000
        ret

;***************************************************************************
;   Function
;      pci_read_reg:
;
;   Description
;       read a register from the PCI config space into EAX/AX/AL
;       IN: ah=bus,device+func=bh,register address=bl
;           number of bytes to read (1,2,4) coded into AL, bits 0-1
;           (0 - byte, 1 - word, 2 - dword)
;***************************************************************************

align 4

pci_read_reg:
        push    ebx esi
        cmp     byte [BOOT_VARS+0x9020], 2;what mechanism will we use?
        je      pci_read_reg_2

                ; mechanism 1
        mov     esi, eax ; save register size into ESI
        and     esi, 3

        call    pci_make_config_cmd
        mov     ebx, eax
                ; get current state
        mov     dx, 0xcf8
        in      eax, dx
        push    eax
                ; set up addressing to config data
        mov     eax, ebx
        and     al, 0xfc; make address dword-aligned
        out     dx, eax
                ; get requested DWORD of config data
        mov     dl, 0xfc
        and     bl, 3
        or      dl, bl   ; add to port address first 2 bits of register address

        or      esi, esi
        jz      pci_read_byte1
        cmp     esi, 1
        jz      pci_read_word1
        cmp     esi, 2
        jz      pci_read_dword1
        jmp     pci_fin_read1

pci_read_byte1:
        in      al, dx
        jmp     pci_fin_read1
pci_read_word1:
        in      ax, dx
        jmp     pci_fin_read1
pci_read_dword1:
        in      eax, dx
        jmp     pci_fin_read1
pci_fin_read1:
                ; restore configuration control
        xchg    eax, [esp]
        mov     dx, 0xcf8
        out     dx, eax

        pop     eax
        pop     esi ebx
        ret
pci_read_reg_2:

        test    bh, 128 ;mech#2 only supports 16 devices per bus
        jnz     pci_read_reg_err

        mov     esi, eax ; save register size into ESI
        and     esi, 3

        push    eax
                ;store current state of config space
        mov     dx, 0xcf8
        in      al, dx
        mov     ah, al
        mov     dl, 0xfa
        in      al, dx

        xchg    eax, [esp]
                ; out 0xcfa,bus
        mov     al, ah
        out     dx, al
                ; out 0xcf8,0x80
        mov     dl, 0xf8
        mov     al, 0x80
        out     dx, al
                ; compute addr
        shr     bh, 3; func is ignored in mechanism 2
        or      bh, 0xc0
        mov     dx, bx

        or      esi, esi
        jz      pci_read_byte2
        cmp     esi, 1
        jz      pci_read_word2
        cmp     esi, 2
        jz      pci_read_dword2
        jmp     pci_fin_read2

pci_read_byte2:
        in      al, dx
        jmp     pci_fin_read2
pci_read_word2:
        in      ax, dx
        jmp     pci_fin_read2
pci_read_dword2:
        in      eax, dx
;       jmp pci_fin_read2
pci_fin_read2:

                ; restore configuration space
        xchg    eax, [esp]
        mov     dx, 0xcfa
        out     dx, al
        mov     dl, 0xf8
        mov     al, ah
        out     dx, al

        pop     eax
        pop     esi ebx
        ret

pci_read_reg_err:
        xor     eax, eax
        dec     eax
        pop     esi ebx
        ret


;***************************************************************************
;   Function
;      pci_write_reg:
;
;   Description
;       write a register from ECX/CX/CL into the PCI config space
;       IN: ah=bus,device+func=bh,register address (dword aligned)=bl,
;           value to write in ecx
;           number of bytes to write (1,2,4) coded into AL, bits 0-1
;           (0 - byte, 1 - word, 2 - dword)
;***************************************************************************

align 4

pci_write_reg:
        push    esi ebx
        cmp     byte [BOOT_VARS+0x9020], 2;what mechanism will we use?
        je      pci_write_reg_2

                ; mechanism 1
        mov     esi, eax ; save register size into ESI
        and     esi, 3

        call    pci_make_config_cmd
        mov     ebx, eax
                ; get current state into ecx
        mov     dx, 0xcf8
        in      eax, dx
        push    eax
                ; set up addressing to config data
        mov     eax, ebx
        and     al, 0xfc; make address dword-aligned
        out     dx, eax
                ; write DWORD of config data
        mov     dl, 0xfc
        and     bl, 3
        or      dl, bl
        mov     eax, ecx

        or      esi, esi
        jz      pci_write_byte1
        cmp     esi, 1
        jz      pci_write_word1
        cmp     esi, 2
        jz      pci_write_dword1
        jmp     pci_fin_write1

pci_write_byte1:
        out     dx, al
        jmp     pci_fin_write1
pci_write_word1:
        out     dx, ax
        jmp     pci_fin_write1
pci_write_dword1:
        out     dx, eax
        jmp     pci_fin_write1
pci_fin_write1:

                ; restore configuration control
        pop     eax
        mov     dl, 0xf8
        out     dx, eax

        xor     eax, eax
        pop     ebx esi

        ret
pci_write_reg_2:

        test    bh, 128 ;mech#2 only supports 16 devices per bus
        jnz     pci_write_reg_err


        mov     esi, eax ; save register size into ESI
        and     esi, 3

        push    eax
                ;store current state of config space
        mov     dx, 0xcf8
        in      al, dx
        mov     ah, al
        mov     dl, 0xfa
        in      al, dx
        xchg    eax, [esp]
                ; out 0xcfa,bus
        mov     al, ah
        out     dx, al
                ; out 0xcf8,0x80
        mov     dl, 0xf8
        mov     al, 0x80
        out     dx, al
                ; compute addr
        shr     bh, 3; func is ignored in mechanism 2
        or      bh, 0xc0
        mov     dx, bx
                ; write register
        mov     eax, ecx

        or      esi, esi
        jz      pci_write_byte2
        cmp     esi, 1
        jz      pci_write_word2
        cmp     esi, 2
        jz      pci_write_dword2
        jmp     pci_fin_write2

pci_write_byte2:
        out     dx, al
        jmp     pci_fin_write2
pci_write_word2:
        out     dx, ax
        jmp     pci_fin_write2
pci_write_dword2:
        out     dx, eax
        jmp     pci_fin_write2
pci_fin_write2:
                ; restore configuration space
        pop     eax
        mov     dx, 0xcfa
        out     dx, al
        mov     dl, 0xf8
        mov     al, ah
        out     dx, al

        xor     eax, eax
        pop     ebx esi
        ret

pci_write_reg_err:
        xor     eax, eax
        dec     eax
        pop     ebx esi
        ret

if defined mmio_pci_addr        ; must be set above
;***************************************************************************
;   Function
;      pci_mmio_init
;
;   Description
;       IN:  bx = device's PCI bus address (bbbbbbbbdddddfff)
;   Returns  eax = user heap space available (bytes)
;   Error codes
;       eax = -1 : PCI user access blocked,
;       eax = -2 : device not registered for uMMIO service
;       eax = -3 : user heap initialization failure
;***************************************************************************
pci_mmio_init:
        cmp     bx, mmio_pci_addr
        jz      @f
        mov     eax, -2
        ret
@@:
        call    init_heap  ; (if not initialized yet)
        or      eax, eax
        jz      @f
        ret
@@:
        mov     eax, -3
        ret


;***************************************************************************
;   Function
;      pci_mmio_map
;
;   Description
;       maps a block of PCI memory to user-accessible linear address
;
;       WARNING! This VERY EXPERIMENTAL service is for one chosen PCI device only!
;       The target device address should be set in kernel var mmio_pci_addr
;
;       IN:  ah = BAR#;
;       IN: ebx = block size (bytes);
;       IN: ecx = offset in MMIO block (in 4K-pages, to avoid misaligned pages);
;
;   Returns eax = MMIO block's linear address in the userspace (if no error)
;
;
;   Error codes
;       eax = -1 : user access to PCI blocked,
;       eax = -2 : an invalid BAR register referred
;       eax = -3 : no i/o space on that BAR
;       eax = -4 : a port i/o BAR register referred
;       eax = -5 : dynamic userspace allocation problem
;***************************************************************************

pci_mmio_map:
        and     edx, 0x0ffff
        cmp     ah, 6
        jc      .bar_0_5
        jz      .bar_rom
        mov     eax, -2
        ret
.bar_rom:
        mov     ah, 8   ; bar6 = Expansion ROM base address
.bar_0_5:
        push    ecx
        add     ebx, 4095
        and     ebx, -4096
        push    ebx
        mov     bl, ah  ; bl = BAR# (0..5), however bl=8 for BAR6
        shl     bl, 1
        shl     bl, 1
        add     bl, 0x10; now bl = BAR offset in PCI config. space
        mov     ax, mmio_pci_addr
        mov     bh, al  ; bh = dddddfff
        mov     al, 2   ; al : DW to read
        call    pci_read_reg
        or      eax, eax
        jnz     @f
        mov     eax, -3 ; empty I/O space
        jmp     mmio_ret_fail
@@:
        test    eax, 1
        jz      @f
        mov     eax, -4 ; damned ports (not MMIO space)
        jmp     mmio_ret_fail
@@:
        pop     ecx     ; ecx = block size, bytes (expanded to whole page)
        mov     ebx, ecx; user_alloc destroys eax, ecx, edx, but saves ebx
        and     eax, 0xFFFFFFF0
        push    eax           ; store MMIO physical address + keep 2DWords in the stack
        stdcall user_alloc, ecx
        or      eax, eax
        jnz     mmio_map_over
        mov     eax, -5 ; problem with page allocation

mmio_ret_fail:
        pop     ecx
        pop     edx
        ret

mmio_map_over:
        mov     ecx, ebx; ecx = size (bytes, expanded to whole page)
        shr     ecx, 12 ; ecx = number of pages
        mov     ebx, eax; ebx = linear address
        pop     eax     ; eax = MMIO start
        pop     edx     ; edx = MMIO shift (pages)
        shl     edx, 12 ; edx = MMIO shift (bytes)
        add     eax, edx; eax = uMMIO physical address
        or      eax, PG_SHARED
        or      eax, PG_UW
        or      eax, PG_NOCACHE
        mov     edi, ebx
        call    commit_pages
        mov     eax, edi
        ret

;***************************************************************************
;   Function
;      pci_mmio_unmap_page
;
;   Description
;       unmaps the linear space previously tied to a PCI memory block
;
;       IN: ebx = linear address of space previously allocated by pci_mmio_map
;       returns eax = 1 if successfully unmapped
;
;   Error codes
;       eax = -1 if no user PCI access allowed,
;       eax =  0 if unmapping failed
;***************************************************************************

pci_mmio_unmap:
        stdcall user_free, ebx
        ret

end if

;-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
uglobal
align 4
; VendID (2), DevID (2), Revision = 0 (1), Class Code (3), FNum (1), Bus (1)
pci_emu_dat:
                times   30*10 db 0
endg
;-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
align 4
sys_pcibios:
        cmp     [pci_access_enabled], 1
        jne     .unsupported_func
        cmp     [pci_bios_entry], 0
        jz      .emulate_bios

        push    ds
        mov     ax, pci_data_sel
        mov     ds, ax
        mov     eax, ebp
        mov     ah, 0B1h
        call    pword [cs:pci_bios_entry]
        pop     ds

        jmp     .return
        ;-=-=-=-=-=-=-=-=
.emulate_bios:
        cmp     ebp, 1                  ; PCI_FUNCTION_ID
        jnz     .not_PCI_BIOS_PRESENT
        mov     edx, 'PCI '
        mov     al, [BOOT_VARS + 0x9020]
        mov     bx, [BOOT_VARS + 0x9022]
        mov     cl, [BOOT_VARS + 0x9021]
        xor     ah, ah
        jmp     .return_abcd

.not_PCI_BIOS_PRESENT:
        cmp     ebp, 2                  ; FIND_PCI_DEVICE
        jne     .not_FIND_PCI_DEVICE
        mov     ebx, pci_emu_dat
..nxt:
        cmp     [ebx], dx
        jne     ..no
        cmp     [ebx + 2], cx
        jne     ..no
        dec     si
        jns     ..no
        mov     bx, [ebx + 4]
        xor     ah, ah
        jmp     .return_ab
..no:
        cmp     word[ebx], 0
        je      ..dev_not_found
        add     ebx, 10
        jmp     ..nxt
..dev_not_found:
        mov     ah, 0x86                ; DEVICE_NOT_FOUND
        jmp     .return_a

.not_FIND_PCI_DEVICE:
        cmp     ebp, 3                  ; FIND_PCI_CLASS_CODE
        jne     .not_FIND_PCI_CLASS_CODE
        mov     esi, pci_emu_dat
        shl     ecx, 8
..nxt2:
        cmp     [esi], ecx
        jne     ..no2
        mov     bx, [esi]
        xor     ah, ah
        jmp     .return_ab
..no2:
        cmp     dword[esi], 0
        je      ..dev_not_found
        add     esi, 10
        jmp     ..nxt2

.not_FIND_PCI_CLASS_CODE:
        cmp     ebp, 8                  ; READ_CONFIG_*
        jb      .not_READ_CONFIG
        cmp     ebp, 0x0A
        ja      .not_READ_CONFIG
        mov     eax, ebp
        mov     ah, bh
        mov     edx, edi
        mov     bh, bl
        mov     bl, dl
        call    pci_read_reg
        mov     ecx, eax
        xor     ah, ah                  ; SUCCESSFUL
        jmp     .return_abc
.not_READ_CONFIG:
        cmp     ebp, 0x0B               ; WRITE_CONFIG_*
        jb      .not_WRITE_CONFIG
        cmp     ebp, 0x0D
        ja      .not_WRITE_CONFIG
        lea     eax, [ebp+1]
        mov     ah, bh
        mov     edx, edi
        mov     bh, bl
        mov     bl, dl
        call    pci_write_reg
        xor     ah, ah                  ; SUCCESSFUL
        jmp     .return_abc
.not_WRITE_CONFIG:
.unsupported_func:
        mov     ah, 0x81                ; FUNC_NOT_SUPPORTED
.return:
        mov     dword[esp + 4 ], edi
        mov     dword[esp + 8], esi
.return_abcd:
        mov     dword[esp + 24], edx
.return_abc:
        mov     dword[esp + 28], ecx
.return_ab:
        mov     dword[esp + 20], ebx
.return_a:
        mov     dword[esp + 32], eax
        ret

PCI_VENDOR_ID           equ 0x00
PCI_CLASS_REVISION      equ 0x08
PCI_HEADER_TYPE         equ 0x0E
PCI_SUBSYSTEM_VENDOR_ID equ 0x2c
PCI_IRQ_LINE            equ 0x3C

proc pci_enum
        push    ebp
        mov     ebp, esp
        push    0
virtual at ebp-4
.devfn          db      ?
.bus            db      ?
end virtual
.loop:
        mov     ah, [.bus]
        mov     al, 2
        mov     bh, [.devfn]
        mov     bl, PCI_VENDOR_ID
        call    pci_read_reg
        cmp     eax, 0xFFFFFFFF
        jnz     .has_device
        test    byte [.devfn], 7
        jnz     .next_func
        jmp     .no_device
.has_device:
        push    eax
        movi    eax, sizeof.PCIDEV
        call    malloc
        pop     ecx
        test    eax, eax
        jz      .nomemory
        mov     edi, eax
        mov     [edi+PCIDEV.vid_did], ecx
        mov     edx, pcidev_list
        list_add_tail edi, edx
        mov     eax, dword [.devfn]
        mov     word [edi+PCIDEV.devfn], ax
        mov     bh, al
        mov     al, 2
        mov     bl, PCI_CLASS_REVISION
        call    pci_read_reg
        shr     eax, 8                                ;FIXME use byte mask
        mov     [edi+PCIDEV.class], eax

        mov     ah, [.bus]
        mov     bh, byte [.devfn]
        mov     al, 2
        mov     bl, PCI_SUBSYSTEM_VENDOR_ID
        call    pci_read_reg
        mov     [edi+PCIDEV.svid_sdid], eax

        mov     ah, [.bus]
        mov     al, 0
        mov     bh, [.devfn]
        mov     bl, PCI_IRQ_LINE
        call    pci_read_reg
        mov     [edi+PCIDEV.irq_line], al

        test    byte [.devfn], 7
        jnz     .next_func
        mov     ah, [.bus]
        mov     al, 0
        mov     bh, [.devfn]
        mov     bl, PCI_HEADER_TYPE
        call    pci_read_reg
        test    al, al
        js      .next_func
.no_device:
        or      byte [.devfn], 7
.next_func:
        inc     dword [.devfn]
        mov     ah, [.bus]
        cmp     ah, [BOOT_VARS+0x9021]
        jbe     .loop
.nomemory:
        leave
        ret
endp


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;internal functions
;ecx (bus << 8)|devfn
;edx register

align 4
pci_bus:
.conf1_index:
; dword CF8 = (0x80000000 | ((reg & 0xF00) << 16) | (bus << 16) | (devfn << 8) | (reg & 0xFC))
				push	edx
				mov		eax, edx                	; eax = reg
				shl		eax, 16                 	; eax = reg << 16
				shl   ecx, 8                  	; ecx = (bus << 16)|(devfn<<8)
				mov		al, dl                  	; eax = (reg << 16)|reg
				and		eax, 0x0F0000FC           ; eax = ((reg & 0xF00) << 16)|(reg & 0xFC)
				lea		eax, [0x80000000+eax+ecx]
				mov 	dx, 0xCF8
				out		dx, eax
				pop		edx
				xor		eax, eax
				ret

align 4
.conf2_index:
; byte CF8 = 0xF0 | (fn << 1)
; byte CFA = bus
				push	edx
				mov		eax, ecx                ; (bus << 8)|devfn
				and		al, 7            				; fn
				lea		eax, [0xF0+eax+eax]
				mov 	dx, 0xCF8
				out		dx, al
				mov		al, ch                  ; bus
				mov		dx, 0xCFA
				out   dx, al
				pop		edx
				xor		eax, eax
				ret

align 4
.conf1_read8:
				call  .conf1_index
				and 	dx, 3
				add		dx, 0xCFC
				in		al, dx
				ret

align 4
.conf1_read16:
				call  .conf1_index
				and 	dx, 2
				add		dx, 0xCFC
				in		ax, dx
				ret

align 4
.conf1_read32:
				call  .conf1_index
				mov  	dx, 0xCFC
				in		eax, dx
				ret

align 4
.conf1_write8:
				call  .conf1_index
				mov		eax, [esp+4]
				and 	dx, 3
				add		dx, 0xCFC
				out		dx, al
				ret   4

align 4
.conf1_write16:
				call  .conf1_index
				mov		eax, [esp+4]
				and 	dx, 2
				add		dx, 0xCFC
				out		dx, ax
				ret   4

align 4
.conf1_write32:
				call  .conf1_index
				mov		eax, [esp+4]
				mov		dx, 0xCFC
				out   dx, eax
				ret   4

align 4
.conf2_read8:
; in (0xC000 | (dev << 8) | reg)
				call  .conf2_index
				and		ecx, 0xF1               ;ecx = dev << 3
				shl		ecx, 5                  ;ecx = dev << 8
				lea   edx, [0xC000+edx+ecx]
				in		al, dx
				ret

align 4
.conf2_read16:
				call  .conf2_index
				and		ecx, 0xF1
				shl		ecx, 5
				lea   edx, [0xC000+edx+ecx]
				in		ax, dx
				ret

align 4
.conf2_read32:
				call  .conf2_index
				and		ecx, 0xF1
				shl		ecx, 5
				lea   edx, [0xC000+edx+ecx]
				in		eax, dx
				ret


PCI_R8	equ 0
PCI_R16	equ 4
PCI_R32 equ 8

PCI_W8	equ 12
PCI_W16	equ 16
PCI_W32 equ 20


align 4
pci_conf1_rw:
;internal function
;eax accessor
;ecx (bus << 8)|devfn

.val      equ esp+4

; dword CF8 = (0x80000000 | ((reg & 0xF00) << 16) | (bus << 16) | (devfn << 8) | (reg & 0xFC))

				pushfd
				cli

				push  edx
				push  eax
				mov		eax, edx                	; eax = reg
				shl		eax, 16                 	; eax = reg << 16
				shl   ecx, 8                  	; ecx = (bus << 16)|(devfn<<8)
				mov		al, dl                  	; eax = (reg << 16)|reg
				and		eax, 0x0F0000FC           ; eax = ((reg & 0xF00) << 16)|(reg & 0xFC)
				lea		eax, [0x80000000+eax+ecx]
				mov 	dx, 0xCF8
				out		dx, eax
				pop		eax
				pop   edx
				jmp   dword [.fntab+eax]
.r32:
				mov  	dx, 0xCFC
				in		eax, dx
.rdone:
				popfd
				ret
.r16:
				and 	dx, 2
				add		dx, 0xCFC
				in		al, dx
				jmp   .rdone
.r8:
				and 	dx, 3
				add		dx, 0xCFC
				in		al, dx
				jmp   .rdone
.w32:
				mov		eax, [esp+8]
				mov		dx, 0xCFC
				out   dx, eax
.wdone:
				popfd
				ret   4
.w16:
				mov		eax, [esp+8]
				and 	dx, 2
				add		dx, 0xCFC
				out		dx, ax
				jmp   .wdone
.w8:
				mov		eax, [esp+8]
				and 	dx, 3
				add		dx, 0xCFC
				out		dx, al
				jmp   .wdone

align 4
.fntab:
				dd .r8
				dd .r16
				dd .r32
				dd .w8
				dd .w16
				dd .w32

align 4
pci_fn_rw	dd pci_conf1_rw

;proc pci_bus_read8   fastcall, busaddr:dword, reg:dword
;proc pci_bus_read16  fastcall, busaddr:dword, reg:dword
;proc pci_bus_read32  fastcall, busaddr:dword, reg:dword

align 4
pci_bus_read8:
				xor	eax, eax
				jmp	dword [pci_fn_rw]

align 4
pci_bus_read16:
				mov		eax, PCI_R16
				jmp	dword [pci_fn_rw]

align 4
pci_bus_read32:
				mov		eax, PCI_R32
				jmp	dword [pci_fn_rw]

;proc pci_bus_write8  fastcall, busaddr:dword, reg:dword, val: dword
;proc pci_bus_write16 fastcall, busaddr:dword, reg:dword, val: dword
;proc pci_bus_write32 fastcall, busaddr:dword, reg:dword, val: dword

align 4
pci_bus_write8:
				mov	  eax, PCI_W8
				jmp	dword [pci_fn_rw]

align 4
pci_bus_write16:
				mov		eax, PCI_W16
				jmp	dword [pci_fn_rw]

align 4
pci_bus_write32:
				mov		eax, PCI_W32
				jmp	dword [pci_fn_rw]

;deprecated proc pci_read8   stdcall, bus:dword, devfn:dword, reg:dword
;deprecated proc pci_read16  stdcall, bus:dword, devfn:dword, reg:dword
;deprecated proc pci_read32  stdcall, bus:dword, devfn:dword, reg:dword

align 4
pci_read8:
.bus			equ esp+4
.devfn 		equ esp+8
.pci_reg 	equ esp+12
.val      equ esp+16

				movzx ecx, byte [.devfn]
				mov   ch, [.bus]
				movzx edx, word [.pci_reg]
				call	pci_bus_read8
				ret 	12

align 4
pci_read16:
.bus			equ esp+4
.devfn 		equ esp+8
.pci_reg 	equ esp+12
.val      equ esp+16

				movzx ecx, byte [.devfn]
				mov   ch, [.bus]
				movzx edx, word [.pci_reg]
				call	pci_bus_read16
				ret 	12

align 4
pci_read32:
.bus			equ esp+4
.devfn 		equ esp+8
.pci_reg 	equ esp+12
.val      equ esp+16

				movzx ecx, byte [.devfn]
				mov   ch, [.bus]
				movzx edx, word [.pci_reg]
				call	pci_bus_read32
				ret 	12

;deprecated proc pci_write8  stdcall, bus:dword, devfn:dword, reg:dword, val:dword
;deprecated proc pci_write16 stdcall, bus:dword, devfn:dword, reg:dword, val:dword
;deprecated proc pci_write32 stdcall, bus:dword, devfn:dword, reg:dword, val:dword

align 4
pci_write8:
.bus			equ esp+4
.devfn 		equ esp+8
.pci_reg 	equ esp+12
.val      equ esp+16

				movzx ecx, byte [.devfn]
				mov   ch, [.bus]
				movzx edx, word [.pci_reg]
				push	dword [esp+16]
				call	pci_bus_write8
				ret 	16

align 4
pci_write16:
.bus			equ esp+4
.devfn 		equ esp+8
.pci_reg 	equ esp+12
.val      equ esp+16

				movzx ecx, byte [.devfn]
				mov   ch, [.bus]
				movzx edx, word [.pci_reg]
				push	dword [esp+16]
				call	pci_bus_write16
				ret 	16

align 4
pci_write32:
.bus			equ esp+4
.devfn 		equ esp+8
.pci_reg 	equ esp+12
.val      equ esp+16

				movzx ecx, byte [.devfn]
				mov   ch, [.bus]
				movzx edx, word [.pci_reg]
				push	dword [esp+16]
				call	pci_bus_write32
				ret 	16



