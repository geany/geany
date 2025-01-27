# For complete documentation of this file, please see Geany's main documentation
[styling]
# Edit these in the colorscheme .conf file instead
default=default
comment=comment_line
commentblock=comment
commentdirective=comment
number=number_1
string=string_1
operator=operator
identifier=identifier_1
cpuinstruction=keyword_1
mathinstruction=keyword_2
register=type
directive=preprocessor
directiveoperand=keyword_3
character=character
stringeol=string_eol
extinstruction=keyword_4

[keywords]
# all items must be in one line
# this is by default a very simple instruction set; not of Intel or so
instructions=aaa aad aam aas adc add addst and bt call clc cld cli cmp dec dek div divst hlt imul inc ink int iret ja jae jb jbe jc jcxz je jecxz jez jg jge jgz jl jle jlz jmp jna jnae jnb jnbe jnc jne jng jnge jnl jnle jno jnp jns jnz jo jp jpe jpo js jsr jz lad ldi lds ldx lea lia loop loope loopne loopnz loopz lsa mov movs movsb movsd movsw movsx mul mulst neg nop not or pop popa popac popad popfd push pusha pushac pushad pushf pushfd rep ret ret rol ror sbb shl shr spi stc std stos stosb stosd stosw sub subst swap test xchg xlat xor adiw subi sbc sbci sbiw andi ori eor com neg sbr cbr tst clr ser rjmp ijmp rcall icall reti cpse cp cpc cpi sbrc sbrs sbic sbis brbs brbc breq brne brcs brcc brsh brlo brmi brpl brge brlt brhs brhc brts brtc brvs brvc brie brid movw ld ldd sts st  lpm in out lsl lsr rol ror asr bset bclr sbi cbi bst bld sec clc sen cln sez clz sei cli ses cls sev clv set clt seh clh sleep wdr
registers=r0 r1 r2 r3 r4 r5 r6 r7 r8 r9 r10 r11 r12 r13 r14 r15 r16 r17 r18 r19 r20 r21 r22 r23 r24 r25 r26 r27 r28 r29 r30 r31 eax ax al ah ebx bx bl bh ecx cx cl ch edx dx dl dh di edi si esi bp ebp esp sp .data .text .bss _start
directives=org list nolist page equivalent word text equ section global extern %macro %endmacro db movsb stosb resb byte %1 %2 %3 %4 %5 %6 %7 %8 %9 %10 .byte .cseg .db .def .device .dseg .dw .endmacro .equ .eseg .exit .include .list .listmac .macro .nolist .org .set


[settings]
# default extension used when saving files
extension=asm

# these characters define word boundaries when making selections and searching
# using word matching options
#wordchars=_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789

# single comments, like # in this file
comment_single=;
# multiline comments
#comment_open=
#comment_close=

# set to false if a comment character/string should start at column 0 of a line, true uses any
# indentation of the line, e.g. setting to true causes the following on pressing CTRL+d
# 		#command_example();
# setting to false would generate this
# #		command_example();
# This setting works only for single line comments
comment_use_indent=true

# context action command (please see Geany's main documentation for details)
context_action_cmd=

[indentation]
#width=4
# 0 is spaces, 1 is tabs, 2 is tab & spaces
#type=1

[build_settings]
# %f will be replaced by the complete filename
# %e will be replaced by the filename without extension
compiler=nasm "%f"

