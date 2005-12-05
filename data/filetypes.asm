[styling]
# foreground;background;bold
default=0x000000,0xffffff,false
comment=0x808080,0xffffff,false
number=0x007f00,0xffffff,false
string=0x1e90ff,0xffffff,false
operator=0x000000,0xffffff,false
identifier=0x000088,0xffffff,false
cpuinstruction=0x991111,0xffffff,true
mathinstruction=0x00007f,0xffffff,true
register=0x000000,0xffffff,true
directive=0x0F673D,0xffffff,true
directiveoperand=0x1e90ff,0xffffff,false;
commentblock=0x808080,0xffffff,false
character=0x1e90ff,0xffffff,false
stringeol=0x000000,0xe0c0e0,false
extinstruction=0x7f7f00,0xffffff,false

[keywords]
# all items must be in one line
# this is by default a very simple instruction set, not of Intel or so
instructions=hlt lad spi add sub mul div jmp jez jgz jlz swap jsr ret pushac popac addst subst mulst divst lsa lds push pop cli ldi ink lia dek ldx
registers=
directives=org list nolist page equivalent word text
