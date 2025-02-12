#-------------------------------------------------------------------------------
#   w65c816s_decoder.py
#   Generate instruction decoder for w65c816s.h emulator.
#-------------------------------------------------------------------------------
import templ

INOUT_PATH = '../chips/w65c816s.h'

# flag bits
EF = (1<<0) # Emulation
CF = (1<<0) # Carry
ZF = (1<<1) # Zero
IF = (1<<2) # IRQ disable
DF = (1<<3) # Decimal mode
BF = (1<<4) # Break (Emulation)
XF = (1<<4) # Index Register Select (Native)
UF = (1<<5) # Unused (Emulated)
MF = (1<<5) # Memory Select (Native)
VF = (1<<6) # Overflow
NF = (1<<7) # Negative

def flag_name(f):
    if f == EF: return 'E'
    elif f == CF: return 'C'
    elif f == ZF: return 'Z'
    elif f == IF: return 'I'
    elif f == DF: return 'D'
    elif f == BF: return 'B'
    elif f == XF: return 'X'
    elif f == UF: return '1'
    elif f == MF: return 'M'
    elif f == VF: return 'V'
    elif f == NF: return 'N'

def branch_name(m, v):
    if m == NF:
        return 'BPL' if v==0 else 'BMI'
    elif m == VF:
        return 'BVC' if v==0 else 'BVS'
    elif m == CF:
        return 'BCC' if v==0 else 'BCS'
    elif m == ZF:
        return 'BNE' if v==0 else 'BEQ'

# addressing mode constants
A_ABS = 0       # Absolute - a
A_AXI = 1       # Absolute Indexed with X Indirect - (a,x)
A_ABX = 2       # Absolute Indexed with X - a,x
A_ABY = 3       # Absolute Indexed with Y - a,y
A_ABI = 4       # Absolute Indirect - (a)
A_ALX = 5       # Absolute Long Indexed with X - al,x
A_ALN = 6       # Absolute Long - al
A_ACC = 7       # Accumulator - A
A_BMV = 8       # Block Move - xyc
A_DXI = 9       # Direct Indexed with X Indirect - (d,x)
A_DIX = 10      # Direct Indexed with X - d,x
A_DIY = 11      # Direct Indexed with Y - d,y
A_DII = 12      # Direct Indirect Indexed with Y - (d),y
A_DLY = 13      # Direct Indirect Long Indexed with Y - [d],y
A_DIL = 14      # Direct Indirect Long - [d]
A_DID = 15      # Direct Indirect - (d)
A_DIR = 16      # Direct - d
A_IMM = 17      # Immediate - #
A_IMP = 18      # Implied - i
A_PCL = 19      # Program Counter Relative Long - rl
A_PCR = 20      # Program Counter Relative - r
A_STC = 21      # Stack - s
A_STR = 22      # Stack Relative - d,s
A_SII = 23      # Stack Relative Indirect Indexed with Y - (d,s),y
A_JMP = 24      # special JMP abs
A_JSR = 25      # special JSR abs
A_STS = 26      # Stack with Signature - s
A_INV = 27      # an invalid instruction

# addressing mode strings
addr_mode_str = [
    'a', '(a,x)', 'a,x', 'a,y', '(a)', 'al,x', 'al', 'A', 'xyc', '(d,x)', 'd,x', 'd,y',
    '(d),y', '[d],y', '[d]', '(d)', 'd', '#', 'i', 'rl', 'r', 's', 'd,s', '(d,s),y',
    'a', 'a', 's', 'INVALID'
]

# memory access modes
M___ = 0        # no memory access
M_R_ = 1        # read access
M__W = 2        # write access
M_RW = 3        # read-modify-write

# addressing-modes and memory accesses for each instruction
# %aaabbbcc => ops[cc][bbb][aaa]
ops = [
    # cc = 00
    [
        # ---         BIT          JMP          JMP()        STY          LDY          CPY          CPX
        [[A_STS,M___],[A_JSR,M_R_],[A_STC,M_R_],[A_STC,M_R_],[A_PCR,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_]],
        [[A_DIR,M_RW],[A_DIR,M_R_],[A_BMV,M_RW],[A_DIR,M__W],[A_DIR,M__W],[A_DIR,M_R_],[A_DIR,M_R_],[A_DIR,M_R_]],
        [[A_STC,M__W],[A_STC,M___],[A_STC,M__W],[A_STC,M___],[A_IMP,M___],[A_IMP,M___],[A_IMP,M___],[A_IMP,M___]],
        [[A_ABS,M_RW],[A_ABS,M_R_],[A_JMP,M_R_],[A_ABI,M_R_],[A_ABS,M__W],[A_ABS,M_R_],[A_ABS,M_R_],[A_ABS,M_R_]],
        [[A_PCR,M_R_],[A_PCR,M_R_],[A_PCR,M_R_],[A_PCR,M_R_],[A_PCR,M_R_],[A_PCR,M_R_],[A_PCR,M_R_],[A_PCR,M_R_]],
        [[A_DIR,M_RW],[A_DIX,M_R_],[A_BMV,M_RW],[A_DIX,M__W],[A_DIX,M__W],[A_DIX,M_R_],[A_STC,M_R_],[A_STC,M_R_]],
        [[A_IMP,M___],[A_IMP,M___],[A_IMP,M___],[A_IMP,M___],[A_IMP,M___],[A_IMP,M___],[A_IMP,M___],[A_IMP,M___]],
        [[A_ABS,M_RW],[A_ABX,M_R_],[A_ALN,M_R_],[A_AXI,M_R_],[A_ABS,M__W],[A_ABX,M_R_],[A_ABI,M_R_],[A_AXI,M_R_]]
    ],
    # cc = 01
    [
        # ORA         AND          EOR          ADC          STA          LDA          CMP          SBC
        [[A_DXI,M_R_],[A_DXI,M_R_],[A_DXI,M_R_],[A_DXI,M_R_],[A_DXI,M__W],[A_DXI,M_R_],[A_DXI,M_R_],[A_DXI,M_R_]],
        [[A_DIR,M_R_],[A_DIR,M_R_],[A_DIR,M_R_],[A_DIR,M_R_],[A_DIR,M__W],[A_DIR,M_R_],[A_DIR,M_R_],[A_DIR,M_R_]],
        [[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_],[A_IMM,M_R_]],
        [[A_ABS,M_R_],[A_ABS,M_R_],[A_ABS,M_R_],[A_ABS,M_R_],[A_ABS,M__W],[A_ABS,M_R_],[A_ABS,M_R_],[A_ABS,M_R_]],
        [[A_DII,M_R_],[A_DII,M_R_],[A_DII,M_R_],[A_DII,M_R_],[A_DII,M__W],[A_DII,M_R_],[A_DII,M_R_],[A_DII,M_R_]],
        [[A_DIX,M_R_],[A_DIX,M_R_],[A_DIX,M_R_],[A_DIX,M_R_],[A_DIX,M__W],[A_DIX,M_R_],[A_DIX,M_R_],[A_DIX,M_R_]],
        [[A_ABY,M_R_],[A_ABY,M_R_],[A_ABY,M_R_],[A_ABY,M_R_],[A_ABY,M__W],[A_ABY,M_R_],[A_ABY,M_R_],[A_ABY,M_R_]],
        [[A_ABX,M_R_],[A_ABX,M_R_],[A_ABX,M_R_],[A_ABX,M_R_],[A_ABX,M__W],[A_ABX,M_R_],[A_ABX,M_R_],[A_ABX,M_R_]]
    ],
    # cc = 02
    [
        # ASL         ROL          LSR          ROR          STX          LDX          DEC          INC
        [[A_STS,M_RW],[A_ALN,M_RW],[A_IMM,M___],[A_STC,M_RW],[A_PCL,M_R_],[A_IMM,M_R_],[A_IMM,M___],[A_IMM,M___]],
        [[A_DIR,M_RW],[A_DIR,M_RW],[A_DIR,M_RW],[A_DIR,M_RW],[A_DIR,M__W],[A_DIR,M_R_],[A_DIR,M_RW],[A_DIR,M_RW]],
        [[A_ACC,M___],[A_ACC,M___],[A_ACC,M___],[A_ACC,M___],[A_IMP,M___],[A_IMP,M___],[A_IMP,M___],[A_IMP,M___]],
        [[A_ABS,M_RW],[A_ABS,M_RW],[A_ABS,M_RW],[A_ABS,M_RW],[A_ABS,M__W],[A_ABS,M_R_],[A_ABS,M_RW],[A_ABS,M_RW]],
        [[A_DID,M_RW],[A_DID,M_RW],[A_DID,M_RW],[A_DID,M_RW],[A_DID,M__W],[A_DID,M_R_],[A_DID,M_RW],[A_DID,M_RW]],
        [[A_DIX,M_RW],[A_DIX,M_RW],[A_DIX,M_RW],[A_DIX,M_RW],[A_DIY,M__W],[A_DIY,M_R_],[A_DIX,M_RW],[A_DIX,M_RW]],
        [[A_ACC,M___],[A_ACC,M_R_],[A_STC,M__W],[A_STC,M___],[A_IMP,M___],[A_IMP,M___],[A_STC,M__W],[A_STC,M___]],
        [[A_ABX,M_RW],[A_ABX,M_RW],[A_ABX,M_RW],[A_ABX,M_RW],[A_ABX,M__W],[A_ABY,M_R_],[A_ABX,M_RW],[A_ABX,M_RW]]
    ],
    # cc = 03
    [
        [[A_STR,M_RW],[A_STR,M_RW],[A_STR,M_RW],[A_STR,M_RW],[A_STR,M__W],[A_STR,M_R_],[A_STR,M_RW],[A_STR,M_RW]],
        [[A_DIL,M_RW],[A_DIL,M_RW],[A_DIL,M_RW],[A_DIL,M_RW],[A_DIL,M__W],[A_DIL,M_R_],[A_DIL,M_RW],[A_DIL,M_RW]],
        [[A_STC,M__W],[A_STC,M_R_],[A_STC,M__W],[A_STC,M_R_],[A_STC,M__W],[A_STC,M_R_],[A_IMP,M_R_],[A_IMP,M___]],
        [[A_ALN,M_RW],[A_ALN,M_RW],[A_ALN,M_RW],[A_ALN,M_RW],[A_ALN,M__W],[A_ALN,M_R_],[A_ALN,M_RW],[A_ALN,M_RW]],
        [[A_SII,M_RW],[A_SII,M_RW],[A_SII,M_RW],[A_SII,M_RW],[A_SII,M_RW],[A_SII,M_R_],[A_SII,M_RW],[A_SII,M_RW]],
        [[A_DLY,M_RW],[A_DLY,M_RW],[A_DLY,M_RW],[A_DLY,M_RW],[A_DLY,M__W],[A_DLY,M_R_],[A_DLY,M_RW],[A_DLY,M_RW]],
        [[A_IMP,M___],[A_IMP,M_RW],[A_IMP,M___],[A_IMP,M___],[A_IMP,M___],[A_IMP,M___],[A_IMP,M_RW],[A_IMP,M___]],
        [[A_ALX,M_RW],[A_ALX,M_RW],[A_ALX,M_RW],[A_ALX,M_RW],[A_ALX,M__W],[A_ALX,M_R_],[A_ALX,M_RW],[A_ALX,M_RW]]
    ]
]

class opcode:
    def __init__(self, op):
        self.code = op
        self.cmt = None
        self.i = 0
        self.src = [None] * 8
    def t(self, src):
        self.src[self.i] = src
        self.i += 1
    def ta(self, src):
        self.src[self.i-1] += src

#-------------------------------------------------------------------------------
#   output a src line
#
out_lines = ''
def l(s) :
    global out_lines
    out_lines += s + '\n'

#-------------------------------------------------------------------------------
def write_op(op):
    if not op.cmt:
        op.cmt = '???'
    l('    /* {} */'.format(op.cmt if op.cmt else '???'))
    for t in range(0, 8):
        if t < op.i:
            l('        case (0x{:02X}<<3)|{}: {}break;'.format(op.code, t, op.src[t]))
        else:
            l('        case (0x{:02X}<<3)|{}: assert(false);break;'.format(op.code, t))

#-------------------------------------------------------------------------------
def cmt(o,cmd):
    cc = o.code & 3
    bbb = (o.code>>2) & 7
    aaa = (o.code>>5) & 7
    addr_mode = ops[cc][bbb][aaa][0]
    o.cmt = cmd;
    if addr_mode != '':
        o.cmt += ' '+addr_mode_str[addr_mode]

#-------------------------------------------------------------------------------
def u_cmt(o,cmd):
    cmt(o,cmd)
    o.cmt += ' (unimpl)'

#-------------------------------------------------------------------------------
def invalid_opcode(op):
    cc = op & 3
    bbb = (op>>2) & 7
    aaa = (op>>5) & 7
    addr_mode = ops[cc][bbb][aaa][0]
    return addr_mode == A_INV

#-------------------------------------------------------------------------------
def enc_addr(op, addr_mode, mem_access):
    if addr_mode == A_IMP or addr_mode == A_ACC or addr_mode == A_STC:
        # no addressing, this still puts the PC on the address bus without
        # incrementing the PC
        op.t('_SA(c->PC);')
    if addr_mode == A_STS:
        # INT signature byte, not used but puts the PC on the address bus without
        # incrementing the PC
        op.t('if(0==c->brk_flags){_VPA();}_SA(c->PC);')
    elif addr_mode == A_IMM or addr_mode == A_PCR:
        # immediate mode
        op.t('_VPA();_SA(c->PC++);')
    elif addr_mode == A_DIR:
        # direct page
        op.t('_VPA();_SA(c->PC++);')
        op.t('_VDA();_SA(_GD());')
    elif addr_mode == A_DIX:
        # direct page + X
        op.t('_VPA();_SA(c->PC++);')
        op.t('c->AD=_GD();_SA(c->AD);')
        op.t('_VDA();_SA((c->AD+_X(c))&0x00FF);')
    elif addr_mode == A_DIY:
        # direct page + Y
        op.t('_VPA();_SA(c->PC++);')
        op.t('c->AD=_GD();_SA(c->AD);')
        op.t('_VDA();_SA((c->AD+_Y(c))&0x00FF);')
    elif addr_mode == A_ABS:
        # absolute
        op.t('_VPA();_SA(c->PC++);')
        op.t('_VPA();_SA(c->PC++);c->AD=_GD();')
        op.t('_SA((_GD()<<8)|c->AD);')
    elif addr_mode == A_ABX:
        # absolute + X
        # this needs to check if a page boundary is crossed, which costs
        # and additional cycle, but this early-out only happens when the
        # instruction doesn't need to write back to memory
        op.t('_VPA();_SA(c->PC++);')
        op.t('_VPA();_SA(c->PC++);c->AD=_GD();')
        op.t('c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_X(c))&0xFF));')
        if mem_access == M_R_:
            # skip next tick if read access and page not crossed
            op.ta('c->IR+=(~((c->AD>>8)-((c->AD+_X(c))>>8)))&1;')
        op.t('_VDA();_SA(c->AD+_X(c));')
    elif addr_mode == A_ABY:
        # absolute + Y
        # same page-boundary-crossed special case as absolute+X
        op.t('_VPA();_SA(c->PC++);')
        op.t('_VPA();_SA(c->PC++);c->AD=_GD();')
        op.t('c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));')
        if mem_access == M_R_:
            # skip next tick if read access and page not crossed
            op.ta('c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;')
        op.t('_VDA();_SA(c->AD+_Y(c));')
    elif addr_mode == A_DXI:
        # (d,x)
        op.t('_VPA();_SA(c->PC++);')
        op.t('c->AD=_GD();_SA(c->AD);')
        op.t('_VDA();c->AD=(c->AD+_X(c))&0xFF;_SA(c->AD);')
        op.t('_VDA();_SA((c->AD+1)&0xFF);c->AD=_GD();')
        op.t('_VDA();_SA((_GD()<<8)|c->AD);')
    elif addr_mode == A_DII:
        # (d),y
        # same page-boundary-crossed special case as absolute+X
        op.t('_VPA();_SA(c->PC++);')
        op.t('_VDA();c->AD=_GD();_SA(c->AD);')
        op.t('_VDA();_SA((c->AD+1)&0xFF);c->AD=_GD();')
        op.t('c->AD|=_GD()<<8;_SA((c->AD&0xFF00)|((c->AD+_Y(c))&0xFF));')
        if mem_access == M_R_:
            # skip next tick if read access and page not crossed
            op.ta('c->IR+=(~((c->AD>>8)-((c->AD+_Y(c))>>8)))&1;')
        op.t('_VDA();_SA(c->AD+_Y(c));')
    elif addr_mode == A_STR or addr_mode == A_SII or addr_mode == A_DIL or addr_mode == A_DLY or addr_mode == A_ALN or addr_mode == A_ALX or addr_mode == A_DID:
        op.t('/* (unimpl) */;')
    elif addr_mode == A_JMP:
        # jmp is completely handled in instruction decoding
        pass
    elif addr_mode == A_JSR:
        # jsr is completely handled in instruction decoding
        pass
    else:
        # invalid instruction
        pass

#-------------------------------------------------------------------------------
def i_brk(o):
    cmt(o, 'BRK')
    o.t('_VDA();if(0==(c->brk_flags&(W65816_BRK_IRQ|W65816_BRK_NMI))){c->PC++;}_SAD(0x0100|_S(c)--,c->PC>>8);if(0==(c->brk_flags&W65816_BRK_RESET)){_WR();}c->PBR=0;')
    o.t('_VDA();_SAD(0x0100|_S(c)--,c->PC);if(0==(c->brk_flags&W65816_BRK_RESET)){_WR();}')
    o.t('_VDA();_SAD(0x0100|_S(c)--,c->P|W65816_UF);if(c->brk_flags&W65816_BRK_RESET){c->AD=0xFFFC;}else{_WR();if(c->brk_flags&W65816_BRK_NMI){c->AD=0xFFFA;}else{c->AD=0xFFFE;}}')
    o.t('_VDA();_SA(c->AD++);c->P|=(W65816_IF|W65816_BF);c->P&=W65816_DF;c->brk_flags=0; /* RES/NMI hijacking */')
    o.t('_VDA();_SA(c->AD);c->AD=_GD(); /* NMI "half-hijacking" not possible */')
    o.t('c->PC=(_GD()<<8)|c->AD;')

#-------------------------------------------------------------------------------
def i_cop(o):
    cmt(o,'COP')
    o.t('_VDA();_SAD(0x0100|_S(c)--,c->PC>>8);_WR();c->PBR=0;')
    o.t('_VDA();_SAD(0x0100|_S(c)--,c->PC);_WR();')
    o.t('_VDA();_SAD(0x0100|_S(c)--,c->P|W65816_UF);_WR();c->AD=0xFFF4;')
    o.t('_VDA();_SA(c->AD++);c->P|=W65816_IF;c->P&=W65816_DF;c->brk_flags=0; /* RES/NMI hijacking */')
    o.t('_VDA();_SA(c->AD);c->AD=_GD(); /* NMI "half-hijacking" not possible */')
    o.t('c->PC=(_GD()<<8)|c->AD;')

#-------------------------------------------------------------------------------
def i_wai(o):
    u_cmt(o,'WAI')
    o.t('')

#-------------------------------------------------------------------------------
def i_stp(o):
    u_cmt(o,'STP')
    o.t('')

#-------------------------------------------------------------------------------
def i_nop(o):
    cmt(o,'NOP')
    o.t('')

#-------------------------------------------------------------------------------
def u_nop(o):
    u_cmt(o,'NOP')
    o.t('')

#-------------------------------------------------------------------------------
def i_wdm(o):
    cmt(o,'WDM')
    o.t('')

#-------------------------------------------------------------------------------
def i_lda(o):
    cmt(o,'LDA')
    o.t('_A(c)=_GD();_NZ(_A(c));')

#-------------------------------------------------------------------------------
def i_ldx(o):
    cmt(o,'LDX')
    o.t('_X(c)=_GD();_NZ(_X(c));')

#-------------------------------------------------------------------------------
def i_ldy(o):
    cmt(o,'LDY')
    o.t('_Y(c)=_GD();_NZ(_Y(c));')

#-------------------------------------------------------------------------------
def i_stz(o):
    cmt(o,'STZ')
    o.ta('_VDA();_SD(0);_WR();')

#-------------------------------------------------------------------------------
def i_sta(o):
    cmt(o,'STA')
    o.ta('_VDA();_SD(_A(c));_WR();')

#-------------------------------------------------------------------------------
def i_stx(o):
    cmt(o,'STX')
    o.ta('_VDA();_SD(_X(c));_WR();')

#-------------------------------------------------------------------------------
def i_sty(o):
    cmt(o,'STY')
    o.ta('_VDA();_SD(_Y(c));_WR();')

#-------------------------------------------------------------------------------
def i_tax(o):
    cmt(o,'TAX')
    o.t('_X(c)=_A(c);_NZ(_X(c));')

#-------------------------------------------------------------------------------
def i_tay(o):
    cmt(o,'TAY')
    o.t('_Y(c)=_A(c);_NZ(_Y(c));')

#-------------------------------------------------------------------------------
def i_txa(o):
    cmt(o,'TXA')
    o.t('_A(c)=_X(c);_NZ(_A(c));')

#-------------------------------------------------------------------------------
def i_txy(o):
    cmt(o,'TXY')
    o.t('_Y(c)=_X(c);_NZ(_Y(c));')

#-------------------------------------------------------------------------------
def i_tya(o):
    cmt(o,'TYA')
    o.t('_A(c)=_Y(c);_NZ(_A(c));')

#-------------------------------------------------------------------------------
def i_tyx(o):
    cmt(o,'TYX')
    o.t('_X(c)=_Y(c);_NZ(_X(c));')

#-------------------------------------------------------------------------------
def i_txs(o):
    cmt(o,'TXS')
    o.t('_S(c)=_X(c);')

#-------------------------------------------------------------------------------
def i_tsx(o):
    cmt(o,'TSX')
    o.t('_X(c)=_S(c);_NZ(_X(c));')

#-------------------------------------------------------------------------------
def i_tsc(o):
    cmt(o,'TSC')
    o.t('c->C=c->S;_NZ(c->C);')

#-------------------------------------------------------------------------------
def i_tcs(o):
    cmt(o,'TCS')
    o.t('c->S=c->C;')

#-------------------------------------------------------------------------------
def i_tsb(o):
    cmt(o,'TSB')
    o.t('c->AD=_GD();if(_E(c)){_WR();}')
    o.t('_VDA();_SD(_A(c)|c->AD);_WR();_Z(_A(c)&c->AD);')

#-------------------------------------------------------------------------------
def i_trb(o):
    cmt(o,'TRB')
    o.t('c->AD=_GD();if(_E(c)){_WR();}')
    o.t('_VDA();_SD(~_A(c)&c->AD);_WR();_Z(_A(c)&c->AD);')

#-------------------------------------------------------------------------------
def i_tcd(o):
    cmt(o,'TCD')
    o.t('c->D=c->C;_NZ(c->C);')

#-------------------------------------------------------------------------------
def i_tdc(o):
    cmt(o,'TDC')
    o.t('c->C=c->D;_NZ(c->C);')

#-------------------------------------------------------------------------------
def i_xba(o):
    cmt(o,'XBA')
    o.t('_SA(c->PC);') # junk read during operation
    o.t('_w65816_xba(c);')

#-------------------------------------------------------------------------------
def i_xce(o):
    cmt(o,'XCE')
    o.t('_w65816_xce(c);')

#-------------------------------------------------------------------------------
def i_php(o):
    cmt(o,'PHP')
    o.t('_VDA();_SAD(0x0100|_S(c)--,c->P|W65816_UF);_WR();')

#-------------------------------------------------------------------------------
def i_plp(o):
    cmt(o,'PLP')
    o.t('_SA(c->PC);') # read junk byte from current PC
    o.t('_VDA();_SA(0x0100|++_S(c));')   # read actual byte
    o.t('c->P=(_GD()|W65816_BF)&~W65816_UF;');

#-------------------------------------------------------------------------------
def i_pha(o):
    cmt(o,'PHA')
    o.t('_VDA();_SAD(0x0100|_S(c)--,_A(c));_WR();')

#-------------------------------------------------------------------------------
def i_pla(o):
    cmt(o,'PLA')
    o.t('_SA(c->PC);') # read junk byte from current PC
    o.t('_VDA();_SA(0x0100|++_S(c));')   # read actual byte
    o.t('_A(c)=_GD();_NZ(_A(c));')

#-------------------------------------------------------------------------------
def i_phx(o):
    cmt(o,'PHX')
    o.t('_VDA();_SAD(0x0100|_S(c)--,_X(c));_WR();')

#-------------------------------------------------------------------------------
def i_plx(o):
    cmt(o,'PLX')
    o.t('_SA(c->PC);') # read junk byte from current PC
    o.t('_VDA();_SA(0x0100|++_S(c));')   # read actual byte
    o.t('_X(c)=_GD();_NZ(_X(c));')

#-------------------------------------------------------------------------------
def i_phy(o):
    cmt(o,'PHY')
    o.t('_VDA();_SAD(0x0100|_S(c)--,_Y(c));_WR();')

#-------------------------------------------------------------------------------
def i_ply(o):
    cmt(o,'PLY')
    o.t('_SA(c->PC);') # read junk byte from current PC
    o.t('_VDA();_SA(0x0100|++_S(c));')   # read actual byte
    o.t('_Y(c)=_GD();_NZ(_Y(c));')

#-------------------------------------------------------------------------------
def i_phb(o):
    cmt(o,'PHB')
    o.t('_VDA();_SAD(0x0100|_S(c)--,c->DBR);_WR();')

#-------------------------------------------------------------------------------
def i_plb(o):
    cmt(o,'PLB')
    o.t('_SA(c->PC);') # read junk byte from current PC
    o.t('_VDA();_SA(0x0100|++_S(c));')   # read actual byte
    o.t('c->DBR=_GD();_NZ(c->DBR);')

#-------------------------------------------------------------------------------
def i_phk(o):
    cmt(o,'PHK')
    o.t('_VDA();_SAD(0x0100|_S(c)--,c->PBR);_WR();')

#-------------------------------------------------------------------------------
def i_phd(o):
    cmt(o,'PHD')
    # write Direct page high byte to stack
    o.t('_VDA();_SAD(0x0100|_S(c)--,c->D>>8);_WR();')
    # write Direct page low byte to stack
    o.t('_VDA();_SAD(0x0100|_S(c)--,c->D);_WR();')

#-------------------------------------------------------------------------------
def i_pld(o):
    cmt(o,'PLD')
    o.t('_SA(c->PC);') # read junk byte from current PC
    # load D low byte from stack
    o.t('_VDA();_SA(0x0100|_S(c)++);')
    # load D high byte from stack
    o.t('_VDA();_SA(0x0100|_S(c));c->AD=_GD();')
    # put address in D
    o.t('c->D=(_GD()<<8)|c->AD;')

#-------------------------------------------------------------------------------
def i_pea(o):
    u_cmt(o,'PEA')
    o.t('')

#-------------------------------------------------------------------------------
def i_pei(o):
    u_cmt(o,'PEI')
    o.t('')

#-------------------------------------------------------------------------------
def i_per(o):
    u_cmt(o,'PER')
    o.t('')

#-------------------------------------------------------------------------------
def i_se(o, f):
    cmt(o,'SE'+flag_name(f))
    o.t('c->P|='+hex(f)+';')

#-------------------------------------------------------------------------------
def i_cl(o, f):
    cmt(o,'CL'+flag_name(f))
    o.t('c->P&=~'+hex(f)+';')

#-------------------------------------------------------------------------------
def i_sep(o):
    cmt(o,'SEP')
    o.t('c->P|=_GD();_SA(c->PC);')
    o.t('') # junk read during P operation

#-------------------------------------------------------------------------------
def i_rep(o):
    cmt(o,'REP')
    o.t('c->P&=~_GD();_SA(c->PC);')
    o.t('') # junk read during P operation

#-------------------------------------------------------------------------------
def i_br(o, m, v):
    cmt(o,branch_name(m,v))
    # if branch not taken?
    o.t('_SA(c->PC);c->AD=c->PC+(int8_t)_GD();if((c->P&'+hex(m)+')!='+hex(v)+'){_FETCH();};')
    # branch taken: shortcut if page not crossed, 'branchquirk' interrupt fix
    o.t('_SA((c->PC&0xFF00)|(c->AD&0x00FF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};')
    # page crossed extra cycle:
    o.t('c->PC=c->AD;')

#-------------------------------------------------------------------------------
def i_bra(o):
    cmt(o,'BRA')
    # branch is always taken - adds a cycle
    o.t('_SA(c->PC);c->AD=c->PC+(int8_t)_GD();')
    # branch taken: shortcut if page not crossed, 'branchquirk' interrupt fix
    o.t('_SA((c->PC&0xFF00)|(c->AD&0x00FF));if((c->AD&0xFF00)==(c->PC&0xFF00)){c->PC=c->AD;c->irq_pip>>=1;c->nmi_pip>>=1;_FETCH();};')
    # page crossed extra cycle:
    o.t('c->PC=c->AD;')

#-------------------------------------------------------------------------------
def i_brl(o):
    u_cmt(o,'BRL')
    o.t('')

#-------------------------------------------------------------------------------
def i_jmp(o):
    cmt(o,'JMP')
    o.t('_VPA();_SA(c->PC++);')
    o.t('_VPA();_SA(c->PC++);c->AD=_GD();')
    o.t('c->PC=(_GD()<<8)|c->AD;')

#-------------------------------------------------------------------------------
def i_jml(o):
    u_cmt(o,'JML')
    o.t('')

#-------------------------------------------------------------------------------
def i_jmpi(o):
    cmt(o,'JMP')
    o.t('_VPA();_SA(c->PC++);')
    o.t('_VPA();_SA(c->PC++);c->AD=_GD();')
    o.t('_VDA();c->AD|=_GD()<<8;_SA(c->AD);')
    o.t('_VDA();_SA((c->AD&0xFF00)|((c->AD+1)&0x00FF));c->AD=_GD();')
    o.t('c->PC=(_GD()<<8)|c->AD;')

#-------------------------------------------------------------------------------
def i_jsr(o):
    cmt(o,'JSR')
    # read low byte of target address
    o.t('_VPA();_SA(c->PC++);')
    # put SP on addr bus, next cycle is a junk read
    o.t('_SA(0x0100|_S(c));c->AD=_GD();')
    # write PC high byte to stack
    o.t('_VDA();_SAD(0x0100|_S(c)--,c->PC>>8);_WR();')
    # write PC low byte to stack
    o.t('_VDA();_SAD(0x0100|_S(c)--,c->PC);_WR();')
    # load target address high byte
    o.t('_VPA();_SA(c->PC);')
    # load PC and done
    o.t('c->PC=(_GD()<<8)|c->AD;')

#-------------------------------------------------------------------------------
def i_jsrx(o):
    cmt(o,'JSR')
    # read low byte of target address
    o.t('_VPA();_SA(c->PC++);')
    # write PC high byte to stack
    o.t('_VDA();_SAD(0x0100|_S(c)--,c->PC>>8);_WR();')
    # write PC low byte to stack
    o.t('_VDA();_SAD(0x0100|_S(c)--,c->PC);_WR();')
    # load target address high byte
    o.t('_VPA();_SA(c->PC);')
    # put PC on addr bus, next cycle is a junk read
    o.t('_SA(c->PC);c->AD=(_GD()<<8)|c->AD;')
    # load PC from pointed address
    o.t('_VDA();_SA(c->AD+_X(c));')
    o.t('_VDA();_SA(c->AD+_X(c)+1);c->AD=_GD();')
    o.t('c->PC=(_GD()<<8)|c->AD;')

#-------------------------------------------------------------------------------
def i_jsl(o):
    u_cmt(o,'JSL')
    o.t('')

#-------------------------------------------------------------------------------
def i_rts(o):
    cmt(o,'RTS')
    # put SP on stack and do a junk read
    o.t('_SA(0x0100|_S(c)++);')
    # load return address low byte from stack
    o.t('_VDA();_SA(0x0100|_S(c)++);')
    # load return address high byte from stack
    o.t('_VDA();_SA(0x0100|_S(c));c->AD=_GD();')
    # put return address in PC, this is one byte before next op, do junk read from PC
    o.t('c->PC=(_GD()<<8)|c->AD;_SA(c->PC++);')
    # next tick is opcode fetch
    o.t('');

#-------------------------------------------------------------------------------
def i_rtl(o):
    u_cmt(o,'RTL')
    o.t('_SA(0x0100|_S(c)++);')

#-------------------------------------------------------------------------------
def i_rti(o):
    cmt(o,'RTI')
    # put SP on stack and do a junk read
    o.t('_SA(0x0100|_S(c)++);')
    # load processor status flag from stack
    o.t('_VDA();_SA(0x0100|_S(c)++);')
    # load return address low byte from stack
    o.t('_VDA();_SA(0x0100|_S(c)++);c->P=(_GD()|W65816_BF)&~W65816_UF;')
    # load return address high byte from stack
    o.t('_VDA();_SA(0x0100|_S(c));c->AD=_GD();')
    # update PC (which is already placed on the right return-to instruction)
    o.t('c->PC=(_GD()<<8)|c->AD;')

#-------------------------------------------------------------------------------
def i_ora(o):
    cmt(o,'ORA')
    o.t('_A(c)|=_GD();_NZ(_A(c));')

#-------------------------------------------------------------------------------
def i_and(o):
    cmt(o,'AND')
    o.t('_A(c)&=_GD();_NZ(_A(c));')

#-------------------------------------------------------------------------------
def i_eor(o):
    cmt(o,'EOR')
    o.t('_A(c)^=_GD();_NZ(_A(c));')

#-------------------------------------------------------------------------------
def i_adc(o):
    cmt(o,'ADC')
    o.t('_w65816_adc(c,_GD());')

#-------------------------------------------------------------------------------
def i_sbc(o):
    cmt(o,'SBC')
    o.t('_w65816_sbc(c,_GD());')

#-------------------------------------------------------------------------------
def i_cmp(o):
    cmt(o,'CMP')
    o.t('_w65816_cmp(c, _A(c), _GD());')

#-------------------------------------------------------------------------------
def i_cpx(o):
    cmt(o,'CPX')
    o.t('_w65816_cmp(c, _X(c), _GD());')

#-------------------------------------------------------------------------------
def i_cpy(o):
    cmt(o,'CPY')
    o.t('_w65816_cmp(c, _Y(c), _GD());')

#-------------------------------------------------------------------------------
def i_dec(o):
    cmt(o,'DEC')
    o.t('c->AD=_GD();if(_E(c)){_WR();}')
    o.t('_VDA();c->AD--;_NZ(c->AD);_SD(c->AD);_WR();')

#-------------------------------------------------------------------------------
def i_inc(o):
    cmt(o,'INC')
    o.t('c->AD=_GD();if(_E(c)){_WR();}')
    o.t('_VDA();c->AD++;_NZ(c->AD);_SD(c->AD);_WR();')

#-------------------------------------------------------------------------------
def i_inca(o):
    cmt(o,'INC')
    o.t('_A(c)++;_NZ(_A(c));')

#-------------------------------------------------------------------------------
def i_deca(o):
    cmt(o,'DEC')
    o.t('_A(c)--;_NZ(_A(c));')

#-------------------------------------------------------------------------------
def i_dex(o):
    cmt(o,'DEX')
    o.t('_X(c)--;_NZ(_X(c));')

#-------------------------------------------------------------------------------
def i_dey(o):
    cmt(o,'DEY')
    o.t('_Y(c)--;_NZ(_Y(c));')

#-------------------------------------------------------------------------------
def i_inx(o):
    cmt(o,'INX')
    o.t('_X(c)++;_NZ(_X(c));')

#-------------------------------------------------------------------------------
def i_iny(o):
    cmt(o,'INY')
    o.t('_Y(c)++;_NZ(_Y(c));')

#-------------------------------------------------------------------------------
def i_asl(o):
    cmt(o,'ASL')
    o.t('_VDA();c->AD=_GD();_WR();')
    o.t('_VDA();_SD(_w65816_asl(c,c->AD));_WR();')

#-------------------------------------------------------------------------------
def i_asla(o):
    cmt(o,'ASL')
    o.t('_A(c)=_w65816_asl(c,_A(c));')

#-------------------------------------------------------------------------------
def i_lsr(o):
    cmt(o,'LSR')
    o.t('_VDA();c->AD=_GD();_WR();')
    o.t('_VDA();_SD(_w65816_lsr(c,c->AD));_WR();')

#-------------------------------------------------------------------------------
def i_lsra(o):
    cmt(o,'LSR')
    o.t('_A(c)=_w65816_lsr(c,_A(c));')

#-------------------------------------------------------------------------------
def i_rol(o):
    cmt(o,'ROL')
    o.t('_VDA();c->AD=_GD();_WR();')
    o.t('_VDA();_SD(_w65816_rol(c,c->AD));_WR();')

#-------------------------------------------------------------------------------
def i_rola(o):
    cmt(o,'ROL')
    o.t('_A(c)=_w65816_rol(c,_A(c));')

#-------------------------------------------------------------------------------
def i_ror(o):
    cmt(o,'ROR')
    o.t('_VDA();c->AD=_GD();_WR();')
    o.t('_VDA();_SD(_w65816_ror(c,c->AD));_WR();')

#-------------------------------------------------------------------------------
def i_rora(o):
    cmt(o,'ROR')
    o.t('_A(c)=_w65816_ror(c,_A(c));')

#-------------------------------------------------------------------------------
def i_bit(o):
    cmt(o,'BIT')
    o.t('_w65816_bit(c,_GD());')

#-------------------------------------------------------------------------------
def i_mvp(o):
    cmt(o,'MVP')
    # read destination bank address
    o.t('_VPA();_SA(c->PC++);')
    # read source bank address
    o.t('_VPA();c->DBR=_GD();_SA(c->PC);')
    # read source data
    o.t('_VDA();_SB(_GD());_SA(c->X--);')
    # write destination data
    o.t('_VDA();_SB(c->DBR);_SA(c->Y--);_WR();')
    # move back PC, still addressing destination
    o.t('if(c->C){c->PC--;}')
    # move to next, still addressing destination
    o.t('c->C--?c->PC--:c->PC++;')

#-------------------------------------------------------------------------------
def i_mvn(o):
    cmt(o,'MVN')
    # read destination bank address
    o.t('_VPA();_SA(c->PC++);')
    # read source bank address
    o.t('_VPA();c->DBR=_GD();_SA(c->PC);')
    # read source data
    o.t('_VDA();_SB(_GD());_SA(c->X++);')
    # write destination data
    o.t('_VDA();_SB(c->DBR);_SA(c->Y++);_WR();')
    # move back PC, still addressing destination
    o.t('if(c->C){c->PC--;}')
    # move to next, still addressing destination
    o.t('c->C--?c->PC--:c->PC++;')

#-------------------------------------------------------------------------------
def enc_op(op):
    o = opcode(op)
    if invalid_opcode(op):
        i_stp(o);
        return o

    # decode the opcode byte
    cc = op & 3
    bbb = (op>>2) & 7
    aaa = (op>>5) & 7
    addr_mode = ops[cc][bbb][aaa][0]
    mem_access = ops[cc][bbb][aaa][1]
    # addressing mode
    enc_addr(o, addr_mode, mem_access);
    # actual instruction
    if cc == 0:
        if aaa == 0:
            if bbb == 0:        i_brk(o)
            elif bbb == 1:      i_tsb(o)
            elif bbb == 2:      i_php(o)
            elif bbb == 3:      i_tsb(o)
            elif bbb == 4:      i_br(o, NF, 0)  # BPL
            elif bbb == 5:      i_trb(o)
            elif bbb == 6:      i_cl(o, CF)
            elif bbb == 7:      i_trb(o)
            else:               u_nop(o)
        elif aaa == 1:
            if bbb == 0:        i_jsr(o)
            elif bbb == 2:      i_plp(o)
            elif bbb == 4:      i_br(o, NF, NF) # BMI
            elif bbb == 6:      i_se(o, CF)
            else:               i_bit(o)
        elif aaa == 2:
            if bbb == 0:        i_rti(o)
            elif bbb == 1:      i_mvp(o)
            elif bbb == 2:      i_pha(o)
            elif bbb == 3:      i_jmp(o)
            elif bbb == 4:      i_br(o, VF, 0)  # BVC
            elif bbb == 5:      i_mvn(o)
            elif bbb == 6:      i_cl(o, IF)
            elif bbb == 7:      i_jmp(o)
            else:               u_nop(o)
        elif aaa == 3:
            if bbb == 0:        i_rts(o)
            elif bbb == 2:      i_pla(o)
            elif bbb == 3:      i_jmpi(o)
            elif bbb == 4:      i_br(o, VF, VF) # BVS
            elif bbb == 6:      i_se(o, IF)
            elif bbb == 7:      i_jmpi(o)
            else:               i_stz(o)
        elif aaa == 4:
            if bbb == 0:        i_bra(o)
            elif bbb == 2:      i_dey(o)
            elif bbb == 4:      i_br(o, CF, 0)  # BCC
            elif bbb == 6:      i_tya(o)
            elif bbb == 7:      i_stz(o)
            else:               i_sty(o)
        elif aaa == 5:
            if bbb == 2:        i_tay(o)
            elif bbb == 4:      i_br(o, CF, CF) # BCS
            elif bbb == 6:      i_cl(o, VF)
            else:               i_ldy(o)
        elif aaa == 6:
            if bbb == 2:        i_iny(o)
            elif bbb == 4:      i_br(o, ZF, 0)  # BNE
            elif bbb == 5:      i_pei(o)
            elif bbb == 6:      i_cl(o, DF)
            elif bbb == 7:      i_jml(o)
            else:               i_cpy(o)
        elif aaa == 7:
            if bbb == 2:        i_inx(o)
            elif bbb == 4:      i_br(o, ZF, ZF) # BEQ
            elif bbb == 5:      i_pea(o)
            elif bbb == 6:      i_se(o, DF)
            elif bbb == 7:      i_jsrx(o)
            else:               i_cpx(o)
    elif cc == 1:
        if aaa == 0:    i_ora(o)
        elif aaa == 1:  i_and(o)
        elif aaa == 2:  i_eor(o)
        elif aaa == 3:  i_adc(o)
        elif aaa == 4:
            if bbb == 2:    i_bit(o)
            else:           i_sta(o)
        elif aaa == 5:  i_lda(o)
        elif aaa == 6:  i_cmp(o)
        else:           i_sbc(o)
    elif cc == 2:
        if aaa == 0:
            if bbb == 0:    i_cop(o)
            elif bbb == 2:  i_asla(o)
            elif bbb == 4:  i_ora(o)
            elif bbb == 6:  i_inca(o)
            else:           i_asl(o)
        elif aaa == 1:
            if bbb == 0:    i_jsl(o)
            elif bbb == 2:  i_rola(o)
            elif bbb == 4:  i_and(o)
            elif bbb == 6:  i_deca(o)
            else:           i_rol(o)
        elif aaa == 2:
            if bbb == 0:    i_wdm(o)
            elif bbb == 2:  i_lsra(o)
            elif bbb == 4:  i_eor(o)
            elif bbb == 6:  i_phy(o)
            else:           i_lsr(o)
        elif aaa == 3:
            if bbb == 0:    i_per(o)
            elif bbb == 2:  i_rora(o)
            elif bbb == 4:  i_adc(o)
            elif bbb == 6:  i_ply(o)
            else:           i_ror(o)
        elif aaa == 4:
            if bbb == 0:    i_brl(o)
            elif bbb == 2:  i_txa(o)
            elif bbb == 4:  i_sta(o)
            elif bbb == 6:  i_txs(o)
            elif bbb == 7:  i_stz(o)
            else:           i_stx(o)
        elif aaa == 5:
            if bbb == 2:    i_tax(o)
            elif bbb == 4:  i_lda(o)
            elif bbb == 6:  i_tsx(o)
            else:           i_ldx(o)
        elif aaa == 6:
            if bbb == 0:        i_rep(o)
            elif bbb == 2:      i_dex(o)
            elif bbb == 4:      i_cmp(o)
            elif bbb == 6:      i_phx(o)
            else:               i_dec(o)
        elif aaa == 7:
            if bbb == 0:        i_sep(o)
            elif bbb == 2:      i_nop(o)
            elif bbb == 4:      i_sbc(o)
            elif bbb == 6:      i_plx(o)
            else:               i_inc(o)
    elif cc == 3:
        if aaa == 0:
            if bbb == 2:    i_phd(o)
            elif bbb == 6:  i_tcs(o)
            else:           i_ora(o)
        elif aaa == 1:
            if bbb == 2:    i_pld(o)
            elif bbb == 6:  i_tsc(o)
            else:           i_and(o)
        elif aaa == 2:
            if bbb == 2:    i_phk(o)
            elif bbb == 6:  i_tcd(o)
            else:           i_eor(o)
        elif aaa == 3:
            if bbb == 2:    i_rtl(o)
            elif bbb == 6:  i_tdc(o)
            else:           i_adc(o)
        elif aaa == 4:
            if bbb == 2:    i_phb(o)
            elif bbb == 6:  i_txy(o)
            else:           i_sta(o)
        elif aaa == 5:
            if bbb == 2:    i_plb(o)
            elif bbb == 6:  i_tyx(o)
            else:           i_lda(o)
        elif aaa == 6:
            if bbb == 2:    i_wai(o)
            elif bbb == 6:  i_stp(o)
            else:           i_cmp(o)
        elif aaa == 7:
            if bbb == 2:    i_xba(o)
            elif bbb == 6:  i_xce(o)
            else:           i_sbc(o)
    # fetch next opcode byte
    if mem_access in [M_R_,M___]:
        o.ta('_FETCH();')
    else:
        o.t('_FETCH();')
    return o

def write_result():
    with open(INOUT_PATH, 'r') as f:
        lines = f.read().splitlines()
        lines = templ.replace(lines, 'decoder', out_lines)
    out_str = '\n'.join(lines) + '\n'
    with open(INOUT_PATH, 'w') as f:
        f.write(out_str)

if __name__ == '__main__':
    for op in range(0, 256):
        write_op(enc_op(op))
    write_result()
