#include <stdio.h>
#include "../../emulator/src/emulator.h"
#include "../../assembler/src/assembler.h"

int main(void) {
    assemble("fibb.asm", "fibb.obj", "fibb.dmp");
    load("fibb.obj");
    run(1);
}
