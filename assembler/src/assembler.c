#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "assembler.h"

static int32_t mem_add = -1;
static uint32_t line_num = 1;
static int16_t offset = 0;
static uint8_t num_blk = 1;

enum ErrorCode {
    NO_ERROR,
    UNRECOGNIZED_TOKEN,
    INVALID_OPERAND,
    INVALID_REGISTER,
    OVERFLOW,
    DUPLICATE_LABEL,
    INVALID_LABEL
};

static char* err_code_str[] = {
    "No Error",
    "Unrecognized Token",
    "Invalid Operand",
    "Invalid Register",
    "Overflow",
    "Duplicate Label",
    "Invalid Label"
};

static enum ErrorCode err_code = NO_ERROR;
char err_str[1000];

enum Operator {
    ADD,
    AND,
    BR,
    BRN,
    BRZ,
    BRP,
    BRNZ,
    BRZP,
    BRNP,
    BRNZP,
    JMP,
    JSR,
    JSRR,
    LD,
    LDI,
    LDR,
    LEA,
    NOT,
    ST,
    STI,
    STR,
    TRAP
};

static uint16_t op_code[] = {
    0x1000,
    0x5000,
    0x0e00,
    0x0800,
    0x0400,
    0x0200,
    0x0c00,
    0x0600,
    0x0a00,
    0x0e00,
    0xc000,
    0x4800,
    0x4000,
    0x2000,
    0xa000,
    0x6000,
    0xe000,
    0x903f,
    0x3000,
    0xb000,
    0x7000,
    0xf000
};

enum Directive {
    ORIG,
    END,
    FILL,
    STRING,
    BLK,
    HALT,
    RET
};

enum Operand {
    NOP,
    NUM,
    NUM5,
    NUM6,
    NUM8,
    REG,
    STRLIT,
    LBL9,
    LBL11
};

static enum Operand operator_requirement[][3] = {
    {REG, REG, NUM5},
    {REG, REG, NUM5},
    {LBL9, NOP, NOP},
    {LBL9, NOP, NOP},
    {LBL9, NOP, NOP},
    {LBL9, NOP, NOP},
    {LBL9, NOP, NOP},
    {LBL9, NOP, NOP},
    {LBL9, NOP, NOP},
    {LBL9, NOP, NOP},
    {REG, NOP, NOP},
    {LBL11, NOP, NOP},
    {REG, NOP, NOP},
    {REG, LBL9, NOP},
    {REG, LBL9, NOP},
    {REG, REG, NUM6},
    {REG, LBL9, NOP},
    {REG, REG, NOP},
    {REG, LBL9, NOP},
    {REG, LBL9, NOP},
    {REG, REG, NUM6},
    {NUM8, NOP, NOP}
};

enum Operand directive_requirement[] = {
    NUM,
    NOP,
    NUM,
    STRLIT,
    NUM,
    NOP,
    NOP
};

struct Pair {
    char key[9];
    uint16_t val;
} lookup[256][100];

static uint8_t cnt[256] = {0};

struct Data {
    int16_t num;
    char lbl[9];
};

enum Type {
    HEADER,
    DIR,
    OP,
    OP_LBL
};

union Prefix {
    uint16_t mem_add;
    enum Operator op;
    enum Directive dir;
};

struct Token {
    enum Type type;
    union Prefix prefix;
    struct Data data;
} code[1 << 16];

struct Token* code_end_ptr = code;

static void trim_token(const char* str, char* out) {
    memset(out, 0, 9);
    const char *i = str;
    char *j = out;
    for (int k = 0; k < 8 && *i && *i != 10; *j = *i != '.' ? *i & 0xdf : '.',
                                             i++, j++, k++);
}

static _Bool check_operator(const char* str, enum Operator* out) {
    if (!str) return 0;
    switch (*(uint64_t*)str) {
    case 0x444441: //ADD
        *out = ADD;
        return 1;
    case 0x444e41: //AND
        *out = AND;
        return 1;
    case 0x5242: //BR
        *out = BR;
        return 1;
    case 0x4e5242: //BRN
        *out = BRN;
        return 1;
    case 0x5a5242: //BRZ
        *out = BRZ;
        return 1;
    case 0x505242: //BRP
        *out = BRP;
        return 1;
    case 0x5a4e5242: //BRNZ
        *out = BRNZ;
        return 1;
    case 0x505a5242: //BRZP
        *out = BRZP;
        return 1;
    case 0x504e5242: //BRNP
        *out = BRNP;
        return 1;
    case 0x505a4e5242: //BRNZP
        *out = BRNZP;
        return 1;
    case 0x504d4a: //JMP
        *out = JMP;
        return 1;
    case 0x52534a: //JSR
        *out = JSR;
        return 1;
    case 0x5252534a: //JSRR
        *out = JSRR;
        return 1;
    case 0x444c: //LD
        *out = LD;
        return 1;
    case 0x49444c: //LDI
        *out = LDI;
        return 1;
    case 0x52444c: //LDR
        *out = LDR;
        return 1;
    case 0x41454c: //LEA
        *out = LEA;
        return 1;
    case 0x544f4e: //NOT
        *out = NOT;
        return 1;
    case 0x5453: //ST
        *out = ST;
        return 1;
    case 0x495453: //STI
        *out = STI;
        return 1;
    case 0x525453: //STR
        *out = STR;
        return 1;
    case 0x50415254: //TRAP
        *out = TRAP;
        return 1;
    default:
        return 0;
    }
}

static _Bool check_directive(const char* str, enum Directive* out) {
    if (!str) return 0;
    switch (*(uint64_t*)str) {
    case 0x4749524f2e: //.ORIG
        *out = ORIG;
        return 1;
    case 0x444e452e: //.END
        *out = END;
        return 1;
    case 0x4c4c49462e: //.FILL
        *out = FILL;
        return 1;
    case 0x5a474e495254532e: //.STRINGZ
        *out = STRING;
        return 1;
    case 0x574b4c422e: //.BLKW
        *out = BLK;
        return 1;
    case 0x544c4148: //HALT
        *out = HALT;
        return 1;
    case 0x544552: //RET
        *out = RET;
        return 1;
    default:
        return 0;
    }
}

static _Bool check_operand_operator(const enum Operator op, char* str) {
    if (!str || mem_add == -1) return 0;
    char* seg_buf = str;
    int num = 0;
    _Bool cont = 1;
    char trim_buf[9];
    for (int i = 0; cont && i < 3; i++) {
        switch (operator_requirement[op][i]) {
        case REG:
            seg_buf = strtok(i ? NULL : str, ",");
            if (!seg_buf) {
                err_code = INVALID_OPERAND;
                return 0;
            }
            for (; *seg_buf == 32 && *seg_buf; seg_buf++);
            if (((*seg_buf) & 0xdf) != 'R') return 0;
            seg_buf++;
            if (!*seg_buf) {
                err_code = INVALID_OPERAND;
                return 0;
            }
            num = atoi(seg_buf);
            if (num < 0 || num > 7) {
                err_code = INVALID_REGISTER;
                return 0;
            }
            switch (op) {
            case JMP:
            case JSRR:
                code_end_ptr->data.num |= num << 6;
                break;
            default:
                switch (i) {
                case 0:
                    code_end_ptr->data.num |= num << 9;
                    break;
                case 1:
                    code_end_ptr->data.num |= num << 6;
                    break;
                }
                break;
            }
            // printf("reg %d ", num);
            break;
        case NUM5:
            seg_buf = strtok(i ? NULL : str, ",");
            if (!seg_buf) {
                err_code = INVALID_OPERAND;
                return 0;
            }
            for (; *seg_buf == 32 && *seg_buf; seg_buf++);
            switch (*seg_buf & 0xdf) {
            case 'R':
                seg_buf++;
                if (!*seg_buf) {
                    err_code = INVALID_OPERAND;
                    return 0;
                }
                num = atoi(seg_buf);
                if (num < 0 || num > 7) {
                    err_code = INVALID_REGISTER;
                    return 0;
                }
                // printf("reg %d ", num);
                break;
            case 'X':
                seg_buf++;
                if (!*seg_buf) {
                    err_code = INVALID_OPERAND;
                    return 0;
                }
                num = strtol(seg_buf, NULL, 16);
                if (num < -16 || num > 15) {
                    err_code = OVERFLOW;
                    return 0;
                }
                code_end_ptr->data.num |= 0x20;
                // printf("num5 0x%04hx ", num);
                break;
            case '#' & 0xdf:
                seg_buf++;
                if (!*seg_buf) {
                    err_code = INVALID_OPERAND;
                    return 0;
                }
                num = atoi(seg_buf);
                if (num < -16 || num > 15) {
                    err_code = OVERFLOW;
                    return 0;
                }
                code_end_ptr->data.num |= 0x20;
                // printf("num5 %hd ", num);
                break;
            default:
                err_code = INVALID_OPERAND;
                return 0;
            }
            code_end_ptr->data.num |= num & 0x1f;
            break;
        case NUM6:
            seg_buf = strtok(i ? NULL : str, ",");
            if (!seg_buf) {
                err_code = INVALID_OPERAND;
                return 0;
            }
            for (; *seg_buf == 32 && *seg_buf; seg_buf++);
            switch (*seg_buf & 0xdf) {
            case 'X':
                seg_buf++;
                if (!*seg_buf) {
                    err_code = INVALID_OPERAND;
                    return 0;
                }
                num = strtol(seg_buf, NULL, 16);
                if (num < -32 || num > 31) {
                    err_code = OVERFLOW;
                    return 0;
                }
                // printf("num6 0x%04hx ", num);
                break;
            case '#' & 0xdf:
                seg_buf++;
                if (!*seg_buf) {
                    err_code = INVALID_OPERAND;
                    return 0;
                }
                num = atoi(seg_buf);
                if (num < -32 || num > 31) {
                    err_code = OVERFLOW;
                    return 0;
                }
                // printf("num6 %hd ", num);
                break;
            default:
                err_code = INVALID_OPERAND;
                return 0;
            }
            code_end_ptr->data.num |= (num & 0x3f);
            break;
        case NUM8:
            seg_buf = strtok(i ? NULL : str, ",");
            if (!seg_buf) {
                err_code = INVALID_OPERAND;
                return 0;
            }
            for (; *seg_buf == 32 && *seg_buf; seg_buf++);
            switch (*seg_buf & 0xdf) {
            case 'X':
                seg_buf++;
                if (!*seg_buf) {
                    err_code = INVALID_OPERAND;
                    return 0;
                }
                num = strtol(seg_buf, NULL, 16);
                if (num < -128 || num > 127) {
                    err_code = OVERFLOW;
                    return 0;
                }
                // printf("num8 0x%04hx ", num);
                break;
            case '#' & 0xdf:
                seg_buf++;
                if (!*seg_buf) {
                    err_code = INVALID_OPERAND;
                    return 0;
                }
                num = atoi(seg_buf);
                if (num < -128 || num > 127) {
                    err_code = OVERFLOW;
                    return 0;
                }
                // printf("num8 %hd ", num);
                break;
            default:
                err_code = INVALID_OPERAND;
                return 0;
            }
            code_end_ptr->data.num |= (num & 0xff);
            break;
        case LBL9:
            seg_buf = strtok(i ? NULL : str, ",");
            if (!seg_buf) {
                err_code = INVALID_OPERAND;
                return 0;
            }
            for (; *seg_buf == 32 && *seg_buf; seg_buf++);
            switch (*seg_buf & 0xdf) {
            case 'X':
                seg_buf++;
                if (!*seg_buf) {
                    err_code = INVALID_OPERAND;
                    return 0;
                }
                num = strtol(seg_buf, NULL, 16);
                if (num < -256 || num > 255) {
                    err_code = OVERFLOW;
                    return 0;
                }
                // printf("num9 0x%04hx ", num);
                code_end_ptr->data.num |= (num & 0x1ff);
                break;
            case '#' & 0xdf:
                seg_buf++;
                if (!*seg_buf) {
                    err_code = INVALID_OPERAND;
                    return 0;
                }
                num = atoi(seg_buf);
                if (num < -256 || num > 255) {
                    err_code = OVERFLOW;
                    return 0;
                }
                // printf("num9 %hd ", num);
                code_end_ptr->data.num |= (num & 0x1ff);
                break;
            default:
                trim_token(seg_buf, trim_buf);
                // printf("lbl %s ", trim_buf);
                memcpy(code_end_ptr->data.lbl, trim_buf, 9);
                code_end_ptr->type = OP_LBL;
                break;
            }
            break;
        case LBL11:
            seg_buf = strtok(i ? NULL : str, ",");
            if (!seg_buf) {
                err_code = INVALID_OPERAND;
                return 0;
            }
            for (; *seg_buf == 32 && *seg_buf; seg_buf++);
            switch (*seg_buf & 0xdf) {
            case 'X':
                seg_buf++;
                if (!*seg_buf) {
                    err_code = INVALID_OPERAND;
                    return 0;
                }
                num = strtol(seg_buf, NULL, 16);
                if (num < -1024 || num > 1023) {
                    err_code = OVERFLOW;
                    return 0;
                }
                // printf("num11 0x%04hx ", num);
                code_end_ptr->data.num = (num & 0x7ff);
                break;
            case '#' & 0xdf:
                seg_buf++;
                if (!*seg_buf) {
                    err_code = INVALID_OPERAND;
                    return 0;
                }
                num = atoi(seg_buf);
                if (num < -1024 || num > 1023) {
                    err_code = OVERFLOW;
                    return 0;
                }
                // printf("num11 %hd ", num);
                code_end_ptr->data.num = (num & 0x7ff);
                break;
            default:
                trim_token(seg_buf, trim_buf);
                // printf("lbl %s ", trim_buf);
                code_end_ptr->data.num = 0;
                memcpy(code_end_ptr->data.lbl, trim_buf, 9);
                code_end_ptr->type = OP_LBL;
                break;
            }
            break;
        case NOP:
            cont = 0;
            break;
        }
    }
    return 1;
}

static _Bool handle_directive(enum Directive dir, char* str) {
    if (directive_requirement[dir] != NOP && !str) return 0;
    int16_t num = 0;
    char buf[100];
    char *buf_ptr = buf;
    int cnt = 0;
    code_end_ptr->type = DIR;
    code_end_ptr->prefix.dir = dir;
    switch (dir) {
    case ORIG:
        if (mem_add != -1) return 0;
        offset = 0;
        code_end_ptr->type = HEADER;
        break;
    case END:
        if (mem_add == -1) return 0;
        (code_end_ptr - offset)->data.num = offset - 1;
        mem_add = -2;
        break;
    case HALT:
        if (mem_add == -1) return 0;
        // printf("%s ", "machine halted");
        break;
    case RET:
        if (mem_add == -1) return 0;
        // printf("%s ", "return from subroutine");
        break;
    default:
        if (mem_add == -1) return 0;
        break;
    }
    switch (directive_requirement[dir]) {
    case NUM:
        switch (*str & 0xdf) {
        case 'X':
            num = (int16_t)(strtol(str + 1, NULL, 16));
            // printf("num 0x%4hx", num);
            break;
        case ('#' & 0xdf):
            num = (int16_t)(atoi(str + 1));
            // printf("num %hd", num);
            break;
        default:
            return 0;
        }
        break;
    case STRLIT:
        if (*str != '\"') return 0;
        str++;
        for (cnt = 0; cnt < 100 && *str && *str != '\"';
             cnt++, *(buf_ptr++) = *(str++));
        if (*str != '\"') return 0;
        *buf_ptr = 0;
        // printf("str %s ", buf);
        break;
    }
    switch (dir) {
    case ORIG:
        mem_add = num - 1;
        code_end_ptr->prefix.mem_add = num;
        break;
    case FILL:
        code_end_ptr->data.num = num;
        break;
    case STRING:
        // printf("len %d", cnt);
        mem_add += cnt;
        offset += cnt;
        for (char *i = buf; *i; i++, code_end_ptr++) {
            code_end_ptr->type = DIR;
            code_end_ptr->prefix.dir = FILL;
            code_end_ptr->data.num = *i;
        }
        code_end_ptr->type = DIR;
        code_end_ptr->prefix.dir = FILL;
        code_end_ptr->data.num = 0;
        break;
    }
    return 1;
}

static _Bool parse(const char* fn) {
    FILE* inf = fopen(fn, "r");
    char line_buf[150];
    char *seg_buf;
    char trim_buf[9];
    enum Operator op;
    enum Directive dir;
    for (fgets(line_buf, 150, inf); !feof(inf); fgets(line_buf, 150, inf),
                                                line_num++, code_end_ptr++) {
        seg_buf = strtok(line_buf, " ");
        if (*seg_buf != 10) {

            trim_token(seg_buf, trim_buf);
            if (check_operator(trim_buf, &op)) {
                code_end_ptr->type = OP;
                code_end_ptr->prefix.op = op;
                // printf("op %s ", trim_buf);
                seg_buf = strtok(NULL, "\n");
                if (!check_operand_operator(op, seg_buf)) {
                    fclose(inf);
                    return 0;
                }
            } else {
                if (check_directive(trim_buf, &dir)) {
                    // printf("dir %s ", trim_buf);
                    seg_buf = strtok(NULL, "\n");
                    if (!handle_directive(dir, seg_buf)) {
                        fclose(inf);
                        return 0;
                    }
                } else {
                    uint64_t key = *(uint64_t*)trim_buf;
                    uint8_t init = *trim_buf;
                    for (int i = 0; i < cnt[init]; i++) {
                        if (*(uint64_t*)(lookup[init][i].key) == key) {
                            err_code = DUPLICATE_LABEL;
                            fclose(inf);
                            return 0;
                        }
                    }
                    memcpy(lookup[init][cnt[init]].key, trim_buf, 9);
                    lookup[init][cnt[init]++].val = mem_add;
                    // printf("lbl %s add 0x%04hx ",
                    //        lookup[init][cnt[init] - 1].key,
                    //        lookup[init][cnt[init] - 1].val);
                    seg_buf = strtok(0, " ");
                    if (seg_buf) {
                        trim_token(seg_buf, trim_buf);
                        if (check_operator(trim_buf, &op)) {
                            // printf("op %s ", trim_buf);
                            code_end_ptr->type = OP;
                            code_end_ptr->prefix.op = op;
                            seg_buf = strtok(NULL, "\n");
                            if (!check_operand_operator(op, seg_buf)) {
                                err_code = UNRECOGNIZED_TOKEN;
                                fclose(inf);
                                return 0;
                            }
                        } else {
                            if (check_directive(trim_buf, &dir)) {
                                // printf("dir %s ", trim_buf);
                                seg_buf = strtok(NULL, "\n");
                                if (!handle_directive(dir, seg_buf)) {
                                    err_code = UNRECOGNIZED_TOKEN;
                                    fclose(inf);
                                    return 0;
                                }
                            }
                        }
                    } else {
                        mem_add--;
                        code_end_ptr--;
                        offset--;
                    }
                }
            }
            mem_add++;
            offset++;
            // putchar(10);
        } else {
            code_end_ptr--;
        }
    }
    fclose(inf);
    return 1;
}

static void disassemble(int16_t ins, char* out) {
    switch (ins & 0xf000) {
    case 1 << 12: //add
        if (ins & 0x0020) {
            sprintf(out, "ADD R%d,R%d,#%d",
                    (ins & 0xe00) >> 9,
                    (ins & 0x1c0) >> 6,
                    (int16_t)((ins & 0x1f) << 11) >> 11);
        } else {
            sprintf(out, "ADD R%d,R%d,R%d",
                    (ins & 0xe00) >> 9,
                    (ins & 0x1c0) >> 6,
                    ins & 0x7);
        }
        return;
    case 5 << 12: //and
        if (ins & 0x0020) {
            sprintf(out, "AND R%d,R%d,#%d",
                    (ins & 0xe00) >> 9,
                    (ins & 0x1c0) >> 6,
                    (int16_t)((ins & 0x1f) << 11) >> 11);
        } else {
            sprintf(out, "AND R%d,R%d,R%d",
                    (ins & 0xe00) >> 9,
                    (ins & 0x1c0) >> 6,
                    ins & 0x7);
        }
        return;
    case 0: //br
        sprintf(out, "BR%s%s%s #%d",
                ins & 0x800 ? "n" : "",
                ins & 0x400 ? "z" : "",
                ins & 0x200 ? "p" : "",
                (int16_t)((ins & 0x1ff) << 7) >> 7);
        return;
    case 0xc << 12: //jmp
        sprintf(out, "JMP R%d", (ins & 0x1c0) >> 6);
        return;
    case 0x4 << 12: //jsr jsrr
        if (ins & 0x800) {
            sprintf(out, "JSR #%d",
                    (int16_t)((ins & 0x7ff) << 5) >> 5);
        } else {
            sprintf(out, "JSRR R%d",
                    (ins & 0x1c0) >> 6);
        }
        return;
    case 0x2 << 12: //ld
        sprintf(out, "LD R%d,#%d",
                (ins & 0xe00) >> 9, (int16_t)((ins & 0x1ff) << 7) >> 7);
        return;
    case 0xa << 12: //ldi
        sprintf(out, "LDI R%d,#%d",
                (ins & 0xe00) >> 9, (int16_t)((ins & 0x1ff) << 7) >> 7);
        return;
    case 0x6 << 12: //ldr
        sprintf(out, "LDR R%d,R%d,#%d",
                (ins & 0xe00) >> 9, (ins & 0x1c0) >> 6,
                (int16_t)((ins & 0x3f) << 10) >> 10);
        return;
    case 0xe << 12: //lea
        sprintf(out, "LEA R%d #%d",
                (ins & 0xe00) >> 9, (int16_t)((ins & 0x1ff) << 7) >> 7);
        return;
    case 0x9 << 12: //not
        sprintf(out, "NOT R%d,R%d",
                (ins & 0xe00) >> 9, (ins & 0x1c0) >> 6);
        return;
    case 0x3 << 12: //st
        sprintf(out, "ST R%d,#%d",
                (ins & 0xe00) >> 9, (int16_t)((ins & 0x1ff) << 7) >> 7);
        return;
    case 0xb << 12: //sti
        sprintf(out, "STI R%d,#%d",
                (ins & 0xe00) >> 9, (int16_t)((ins & 0x1ff) << 7) >> 7);
        return;
    case 0x7 << 12: //str
        sprintf(out, "STR R%d,R%d,#%d",
                (ins & 0xe00) >> 9, (ins & 0x1c0) >> 6,
                (int16_t)((ins & 0x3f) << 10) >> 10);
        return;
    case 0xf << 12: //trap
        sprintf(out, "TRAP x%x", ins & 0xff);
        return;
    }
}

static _Bool commit(const char* fn_out, const char* fn_dmp) {
    FILE* fout = fopen(fn_out, "wb");
    FILE* fdmp = 0;
    if (fn_dmp) fdmp = fopen(fn_dmp, "w");
    uint16_t out = 0;
    uint64_t key = 0;
    struct Pair* tgt = 0;
    int j = 0;
    char buf[100];
    fwrite(&num_blk, 1, 1, fout);
    for (struct Token* i = code; i != code_end_ptr; i++, mem_add++) {
        switch (i->type) {
        case OP:
            out = op_code[i->prefix.op] | i->data.num;
            disassemble(out, buf);
            if (fdmp) {
                fprintf(fdmp, "0x%04hx 0x%04hx %s\n", mem_add, out, buf);
            }
            fwrite(&out, 2, 1, fout);
            break;
        case OP_LBL:
            key = *(uint64_t*)(i->data.lbl);
            for (j = 0, tgt = lookup[*(i->data.lbl)];
                 *(uint64_t*)(tgt->key) != key && j < cnt[*(i->data.lbl)];
                 j++, tgt++);
            if (j == cnt[*(i->data.lbl)]) {
                err_code = INVALID_LABEL;
                return 0;
            }
            out = op_code[i->prefix.op] | i->data.num |
                  ((tgt->val - (mem_add + 1)) &
                  (i->prefix.op == JSR ? 0x7ff : 0x1ff));
            disassemble(out, buf);
            if (fdmp) {
                fprintf(fdmp, "0x%04hx 0x%04hx %s %s\n", mem_add, out, buf, tgt->key);
            }
            fwrite(&out, 2, 1, fout);
            break;
        case DIR:
            switch (i->prefix.dir) {
            case HALT:
                out = 0xff25;
                fwrite(&out, 2, 1, fout);
                disassemble(out, buf);
                if (fdmp) {
                    fprintf(fdmp, "0x%04hx 0x%04hx %s\n", mem_add, out, buf);
                }
                break;
            case RET:
                out = 0xc1c0;
                fwrite(&out, 2, 1, fout);
                disassemble(out, buf);
                if (fdmp) {
                    fprintf(fdmp, "0x%04hx 0x%04hx %s\n", mem_add, out, buf);
                }
                break;
            case FILL:
                out = i->data.num;
                fwrite(&out, 2, 1, fout);
                disassemble(out, buf);
                if (fdmp) {
                    fprintf(fdmp, "0x%04hx 0x%04hx %s\n", mem_add, out, buf);
                }
                break;
            }
            break;
        case HEADER:
            mem_add = i->prefix.mem_add - 1;
            if (fdmp) {
                fprintf(fdmp, "mem add 0x%04hx offset %d\n", i->prefix.mem_add, i->data.num);
            }
            fwrite(&(i->prefix.mem_add), 2, 1, fout);
            fwrite(&(i->data.num), 2, 1, fout);
            break;
        default:
            fclose(fout);
            return 0;
        }
    }
    fclose(fout);
    if (fn_dmp) fclose(fdmp);
    return 1;
}

_Bool assemble(const char* fn_in, const char* fn_out, const char* fn_dmp) {
    code_end_ptr = code;
    mem_add = -1;
    line_num = 1;
    offset = 0;
    num_blk = 1;
    memset(cnt, 0, 256);
    if (!parse(fn_in)) {
        printf("Error in Pass 1 on Line %d\n"
               "Error Code %d %s\n", line_num, err_code,
               err_code_str[err_code]);
        snprintf(err_str, 1000,
                 "Error in Pass 1 on Line %d\n"
                 "Error Code %d %s\n", line_num, err_code,
                 err_code_str[err_code]);
        return 0;
    }
    puts("Pass 1 No Error");
    if (!commit(fn_out, fn_dmp)) {
        printf("Error in Pass 2 on Line %d\n"
               "Error Code %d %s\n", line_num, err_code,
               err_code_str[err_code]);
        snprintf(err_str, 1000,
                 "Error in Pass 2 on Line %d\n"
                 "Error Code %d %s\n", line_num, err_code,
                 err_code_str[err_code]);
        return 0;
    }
    puts("Pass 2 No Error");
    snprintf(err_str, 1000, "%s", "No Error");
    return 1;
}
