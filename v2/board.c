// Header
#include <stdio.h>

// Bitboard data type
#define U64 unsigned long long

// Bitboard representation of a chessboard
// 0x0000000000000001 = a1
// 0x0000000000000002 = b1
// 0x0000000000000003 = c1
// 0x0000000000000004 = d1
// 0x0000000000000005 = e1
// 0x0000000000000006 = f1
// 0x0000000000000007 = g1
// 0x0000000000000008 = h1
// 0x0000000000000100 = a2
enum {
    a8, b8, c8, d8, e8, f8, g8, h8,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a1, b1, c1, d1, e1, f1, g1, h1
};

// Color
enum {white, black};

/*
"a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
"a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
"a6", "b6", "c6", "d6", "e6", f6", g6", h6",
"a5", "b5", c5", d5", e5", f5", g5", h5",
"a4", b4", c4", d4", e4", f4", g4", h4",
"a3", b3", c3", d3", e3", f3", g3", h3",
"a2", b2", c2", d2", e2", f2", g2", h2",
"a1", b1", c1", d1", e1", f1", g1", h1"
*/


// Macros
#define set_bit(bitboard, square) (bitboard |= (1ULL << square)) // Set the bit at the square position // (1ULL << square) shifts the bit 1 to the left by square number of times and ORs it with the bitboard
#define get_bit(bitboard, square) (bitboard & (1ULL << square)) // Get the bit at the square position // (1ULL << square) shifts the bit 1 to the left by square number of times and ANDs it with the bitboard
#define pop_bit(bitboard, square) (bitboard &= ~(1ULL << square)) // Pop the bit at the square position // (1ULL << square) shifts the bit 1 to the left by square number of times, negation makes that bit reset to 0 and ANDs it with the bitboard

// Print Bitboard
void print_bitboard(U64 bitboard){
    printf("\n");
    for (int rank = 0; rank < 8; rank++){
        

        for (int file = 0; file < 8; file++){
            int square = rank * 8 + file;
            
            if (!file){
                printf("%d  ", 8 - rank);
            }
            
            printf("%d ", get_bit(bitboard, square) ? 1 : 0);


        }

        printf("\n");
    }
    printf("   a b c d e f g h\n");

    printf("\nBitboard: %llud\n", bitboard);
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++ Attacks

const U64 not_a_file = 18374403900871474942ULL; // 0xFEFEFEFEFEFEFEFE
const U64 not_h_file = 9187201950435737471ULL;  // 0x7F7F7F7F7F7F7F7F
const U64 not_hg_file = 4557430888798830399ULL; // 0x3F3F3F3F3F3F3F3F
const U64 not_ab_file = 18229723555195321596ULL; // 0xFDFDFDFDFDFDFDFD


const U64 not_rank_1 = 0xFFFFFFFFFFFFFF00ULL;
const U64 not_rank_8 = 0x00FFFFFFFFFFFFFFULL;
const U64 not_rank_12 = 0xFFFFFFFFFFFF0000ULL;
const U64 not_rank_78 = 0x0000FFFFFFFFFFFFULL;


// --------------------------------- Pawn Attacks ---------------------------------
U64 pawn_attacks[2][64];
U64 mask_pawn_attacks(int square, int side){
    // pawn bitboard
    U64 bitboard = 0ULL;

    // result attacks bitboard
    U64 attacks = 0ULL;

    // set piece on board
    set_bit(bitboard, square);

    // white pawn attacks
    if (side == white){
        if (square < 8){
            return (U64) 0UL;
        }
        if ((bitboard >> 7) & not_a_file) attacks |= (bitboard >> 7);
        if ((bitboard >> 9) & not_h_file) attacks |= (bitboard >> 9);
    }
    else{
        if (square > 55){
            return (U64) 0ULL;
        } 
        if ((bitboard << 7) & not_h_file) attacks |= (bitboard << 7);
        if ((bitboard << 9) & not_a_file) attacks |= (bitboard << 9);
    }

    // return attack map
    return attacks;
}

// --------------------------------- Knight Attacks ---------------------------------
U64 knight_attacks[64];
U64 mask_knight_attacks(int square){
    // knight bitboard
    U64 bitboard = 0ULL;

    // attacks bitboard
    U64 attacks = 0ULL;

    set_bit(bitboard, square);

    // knight attacks 17, 15, 10, 6
    // 17 = Right Down Down(+) or Left Up Up(-)
    // 15 = Left Down Down(+) or Right Up Up(-)
    // 10 = Right Right Down(+) or Left Left Up(-)
    // 6 = Left Left Down(+) or Right Right Up(-)

    if ((bitboard >> 17) & not_h_file) attacks |= (bitboard >> 17);
    if ((bitboard >> 15) & not_a_file) attacks |= (bitboard >> 15);
    if ((bitboard >> 10) & not_hg_file) attacks |= (bitboard >> 10);
    if ((bitboard >> 6) & not_ab_file) attacks |= (bitboard >> 6);

    if ((bitboard << 17) & not_a_file) attacks |= (bitboard << 17);
    if ((bitboard << 15) & not_h_file) attacks |= (bitboard << 15);
    if ((bitboard << 10) & not_ab_file) attacks |= (bitboard << 10);
    if ((bitboard << 6) & not_hg_file) attacks |= (bitboard << 6);

    // return attack map
    return attacks;
}


//-----------------------------------------------------------------Initialize attacks -------------------------------------
void init_leaper_attacks(int side){
    // pawn attacks
    for(int square = 0; square < 64; square++){
        pawn_attacks[side][square] = mask_pawn_attacks(square, side);
    }
}

// Driver
int main() {
    printf("Bitboard Chess Engine\n");

    // initialize attacks
    init_leaper_attacks(white); // white pawns
    init_leaper_attacks(black); // black pawns

    U64 bitboard = 0ULL;
    for (int file = 0; file < 8; file++){
        if (file > 1 && file < 6){
            continue;
        }
        for (int rank = 0; rank < 8; rank++){
            if (rank > 1 && rank < 6){
                continue;
            }
            int square = rank * 8 + file;
            printf("\n%c%d\n",(char)(file + 'a'),(8 - rank));
            print_bitboard(mask_knight_attacks(square));
        }
    }


    return 0;
}