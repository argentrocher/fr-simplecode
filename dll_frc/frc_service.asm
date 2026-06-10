ASM64CPP
//blob

include defaut:

global DllMain :                        ; declaration du nom du point d'entrer fichier (de base main )

section .rsrc
version_simple "1.0.0.0" "frc_service.dll" "fr-simplecode, lié au compte Google argentropcher"       ; version | nom de exe | copyright
manifest_simple "frc_service.dll" "1.0.0.0" false false false false   ; nom de exe | version | dpiAware | perMonitorV2 | requireAdmin | commonV6 

; manifest_xml "contenu"  ; fait le manifest complet
; version_full fileVersion | productVersion | companyName | fileDescription | productName | originalFilename | copyright

section .data

utf16 msgutf16 "hello\n"
ascii msg "hello !\n"
ascii msg2 "hello 2 !\n"
buffer xxx 256

ascii msg_fin "fin !\n"

ascii give "dll message !\n"

; version
little version_sys 0x00000001

;pour view
buffer affiche_buf 20
ascii end_line "\n"
ascii _rsp "rsp:\t"
ascii _rax "rax:\t"
ascii _rcx "rcx:\t"
ascii _rdx "rdx:\t"
ascii _rbx "rbx:\t"
ascii _rsi "rsi:\t"
ascii _rdi "rdi:\t"
ascii _r8 "r8:\t"
ascii _r9 "r9:\t"
ascii _r10 "r10:\t"
ascii _r11 "r11:\t"
ascii _r12 "r12:\t"
ascii _r13 "r13:\t"
ascii _r14 "r14:\t"
ascii _r15 "r15:\t"
ascii _xmm0 "xmm0:\t"
ascii _xmm1 "xmm1:\t"

;pour parse_dll_arg/dll_service (tampons qui stocke tout les argument pointeur)
buffer address_table 184  ;soit dll, function, return type, (type, arg)*10 max  (argument de taille variable car malloc)
ascii separator ";" ;séparateur recherche
ascii _char "char"
ascii _int "int"
ascii _bool "bool"
ascii _double "double"
ascii _addr "addr"
ascii _pointor "pointor"
ascii _hex "hex"
ascii _struct "struct"
ascii _true "true"
ascii _false "false"
utf16 error_mem "erreur : allocation mémoire !\n"
utf16 error_arg_enough "erreur : arguments fournit inférieur à 2 (dll et fonction obligatoire) !\n"
utf16 error_arg_excessively "erreur : arguments fournit supérieur à 10 !\n"
utf16 error_load_dll "erreur : la dll n'a pas pu être chargée (argument 1) !\n"
utf16 error_load_function "erreur : la fonction liée à la dll fournit est introuvable !\n"
utf16 error_arg_invalid "erreur : arguments fournit invalide (doit être type;contenu) !\n"
utf16 error_arg_obj "erreur : arguments incorrect dans contenu !\n"
buffer return_buffer 10000 ;10000 charactères ascii max de retour
;pour dll_service_mem et free_dll_mem et verify_dll_mem
buffer mem_dll_buffer 408  ;soit 50 emplacements + 1 emplacement compteur dont verify_dll_mem renvoie 50-occupé

ascii debut "start\n"
ascii string "Impossible de récupérer la fenêtre.\n"

section .edata

;test non garanti
extern print msvcrt.printf
local give_buffer give
local test printfunc2 
local inverse_bit call_inverse_bit
local string_replace
local tdll testdll
;dll de fr-simplecode
local version
local view
local null
local dll_return
local dll_service
local dll_service_mem
local verify_dll_mem
local free_dll_mem

section .idata

kernel32.dll ExitProcess LoadLibraryA GetProcAddress FreeLibrary
msvcrt.dll printf wprintf malloc free

kernel32.dll GetConsoleWindow
msvcrt.dll puts
user32.dll SetWindowPos

section .text

proc hello
		sub rsp, 28h
        mov rax, $printfunc
        call rax
        mov rax, $exit
        call rax
		add rsp, 28h
        ret
end hello

proc printfunc
        sub rsp, 28h

        mov rcx, $msg
        mov rax, $printf
        mov rax, [rax]
        call rax

        add rsp, 28h
        ret
end printfunc

proc exit
        sub rsp, 28h
        xor ecx, ecx
        mov rax, $ExitProcess
        mov rax, [rax]
        call rax
        add rsp, 28h
        ret
end exit

;ok version relative

proc hello2
		sub rsp, 28h
        lea rax, [rip+@printfunc2]
        call rax
        mov rax, $exit
        call rax
		add rsp, 28h
		mov eax, 1
        ret
end hello2

proc printfunc2
        sub rsp, 28h

        lea rcx, [rip+@msg2]
        mov rax, [rip+@printf]
        call rax

        add rsp, 28h
        ret
end printfunc2

proc exit2
        sub rsp, 28h
        xor ecx, ecx
        lea rax, [rip+@ExitProcess]
        mov rax, [rax]
        call rax
        add rsp, 28h
        ret
end exit2

; version relative test in dll

;code lancer par défaut en dll en mettant ce code a entrypoint
proc DllMain
    mov eax, 1
    ret
end DllMain

;(int)->(int)
proc inverse_bit
    sub rsp, 28h
    mov [rsp+20h], rdx
    mov [rsp+10h], rbx

    mov rax, rcx   ; Copie RCX dans RAX

    ; Échange octets 0 et 1
    mov ebx, eax
    and ebx, 0x0000FFFF
    shl ebx, 8
    mov edx, eax
    and edx, 0x00FF0000
    shr edx, 8
    and eax, 0xFF000000
    or eax, ebx
    or eax, edx

    ; Échange octets 2 et 3
    mov ebx, eax
    and ebx, 0xFFFF0000
    shl ebx, 8
    mov edx, eax
    and edx, 0xFF000000
    shr edx, 8
    and eax, 0x0000FFFF
    or eax, ebx
    or eax, edx

    ; Échange octets 4 et 5 (partie haute de RAX)
    mov ebx, eax
    shr ebx, 16
    and ebx, 0x0000FFFF
    shl ebx, 8
    mov edx, eax
    shr edx, 24
    and edx, 0x000000FF
    shl edx, 16
    or eax, ebx
    or eax, edx

    ; Échange octets 6 et 7 (partie très haute de RAX)
    mov ebx, eax
    shr ebx, 24
    and ebx, 0x000000FF
    shl ebx, 8
    mov edx, eax
    shr edx, 32
    and edx, 0x000000FF
    shl edx, 24
    or eax, ebx
    or eax, edx

    ; Retour du résultat
    mov rcx, rax

    mov rdx, [rsp+20h]
    mov rbx, [rsp+10h]
    add rsp, 28h
    ret
end inverse_bit

proc call_inverse_bit
	sub rsp, 38h
    mov [rsp+20h], rdx
    mov [rsp+30h], rbx
	lea rax, [rip+@inverse_bit]
	call rax
	mov rdx, [rsp+20h]
    mov rbx, [rsp+30h]
    add rsp, 38h
	mov eax, 1
    ret
end call_inverse_bit

;(char;char;char)->(char)
proc string_replace
	sub rsp, 38h
    mov [rsp+20h], rbx
    mov [rsp+28h], rsi
    mov [rsp+30h], rdi
	
	lea rax, [rip+@asm_replace]
    call rax
	
	mov rbx, [rsp+20h]
    mov rsi, [rsp+28h]
    mov rdi, [rsp+30h]
    add rsp, 38h
    ret
end string_replace

;(void)->(int)
proc version
	xor rax, rax 
	lea rcx, [rip+@version_sys]
	mov eax, [rcx]
	ret
end version

;(mutliple)->(mutliple)
proc null
	mov rax, rcx
	ret
end null

;(mutliple)->(mutliple)
proc view
	sub rsp, 98h
	movdqu [rsp+88h], xmm0
	movdqu [rsp+78h], xmm1
	mov [rsp+70h], r8
	mov [rsp+68h], r9
	mov [rsp+60h], r10
	mov [rsp+58h], r11
	mov [rsp+50h], r12
	mov [rsp+48h], r13
	mov [rsp+40h], r14
	mov [rsp+38h], r15
	mov [rsp+30h], rcx
	mov [rsp+28h], rdx
    mov [rsp+20h], rbx
    mov [rsp+18h], rsi
    mov [rsp+10h], rdi
	mov [rsp+8h], rax
	
	lea rcx, [rip+@_rsp]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, rsp
	add rcx, 98h
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@end_line]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@_rax]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, [rsp+8h]
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@end_line]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@_rcx]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, [rsp+30h]
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@end_line]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@_rdx]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, [rsp+28h]
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@end_line]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@_rbx]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, [rsp+20h]
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@end_line]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@_rsi]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, [rsp+18h]
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@end_line]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@_rdi]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, [rsp+10h]
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@end_line]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@_r8]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, [rsp+70h]
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@end_line]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@_r9]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, [rsp+68h]
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@end_line]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@_r10]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, [rsp+60h]
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@end_line]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@_r11]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, [rsp+58h]
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@end_line]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@_r12]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, [rsp+50h]
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@end_line]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@_r13]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, [rsp+48h]
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@end_line]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@_r14]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, [rsp+40h]
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@end_line]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@_r15]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, [rsp+38h]
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@end_line]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@_xmm0]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, [rsp+88h]
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, [rsp+90h]
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@end_line]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@_xmm1]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, [rsp+78h]
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	mov rcx, [rsp+80h]
	lea rax, [rip+@asm_addr_ascii]
	mov r8, 0
	lea rdx, [rip+@affiche_buf]
	call rax
	
	lea rcx, [rip+@affiche_buf]
	mov rax, [rip+@printf]
    call rax
	
	lea rcx, [rip+@end_line]
	mov rax, [rip+@printf]
    call rax
	
	movdqu xmm0, [rsp+88h]
	movdqu xmm1, [rsp+78h]
	mov r8, [rsp+70h]
	mov r9, [rsp+68h]
	mov rcx, [rsp+30h]
	mov rdx, [rsp+28h]
	mov rbx, [rsp+20h]
    ;mov rsi, [rsp+18h]
    ;mov rdi, [rsp+10h]
    add rsp, 98h
	
	mov rax, rcx
	
    ret
end view

;rcx pointeur, rax sortie hex
proc hex_addr_hex
	sub rsp, 28h
    mov [rsp+20h], rbx
    mov [rsp+18h], rsi
    mov [rsp+10h], rdi
    mov [rsp+8h], rcx

    mov rsi, rcx

    lea rax, [rip+@asm_strlen]
    call rax
	mov rbx, rax

    ; Vérifier si la longueur > 16 (64 bits max)
    cmp rax, 16
    ja .error

    xor rax, rax
    ; RSI = pointeur sur la chaîne, RDI = index
    xor rdi, rdi

	.convert_loop:
    ; Si index >= longueur, on a fini
    cmp rdi, rbx
    jae .done

    ; Charger le caractère actuel dans RCX (étendu sur 64 bits)
    mov cl, byte ptr [rsi + rdi]

    ; Vérifier si c'est un digit (0-9)
    cmp cl, '0'
    jb .invalid_char
    cmp cl, '9'
    jbe .is_digit

    ; Vérifier si c'est une lettre (A-F)
    cmp cl, 'A'
    jb .check_lowercase
    cmp cl, 'F'
    jbe .is_uppercase

	.check_lowercase:
    cmp cl, 'a'
    jb .invalid_char
    cmp cl, 'f'
    ja .invalid_char
    sub cl, 'a' - 10       ; Convertir a-f en 10-15
    jmp .got_value

	.is_uppercase:
    sub cl, 'A' - 10       ; Convertir A-F en 10-15
    jmp .got_value

	.is_digit:
    ; Convertir 0-9 en valeur (0-9)
    sub cl, '0'

	.got_value:
    ; Garder seulement les 4 bits bas (valeur 0-15)
    and rcx, 0Fh
    ; Déplacer RAX de 4 bits et ajouter la nouvelle valeur
    shl rax, 4
    or rax, rcx
    ; Passer au caractère suivant
    inc rdi
    jmp .convert_loop

	.invalid_char:
	.error:
    ; Erreur : RCX = 1
    mov rcx, 1
    jmp .cleanup

	.done:
    ; Succès : RCX = 0
    xor rcx, rcx

	.cleanup:
    ; Restaurer RBX, RSI, RDI (RCX contient le code d'erreur)
    mov rbx, [rsp+20h]
    mov rsi, [rsp+18h]
    mov rdi, [rsp+10h]
    add rsp, 28h
    ret
end hex_addr_hex

proc hex_char_hex
	sub rsp, 28h
    mov [rsp+20h], rbx
    mov [rsp+18h], rsi
    mov [rsp+10h], rdi
    

    mov rsi, rcx
	
	lea rax, [rip+@asm_strlen]
    call rax
	cmp rax, 0
	je .error
	
    ; RSI = pointeur sur la chaîne, RDI = index
    xor rdi, rdi
	
	xor rax, rax
	mov rdx, 0 ; savoir si on écrit dans le haut ou le bas (0 ou 1 de rax)
	mov rbx, 0 ; savoir sur quel emplacement écrire rax chargé

	.convert_loop:
    ; Charger le caractère actuel dans RCX (étendu sur 64 bits)
    mov cl, byte ptr [rsi + rdi]

    ; Vérifier si c'est un digit (0-9)
    cmp cl, '0'
    jb .invalid_char
    cmp cl, '9'
    jbe .is_digit

    ; Vérifier si c'est une lettre (A-F)
    cmp cl, 'A'
    jb .check_lowercase
    cmp cl, 'F'
    jbe .is_uppercase

	.check_lowercase:
    cmp cl, 'a'
    jb .invalid_char
    cmp cl, 'f'
    ja .invalid_char
    sub cl, 'a' - 10       ; Convertir a-f en 10-15
    jmp .got_value

	.is_uppercase:
    sub cl, 'A' - 10       ; Convertir A-F en 10-15
    jmp .got_value

	.is_digit:
    ; Convertir 0-9 en valeur (0-9)
    sub cl, '0'

	.got_value:
	; Garder seulement les 4 bits bas (valeur 0-15)
    and rcx, 0Fh
    ; Déplacer RAX de 4 bits et ajouter la nouvelle valeur
    shl rax, 4
    or rax, rcx
	
	cmp rdx, 0
	je .above
	
	;below on rend rax dans la chaine par 1 octet
	mov rdx, 0
	mov byte ptr [rsi + rbx], al
	xor rax, rax
	inc rbx
	inc rdi
	jmp .convert_loop
	
	.above:
	mov rdx, 1
    ; Passer au caractère suivant
    inc rdi
    jmp .convert_loop

	.error:
    ; Erreur : RCX = 1
    mov rcx, 1
    jmp .cleanup

	.drop_end:
	; finir si nombre impaire de charactère
	mov rdx, 0
	shl rax, 4
	mov byte ptr [rsi + rbx], al
	xor rax, rax
	inc rbx

	.invalid_char:
	cmp rdx, 1
	je .drop_end
	; ecrire le dernier octet en 00
	xor rax, rax
	mov byte ptr [rsi + rbx], al
	
    ; Succès : RCX = 0
    xor rcx, rcx
	
	mov rax, rsi ; donne l'adresse de rcx (car modifier le contenu)

	.cleanup:
    ; Restaurer RBX, RSI, RDI (RCX contient le code d'erreur)
    mov rbx, [rsp+20h]
    mov rsi, [rsp+18h]
    mov rdi, [rsp+10h]
    add rsp, 28h
    ret
end hex_char_hex

; rcx = adresse source (octets à convertir)
; rdx = adresse de sortie (chaîne de caractères)
; r8  = nombre d'octets à convertir
proc char_hex_char
	sub rsp, 28h
    mov [rsp+20h], rbx
    mov [rsp+18h], rsi
    mov [rsp+10h], rdi

    mov rsi, rcx    ; rsi = source
    mov rdi, rdx    ; rdi = destination
    xor rbx, rbx    ; rbx = compteur d'octets traités

.convert_loop:
    cmp rbx, r8
    jge .done       ; Si tous les octets sont traités, on termine

    ; Charger l'octet actuel dans al
    mov al, byte ptr [rsi + rbx]

    ; Convertir les 4 bits hauts en caractère hexadécimal
    mov cl, al
    shr cl, 4       ; Déplacer les 4 bits hauts vers le bas
    call .nibble_to_hex
    mov byte ptr [rdi], al
    inc rdi

    ; Convertir les 4 bits bas en caractère hexadécimal
	mov al, byte ptr [rsi + rbx]
    mov cl, al
    and cl, 0Fh     ; Garder seulement les 4 bits bas
    call .nibble_to_hex
    mov byte ptr [rdi], al
    inc rdi

    ; Passer à l'octet suivant
    inc rbx
    jmp .convert_loop

	.nibble_to_hex:
    ; Entrée : cl = valeur 4 bits (0-15)
    ; Sortie : al = caractère hexadécimal (0-9, A-F)
    cmp cl, 9
    jbe .is_digit
    add cl, 'A' - 10		; Convertir 10-15 en A-F
    jmp .store_char
	.is_digit:
    add cl, '0'             ; Convertir 0-9 en '0'-'9'
	.store_char:
    mov al, cl
    ret

	.done:
    ; Ajouter un terminateur nul à la fin de la chaîne
    mov byte ptr [rdi], 0

    ; Retourner l'adresse de la chaîne de sortie dans rax
    mov rax, rdx

	.cleanup:
    ; Restaurer les registres
    mov rbx, [rsp+20h]
    mov rsi, [rsp+18h]
    mov rdi, [rsp+10h]
    add rsp, 28h
    ret
end char_hex_char

;renvoie le contenu du tampon de retour ou rien si vide
;(char)->(mutliple)
proc dll_return
	sub rsp, 48h
	mov [rsp+20h], rbx
    mov [rsp+28h], rsi
    mov [rsp+30h], rdi
	mov [rsp+38h], rcx
	
	mov rbx, rcx
	
	;hex et char sont identique puisque déjà parser
	lea rcx, [rip+@_char]
	lea rax, [rip+@asm_strlen]
	call rax
	
	mov rcx, rbx
	lea rdx, [rip+@_char]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .char
	
	lea rcx, [rip+@_hex]
	lea rax, [rip+@asm_strlen]
	call rax
	mov rdi, rax
	
	mov rcx, rbx
	lea rdx, [rip+@_hex]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .char
	
	lea rcx, [rip+@_double]
	lea rax, [rip+@asm_strlen]
	call rax
	mov rdi, rax
	
	mov rcx, rbx
	lea rdx, [rip+@_double]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .double
	
	; par défaut, c'est la valeur de rax comme type donc on copie les 8 octets sur rax
	mov rax, [rip+@return_buffer]
	jmp .end
	
	.double:
	lea rax, [rip+@return_buffer]
	mov rdx, [rax]
	add rax, 8
	mov rcx, [rax]
	movq xmm0, rdx
	pslldq  xmm0, 8
	movq xmm0, rcx
	jmp .end
	
	.char:
	lea rax, [rip+@return_buffer]
	
	.end:
	mov rbx, [rsp+20h]
    mov rsi, [rsp+28h]
    mov rdi, [rsp+30h]
	mov rcx, [rsp+38h]
	add rsp, 48h
	ret
end dll_return

;parse la chaine d'argument reçu sur le pointeur rcx
proc parse_dll_arg
	sub rsp, 38h
	mov [rsp+20h], rbx
    mov [rsp+28h], rsi
    mov [rsp+30h], rdi
	
	test rcx, rcx
	jz .error
	
	;pointeur pour écrire les arguments au bonne endroit
	mov rdi, 0
	;pointeur de début de chaine rcx
	mov rsi, rcx
	
	.next_arg:
	mov rcx, rsi
	lea rdx, [rip+@separator]
	lea rax, [rip+@asm_strstr]
	call rax
	test rcx, rcx
	jz .fin_arg
	
	cmp rdi, 183
	ja .error_max
	
	mov rbx, rax
	mov rcx, rax  ; allocation de la taille de rax (position du ;)
	inc rcx       ; +1
	mov rax, [rip+@malloc]
	call rax
	test rax, rax
	jz .error
	
	mov rcx, rsi
	mov rdx, rax
	mov r8, rbx
	
	add rsi, rbx
	inc rsi       ; passer le séparteur pour arg suivant
	mov rbx, rdx
	
	lea rax, [rip+@asm_copy]
	call rax
	
	lea rax, [rip+@address_table]
	add rax, rdi
	mov [rax], rbx  ; écriture de l'adresse du texte copier dans address_table
	
	add rdi, 8
	jmp .next_arg
	
	.fin_arg:
	cmp rdi, 16
	jb .error_min     ; doit contenir aumoins la dll et la fonction et type return donc 3 adresses donc 3 arguments soit 24 octets (dll peut être après)
	
	mov rcx, rsi
	lea rax, [rip+@asm_strlen]
	call rax
	test rax, rax
	jz .verify
	
	cmp rdi, 183
	ja .error_max
	
	mov rbx, rax
	mov rcx, rax  ; allocation de la taille de rax (position du ;)
	inc rcx       ; +1
	mov rax, [rip+@malloc]
	call rax
	test rax, rax
	jz .error
	
	mov rcx, rsi
	mov rdx, rax
	mov r8, rbx
	mov rbx, rdx
	lea rax, [rip+@asm_copy]
	call rax
	
	lea rax, [rip+@address_table]
	add rax, rdi
	mov [rax], rbx  ; écriture de l'adresse du texte copier dans address_table
	
	add rdi, 8
	
	mov rdx, 1
	jmp .end
	
	.verify:
	cmp rdi, 24   ; pas trois arguments si len du troisième est null
	jb .error_min
	mov rdx, 1
	jmp .end
	
	.error_max:
	mov rdx, 2    ; la taille du tampon qui contient les adresses malloc est d'une taille de 176 octets si trop d'argument (plus de 12 donc 10 arguments + 10 type + nom de dll + nom de foncton)
	jmp .end
	
	.error_min:
	mov rdx, 3    ; nombre d'argument trop faible min 2 
	jmp .end
	
	.error:
	mov rdx, 0
	
	.end:
	mov rsi, rdx
	
	mov rax, rdi
	xor rdx, rdx
	mov rcx, 8
	div rcx
	; rax = nombre d'argument
	
	mov rdx, rsi  ; resultat 0 error, 1 ok, 2 nombre d'argument dépassé par rapport au tampon max 176 octets, 3 arguments inférieur à deux
	
	mov rbx, [rsp+20h]
    mov rsi, [rsp+28h]
    mov rdi, [rsp+30h]
	add rsp, 38h
	ret
end parse_dll_arg

proc debug_parse_dll_arg
	sub rsp, 38h
	mov [rsp+20h], rbx
    mov [rsp+28h], rsi
    mov [rsp+30h], rdi
	
	mov rsi, 1
	mov rbx, 0
	mov rdi, rcx
	.affiche:
	cmp rsi, rdi
	ja .end
	
	lea rcx, [rip+@address_table]
	add rcx, rbx
	mov rcx, [rcx]
	mov rax, [rip+@printf]
	call rax
	
	lea rcx, [rip+@end_line]
	mov rax, [rip+@printf]
    call rax
	
	inc rsi
	add rbx, 8
	jmp .affiche
	
	.end:
	
	mov rbx, [rsp+20h]
    mov rsi, [rsp+28h]
    mov rdi, [rsp+30h]
	add rsp, 38h
	ret
end debug_parse_dll_arg

proc free_dll_parse_arg
	sub rsp, 38h
	mov [rsp+20h], rbx
    mov [rsp+28h], rsi
    mov [rsp+30h], rdi
	
	mov rsi, 1
	mov rbx, 0
	mov rdi, rcx
	.other:
	cmp rsi, rdi
	ja .end
	
	lea rcx, [rip+@address_table]
	add rcx, rbx
	mov rcx, [rcx]
	mov rax, [rip+@free]
	call rax
	
	inc rsi
	add rbx, 8
	jmp .other
	
	.end:
	
	mov rbx, [rsp+20h]
    mov rsi, [rsp+28h]
    mov rdi, [rsp+30h]
	add rsp, 38h
	ret
end free_dll_parse_arg

; fonction qui renvoie 1 sur rax si l'argument trois est pointeur
proc parse_type_access
	sub rsp, 28h
	mov [rsp+20h], rbx
    mov [rsp+18h], rsi
    mov [rsp+10h], rdi
	
	lea rbx, [rip+@address_table]
	add rbx, 16
	mov rbx, [rbx]
	test rbx, rbx
	jz .standard
	
	lea rcx, [rip+@_pointor]
	lea rax, [rip+@asm_strlen]
	call rax
	
	mov rcx, rbx
	lea rdx, [rip+@_pointor]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .pointor
	jmp .standard
	
	.pointor:
	mov rax, 1
	jmp .end
	.standard:
	xor rax, rax
	.end:
	mov rbx, [rsp+20h]
    mov rsi, [rsp+18h]
    mov rdi, [rsp+10h]
	add rsp, 28h
	ret
end parse_type_access

;fonction qui modifie le return sur le tempon return_buffer si c'est un char (ne fait rien si pas écrit char après pointor ou juste char)
; rcx = entrée argument, rax = sortie argument
proc return_type
	sub rsp, 38h
	mov [rsp+20h], rbx
    mov [rsp+18h], rsi
    mov [rsp+10h], rdi
	mov [rsp+28h], rcx
	
	mov rax, rcx
	
	lea rbx, [rip+@address_table]
	add rbx, 16
	mov rbx, [rbx]
	test rbx, rbx
	jz .end
	
	lea rcx, [rip+@_pointor]
	lea rax, [rip+@asm_strlen]
	call rax
	mov rdi, rax
	
	mov rcx, rbx
	lea rdx, [rip+@_pointor]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .pointor
	jmp .standard
	
	.pointor:
	add rbx, rdi
	
	.standard:
	lea rcx, [rip+@_char]
	lea rax, [rip+@asm_strlen]
	call rax
	
	mov rcx, rbx
	lea rdx, [rip+@_char]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .modify
	
	lea rcx, [rip+@_hex]
	lea rax, [rip+@asm_strlen]
	call rax
	mov rdi, rax
	
	mov rcx, rbx
	lea rdx, [rip+@_hex]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .modify_hex
	
	lea rcx, [rip+@_addr]
	lea rax, [rip+@asm_strlen]
	call rax
	mov rdi, rax
	
	mov rcx, rbx
	lea rdx, [rip+@_addr]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .modify_addr
	
	lea rcx, [rip+@_double]
	lea rax, [rip+@asm_strlen]
	call rax
	mov rdi, rax
	
	mov rcx, rbx
	lea rdx, [rip+@_double]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .modify_double
	
	; par défaut, on enregistre la valeur de rax dans le tampon return_buffer et on ne change rien
	lea rax, [rip+@return_buffer]
	mov rcx, [rsp+28h]
	mov [rax], rcx
	xor rcx, rcx
	add rax, 8	; vider les 8 octets suivants par sécurité si il y avait déjà autre chose (le tampon fait 10000 octets donc grande marge)
	mov [rax], rcx
	mov rax, [rsp+28h]
	jmp .end
	
	.modify_double:
	lea rax, [rip+@return_buffer]
	movq rcx, xmm0
	psrldq  xmm0, 8
	movq rdx, xmm0
	pslldq xmm0, 8
	movq xmm0, rcx
	mov [rax],rdx
	add rax, 8
	mov [rax], rcx
	xor rcx, rcx
	add rax, 8	; vider les 8 octets suivants par sécurité si il y avait déjà autre chose (le tampon fait 10000 octets donc grande marge)
	mov [rax], rcx
	mov rax, [rsp+28h]
	jmp .end
	
	.modify_addr:
	lea rsi, [rip+@return_buffer]
	add rsi, 100	; emplacement loin pour copier le tampon RCX
	mov rcx, [rsp+28h]
	bswap rcx		; inverse les octets car sinon on voit l'adresse dans le sens oposé à celui fournit
	mov [rsi], rcx
	mov rcx, rsi
	lea rdx, [rip+@return_buffer]
	mov r8, 8		; 8 octets a copier
	lea rax, [rip+@char_hex_char]
	call rax
	xor rcx, rcx	; nétoyage
	mov [rsi], rcx
	jmp .end_modify
	
	.modify_hex:
	mov rcx, rbx
	add rcx, rdi
	xor r8, r8
	lea rax, [rip+@asm_addr_ascii_int]
	call rax
	cmp rax, 10000
	ja .limit_hex

	.continue_hex:
	mov rcx, [rsp+28h]
	lea rdx, [rip+@return_buffer]
	mov r8, rax ;longueur à copier
	lea rax, [rip+@char_hex_char]
	call rax
	jmp .end_modify
	
	.limit_hex:
	mov rax, 10000
	jmp .continue_hex
	
	.modify:
	mov rcx, [rsp+28h]
	lea rdx, [rip+@return_buffer]
	mov r8, 10000 ; taille de return_buffer max
	lea rax, [rip+@asm_copy]
	call rax
	
	.end_modify:
	lea rax, [rip+@return_buffer]
	
	.end:
	mov rbx, [rsp+20h]
    mov rsi, [rsp+18h]
    mov rdi, [rsp+10h]
	mov rcx, [rsp+28h]
	add rsp, 38h
	ret
end return_type

proc transform_arg
	sub rsp, 0xA8
	movdqu [rsp+88h], xmm0
	movdqu [rsp+78h], xmm1
	mov [rsp+70h], r8
	mov [rsp+68h], r9
	movdqu [rsp+58h], xmm2
	movdqu [rsp+48h], xmm3
	mov [rsp+40h], r10
	mov [rsp+38h], r11
	mov [rsp+30h], rcx
	mov [rsp+28h], rdx
    mov [rsp+20h], rbx
    mov [rsp+18h], rsi
    mov [rsp+10h], rdi
	
	lea rbx, [rip+@address_table]
	add rbx, r15
	mov [rsp+98h], rbx
	mov rbx, [rbx]
	
	xor r14, r14 ;tampon adresse qui contient ce qu'il y a a écrire comme arg 
	; test de type
	
	;pointor (ce n'est pas une fonction qui est appeler mais l'adresse d'un tampon), donc erreur d'office car type pour return
	lea rcx, [rip+@_pointor]
	lea rax, [rip+@asm_strlen]
	call rax
	
	mov rcx, rbx
	lea rdx, [rip+@_pointor]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .pointor
	
	;char
	lea rcx, [rip+@_char]
	lea rax, [rip+@asm_strlen]
	call rax
	
	mov rcx, rbx
	lea rdx, [rip+@_char]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .char
	
	;int
	lea rcx, [rip+@_int]
	lea rax, [rip+@asm_strlen]
	call rax
	
	mov rcx, rbx
	lea rdx, [rip+@_int]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .int
	
	;bool
	lea rcx, [rip+@_bool]
	lea rax, [rip+@asm_strlen]
	call rax
	
	mov rcx, rbx
	lea rdx, [rip+@_bool]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .bool
	
	;double
	lea rcx, [rip+@_double]
	lea rax, [rip+@asm_strlen]
	call rax
	
	mov rcx, rbx
	lea rdx, [rip+@_double]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .double
	
	;hex
	lea rcx, [rip+@_hex]
	lea rax, [rip+@asm_strlen]
	call rax
	
	mov rcx, rbx
	lea rdx, [rip+@_hex]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .hex
	
	;addr
	lea rcx, [rip+@_addr]
	lea rax, [rip+@asm_strlen]
	call rax
	
	mov rcx, rbx
	lea rdx, [rip+@_addr]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .addr
	
	;struct
	lea rcx, [rip+@_struct]
	lea rax, [rip+@asm_strlen]
	call rax
	
	mov rcx, rbx
	lea rdx, [rip+@_struct]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .struct
	
	.error
	
	.pointor:
	;cmp r15, 16
	;jne .error
	mov rax, 1
	jmp .end
	
	.char:
	mov rbx, [rsp+98h]
	add rbx, 8
	mov r14, [rbx]
	mov rax, 2
	jmp .end
	
	.bool:
	mov rbx, [rsp+98h]
	add rbx, 8
	mov rbx, [rbx]
	
	lea rcx, [rip+@_true]
	lea rax, [rip+@asm_strlen]
	call rax
	
	mov rcx, rbx
	lea rdx, [rip+@_true]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .true
	
	lea rcx, [rip+@_false]
	lea rax, [rip+@asm_strlen]
	call rax
	
	mov rcx, rbx
	lea rdx, [rip+@_false]
	mov r8, rax
	lea rax, [rip+@asm_strcmp]
	call rax
	cmp rax, 1
	je .false
	
	mov rcx, rbx
	xor r8, r8
	lea rax, [rip+@asm_addr_ascii_int]
	call rax
	cmp rax, 0
	ja .true
	
	.false:
	mov r14, 0
	jmp .bool_end
	
	.true:
	mov r14, 1
	
	.bool_end:
	mov rax, 2
	jmp .end
	
	.int:
	mov rbx, [rsp+98h]
	add rbx, 8
	mov rcx, [rbx]
	xor r8, r8
	lea rax, [rip+@asm_addr_ascii_int]
	call rax
	mov r14, rax
	mov rax, 2
	jmp .end
	
	.double:
	mov rbx, [rsp+98h]
	add rbx, 8
	mov rcx, [rbx]
	lea rax, [rip+@asm_addr_ascii_double]
	call rax
	movq r14, xmm0
	mov rax, 3
	jmp .end
	
	.addr:
	mov rbx, [rsp+98h]
	add rbx, 8
	mov rcx, [rbx]
	lea rax, [rip+@hex_addr_hex]
	call rax
	cmp rcx, 1
	je .error_other
	mov r14, rax
	mov rax, 2
	jmp .end
	
	.hex:
	mov rbx, [rsp+98h]
	add rbx, 8
	mov rcx, [rbx]
	lea rax, [rip+@hex_char_hex]
	call rax
	cmp rcx, 1
	je .error_other
	mov r14, rax
	mov rax, 2
	jmp .end
	
	.struct:
	
	mov rax, 2
	jmp .end
	
	.error_other:
	mov rax, 5
	jmp .end
	
	.error:
	mov rax, 0
	
	.end:
	
	movdqu xmm0, [rsp+88h]
	movdqu xmm1, [rsp+78h]
	mov r8, [rsp+70h]
	mov r9, [rsp+68h]
	movdqu xmm2, [rsp+58h]
	movdqu xmm3, [rsp+48h]
	mov r10, [rsp+40h]
	mov r11, [rsp+38h]
	mov rcx, [rsp+30h]
	mov rdx, [rsp+28h]
	mov rbx, [rsp+20h]
    mov rsi, [rsp+18h]
    mov rdi, [rsp+10h]
	
    add rsp, 0xA8
    ret
end transform_arg

proc dll_caller
	push rbp
	mov rbp,rsp
	sub rsp, 90h
	mov r10, rbx
    mov [rsp+18h], rsi
    mov [rsp+10h], rdi 
	
	mov rsi, rcx
	
	cmp rdi, 1
	je .call_get_addr
	
	mov rbx, rdx
	mov rdi, 3
	mov r15, 24
	
	mov rax, 32
	mov [rsp+8h], rax
	
	.boucle:
	cmp rdi, rbx
	jae .caller
	
	lea rax, [rip+@transform_arg]
	call rax
	cmp rax, 1
	jbe .error_arg_invalid
	test rax, rax
	jz .error_arg_invalid
	cmp rax, 5
	je .error_arg_obj
	cmp rax, 2
	je .type_addr
	cmp rax, 3
	je .type_xmm
	
	.type_addr:
	cmp r15, 24
	je ._rcx_
	cmp r15, 40
	je ._rdx_
	cmp r15, 56
	je ._r8_
	cmp r15, 72
	je ._r9_
	jmp ._stack_
	
	._rcx_:
	mov rcx, r14
	jmp .end_boucle
	
	._rdx_:
	mov rdx, r14
	jmp .end_boucle
	
	._r8_:
	mov r8, r14
	jmp .end_boucle
	
	._r9_:
	mov r9, r14
	jmp .end_boucle
	
	._stack_:
	mov rax, [rsp+8h]
	mov [rsp+rax], r14
	add rax, 8
	mov [rsp+8h], rax
	jmp .end_boucle
	
	.type_xmm:
	cmp r15, 24
	je ._xmm0_
	cmp r15, 40
	je ._xmm1_
	cmp r15, 56
	je ._xmm2_
	cmp r15, 72
	je ._xmm3_
	jmp ._stack_
	
	._xmm0_:
	movq xmm0, r14
	jmp .end_boucle
	
	._xmm1_:
	movq xmm1, r14
	jmp .end_boucle
	
	._xmm2_:
	movq xmm2, r14
	jmp .end_boucle
	
	._xmm3_:
	movq xmm3, r14
	;jmp .end_boucle
	
	.end_boucle:
	add r15, 16
	add rdi, 2
	jmp .boucle
	
	.call_get_addr:
	mov rax, rsi
	jmp .end
	
	.caller:
	mov rax, 16
	mov [rsp+8h], rax
	mov rax, rsi
	
	mov rbx, r10
	mov rsi, [rsp+18h]
	mov rdi, [rsp+10h]
	
	mov r10, 0
	mov [rsp+10h], r10
	mov [rsp+18h], r10
	;mov r10, [rsp+98h]
	;mov [rsp], r10
	
	;mov [rsp+10h], rbp
	;mov rbp, rsp
	;add rbp, 152  ; soit 98h donc taille de la fonction de pile
	call rax
	;mov rbp, [rsp+10h]
	
	mov r10, rbx
    mov [rsp+18h], rsi
    mov [rsp+10h], rdi 
	
	jmp .end
	
	.error_arg_obj:
	lea rcx, [rip+@error_arg_obj]
	mov rax, [rip+@wprintf]
	call rax
	jmp .end
	
	.error_arg_invalid:
	lea rcx, [rip+@error_arg_invalid]
	mov rax, [rip+@wprintf]
	call rax
	
	.end:
	mov rbx, r10
    mov rsi, [rsp+18h]
    mov rdi, [rsp+10h]
	add rsp, 90h
	pop rbp
	ret
end dll_caller

;(char)->(mutliple) 
proc dll_service
	sub rsp, 98h
	;movdqu [rsp+88h], xmm0
	;movdqu [rsp+78h], xmm1
	;mov [rsp+70h], r8
	;mov [rsp+68h], r9
	mov [rsp+60h], r10
	mov [rsp+58h], r11
	mov [rsp+50h], r12
	mov [rsp+48h], r13
	mov [rsp+40h], r14
	mov [rsp+38h], r15
	;mov [rsp+30h], rcx
	;mov [rsp+28h], rdx
    mov [rsp+20h], rbx
    mov [rsp+28h], rsi
    mov [rsp+30h], rdi
	;mov [rsp+8h], rax
	
	;parsing des arguments sur rcx ;
	lea rax, [rip+@parse_dll_arg]
	call rax
	
	test rdx, rdx ; error malloc ou autre
	jz .error_mem
	cmp rdx, 2 ; trop d'argument
	je .error_arg_excessively
	cmp rdx, 3 ; pas d'argument suffisant
	je .error_arg_enough
	test rax, rax  ; aucun argument (pas possible car code d'erreur)
	jz .end
	
	mov rbx, rax   ; sauvgarde du nombre d'argument dans rbx 
	
	
	;debugage
	;mov rcx, rax
	;lea rax, [rip+@debug_parse_dll_arg]
	;call rax
	
	;pour le type pointeur et non fonction si rax = 1
	lea rax, [rip+@parse_type_access]
	call rax
	mov rdi, rax
	
	;charger la dll (argument 1)
	mov rcx, [rip+@address_table]
	mov rax, [rip+@LoadLibraryA]
	call rax
	test rax, rax
	jz .error_load_dll
	
	mov rsi, rax  ; sauvgarde du handle de la dll pour fermé après
	
	;charger la fonction
	lea rdx, [rip+@address_table]
	add rdx, 8
	mov rdx, [rdx]  ; contenu du deuxième argument, nom de fonction
	mov rcx, rsi    ; handle de la dll
	mov rax, [rip+@GetProcAddress]
	call rax
	test rax, rax
	jz .error_load_function
	
	mov rcx, rax  ; argument pour fonction de traitement de la dll
	mov rdx, rbx  ; nombre d'argument dans address_table
	; rdi si 1 alors c'est un pointeur pas une fonction
	lea rax, [rip+@dll_caller]
	call rax
	
	;copier si c'est un pointeur on copie la string sur l'adresse de return_buffer (max 10000 charactères ascii) et on change rax sur return_buffer
	mov rcx, rax
	lea rax, [rip+@return_type]
	call rax
	
	mov r15, rax ;enregistrement de la réponse
	
	; libérer la dll
	mov rcx, rsi
	mov rax, [rip+@FreeLibrary]
	call rax
	
	jmp .end
	
	.error_mem:
	lea rcx, [rip+@error_mem]
	mov rax, [rip+@wprintf]
	call rax
	jmp .end
	
	.error_arg_enough:
	lea rcx, [rip+@error_arg_enough]
	mov rax, [rip+@wprintf]
	call rax
	jmp .end
	
	.error_arg_excessively:
	lea rcx, [rip+@error_arg_excessively]
	mov rax, [rip+@wprintf]
	call rax
	jmp .end
	
	.error_load_dll:
	lea rcx, [rip+@error_load_dll]
	mov rax, [rip+@wprintf]
	call rax
	jmp .end
	
	.error_load_function:
	lea rcx, [rip+@error_load_function]
	mov rax, [rip+@wprintf]
	call rax
	
	
	.end:
	;debugage
	;lea rcx, [rip+@msg_fin]
	;mov rax, [rip+@printf]
	;call rax
	
	;free des tampons d'arguments aloué
	mov rcx, rbx ; récupération du nombre d'argument a free
	lea rax, [rip+@free_dll_parse_arg]
	call rax
	
	mov rax, r15  ;réponse de la fonction réstorer
	
	;movdqu xmm0, [rsp+88h]
	;movdqu xmm1, [rsp+78h]
	;mov r8, [rsp+70h]
	;mov r9, [rsp+68h]
	mov r10, [rsp+60h]
	mov r11, [rsp+58h]
	mov r12, [rsp+50h]
	mov r13, [rsp+48h]
	mov r14, [rsp+40h]
	mov r15, [rsp+38h]
	;mov rcx, [rsp+30h]
	;mov rdx, [rsp+28h]
	mov rbx, [rsp+20h]
    mov rsi, [rsp+28h]
    mov rdi, [rsp+30h]
    add rsp, 98h
    ret
end dll_service

;(void)->(int) verify_dll_mem renvoie le nombre d'emplacement disponible pour sauvgardé une dll, max 50
proc verify_dll_mem
	sub rsp, 38h
	mov [rsp+20h], rbx
    mov [rsp+28h], rsi
    mov [rsp+30h], rdi
	
	mov rcx, [rip+@mem_dll_buffer]
	mov rax, 50
	sub rax, rcx
	
	mov rbx, [rsp+20h]
    mov rsi, [rsp+28h]
    mov rdi, [rsp+30h]
    add rsp, 38h
    ret
end verify_dll_mem

;(char)->(mutliple) sauvgarde de l'adresse de la dll (RCX) (ou free si déjà 50 aloués)
proc save_dll_mem
	sub rsp, 38h
	mov [rsp+20h], rbx
    mov [rsp+28h], rsi
    mov [rsp+30h], rdi
	
	; récupération du nombre de dll déjà sauvgardé
	mov rbx, [rip+@mem_dll_buffer]
	cmp rbx, 50
	jae .free_zone
	
	imul rax, rbx, 8
	lea rdi, [rip+@mem_dll_buffer]
	add rdi, 8
	add rdi, rax ;déplacement de rdi à l'adresse d'écriture dans le tempon
	mov [rdi], rcx ;écriture de l'adresse de la dll
	
	inc rbx
	sub rdi, rax
	sub rdi, 8
	mov [rdi], rbx ;incrémente le compteur
	
	jmp .end
	
	.free_zone:
	; libérer dll RCX
	mov rax, [rip+@FreeLibrary]
	call rax
	
	.end:
	mov rbx, [rsp+20h]
    mov rsi, [rsp+28h]
    mov rdi, [rsp+30h]
    add rsp, 38h
    ret
end save_dll_mem

;(int)->(bool) 
proc free_dll_mem
    sub rsp, 38h
    mov [rsp+20h], rbx
    mov [rsp+28h], rsi
    mov [rsp+30h], rdi
	mov [rsp+8h], rcx

    ; Charger l'adresse du tampon et le compteur
    lea rdi, [rip+@mem_dll_buffer]  ; RDI = pointeur vers le compteur
    mov rbx, [rdi]                  ; RBX = compteur actuel (nombre de DLL stockées)

    ; Cas 1: RCX == 0 -> tout libérer
    test rcx, rcx
    jz .free_all

    ; Cas 2: RCX doit être entre 1 et 50
    cmp rcx, 1
    jl .invalid_index
    cmp rcx, 50
    jg .invalid_index

    ; Vérifier que RCX <= RBX (index valide dans le tampon)
    cmp rcx, rbx
    jg .invalid_index

    ; Calculer l'adresse de la DLL à libérer (index = RCX)
	dec rcx
    imul rax, rcx, 8
    mov rsi, rdi
	add rsi, 8                      ; RSI = début du tampon d'adresses
    add rsi, rax                    ; RSI = adresse de la DLL à libérer
	
	mov rdi, [rsp+8h] ; (! écrase rsp+8h donc on met sur rdi)
    ; Libérer la DLL
    mov rcx, [rsi]                  ; RCX = adresse de la DLL
	test rcx, rcx
	jz .invalid_index
    mov rax, [rip+@FreeLibrary]
    call rax

    ; Décaler les adresses suivantes d'un bloc (8 octets)
    ; RSI = source pour le décalage
    mov rdx, rbx                    ; RDX = nombre total de DLL
	mov rax, rdi
    sub rdx, rax                    ; RDX = nombre d'adresses à décaler (RBX - RCX)

    ; Boucle de décalage
    test rdx, rdx
    jz .decrement_counter           ; Si plus rien à décaler
    .shift_loop:
    mov rax, [rsi+8]            ; Charger l'adresse suivante
    mov [rsi], rax              ; La copier à l'emplacement actuel
    add rsi, 8
    dec rdx
	test rdx, rdx
    jnz .shift_loop

	mov rax, 0			; nétoyage
	mov [rsi], rax

    ; Décrémenter le compteur
    .decrement_counter:
    mov rdi, [rip+@mem_dll_buffer]
	dec rdi
	mov [rip+@mem_dll_buffer], rdi

    ; Retourner 1 (succès)
    mov rax, 1
    jmp .end

    ; Cas 1: Tout libérer
    .free_all:
    test rbx, rbx
    jz .invalid_index           ; Si déjà vide, retourner 0
    mov rsi, rdi
	add rsi, 8                  ; RSI = début du tampon d'adresses
    xor rdi, rdi                ; Compteur pour la boucle

    .free_loop:
    mov rcx, [rsi]          ; Charger l'adresse de la DLL
	test rcx, rcx
	jz .invalid_index       ; si pas d'adresse donc NULL pointor
    mov rax, [rip+@FreeLibrary]
    call rax
	mov rax, 0              ; nétoyage
	mov [rsi], rax
    add rsi, 8
    inc rdi
    cmp rdi, rbx
    jl .free_loop

	; Réinitialiser le compteur à 0
    mov rdi, 0
	mov [rip+@mem_dll_buffer], rdi

    mov rax, 1
    jmp .end

    ; Cas d'erreur: index invalide
	.invalid_index:
	xor rax, rax			; RAX = 0 (échec)

    .end:
    mov rbx, [rsp+20h]
    mov rsi, [rsp+28h]
    mov rdi, [rsp+30h]
    add rsp, 38h
    ret
end free_dll_mem

;similaire mais pas de free de dll, à liberer avec free_dll_mem
;(char)->(mutliple) 
proc dll_service_mem
	sub rsp, 98h
	;movdqu [rsp+88h], xmm0
	;movdqu [rsp+78h], xmm1
	;mov [rsp+70h], r8
	;mov [rsp+68h], r9
	mov [rsp+60h], r10
	mov [rsp+58h], r11
	mov [rsp+50h], r12
	mov [rsp+48h], r13
	mov [rsp+40h], r14
	mov [rsp+38h], r15
	;mov [rsp+30h], rcx
	;mov [rsp+28h], rdx
    mov [rsp+20h], rbx
    mov [rsp+28h], rsi
    mov [rsp+30h], rdi
	mov [rsp+8h], rax
	
	;parsing des arguments sur rcx ;
	lea rax, [rip+@parse_dll_arg]
	call rax
	
	test rdx, rdx ; error malloc ou autre
	jz .error_mem
	cmp rdx, 2 ; trop d'argument
	je .error_arg_excessively
	cmp rdx, 3 ; pas d'argument suffisant
	je .error_arg_enough
	test rax, rax  ; aucun argument (pas possible car code d'erreur)
	jz .end
	
	mov rbx, rax   ; sauvgarde du nombre d'argument dans rbx 
	
	
	;debugage
	;mov rcx, rax
	;lea rax, [rip+@debug_parse_dll_arg]
	;call rax
	
	;pour le type pointeur et non fonction si rax = 1
	lea rax, [rip+@parse_type_access]
	call rax
	mov rdi, rax
	
	;charger la dll (argument 1)
	mov rcx, [rip+@address_table]
	mov rax, [rip+@LoadLibraryA]
	call rax
	test rax, rax
	jz .error_load_dll
	
	mov rsi, rax  ; sauvgarde du handle de la dll pour enregistrer après si ok
	
	;charger la fonction
	lea rdx, [rip+@address_table]
	add rdx, 8
	mov rdx, [rdx]  ; contenu du deuxième argument, nom de fonction
	mov rcx, rsi    ; handle de la dll
	mov rax, [rip+@GetProcAddress]
	call rax
	test rax, rax
	jz .error_load_function
	
	mov rcx, rax  ; argument pour fonction de traitement de la dll
	mov rdx, rbx  ; nombre d'argument dans address_table
	; rdi si 1 alors c'est un pointeur pas une fonction
	lea rax, [rip+@dll_caller]
	call rax
	
	;copier si c'est un pointeur on copie la string sur l'adresse de return_buffer (max 10000 charactères ascii) et on change rax sur return_buffer
	mov rcx, rax
	lea rax, [rip+@return_type]
	call rax
	
	mov r15, rax ;enregistrement de la réponse
	
	; enregistrer la dll
	mov rcx, rsi
	lea rax, [rip+@save_dll_mem]
	call rax
	
	jmp .end
	
	.error_mem:
	lea rcx, [rip+@error_mem]
	mov rax, [rip+@wprintf]
	call rax
	jmp .end
	
	.error_arg_enough:
	lea rcx, [rip+@error_arg_enough]
	mov rax, [rip+@wprintf]
	call rax
	jmp .end
	
	.error_arg_excessively:
	lea rcx, [rip+@error_arg_excessively]
	mov rax, [rip+@wprintf]
	call rax
	jmp .end
	
	.error_load_dll:
	lea rcx, [rip+@error_load_dll]
	mov rax, [rip+@wprintf]
	call rax
	jmp .end
	
	.error_load_function:
	lea rcx, [rip+@error_load_function]
	mov rax, [rip+@wprintf]
	call rax
	
	
	.end:
	;debugage
	;lea rcx, [rip+@msg_fin]
	;mov rax, [rip+@printf]
	;call rax
	
	;free des tampons d'arguments aloué
	mov rcx, rbx ; récupération du nombre d'argument a free
	lea rax, [rip+@free_dll_parse_arg]
	call rax
	
	mov rax, r15  ;réponse de la fonction réstorer
	
	;movdqu xmm0, [rsp+88h]
	;movdqu xmm1, [rsp+78h]
	;mov r8, [rsp+70h]
	;mov r9, [rsp+68h]
	mov r10, [rsp+60h]
	mov r11, [rsp+58h]
	mov r12, [rsp+50h]
	mov r13, [rsp+48h]
	mov r14, [rsp+40h]
	mov r15, [rsp+38h]
	;mov rcx, [rsp+30h]
	;mov rdx, [rsp+28h]
	mov rbx, [rsp+20h]
    mov rsi, [rsp+28h]
    mov rdi, [rsp+30h]
    add rsp, 98h
    ret
end dll_service_mem


proc testdll
push rbp
;mov rbp,rsp
sub rsp,50h  
lea rcx, [rip+@debut]
mov rax, [rip+@puts]
call rax                                         
mov rax, [rip+@GetConsoleWindow]
call rax                                  
;mov [rbp-8h],rax           
;cmp qword ptr [rbp-8h],0
cmp rax, 0
jne .suite 
lea rcx, [rip+@string]
mov rax, [rip+@puts]
call rax                        
mov eax,1                                
jmp .end
.suite:                 
;mov rax, [rbp-8h]
mov dword ptr [rsp+30h],44h
mov dword ptr [rsp+28h],190h
mov dword ptr [rsp+20h],320h
mov r9d,64h
mov r8d,64h
mov edx,0
mov rcx,rax
mov rax, [rip+@SetWindowPos]
call rax
mov eax,0
.end:
add rsp,50h                                
pop rbp
ret
end testdll

