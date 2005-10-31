#include "string.h"
#include "as.h"
#include "flonum.h"
#include "expr.h"
#include "i386.h"

#include "i386-opcode.h"
/* these are to get rid of the compiler "defined but not used" messages */
const reg_entry **use_it1 = &i386_regtab_end;
const seg_entry *use_it2 = &cs;
const seg_entry *use_it3 = &es;
const seg_entry *use_it4 = &fs;
const seg_entry *use_it5 = &gs;
const seg_entry **use_it6 = one_byte_segment_defaults;
const seg_entry **use_it7 = two_byte_segment_defaults;

static char **get_operand(
    unsigned long type);
static char *get_suffix(
    unsigned long type);

int
main(void)
{
    const template *t;
    const prefix_entry *p;
    long prefix;

    unsigned long i, j, type0, type1;
    char **op0, **op1;
    char *suffix;

	for(t = i386_optab; t < i386_optab_end; t++){
	    /*
	     * Don't use the table entries that are prefixes and not
	     * instructions.
	     */
	    prefix = 0;
	    for(p = i386_prefixtab; p < i386_prefixtab_end; p++){
		prefix = (p->prefix_code == t->base_opcode);
		if(prefix)
		    break;
	    }
	    if(prefix)
		continue;
	    /*
	     * The string instructions with operands take only specific
	     * operands and are not checked here.
	     */
	    if(t->operands != 0 && IS_STRING_INSTRUCTION(t->base_opcode))
		continue;
	   
	    if(t->operands == 0){
		if((t->opcode_modifier & W) == 0) {
		    printf("\t%s\n", t->name);
		}
		else{
		    printf("\t%sb\n", t->name);
		    printf("\t%sw\n", t->name);
		    printf("\t%sl\n", t->name);
		}
	    }

	    if(t->operands == 1){
		for(i = 0; i < 32; i++){
		    type0 = 1 << i;
		    if((type0 & t->operand_types[0]) == 0)
			continue;

		    /* These only take byte displacement */
		    if(IS_LOOP_ECX_TIMES(t->base_opcode) &&
		       (type0 == Disp16 || type0 == Disp32))
			continue;

		    /* These only take byte displacement */
		    if((strcmp(t->name, "jcxz") == 0 ||
		        strcmp(t->name, "jecxz") == 0) &&
		       (type0 == Disp16 || type0 == Disp32))
			continue;

		    if(type0 == Disp8 &&
		       ((t->operand_types[0] & (Disp16 | Disp32)) != 0))
			continue;

		    suffix = "";
		    if((type0 & Mem) != 0)
			suffix = get_suffix(type0);

		    /*
		     * This is to avoid the problem with the
		     * fildll opcode which is a fildq and
		     * fistpll opcode which is a fistpq
		     */
		    if((strcmp(t->name, "fildl") == 0 ||
			strcmp(t->name, "fistpl") == 0) &&
			strcmp(suffix, "l") == 0)
			suffix = "";

		    /*
		     * This is to avoid the problems with the
		     * fisttpl opcode and the fisttpll opcodes.
		     */
		    if((strcmp(t->name, "fisttpl") == 0 ||
			strcmp(t->name, "fisttpll") == 0))
			continue;
		
		    /* fwait prefixed instructions */
		    if((t->base_opcode & 0xff00) == 0x9b00 &&
		       strcmp(suffix, "w") == 0)
			continue;

		    op0 = get_operand(type0);
		    for( ; *op0; op0++){
			printf("\t%s%s\t%s\n", t->name, suffix, *op0);
		    }
		}
	    }

	    if(t->operands == 2){
		for(i = 0; i < 32; i++){
		    type0 = 1 << i;
		    if((type0 & t->operand_types[0]) == 0)
			continue;
		    for(j = 0; j < 32; j++){
			type1 = 1 << j;
			if((type1 & t->operand_types[1]) == 0)
			    continue;
			if((type0 & RegALL) != 0 && (type1 & RegALL) != 0)
			    if(type0 != type1)
				continue;

			suffix = "";
			if((type0 & (Imm|Imm1)) != 0 && (type1 & Mem) != 0)
			    suffix = get_suffix(type0);
			if((type0 & Mem) != 0 && (type1 & (Imm|Imm1)) != 0)
			    suffix = get_suffix(type1);

			op0 = get_operand(type0);
			op1 = get_operand(type1);
			for( ; *op0; op0++){
			    for( ; *op1; op1++){
				printf("\t%s%s\t%s,%s\n", t->name, suffix,
				       *op0, *op1);
				if(t->opcode_modifier & D){
				    printf("\t%s%s\t%s,%s\n", t->name, suffix,
					   *op1, *op0);
				}
			    }
			}
		    }
		}
	    }
	}
	return(0);
}

static
char *
get_suffix(
unsigned long type)
{
	switch(type){
	case Imm8:	return("b");
	case Imm8S:	return("b");
	case Imm16:	return("w");
	case Imm32:	return("l");
	case Imm1:	return("l"); /* all */
	case Disp8:	return("b");
	case Disp16:	return("w");
	case Disp32:	return("l");
	case Mem8:	return("b");
	case Mem16:	return("w");
	case Mem32:	return("l");
	case BaseIndex:	return("l"); /* all */
	default:	return("");
	}
}

static char *Reg8_table[] = { "%bl", NULL };
static char *Reg16_table[] = { "%bx", NULL };
static char *Reg32_table[] = { "%ebx", NULL };
static char *RegMM_table[] = { "%mm3", NULL };
static char *RegXMM_table[] = { "%xmm5", NULL };
static char *Imm8_table[] = { "$0x7f", NULL };
static char *Imm8S_table[] = { "$0xfe", NULL };
static char *Imm16_table[] = { "$0xface", NULL };
static char *Imm32_table[] = { "$0xcafebabe", NULL };
static char *Imm1_table[] = { "$0", "$1", NULL };
static char *Disp8_table[] = { "0x45", NULL };
static char *Disp16_table[] = { "0x7eed", NULL };
static char *Disp32_table[] = { "0xbabecafe", NULL };
static char *Mem8_table[] = { "0x88888888", NULL };
static char *Mem16_table[] = { "0x1616", NULL };
static char *Mem32_table[] = { "0x32323232", NULL };
static char *BaseIndex_table[] = { "0xdeadbeef(%ebx,%ecx,8)", NULL };
static char *InOutPortReg_table[] = { "%dx", NULL };
static char *ShiftCount_table[] = { "%cl", NULL };
static char *Control_table[] = { "%cr0", NULL };
static char *Debug_table[] = { "%db0", NULL };
static char *Test_table[] = { "%tr3", NULL };
static char *FloatReg_table[] = { "%st(1)", NULL };
static char *FloatAcc_table[] = { "%st", NULL };
static char *SReg2_table[] = { "%ds", NULL };
static char *SReg3_table[] = { "%fs", NULL };
static char *Acc_table[] = { "%eax", NULL };
static char *JumpAbsolute_table[] = { "*0xbadeface", NULL };
static char *Abs8_table[] = { "0xab", NULL };
static char *Abs16_table[] = { "0xabcd", NULL };
static char *Abs32_table[] = { "0xabcdef01", NULL };
static char *hosed_table[] = { "hosed", NULL };

static
char **
get_operand(
unsigned long type)
{
	switch(type){
	case Reg8:	return(Reg8_table);
	case Reg16:	return(Reg16_table);
	case Reg32:	return(Reg32_table);
	case RegMM:	return(RegMM_table);
	case RegXMM:	return(RegXMM_table);
	case Imm8:	return(Imm8_table);
	case Imm8S:	return(Imm8S_table);
	case Imm16:	return(Imm16_table);
	case Imm32:	return(Imm32_table);
	case Imm1:	return(Imm1_table);
	case Disp8:	return(Disp8_table);
	case Disp16:	return(Disp16_table);
	case Disp32:	return(Disp32_table);
	case Mem8:	return(Mem8_table);
	case Mem16:	return(Mem16_table);
	case Mem32:	return(Mem32_table);
	case BaseIndex:	return(BaseIndex_table);
	case InOutPortReg:	return(InOutPortReg_table);
	case ShiftCount:	return(ShiftCount_table);
	case Control:	return(Control_table);
	case Debug:	return(Debug_table);
	case Test:	return(Test_table);
	case FloatReg:	return(FloatReg_table);
	case FloatAcc:	return(FloatAcc_table);
	case SReg2:	return(SReg2_table);
	case SReg3:	return(SReg3_table);
	case Acc:	return(Acc_table);
	case JumpAbsolute:	return(JumpAbsolute_table);
	case Abs8:	return(Abs8_table);
	case Abs16:	return(Abs16_table);
	case Abs32:	return(Abs32_table);
	default:	return(hosed_table);
	}
}
