[styling]
# foreground;background;bold;italic
default=0x000000;0xffffff;false;false
comment=0x808080;0xffffff;false;false
number=0x007f00;0xffffff;false;false
string=0x1e90ff;0xffffff;false;false
operator=0x000000;0xffffff;false;false
identifier=0x000088;0xffffff;false;false
cpuinstruction=0x991111;0xffffff;true;false
mathinstruction=0x00007f;0xffffff;true;false
register=0x000000;0xffffff;true;false
directive=0x0F673D;0xffffff;true;false
directiveoperand=0x1e90ff;0xffffff;false;false
commentblock=0x808080;0xffffff;false;false
character=0x1e90ff;0xffffff;false;false
stringeol=0x000000;0xe0c0e0;false;false
extinstruction=0x7f7f00;0xffffff;false;false

[keywords]
# all items must be in one line
# this is by default a very simple instruction set; not of Intel or so
instructions=hlt lad spi add sub mul div jmp jez jgz jlz swap jsr ret pushac popac addst subst mulst divst lsa lds push pop cli ldi ink lia dek ldx
registers=
directives=org list nolist page equivalent word text
