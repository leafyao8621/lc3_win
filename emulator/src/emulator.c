#include <stdio.h>
#include "emulator.h"

static int16_t mem[1 << 16];
_Bool halt = 0;
_Bool out_flag = 0;
char out_buf[1000];
static struct CPU CPU;

void disassemble(int16_t ins, char* out) {
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

void execute(int16_t ins, char* out) {
    int16_t res = 0;
    char *op = out;
    switch (ins & 0xf000) {
    case 1 << 12: //add
        if (ins & 0x0020) {
            res = CPU.r[(ins & 0x1c0) >> 6] +
                  ((int16_t)((ins & 0x1f) << 11) >> 11);
        } else {
            res = CPU.r[(ins & 0x1c0) >> 6] +
                  CPU.r[ins & 0x7];
        }
        CPU.r[(ins & 0xe00) >> 9] = res;
        CPU.cc = res ? (res < 0 ? 0x4 : 0x1) : 0x2;
        return;
    case 5 << 12: //and
        if (ins & 0x0020) {
            res = CPU.r[(ins & 0x1c0) >> 6] &
                  ((int16_t)((ins & 0x1f) << 11) >> 11);
        } else {
            res = CPU.r[(ins & 0x1c0) >> 6] &
                  CPU.r[ins & 0x7];
        }
        CPU.r[(ins & 0xe00) >> 9] = res;
        CPU.cc = res ? (res < 0 ? 0x4 : 0x1) : 0x2;
        return;
    case 0: //br
        if (ins & 0xe00) {
            if ((ins & 0xe00) >> 9 == CPU.cc) {
                CPU.pc += (int16_t)((ins & 0x1ff) << 7) >> 7;
            }
        } else {
            CPU.pc += (int16_t)((ins & 0x1ff) << 7) >> 7;
        }
        return;
    case 0xc << 12: //jmp
        CPU.pc = CPU.r[(ins & 0x1c0) >> 6];
        return;
    case 0x4 << 12: //jsr jsrr
        CPU.r[7] = CPU.pc;
        CPU.pc += ins & 0x800 ? (int16_t)((ins & 0x7ff) << 5) >> 5 :
                                CPU.r[(ins & 0x1c0) >> 6];
        return;
    case 0x2 << 12: //ld
        res = mem[CPU.pc + ((int16_t)((ins & 0x1ff) << 7) >> 7) + 1];
        CPU.r[(ins & 0xe00) >> 9] = res;
        CPU.cc = res ? (res < 0 ? 0x4 : 0x1) : 0x2;
        return;
    case 0xa << 12: //ldi
        res = mem[mem[CPU.pc + ((int16_t)((ins & 0x1ff) << 7) >> 7) + 1]];
        CPU.r[(ins & 0xe00) >> 9] = res;
        CPU.cc = res ? (res < 0 ? 0x4 : 0x1) : 0x2;
        return;
    case 0x6 << 12: //ldr
        res = mem[(uint16_t)(CPU.r[(ins & 0x1cf) >> 6]) +
                  ((int16_t)((ins & 0x3f) << 10) >> 10)];
        CPU.r[(ins & 0xe00) >> 9] = res;
        CPU.cc = res ? (res < 0 ? 0x4 : 0x1) : 0x2;
        return;
    case 0xe << 12: //lea
        res = CPU.pc + ((int16_t)((ins & 0x1ff) << 7) >> 7) + 1;
        CPU.r[(ins & 0xe00) >> 9] = res;
        CPU.cc = res ? (res < 0 ? 0x4 : 0x1) : 0x2;
        return;
    case 0x9 << 12: //not
        res = ~CPU.r[(ins & 0x1c0) >> 6];
        CPU.r[(ins & 0xe00) >> 9] = res;
        CPU.cc = res ? (res < 0 ? 0x4 : 0x1) : 0x2;
        return;
    case 0x3 << 12: //st
        mem[CPU.pc + ((int16_t)((ins & 0x1ff) << 7) >> 7) + 1] =
        CPU.r[(ins & 0xe00) >> 9];
        return;
    case 0xb << 12: //sti
        mem[mem[CPU.pc + ((int16_t)((ins & 0x1ff) << 7) >> 7) + 1]] =
        CPU.r[(ins & 0xe00) >> 9];
        return;
    case 0x7 << 12: //str
        mem[(uint16_t)(CPU.r[(ins & 0x1c0) >> 6]) +
            ((int16_t)((ins & 0x3f) << 10) >> 10)] =
        CPU.r[(ins & 0xe00) >> 9];
        return;
    case 0xf << 12: //trap
        switch (ins & 0xff) {
        case 0x21:
            out_flag = 1;
            if (out) {
                *out = CPU.r[0];
                out[1] = 0;
            }
            return;
        case 0x22:
            out_flag = 1;
            for (int16_t* i = mem + CPU.r[0]; *i; *(op++) = *(i++));
            return;
        case 0x25:
            halt = 1;
            return;
        }
    }
}

struct CPU get_cpu(void) {
    return CPU;
}

int16_t get_mem(int add) {
    return mem[add];
}

void print_cpu(void) {
    for (int16_t i = 0, *j = CPU.r; i < 8; printf("R%d: 0x%04hx %hd\n", i++, *j, *j), j++);
    printf("CC: %c\n", CPU.cc & 0x4 ? 'n' : CPU.cc & 0x2 ? 'z' : 'p');
    printf("PC: 0x%04hx\n", CPU.pc);
}

void load(char* fn) {
    FILE* inf = fopen(fn, "rb");
    uint8_t n_blk = 0;
    fread(&n_blk, 1, 1, inf);
    char buf[100];
    for (int i = 0; i < n_blk && !feof(inf); i++) {
        uint16_t header[2];
        fread(header, 2, 2, inf);
        if (!i) CPU.pc = header[0];
        int16_t* mem_ptr = mem + header[0];
        for (int j = 0; j < header[1] && !feof(inf); j++, fread(mem_ptr++, 2, 1, inf));
    }
    fclose(inf);
}

void step(_Bool is_console) {
    if (!halt) {
        execute(mem[CPU.pc], out_buf);
        CPU.pc++;
        if (is_console) {
            if (out_flag) printf("%s", out_buf);
            out_flag = 0;
        }
    }
}

void run(_Bool dbg) {
    char buf[50];
    for (halt = 0; !halt;) {
        disassemble(mem[CPU.pc], buf);
        if (dbg) {
            printf("mem 0x%04hx\nins 0x%04hx\nasm %s\n",
                   CPU.pc, mem[CPU.pc], buf);
        }
        // execute(mem[CPU.pc], out_buf);
        step(1);
        if (dbg) {
            putchar(10);
            print_cpu();
            putchar(10);
        }
    }
}

_Bool get_out_flag(void) {
    return out_flag;
}

void reset_out_flag(void) {
    out_flag = 0;
}

_Bool get_halt(void) {
    return halt;
}

void set_halt(void) {
    halt = 1;
}

void unhalt(void) {
    halt = 0;
}