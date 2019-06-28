#include <stdio.h>
#include "../../assembler/src/assembler.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        puts("usage: ifn ofn dfn");
    }
    if (argc == 3) {
        return !assemble(argv[1], argv[2], 0);
    } else {
        return !assemble(argv[1], argv[2], argv[3]);
    }
}