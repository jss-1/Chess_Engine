// main.c
#include <stdio.h>
#include "attack.h"
#include "board.h"

// Assumes you have a function like:
//   void init_startpos(position *pos);
// that sets up 'pos' to the standard chess opening position.

int main(int argc, char **argv) {
    // 1) Build all attack tables exactly once
    init_attacks();

    // 2) Initialize a position to the standard start
    Position pos;
    init_startpos(&pos);

    // 3) (Example) Do a perft count to depth 5
    uint64_t nodes = perft(&pos, 5);
    printf("Perft(5) = %llu\n", (unsigned long long)nodes);

    // 4) Or hand off to your search/UI
    // run_engine(&pos);

    return 0;
}
