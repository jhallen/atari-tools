/* Convert Mac65 tokenized format to ASCII */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    int x;
    char *name = 0;
    FILE *f;
    unsigned char hdr[4];
    int size;
    int idx;
    unsigned char *src;
    for (x = 1; argv[x]; ++x) {
        if (!strcmp(argv[x], "-h") || !strcmp(argv[x], "--help")) {
            printf("Detokenize Mac65 assembly source\n");
            printf("%s name\n", argv[0]);
            return 0;
        } else {
            if (name) {
                printf("Syntax error\n");
                return -1;
            } else {
                name = argv[x];
            }
        }
    }
    if (!name) {
        printf("Syntax error\n");
        return -1;
    }
    f = fopen(name, "r");
    if (!f) {
        printf("Couldn't open %s\n", name);
        return -1;
    }
    if (4 != fread(hdr, 1, 4, f)) {
        printf("Couldn't read header from file\n");
        return -1;
    }
    if (!(hdr[0] == 0xFE && hdr[1] == 0xFE)) {
        printf("File is not in Mac65 tokenized source format (missing 0xFEFE header)\n");
        return -1;
    }
    size = ((int)hdr[3] << 8) + (int)hdr[2];
    if (!size) {
        printf("File is empty?\n");
        return -1;
    }
    src = (unsigned char *)malloc(size);
    if (size != fread(src, 1, size, f)) {
        printf("Couldn't read rest of file (size %d indicated in header)\n", size);
        return -1;
    }
    for (idx = 0; idx < size;) {
        char label[256];
        int x;
        int linum = (int)src[idx] + ((int)src[idx+1] << 8);
        char *tok = "";
        int typ = 0;
        int len = src[idx+2];
        int ismac = 0;
        label[0] = 0;
        idx += 3;
        len -= 3;

        /* Is there a label? */
        if (len && (src[idx] & 0x80)) {
            /* We have a label */
            int ll = (src[idx] & 0x7F);
            ++idx; --len;
            for (x = 0; x != ll; ++x) {
                label[x] = src[idx + x];
            }
            idx += x;
            len -= x;
            label[x] = 0;
        }

        /* First token */
        if (len) { --len; switch(src[idx++]) {
            case 0: tok = "ERROR -"; break;
            case 1: tok = ".IF"; break;
            case 2: tok = ".ELSE"; break;
            case 3: tok = ".ENDIF"; break;
            case 4: tok = ".MACRO"; break;
            case 5: tok = ".ENDM"; break;
            case 6: tok = ".TITLE"; break;
            case 7: typ = 1; break; /* Macro name */
            case 8: tok = ".PAGE"; break;
            case 9: tok = ".WORD"; break;
            case 10: tok = ".ERROR"; break;
            case 11: tok = ".BYTE"; break;
            case 12: tok = ".SBYTE"; break;
            case 13: tok = ".DBYTE"; break;
            case 14: tok = ".END"; break;
            case 15: tok = ".OPT"; break;
            case 16: tok = ".TAB"; break;
            case 17: tok = ".INCLUDE"; break;
            case 18: tok = ".DS"; break;
            case 19: tok = ".ORG"; break;
            case 20: tok = ".EQU"; break;
            case 21: tok = "BRA"; break;
            case 22: tok = "TRB"; break;
            case 23: tok = "TSB"; break;
            case 24: tok = ".FLOAT"; break;
            case 25: tok = ".CBYTE"; break;
            case 26: tok = ";"; break;
            case 27: tok = ".LOCAL"; break;
            case 28: tok = ".SET"; break;
            case 29: tok = "*="; break;
            case 30: tok = "="; break;
            case 31: tok = ".="; break;
            case 32: tok = "JSR"; break;
            case 33: tok = "JMP"; break;
            case 34: tok = "DEC"; break;
            case 35: tok = "INC"; break;
            case 36: tok = "LDX"; break;
            case 37: tok = "LDY"; break;
            case 38: tok = "STX"; break;
            case 39: tok = "STY"; break;
            case 40: tok = "CPX"; break;
            case 41: tok = "CPY"; break;
            case 42: tok = "BIT"; break;
            case 43: tok = "BRK"; break;
            case 44: tok = "CLC"; break;
            case 45: tok = "CLD"; break;
            case 46: tok = "CLI"; break;
            case 47: tok = "CLV"; break;
            case 48: tok = "DEX"; break;
            case 49: tok = "DEY"; break;
            case 50: tok = "INX"; break;
            case 51: tok = "INY"; break;
            case 52: tok = "NOP"; break;
            case 53: tok = "PHA"; break;
            case 54: tok = "PHP"; break;
            case 55: tok = "PLA"; break;
            case 56: tok = "PLP"; break;
            case 57: tok = "RTI"; break;
            case 58: tok = "RTS"; break;
            case 59: tok = "SEC"; break;
            case 60: tok = "SED"; break;
            case 61: tok = "SEI"; break;
            case 62: tok = "TAX"; break;
            case 63: tok = "TAY"; break;
            case 64: tok = "TSX"; break;
            case 65: tok = "TXA"; break;
            case 66: tok = "TXS"; break;
            case 67: tok = "TYA"; break;
            case 68: tok = "BCC"; break;
            case 69: tok = "BCS"; break;
            case 70: tok = "BEQ"; break;
            case 71: tok = "BMI"; break;
            case 72: tok = "BNE"; break;
            case 73: tok = "BPL"; break;
            case 74: tok = "BVC"; break;
            case 75: tok = "BVS"; break;
            case 76: tok = "ORA"; break;
            case 77: tok = "AND"; break;
            case 78: tok = "EOR"; break;
            case 79: tok = "ADC"; break;
            case 80: tok = "STA"; break;
            case 81: tok = "LDA"; break;
            case 82: tok = "CMP"; break;
            case 83: tok = "SBC"; break;
            case 84: tok = "ASL"; break;
            case 85: tok = "ROL"; break;
            case 86: tok = "LSR"; break;
            case 87: tok = "ROR"; break;
            case 88: typ = 2; break; /* Comment line */
            case 89: tok = "STZ"; break;
            case 90: tok = "DEA"; break;
            case 91: tok = "INA"; break;
            case 92: tok = "PHX"; break;
            case 93: tok = "PHY"; break;
            case 94: tok = "PLX"; break;
            case 95: tok = "PLY"; break;
            default: {
                printf("Unknown token %d\n", src[idx - 1]);
                return -1;
            }
        } }

        if (typ == 0) { /* Normal */
            if (label[0])
                printf("%s	%s	", label, tok);
            else
                printf("	%s	", tok);
        } else if (typ == 1) { /* Macro then normal */
            if (label[0])
                printf("%s	", label);
            else
                printf("	");
            ismac = 1;
        } else if (typ ==2) { /* Comment */
            if (label[0])
                printf("%s ", label);
            for (x = 0; x != len; ++x)
                putchar(src[idx + x]);
            printf("\n");
            goto next;
        }


        while (len) {
            tok = "";
            typ = 0;
            --len;
            /* Is there a string? */
            if (src[idx] & 0x80) {
                /* We have a label */
                int ll = (src[idx] & 0x7F);
                ++idx;
                for (x = 0; x != ll; ++x) {
                    label[x] = src[idx + x];
                }
                idx += x;
                len -= x;
                label[x] = 0;
                typ = 7;
            } else {
                switch (src[idx++]) {
                    case 5: tok = "$"; typ = 1; break;
                    case 6: tok = "$"; typ = 2; break;
                    case 7: tok = ""; typ = 3; break;
                    case 8: tok = ""; typ = 4; break;
                    case 10: tok = "'"; typ = 5; break;
                    case 11: tok = "%$"; break;
                    case 12: tok = "%"; break;
                    case 13: tok = "*"; break;
                    case 18: tok = "+"; break;
                    case 19: tok = "-"; break;
                    case 20: tok = "*"; break;
                    case 21: tok = "/"; break;
                    case 22: tok = "&"; break;
                    case 24: tok = "="; break;
                    case 25: tok = "<="; break;
                    case 26: tok = ">="; break;
                    case 27: tok = "<>"; break;
                    case 28: tok = ">"; break;
                    case 29: tok = "<"; break;
                    case 30: tok = "-"; break;
                    case 31: tok = "["; break;
                    case 32: tok = "]"; break;
                    case 36: tok = "!"; break;
                    case 37: tok = "^"; break;
                    case 39: tok = "\\"; break;
                    case 47: tok = ".REF"; break;
                    case 48: tok = ".DEF"; break;
                    case 49: tok = ".NOT"; break;
                    case 50: tok = ".AND"; break;
                    case 51: tok = ".OR"; break;
                    case 52: tok = "<"; break;
                    case 53: tok = ">"; break;
                    case 54: tok = ",X)"; break;
                    case 55: tok = "),Y"; break;
                    case 56: tok = ",Y"; break;
                    case 57: tok = ",X"; break;
                    case 58: tok = ")"; break;
                    case 59: tok = ";"; typ = 6; break;
                    case 61: tok = ","; break;
                    case 62: tok = "#"; break;
                    case 63: tok = "A"; break;
                    case 64: tok = "("; break;
                    case 65: tok = "\""; break;
                    case 69: tok = "NO"; break;
                    case 70: tok = "OBJ"; break;
                    case 71: tok = "ERR"; break;
                    case 72: tok = "EJECT"; break;
                    case 73: tok = "LIST"; break;
                    case 74: tok = "XREF"; break;
                    case 75: tok = "MLIST"; break;
                    case 76: tok = "CLIST"; break;
                    case 77: tok = "NUM"; break;
                    default: {
                        printf("Unknown token %d\n", src[idx - 1]);
                        return -1;
                    }
                }
            }
            if (typ == 0) {
                printf("%s", tok);
            } else if (typ == 1) {
                printf("%s%4.4x", tok, (int)src[idx] + ((int)src[idx + 1]));
                idx += 2; len -= 2;
            } else if (typ == 2) {
                printf("%s%2.2x", tok, (int)src[idx]);
                idx += 1; len -= 1;
            } else if (typ == 3) {
                printf("%s%d", tok, (int)src[idx] + ((int)src[idx + 1]));
                idx += 2; len -= 2;
            } else if (typ == 4) {
                printf("%s%d", tok, (int)src[idx]);
                idx += 1; len -= 1;
            } else if (typ == 5) {
                printf("%s%c", tok, src[idx]);
                idx += 1; len -= 1;
            } else if (typ == 6) {
                putchar('\t');
                for (x = 0; x != len; ++x) {
                    putchar(src[idx + x]);
                }
                idx += x;
                len -= x;
            } else if (typ == 7) {
                if (ismac) {
                    printf("%s	", label);
                    ismac = 0;
                } else
                    printf("%s", label);
            }
        }

        printf("\n");

        next:
        idx += len;
    }
}
