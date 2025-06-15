#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/time.h>
#include <x86intrin.h>
#include <limits.h>
#include <unistd.h> // For usleep()
#include <math.h>   // For abs() in chebyshev_distance

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------*/

#define U64 uint64_t
#define U16 uint16_t

#define empty_board "8/8/8/8/8/8/8/8 b - - "
#define start_position "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
#define tricky_position "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 "

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------*/

typedef enum {
    a8, b8, c8, d8, e8, f8, g8, h8,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a1, b1, c1, d1, e1, f1, g1, h1, no_sq
} square_index;

const char *square_ascii[] = {
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
};

typedef enum { no_piece = -1, P = 0, N, B, R, Q, K, p, n, b, r, q, k } piece_index;
char piece_ascii[12] = "PNBRQKpnbrqk";
char *piece_unicode[12] = {"♙", "♘", "♗", "♖", "♕", "♔", "♟︎", "♞", "♝", "♜", "♛", "♚"};
int piece_char_index[] = {
    ['P'] = P, ['N'] = N, ['B'] = B, ['R'] = R, ['Q'] = Q, ['K'] = K,
    ['p'] = p, ['n'] = n, ['b'] = b, ['r'] = r, ['q'] = q, ['k'] = k
};

typedef enum { white, black, both } color;
typedef enum { no_castle = 0, wk = 1, wq = 2, bk = 4, bq = 8 } castle_flags;

typedef struct {
    U64 pieces[12];
    U64 occupied[3];
    piece_index board[64];
    color side;
    castle_flags castle;
    square_index en_passant_square;
    uint8_t halfmove_clock;
    uint8_t fullmove_number;
    U64 hash_key;
} game_state;

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------*/

#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))
#define get_bit(bitboard, square) ((bitboard) & (1ULL << (square)))
#define pop_bit(bitboard, square) ((bitboard) &= ~(1ULL << (square)))
#define count_bits(bitboard) (__builtin_popcountll(bitboard))
#define lsb_index(bitboard) (bitboard == 0 ? no_sq : __builtin_ctzll(bitboard))

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------*/

U64 zobrist_piece_keys[12][64];
U64 zobrist_side_key;
U64 zobrist_castle_keys[16]; 
U64 zobrist_enpassant_keys[9]; 
U64 random_state = 1804289383;

U64 get_random_U64() {
    U64 number = random_state;
    number ^= number << 13;
    number ^= number >> 7;
    number ^= number << 17;
    random_state = number;
    return number;
}

void init_zobrist_keys() {
    for (int piece = P; piece <= k; piece++) {
        for (int square = 0; square < 64; square++) {
            zobrist_piece_keys[piece][square] = get_random_U64();
        }
    }

    zobrist_side_key = get_random_U64();

    for (int i = 0; i < 16; i++) {
        zobrist_castle_keys[i] = get_random_U64();
    }

    for (int i = 0; i < 9; i++) {
        zobrist_enpassant_keys[i] = get_random_U64();
    }
}

U64 generate_hash_key(const game_state* gs) {
    U64 final_key = 0;

    for (int piece = P; piece <= k; piece++) {
        U64 bitboard = gs->pieces[piece];
        while (bitboard) {
            int sq = lsb_index(bitboard);
            final_key ^= zobrist_piece_keys[piece][sq];
            pop_bit(bitboard, sq);
        }
    }

    if (gs->en_passant_square != no_sq) {
        // Use the file of the en passant square as the index
        final_key ^= zobrist_enpassant_keys[gs->en_passant_square % 8];
    }

    final_key ^= zobrist_castle_keys[gs->castle];

    if (gs->side == black) {
        final_key ^= zobrist_side_key;
    }

    return final_key;
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------*/

void print_board(const game_state* restrict gs) {
    printf("\nBoard from Mailbox:\n");
    for (int r = 0; r < 8; r++) {
        printf("%d ", 8 - r);
        for (int f = 0; f < 8; f++) {
            square_index sq_val = (square_index)(r * 8 + f);
            piece_index p = gs->board[sq_val];
            if (p != no_piece && p < 12) {
                printf(" %s ", piece_unicode[p]);
            } else {
                if ((r + f) % 2 == 0) {
                     printf(" . ");
                } else {
                     printf("   ");
                }
            }
        }
        printf("\n");
    }
    puts("   a  b  c  d  e  f  g  h\n");
    printf("Side to move: %s\n", (gs->side == white) ? "White" : "Black");
    printf("Castling Rights: %c%c%c%c\n",
           (gs->castle & wk) ? 'K' : '-',
           (gs->castle & wq) ? 'Q' : '-',
           (gs->castle & bk) ? 'k' : '-',
           (gs->castle & bq) ? 'q' : '-');
    printf("En Passant Square: %s\n", (gs->en_passant_square == no_sq) ? "None" : square_ascii[gs->en_passant_square]);
    printf("Halfmove Clock: %d\n", gs->halfmove_clock);
    printf("Fullmove Number: %d\n", gs->fullmove_number);
}

void initialize_empty_board(game_state* restrict gs) {
    memset(gs->pieces, 0, sizeof(gs->pieces));
    memset(gs->occupied, 0, sizeof(gs->occupied));
    for(int i=0; i<64; ++i) gs->board[i] = no_piece;
    gs->side = white;
    gs->castle = no_castle;
    gs->en_passant_square = no_sq;
    gs->halfmove_clock = 0;
    gs->fullmove_number = 1;
}

void parse_fen(const char *fen, game_state *restrict gs) {
    initialize_empty_board(gs);
    const char *fen_ptr = fen;
    int current_sq_idx = 0;

    while (*fen_ptr && *fen_ptr != ' ') {
        if (isalpha(*fen_ptr)) {
            piece_index p_idx = piece_char_index[(unsigned char)*fen_ptr];
            if (p_idx != no_piece && current_sq_idx < 64) {
                gs->board[current_sq_idx] = p_idx;
                set_bit(gs->pieces[p_idx], current_sq_idx);
            }
            current_sq_idx++;
        } else if (isdigit(*fen_ptr)) {
            int empty_squares = *fen_ptr - '0';
            for (int i = 0; i < empty_squares; i++) {
                if (current_sq_idx < 64) {
                    gs->board[current_sq_idx] = no_piece;
                }
                current_sq_idx++;
            }
        } else if (*fen_ptr == '/') {}
        fen_ptr++;
    }

    while (*fen_ptr == ' ') fen_ptr++;

    for (int piece_val = P; piece_val <= K; piece_val++) gs->occupied[white] |= gs->pieces[piece_val];
    for (int piece_val = p; piece_val <= k; piece_val++) gs->occupied[black] |= gs->pieces[piece_val];
    gs->occupied[both] = gs->occupied[white] | gs->occupied[black];

    if (*fen_ptr) {
        gs->side = (*fen_ptr == 'w') ? white : black;
        fen_ptr++;
    }

    while (*fen_ptr == ' ') fen_ptr++;
    
    if (*fen_ptr) {
        gs->castle = no_castle;
        while (*fen_ptr && *fen_ptr != ' ') {
            if (*fen_ptr == '-') { fen_ptr++; break;}
            switch (*fen_ptr) {
                case 'K': gs->castle |= wk; break;
                case 'Q': gs->castle |= wq; break;
                case 'k': gs->castle |= bk; break;
                case 'q': gs->castle |= bq; break;
            }
            fen_ptr++;
        }
    }

    while (*fen_ptr == ' ') fen_ptr++;
    
    if (*fen_ptr) {
        if (*fen_ptr == '-') {
            gs->en_passant_square = no_sq;
            fen_ptr++;
        } else {
            if (isalpha(*fen_ptr) && (*fen_ptr >= 'a' && *fen_ptr <= 'h') && isdigit(*(fen_ptr + 1)) && (*(fen_ptr+1) >= '1' && *(fen_ptr+1) <= '8') ) {
                int file = *fen_ptr - 'a';
                int rank_char_val = *(fen_ptr + 1) - '1';
                gs->en_passant_square = (square_index)((7 - rank_char_val) * 8 + file);
                fen_ptr += 2;
            } else {
                 gs->en_passant_square = no_sq;
                 while (*fen_ptr && *fen_ptr != ' ') fen_ptr++;
            }
        }
    }

    while (*fen_ptr == ' ') fen_ptr++;

    if (*fen_ptr) {
        char *next_field_ptr;
        long val = strtol(fen_ptr, &next_field_ptr, 10);
        if (val >= 0 && val <= 200) gs->halfmove_clock = (int)val;
        else gs->halfmove_clock = 0;
        fen_ptr = next_field_ptr;
    }

    while (*fen_ptr == ' ') fen_ptr++;

    if (*fen_ptr) {
        long val = strtol(fen_ptr, NULL, 10);
        if (val >= 1 && val <= 2000) gs->fullmove_number = (int)val;
        else gs->fullmove_number = 1;
    }
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------*/

const U64 pawn_attacks[2][64] = {
    {
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
    0x0000000000000002ULL,0x0000000000000005ULL,0x000000000000000aULL,0x0000000000000014ULL,0x0000000000000028ULL,0x0000000000000050ULL,0x00000000000000a0ULL,0x0000000000000040ULL,
    0x0000000000000200ULL,0x0000000000000500ULL,0x0000000000000a00ULL,0x0000000000001400ULL,0x0000000000002800ULL,0x0000000000005000ULL,0x000000000000a000ULL,0x0000000000004000ULL,
    0x0000000000020000ULL,0x0000000000050000ULL,0x00000000000a0000ULL,0x0000000000140000ULL,0x0000000000280000ULL,0x0000000000500000ULL,0x0000000000a00000ULL,0x0000000000400000ULL,
    0x0000000002000000ULL,0x0000000005000000ULL,0x000000000a000000ULL,0x0000000014000000ULL,0x0000000028000000ULL,0x0000000050000000ULL,0x00000000a0000000ULL,0x0000000040000000ULL,
    0x0000000200000000ULL,0x0000000500000000ULL,0x0000000a00000000ULL,0x0000001400000000ULL,0x0000002800000000ULL,0x0000005000000000ULL,0x000000a000000000ULL,0x0000004000000000ULL,
    0x0000020000000000ULL,0x0000050000000000ULL,0x00000a0000000000ULL,0x0000140000000000ULL,0x0000280000000000ULL,0x0000500000000000ULL,0x0000a00000000000ULL,0x0000400000000000ULL,
    0x0002000000000000ULL,0x0005000000000000ULL,0x000a000000000000ULL,0x0014000000000000ULL,0x0028000000000000ULL,0x0050000000000000ULL,0x00a0000000000000ULL,0x0040000000000000ULL,
    },
    {
    0x0000000000000200ULL,0x0000000000000500ULL,0x0000000000000a00ULL,0x0000000000001400ULL,0x0000000000002800ULL,0x0000000000005000ULL,0x000000000000a000ULL,0x0000000000004000ULL,
    0x0000000000020000ULL,0x0000000000050000ULL,0x00000000000a0000ULL,0x0000000000140000ULL,0x0000000000280000ULL,0x0000000000500000ULL,0x0000000000a00000ULL,0x0000000000400000ULL,
    0x0000000002000000ULL,0x0000000005000000ULL,0x000000000a000000ULL,0x0000000014000000ULL,0x0000000028000000ULL,0x0000000050000000ULL,0x00000000a0000000ULL,0x0000000040000000ULL,
    0x0000000200000000ULL,0x0000000500000000ULL,0x0000000a00000000ULL,0x0000001400000000ULL,0x0000002800000000ULL,0x0000005000000000ULL,0x000000a000000000ULL,0x0000004000000000ULL,
    0x0000020000000000ULL,0x0000050000000000ULL,0x00000a0000000000ULL,0x0000140000000000ULL,0x0000280000000000ULL,0x0000500000000000ULL,0x0000a00000000000ULL,0x0000400000000000ULL,
    0x0002000000000000ULL,0x0005000000000000ULL,0x000a000000000000ULL,0x0014000000000000ULL,0x0028000000000000ULL,0x0050000000000000ULL,0x00a0000000000000ULL,0x0040000000000000ULL,
    0x0200000000000000ULL,0x0500000000000000ULL,0x0a00000000000000ULL,0x1400000000000000ULL,0x2800000000000000ULL,0x5000000000000000ULL,0xa000000000000000ULL,0x4000000000000000ULL,
    0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,0x0000000000000000ULL,
    }
};

const U64 knight_attacks[64] = {
    0x0000000000020400ULL,0x0000000000050800ULL,0x00000000000a1100ULL,0x0000000000142200ULL,0x0000000000284400ULL,0x0000000000508800ULL,0x0000000000a01000ULL,0x0000000000402000ULL,
    0x0000000002040004ULL,0x0000000005080008ULL,0x000000000a110011ULL,0x0000000014220022ULL,0x0000000028440044ULL,0x0000000050880088ULL,0x00000000a0100010ULL,0x0000000040200020ULL,
    0x0000000204000402ULL,0x0000000508000805ULL,0x0000000a1100110aULL,0x0000001422002214ULL,0x0000002844004428ULL,0x0000005088008850ULL,0x000000a0100010a0ULL,0x0000004020002040ULL,
    0x0000020400040200ULL,0x0000050800080500ULL,0x00000a1100110a00ULL,0x0000142200221400ULL,0x0000284400442800ULL,0x0000508800885000ULL,0x0000a0100010a000ULL,0x0000402000204000ULL,
    0x0002040004020000ULL,0x0005080008050000ULL,0x000a1100110a0000ULL,0x0014220022140000ULL,0x0028440044280000ULL,0x0050880088500000ULL,0x00a0100010a00000ULL,0x0040200020400000ULL,
    0x0204000402000000ULL,0x0508000805000000ULL,0x0a1100110a000000ULL,0x1422002214000000ULL,0x2844004428000000ULL,0x5088008850000000ULL,0xa0100010a0000000ULL,0x4020002040000000ULL,
    0x0400040200000000ULL,0x0800080500000000ULL,0x1100110a00000000ULL,0x2200221400000000ULL,0x4400442800000000ULL,0x8800885000000000ULL,0x100010a000000000ULL,0x2000204000000000ULL,
    0x0004020000000000ULL,0x0008050000000000ULL,0x00110a0000000000ULL,0x0022140000000000ULL,0x0044280000000000ULL,0x0088500000000000ULL,0x0010a00000000000ULL,0x0020400000000000ULL,
};

const U64 king_attacks[64] = {
    0x0000000000000302ULL,0x0000000000000705ULL,0x0000000000000e0aULL,0x0000000000001c14ULL,0x0000000000003828ULL,0x0000000000007050ULL,0x000000000000e0a0ULL,0x000000000000c040ULL,
    0x0000000000030203ULL,0x0000000000070507ULL,0x00000000000e0a0eULL,0x00000000001c141cULL,0x0000000000382838ULL,0x0000000000705070ULL,0x0000000000e0a0e0ULL,0x0000000000c040c0ULL,
    0x0000000003020300ULL,0x0000000007050700ULL,0x000000000e0a0e00ULL,0x000000001c141c00ULL,0x0000000038283800ULL,0x0000000070507000ULL,0x00000000e0a0e000ULL,0x00000000c040c000ULL,
    0x0000000302030000ULL,0x0000000705070000ULL,0x0000000e0a0e0000ULL,0x0000001c141c0000ULL,0x0000003828380000ULL,0x0000007050700000ULL,0x000000e0a0e00000ULL,0x000000c040c00000ULL,
    0x0000030203000000ULL,0x0000070507000000ULL,0x00000e0a0e000000ULL,0x00001c141c000000ULL,0x0000382838000000ULL,0x0000705070000000ULL,0x0000e0a0e0000000ULL,0x0000c040c0000000ULL,
    0x0003020300000000ULL,0x0007050700000000ULL,0x000e0a0e00000000ULL,0x001c141c00000000ULL,0x0038283800000000ULL,0x0070507000000000ULL,0x00e0a0e000000000ULL,0x00c040c000000000ULL,
    0x0302030000000000ULL,0x0705070000000000ULL,0x0e0a0e0000000000ULL,0x1c141c0000000000ULL,0x3828380000000000ULL,0x7050700000000000ULL,0xe0a0e00000000000ULL,0xc040c00000000000ULL,
    0x0203000000000000ULL,0x0507000000000000ULL,0x0a0e000000000000ULL,0x141c000000000000ULL,0x2838000000000000ULL,0x5070000000000000ULL,0xa0e0000000000000ULL,0x40c0000000000000ULL
};

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------*/

U64 bishop_masks[64], rook_masks[64];
bool slider_mask = 0;
bool slider_attacks = 0;

void init_bishop_masks() {
    for(int sq = 0; sq < 64; ++sq){
        U64 a = 0;
        int tr = sq / 8, tf = sq & 7, r, f;
        for (r = tr + 1, f = tf + 1; r <= 6 && f <= 6; r++, f++) a |= 1ULL << (r * 8 + f);
        for (r = tr - 1, f = tf + 1; r >= 1 && f <= 6; r--, f++) a |= 1ULL << (r * 8 + f);
        for (r = tr + 1, f = tf - 1; r <= 6 && f >= 1; r++, f--) a |= 1ULL << (r * 8 + f);
        for (r = tr - 1, f = tf - 1; r >= 1 && f >= 1; r--, f--) a |= 1ULL << (r * 8 + f);
        bishop_masks[sq] = a;
    }
}

void init_rook_masks() {
    for(int sq = 0; sq < 64; ++sq){
        U64 a = 0;
        int tr = sq / 8, tf = sq & 7, r, f;
        for (r = tr + 1; r <= 6; r++) a |= 1ULL << (r * 8 + tf);
        for (r = tr - 1; r >= 1; r--) a |= 1ULL << (r * 8 + tf);
        for (f = tf + 1; f <= 6; f++) a |= 1ULL << (tr * 8 + f);
        for (f = tf - 1; f >= 1; f--) a |= 1ULL << (tr * 8 + f);
        rook_masks[sq] = a;
    }
}

void init_slider_masks() {
    if (!slider_mask){
        init_bishop_masks();
        init_rook_masks();
        slider_mask = 1;
    }
}

U64 bishop_attacks_bruteforce(int sq, U64 blockers){
    U64 a = 0; int tr = sq / 8, tf = sq & 7, r, f;
    for (r = tr + 1, f = tf + 1; r <= 7 && f <= 7; r++, f++) { U64 b = 1ULL << (r * 8 + f); a |= b; if (b & blockers) break; }
    for (r = tr - 1, f = tf + 1; r >= 0 && f <= 7; r--, f++) { U64 b = 1ULL << (r * 8 + f); a |= b; if (b & blockers) break; }
    for (r = tr + 1, f = tf - 1; r <= 7 && f >= 0; r++, f--) { U64 b = 1ULL << (r * 8 + f); a |= b; if (b & blockers) break; }
    for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--) { U64 b = 1ULL << (r * 8 + f); a |= b; if (b & blockers) break; }
    return a;
}

U64 rook_attacks_bruteforce(int sq, U64 blockers){
    U64 a = 0; int tr = sq / 8, tf = sq & 7, r, f;
    for (r = tr + 1; r <= 7; r++) { U64 b = 1ULL << (r * 8 + tf); a |= b; if (b & blockers) break; }
    for (r = tr - 1; r >= 0; r--) { U64 b = 1ULL << (r * 8 + tf); a |= b; if (b & blockers) break; }
    for (f = tf + 1; f <= 7; f++) { U64 b = 1ULL << (tr * 8 + f); a |= b; if (b & blockers) break; }
    for (f = tf - 1; f >= 0; f--) { U64 b = 1ULL << (tr * 8 + f); a |= b; if (b & blockers) break; }
    return a;
}

typedef struct { U64 mask; U64 *attacks; } magic;
magic bishopM[64], rookM[64];

void init_magic_bitboards(void)
{
    if (!slider_attacks){
        for (int sq = 0; sq < 64; ++sq){
            {
                U64 mask = bishop_masks[sq]; int bits = count_bits(mask); size_t table_size  = 1ULL << bits;
                bishopM[sq].mask = mask; bishopM[sq].attacks = (U64 *)malloc(table_size * sizeof(U64));
                memset(bishopM[sq].attacks, 0, table_size * sizeof(U64));
                for (U64 subset = 0ULL;; subset = (subset - mask) & mask) {
                    U64 index = _pext_u64(subset, mask); bishopM[sq].attacks[index] = bishop_attacks_bruteforce(sq, subset);
                    if (subset == mask) break;
                }
            }
            {
                U64 mask = rook_masks[sq]; int bits = count_bits(mask); size_t table_size  = 1ULL << bits;
                rookM[sq].mask = mask; rookM[sq].attacks = (U64 *)malloc(table_size * sizeof(U64));
                memset(rookM[sq].attacks, 0, table_size * sizeof(U64));
                for (U64 subset = 0ULL;; subset = (subset - mask) & mask) {
                    U64 index = _pext_u64(subset, mask); rookM[sq].attacks[index] = rook_attacks_bruteforce(sq, subset);
                    if (subset == mask) break;
                }
            }
        }
        slider_attacks = 1;
    }
}

static inline U64 bishop_attacks(int sq, U64 occupancy) {
    U64 subset = occupancy & bishopM[sq].mask; U64 index  = _pext_u64(subset, bishopM[sq].mask); return bishopM[sq].attacks[index];
}
static inline U64 rook_attacks(int sq, U64 occupancy) {
    U64 subset = occupancy & rookM[sq].mask; U64 index  = _pext_u64(subset, rookM[sq].mask); return rookM[sq].attacks[index];
}
static inline U64 queen_attacks(int sq, U64 occupancy) {
    return bishop_attacks(sq, occupancy) | rook_attacks(sq, occupancy);
}

void init_all() {
    init_zobrist_keys();
    init_slider_masks();
    init_magic_bitboards();
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------*/

typedef struct {
    U16 moves[256];
    uint8_t count;
} moves_struct;

typedef struct {
    U16 move;
    castle_flags prev_castle;
    square_index prev_en_passant_square;
    uint8_t prev_halfmove_clock;
    piece_index captured_piece;
    U64 prev_hash_key;
} undo_info;

typedef struct {
    undo_info entries[1024];
    uint8_t ply_count;
} game_history;

typedef enum { normal = 0, promotion = 1, enpassant = 2, castling = 3 } move_flags;
typedef enum { promo_knight = 0, promo_bishop = 1, promo_rook = 2, promo_queen = 3 } promo_pieces;
piece_index white_promo_map[] = { N, B, R, Q };
piece_index black_promo_map[] = { n, b, r, q };

static inline bool is_square_attacked(const game_state* restrict gs, int square, int attacker_side) {
    if ((attacker_side == white) && (pawn_attacks[black][square] & gs->pieces[P])) return 1;
    if ((attacker_side == black) && (pawn_attacks[white][square] & gs->pieces[p])) return 1;
    if (knight_attacks[square] & ((attacker_side == white) ? gs->pieces[N] : gs->pieces[n])) return 1;
    if (bishop_attacks(square, gs->occupied[both]) & ((attacker_side == white) ? gs->pieces[B] : gs->pieces[b])) return 1;
    if (rook_attacks(square, gs->occupied[both]) & ((attacker_side == white) ? gs->pieces[R] : gs->pieces[r])) return 1;
    if (queen_attacks(square, gs->occupied[both]) & ((attacker_side == white) ? gs->pieces[Q] : gs->pieces[q])) return 1;
    if (king_attacks[square] & ((attacker_side == white) ? gs->pieces[K] : gs->pieces[k])) return 1;
    return 0;
}

#define encode_move(source, target, move_flag, promo_piece) ((U16)( (source) | ((target) << 6) | ((move_flag) << 12) | ((promo_piece) << 14) ))
#define get_move_source(move)       ((square_index)((move) & 0x3f))
#define get_move_target(move)       ((square_index)(((move) >> 6) & 0x3f))
#define get_move_flag(move)         ((move_flags)(((move) >> 12) & 0x3))
#define get_move_promo_piece(move)  ((promo_pieces)(((move) >> 14) & 0x3))

static inline void add_move(moves_struct *move_list, U16 move) {
    move_list->moves[move_list->count] = move;
    move_list->count++;
}

const int castling_rights[64] = {
     7, 15, 15, 15,  3, 15, 15, 11,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    13, 15, 15, 15, 12, 15, 15, 14
};

void generate_moves(const game_state* restrict gs, moves_struct *move_list) {
    move_list->count = 0;
    
    U64 bitboard, attacks_bb;
    square_index from_sq, to_sq;
    
    for (piece_index piece = P; piece <= k; piece++) {
        if ((gs->side == white && (piece > K)) || (gs->side == black && (piece < p))) continue;

        bitboard = gs->pieces[piece];
        
        while(bitboard) {
            from_sq = lsb_index(bitboard);
            pop_bit(bitboard, from_sq);
            const bool on_7th_rank = (from_sq >= a7 && from_sq <= h7);
            const bool on_2nd_rank = (from_sq >= a2 && from_sq <= h2);
            if (piece == P) {
                to_sq = from_sq - 8;
                if (to_sq >= a8 && gs->board[to_sq] == no_piece) {
                    if (on_7th_rank) {
                        add_move(move_list, encode_move(from_sq, to_sq, promotion, promo_queen));
                        add_move(move_list, encode_move(from_sq, to_sq, promotion, promo_rook));
                        add_move(move_list, encode_move(from_sq, to_sq, promotion, promo_bishop));
                        add_move(move_list, encode_move(from_sq, to_sq, promotion, promo_knight));
                    } else {
                        add_move(move_list, encode_move(from_sq, to_sq, normal, 0));
                        if (on_2nd_rank && gs->board[to_sq - 8] == no_piece) {
                           add_move(move_list, encode_move(from_sq, to_sq - 8, normal, 0));
                        }
                    }
                }
                attacks_bb = pawn_attacks[white][from_sq] & gs->occupied[black];
                while(attacks_bb) {
                    to_sq = lsb_index(attacks_bb);
                    pop_bit(attacks_bb, to_sq);
                    if (on_7th_rank) {
                        add_move(move_list, encode_move(from_sq, to_sq, promotion, promo_queen));
                        add_move(move_list, encode_move(from_sq, to_sq, promotion, promo_rook));
                        add_move(move_list, encode_move(from_sq, to_sq, promotion, promo_bishop));
                        add_move(move_list, encode_move(from_sq, to_sq, promotion, promo_knight));
                    } else {
                        add_move(move_list, encode_move(from_sq, to_sq, normal, 0));
                    }
                }
                if (gs->en_passant_square != no_sq) {
                    if (pawn_attacks[white][from_sq] & (1ULL << gs->en_passant_square)) {
                        add_move(move_list, encode_move(from_sq, gs->en_passant_square, enpassant, 0));
                    }
                }
            } 
            else if (piece == p) {
                to_sq = from_sq + 8;
                if (to_sq <= h1 && gs->board[to_sq] == no_piece) {
                    if (on_2nd_rank) {
                        add_move(move_list, encode_move(from_sq, to_sq, promotion, promo_queen));
                        add_move(move_list, encode_move(from_sq, to_sq, promotion, promo_rook));
                        add_move(move_list, encode_move(from_sq, to_sq, promotion, promo_bishop));
                        add_move(move_list, encode_move(from_sq, to_sq, promotion, promo_knight));
                    } else {
                        add_move(move_list, encode_move(from_sq, to_sq, normal, 0));
                        if (on_7th_rank && gs->board[to_sq + 8] == no_piece) {
                            add_move(move_list, encode_move(from_sq, to_sq + 8, normal, 0));
                        }
                    }
                }
                attacks_bb = pawn_attacks[black][from_sq] & gs->occupied[white];
                while(attacks_bb) {
                    to_sq = lsb_index(attacks_bb);
                    pop_bit(attacks_bb, to_sq);
                    if (on_2nd_rank) {
                        add_move(move_list, encode_move(from_sq, to_sq, promotion, promo_queen));
                        add_move(move_list, encode_move(from_sq, to_sq, promotion, promo_rook));
                        add_move(move_list, encode_move(from_sq, to_sq, promotion, promo_bishop));
                        add_move(move_list, encode_move(from_sq, to_sq, promotion, promo_knight));
                    } else {
                        add_move(move_list, encode_move(from_sq, to_sq, normal, 0));
                    }
                }
                if (gs->en_passant_square != no_sq) {
                    if (pawn_attacks[black][from_sq] & (1ULL << gs->en_passant_square)) {
                        add_move(move_list, encode_move(from_sq, gs->en_passant_square, enpassant, 0));
                    }
                }
            } 
            else {
                U64 friendly_occupancy = gs->occupied[gs->side];
                if      (piece == N || piece == n) attacks_bb = knight_attacks[from_sq] & ~friendly_occupancy;
                else if (piece == B || piece == b) attacks_bb = bishop_attacks(from_sq, gs->occupied[both]) & ~friendly_occupancy;
                else if (piece == R || piece == r) attacks_bb = rook_attacks(from_sq, gs->occupied[both]) & ~friendly_occupancy;
                else if (piece == Q || piece == q) attacks_bb = queen_attacks(from_sq, gs->occupied[both]) & ~friendly_occupancy;
                else if (piece == K || piece == k) {
                    attacks_bb = king_attacks[from_sq] & ~friendly_occupancy;
                    if (piece == K) {
                        if ((gs->castle & wk) && !get_bit(gs->occupied[both], f1) && !get_bit(gs->occupied[both], g1) && !is_square_attacked(gs, e1, black) && !is_square_attacked(gs, f1, black)) add_move(move_list, encode_move(e1, g1, castling, 0));
                        if ((gs->castle & wq) && !get_bit(gs->occupied[both], d1) && !get_bit(gs->occupied[both], c1) && !get_bit(gs->occupied[both], b1) && !is_square_attacked(gs, e1, black) && !is_square_attacked(gs, d1, black)) add_move(move_list, encode_move(e1, c1, castling, 0));
                    } else { 
                        if ((gs->castle & bk) && !get_bit(gs->occupied[both], f8) && !get_bit(gs->occupied[both], g8) && !is_square_attacked(gs, e8, white) && !is_square_attacked(gs, f8, white)) add_move(move_list, encode_move(e8, g8, castling, 0));
                        if ((gs->castle & bq) && !get_bit(gs->occupied[both], d8) && !get_bit(gs->occupied[both], c8) && !get_bit(gs->occupied[both], b8) && !is_square_attacked(gs, e8, white) && !is_square_attacked(gs, d8, white)) add_move(move_list, encode_move(e8, c8, castling, 0));
                    }
                } else { attacks_bb = 0; }
                
                while(attacks_bb) {
                    to_sq = lsb_index(attacks_bb);
                    pop_bit(attacks_bb, to_sq);
                    add_move(move_list, encode_move(from_sq, to_sq, normal, 0));
                }
            }
        }
    }
}

bool make_move(game_state* restrict gs, U16 move, game_history* restrict history) {
    game_state gs_copy = *gs;

    square_index from = get_move_source(move);
    square_index to = get_move_target(move);
    move_flags flag = get_move_flag(move);
    promo_pieces promo_type = get_move_promo_piece(move);
    piece_index piece_to_move = gs->board[from];
    piece_index captured_piece = gs->board[to];

    if (history) {
        undo_info* undo = &history->entries[history->ply_count];
        undo->move = move;
        undo->prev_castle = gs->castle;
        undo->prev_en_passant_square = gs->en_passant_square;
        undo->prev_halfmove_clock = gs->halfmove_clock;
        undo->prev_hash_key = gs->hash_key; 
        undo->captured_piece = (flag == enpassant) ? (gs->side == white ? p : P) : captured_piece;
    }
    
    
    gs->hash_key ^= zobrist_castle_keys[gs->castle];
    if (gs->en_passant_square != no_sq) {
        gs->hash_key ^= zobrist_enpassant_keys[gs->en_passant_square % 8];
    }
    
    gs->hash_key ^= zobrist_piece_keys[piece_to_move][from]; 
    gs->hash_key ^= zobrist_piece_keys[piece_to_move][to];   
    
    gs->board[to] = piece_to_move;
    gs->board[from] = no_piece;
    pop_bit(gs->pieces[piece_to_move], from);
    set_bit(gs->pieces[piece_to_move], to);

    gs->halfmove_clock++;
    if (piece_to_move == P || piece_to_move == p) gs->halfmove_clock = 0;
    
    if (captured_piece != no_piece) {
        gs->hash_key ^= zobrist_piece_keys[captured_piece][to];
        pop_bit(gs->pieces[captured_piece], to);
        gs->halfmove_clock = 0;
    }
    
    gs->en_passant_square = no_sq;

    if(flag == promotion) {
        piece_index promoted_piece = (gs->side == white) ? white_promo_map[promo_type] : black_promo_map[promo_type];
        gs->hash_key ^= zobrist_piece_keys[piece_to_move][to]; 
        gs->hash_key ^= zobrist_piece_keys[promoted_piece][to];
        pop_bit(gs->pieces[piece_to_move], to);
        set_bit(gs->pieces[promoted_piece], to);
        gs->board[to] = promoted_piece;
    } else if (flag == enpassant) {
        square_index captured_pawn_sq = (gs->side == white) ? to + 8 : to - 8;
        piece_index captured_pawn = (gs->side == white) ? p : P;
        gs->hash_key ^= zobrist_piece_keys[captured_pawn][captured_pawn_sq];
        pop_bit(gs->pieces[captured_pawn], captured_pawn_sq);
        gs->board[captured_pawn_sq] = no_piece;
        gs->halfmove_clock = 0;
    } else if (flag == castling) {
        switch(to) {
            
            case g1: 
                gs->hash_key ^= zobrist_piece_keys[R][h1] ^ zobrist_piece_keys[R][f1];
                pop_bit(gs->pieces[R], h1); set_bit(gs->pieces[R], f1); 
                gs->board[h1] = no_piece; gs->board[f1] = R; 
                break;
            case c1: 
                gs->hash_key ^= zobrist_piece_keys[R][a1] ^ zobrist_piece_keys[R][d1];
                pop_bit(gs->pieces[R], a1); set_bit(gs->pieces[R], d1);
                gs->board[a1] = no_piece; gs->board[d1] = R; 
                break;
            case g8:
                gs->hash_key ^= zobrist_piece_keys[r][h8] ^ zobrist_piece_keys[r][f8];
                pop_bit(gs->pieces[r], h8); set_bit(gs->pieces[r], f8);
                gs->board[h8] = no_piece; gs->board[f8] = r; 
                break;
            case c8:
                gs->hash_key ^= zobrist_piece_keys[r][a8] ^ zobrist_piece_keys[r][d8];
                pop_bit(gs->pieces[r], a8); set_bit(gs->pieces[r], d8);
                gs->board[a8] = no_piece; gs->board[d8] = r; 
                break;
            default: break;
        }
    } else if (piece_to_move == P && to == from - 16) {
        gs->en_passant_square = from - 8;
    } else if (piece_to_move == p && to == from + 16) {
        gs->en_passant_square = from + 8;
    }

    gs->castle &= castling_rights[from];
    gs->castle &= castling_rights[to];
    
    if (gs->side == black) gs->fullmove_number++;
    
    
    gs->side = (gs->side == white) ? black : white;

    
    gs->hash_key ^= zobrist_castle_keys[gs->castle];
    if (gs->en_passant_square != no_sq) {
        gs->hash_key ^= zobrist_enpassant_keys[gs->en_passant_square % 8];
    }
    gs->hash_key ^= zobrist_side_key;

    gs->occupied[white] = gs->pieces[P] | gs->pieces[N] | gs->pieces[B] | gs->pieces[R] | gs->pieces[Q] | gs->pieces[K];
    gs->occupied[black] = gs->pieces[p] | gs->pieces[n] | gs->pieces[b] | gs->pieces[r] | gs->pieces[q] | gs->pieces[k];
    gs->occupied[both] = gs->occupied[white] | gs->occupied[black];

    square_index king_sq = lsb_index(gs->pieces[(gs->side == white) ? k : K]);
    if (is_square_attacked(gs, king_sq, gs->side)) {
        *gs = gs_copy;
        return false;
    }
    
    if (history) history->ply_count++;
    return true;
}

void unmake_move(game_state* restrict gs, game_history* restrict history) {
    if (history->ply_count == 0) return;

    history->ply_count--;
    undo_info* undo = &history->entries[history->ply_count];
    U16 move = undo->move;

    square_index from = get_move_source(move);
    square_index to = get_move_target(move);
    move_flags flag = get_move_flag(move);
    promo_pieces promo_type = get_move_promo_piece(move);
    piece_index captured_p = undo->captured_piece;

    piece_index piece_that_moved = gs->board[to];

    if (gs->side == white) gs->fullmove_number--;
    gs->side = (gs->side == white) ? black : white;

    gs->castle = undo->prev_castle;
    gs->en_passant_square = undo->prev_en_passant_square;
    gs->hash_key = undo->prev_hash_key;
    gs->halfmove_clock = undo->prev_halfmove_clock;
    
    if (flag == promotion) {
        piece_index original_pawn = (gs->side == white) ? P : p;
        pop_bit(gs->pieces[piece_that_moved], to);
        set_bit(gs->pieces[original_pawn], to);
        gs->board[to] = original_pawn;
        piece_that_moved = original_pawn;
    }
    
    gs->board[from] = piece_that_moved;
    gs->board[to] = (flag == enpassant) ? no_piece : captured_p;
    
    pop_bit(gs->pieces[piece_that_moved], to);
    set_bit(gs->pieces[piece_that_moved], from);

    if (captured_p != no_piece) {
        if (flag == enpassant) {
            square_index captured_pawn_sq = (gs->side == white) ? to + 8 : to - 8;
            set_bit(gs->pieces[captured_p], captured_pawn_sq);
            gs->board[captured_pawn_sq] = captured_p;
        } else {
            set_bit(gs->pieces[captured_p], to);
        }
    }

    if (flag == castling) {
        switch(to) {
            case g1:
                pop_bit(gs->pieces[R], f1); set_bit(gs->pieces[R], h1);
                gs->board[f1] = no_piece; gs->board[h1] = R;
                break;
            case c1:
                pop_bit(gs->pieces[R], d1); set_bit(gs->pieces[R], a1);
                gs->board[d1] = no_piece; gs->board[a1] = R;
                break;
            case g8:
                pop_bit(gs->pieces[r], f8); set_bit(gs->pieces[r], h8);
                gs->board[f8] = no_piece; gs->board[h8] = r;
                break;
            case c8:
                pop_bit(gs->pieces[r], d8); set_bit(gs->pieces[r], a8);
                gs->board[d8] = no_piece; gs->board[a8] = r;
                break;
        }
    }
    
    gs->occupied[white] = gs->pieces[P] | gs->pieces[N] | gs->pieces[B] | gs->pieces[R] | gs->pieces[Q] | gs->pieces[K];
    gs->occupied[black] = gs->pieces[p] | gs->pieces[n] | gs->pieces[b] | gs->pieces[r] | gs->pieces[q] | gs->pieces[k];
    gs->occupied[both] = gs->occupied[white] | gs->occupied[black];
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------*/

long perft_nodes;

int get_time_ms() {
    struct timeval time_value; gettimeofday(&time_value, NULL);
    return time_value.tv_sec * 1000 + time_value.tv_usec / 1000;
}

void perft_driver(game_state* restrict gs, int depth, game_history* restrict history) {
    if (depth == 0) {
        perft_nodes++;
        return;
    }

    moves_struct move_list;
    generate_moves(gs, &move_list);

    for (int i = 0; i < move_list.count; i++) {
        if (make_move(gs, move_list.moves[i], history)) {
            perft_driver(gs, depth - 1, history);
            unmake_move(gs, history);
        }
    }
}

void perft_test(game_state* restrict gs, int depth) {
    printf("\n     Performance test - Depth: %d\n\n", depth);
    perft_nodes = 0;
    moves_struct root_moves;
    generate_moves(gs, &root_moves);
    game_history history_stack;
    history_stack.ply_count = 0;
    long start_time = get_time_ms();

    char promo_char_map[] = { [N] = 'n', [B] = 'b', [R] = 'r', [Q] = 'q' };

    for (int i = 0; i < root_moves.count; i++) {
        U16 move = root_moves.moves[i];
        
        if (make_move(gs, move, &history_stack)) {
            long nodes_before_this_move = perft_nodes;
            
            perft_driver(gs, depth - 1, &history_stack);
            
            unmake_move(gs, &history_stack);
            
            square_index from = get_move_source(move);
            square_index to = get_move_target(move);
            move_flags flag = get_move_flag(move);
            char promotion_char = ' ';

            if (flag == promotion) {
                promo_pieces promo_val = get_move_promo_piece(move);
                piece_index promoted_piece = (gs->side == white) ? white_promo_map[promo_val] : black_promo_map[promo_val];
                promotion_char = promo_char_map[promoted_piece % 6];
            }

            printf("     move: %s%s%c  nodes: %ld\n", square_ascii[from], square_ascii[to], promotion_char, perft_nodes - nodes_before_this_move);
        }
    }
    printf("\n    Depth: %d\n    Nodes: %ld\n    Time: %ldms\n\n", depth, perft_nodes, get_time_ms() - start_time);
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------*/

void initialize_start_position(game_state* restrict gs) {
    parse_fen(start_position, gs);
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------*/

typedef struct {
    int opening;
    int endgame;
} Score;

Score count_material(const game_state* gs) {
    const int opening_piece_values[6] = {128, 781, 825, 1276, 2538, 0};
    const int endgame_piece_values[6] = {213, 854, 915, 1380, 2682, 0};

    Score white_score = {0, 0};
    Score black_score = {0, 0};
    Score final_score;

    for (piece_index piece = P; piece <= K; piece++) {
        int count = count_bits(gs->pieces[piece]);
        white_score.opening += count * opening_piece_values[piece];
        white_score.endgame += count * endgame_piece_values[piece];
    }

    for (piece_index piece = p; piece <= k; piece++) {
        int count = count_bits(gs->pieces[piece]);
        black_score.opening += count * opening_piece_values[piece % 6];
        black_score.endgame += count * endgame_piece_values[piece % 6];
    }

    final_score.opening = white_score.opening - black_score.opening;
    final_score.endgame = white_score.endgame - black_score.endgame;

    return final_score;
}
const int pawn_psqt_opening[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     98, 134,  61,  95,  68, 126,  34, -11,
     -6,   7,  26,  31,  65,  56,  25, -20,
    -14,  13,   6,  21,  23,  12,  17, -23,
    -27,  -2,  -5,  12,  17,   6,  10, -25,
    -26,  -4,  -4, -10,   3,   3,  33, -12,
    -35,  -1, -20, -23, -15,  24,  38, -22,
      0,   0,   0,   0,   0,   0,   0,   0
};

const int pawn_psqt_endgame[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8,  10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0
};

const int knight_psqt_opening[64] = {
    -167, -89, -34, -49,  61, -97, -15, -107,
     -73, -41,  72,  36,  23,  62,   7,  -17,
     -47,  60,  37,  65,  84, 129,  73,   44,
      -9,  17,  19,  53,  37,  69,  18,   22,
     -13,   4,  16,  13,  28,  19,  21,   -8,
     -23,  -9,  12,  10,  19,  17,  25,  -16,
     -29, -53, -12,  -3,  -1,  18, -14,  -19,
    -105, -21, -58, -33, -17, -28, -19,  -23
};

const int knight_psqt_endgame[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64
};

const int bishop_psqt_opening[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21
};

const int bishop_psqt_endgame[64] = {
    -14, -21, -11,  -8, -7,  -9, -17, -24,
     -8,  -4,   7, -12, -3, -13,  -4, -14,
      2,  -8,   0,  -1, -2,   6,   0,   4,
     -3,   9,  12,   9,  7,  10,   3,  -4,
     -6,   3,  13,  19,  7,  10,  -3,  -9,
    -12,  -3,   8,  10, 13,   3,  -7, -15,
    -14, -18,  -7,  -1,  4,  -9, -15, -27,
    -23,  -9, -23,  -5, -9, -16,  -5, -17
};

const int rook_psqt_opening[64] = {
     32,  42,  32,  51, 63,  9,  31,  43,
     27,  32,  58,  62, 80, 67,  26,  44,
     -5,  19,  26,  36, 17, 45,  61,  16,
    -24, -11,   7,  26, 24, 35,  -8, -20,
    -36, -26, -12,  -1,  9, -7,   6, -23,
    -45, -25, -16, -17,  3,  0,  -5, -33,
    -44, -16, -20,  -9, -1, 11,  -6, -71,
    -19, -13,   1,  17, 16,  7, -37, -26
};

const int rook_psqt_endgame[64] = {
    13, 10, 18, 15, 12,  12,   8,   5,
    11, 13, 13, 11, -3,   3,   8,   3,
     7,  7,  7,  5,  4,  -3,  -5,  -3,
     4,  3, 13,  1,  2,   1,  -1,   2,
     3,  5,  8,  4, -5,  -6,  -8, -11,
    -4,  0, -5, -1, -7, -12,  -8, -16,
    -6, -6,  0,  2, -9,  -9, -11,  -3,
    -9,  2,  3, -1, -5, -13,   4, -20
};

const int queen_psqt_opening[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50
};

const int queen_psqt_endgame[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41
};

const int king_psqt_opening[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -40, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14
};

const int king_psqt_endgame[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  15,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43
};

const int* opening_psqts[6] = {
    pawn_psqt_opening, knight_psqt_opening, bishop_psqt_opening,
    rook_psqt_opening, queen_psqt_opening, king_psqt_opening
};

const int* endgame_psqts[6] = {
    pawn_psqt_endgame, knight_psqt_endgame, bishop_psqt_endgame,
    rook_psqt_endgame, queen_psqt_endgame, king_psqt_endgame
};

Score evaluate_psqt(const game_state* gs) {
    Score total_score = {0, 0};
    U64 bitboard;

    for (piece_index piece = P; piece <= k; piece++) {
        bitboard = gs->pieces[piece];
        bool is_white = piece <= K;
        int piece_type_idx = piece % 6;

        const int* psqt_open = opening_psqts[piece_type_idx];
        const int* psqt_end = endgame_psqts[piece_type_idx];

        while (bitboard) {
            int square = lsb_index(bitboard);
            pop_bit(bitboard, square);

            int psqt_square = is_white ? square : (square ^ 56);

            if (is_white) {
                total_score.opening += psqt_open[psqt_square];
                total_score.endgame += psqt_end[psqt_square];
            } else {
                total_score.opening -= psqt_open[psqt_square];
                total_score.endgame -= psqt_end[psqt_square];
            }
        }
    }
    return total_score;
}

const Score DOUBLED_PAWN_PENALTY = {-12, -29};
const Score ISOLATED_PAWN_PENALTY = {-11, -15};

const Score PASSED_PAWN_BONUS[8] = {
    {  0,   0}, {  5,  15}, {  7,  22}, { 13,  36},
    { 21,  62}, { 34, 119}, { 51, 198}, {  0,   0}
};

U64 file_masks[8];
U64 adjacent_files_masks[8];
U64 passed_pawn_masks[2][64];

void init_pawn_masks() {
    static bool masks_initialized = false;
    if (masks_initialized) return;

    U64 current_file = 0x0101010101010101ULL;
    for (int i = 0; i < 8; i++) {
        file_masks[i] = current_file;
        current_file <<= 1;
    }

    for (int i = 0; i < 8; i++) {
        adjacent_files_masks[i] = 0;
        if (i > 0) adjacent_files_masks[i] |= file_masks[i - 1];
        if (i < 7) adjacent_files_masks[i] |= file_masks[i + 1];
    }

    for (int sq = 0; sq < 64; sq++) {
        int file = sq % 8;
        
        passed_pawn_masks[white][sq] = adjacent_files_masks[file] | file_masks[file];
        passed_pawn_masks[black][sq] = adjacent_files_masks[file] | file_masks[file];

        U64 white_forward_squares = 0;
        for(int r = (sq / 8) - 1; r >= 0; r--) {
            white_forward_squares |= (255ULL << (r * 8));
        }
        passed_pawn_masks[white][sq] &= white_forward_squares;

        U64 black_forward_squares = 0;
        for(int r = (sq / 8) + 1; r <= 7; r++) {
            black_forward_squares |= (255ULL << (r * 8));
        }
        passed_pawn_masks[black][sq] &= black_forward_squares;
    }
    masks_initialized = true;
}

Score evaluate_side(U64 friendly_pawns, U64 enemy_pawns, color c) {
    Score score = {0, 0};
    U64 pawns_copy = friendly_pawns;

    for (int file = 0; file < 8; file++) {
        U64 pawns_on_file = friendly_pawns & file_masks[file];
        int count = count_bits(pawns_on_file);
        if (count > 1) {
            score.opening += (count - 1) * DOUBLED_PAWN_PENALTY.opening;
            score.endgame += (count - 1) * DOUBLED_PAWN_PENALTY.endgame;
        }
    }

    while (pawns_copy) {
        int sq = lsb_index(pawns_copy);
        pop_bit(pawns_copy, sq);
        int file = sq % 8;
        int rank = (c == white) ? (sq / 8) : 7 - (sq / 8);

        if ((friendly_pawns & adjacent_files_masks[file]) == 0) {
            score.opening += ISOLATED_PAWN_PENALTY.opening;
            score.endgame += ISOLATED_PAWN_PENALTY.endgame;
        }

        if ((passed_pawn_masks[c][sq] & enemy_pawns) == 0) {
            score.opening += PASSED_PAWN_BONUS[rank].opening;
            score.endgame += PASSED_PAWN_BONUS[rank].endgame;
        }
    }
    return score;
}

Score evaluate_pawns(const game_state* gs) {
    init_pawn_masks();

    U64 white_pawns = gs->pieces[P];
    U64 black_pawns = gs->pieces[p];

    Score white_score = evaluate_side(white_pawns, black_pawns, white);
    Score black_score = evaluate_side(black_pawns, white_pawns, black);

    Score final_score;
    final_score.opening = white_score.opening - black_score.opening;
    final_score.endgame = white_score.endgame - black_score.endgame;

    return final_score;
}


const Score BISHOP_PAIR_BONUS = {47, 64};

const Score imbalance_table[5][5] = {
    {{  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}},
    {{  7, -11}, {  0,   0}, {  0,   0}, {  0,   0}, {  0,   0}},
    {{  2,  13}, { -6,   1}, {  0,   0}, {  0,   0}, {  0,   0}},
    {{-11,  16}, {  4,  -9}, { -3, -11}, {  0,   0}, {  0,   0}},
    {{-10,  -8}, {  0,  -7}, {  2,   4}, { -3,  10}, {  0,   0}}
};

Score evaluate_imbalance(const game_state* gs) {
    Score total_score = {0, 0};
    int white_counts[6] = {0};
    int black_counts[6] = {0};

    for (piece_index piece = P; piece <= K; piece++) {
        white_counts[piece] = count_bits(gs->pieces[piece]);
    }
    for (piece_index piece = p; piece <= k; piece++) {
        black_counts[piece % 6] = count_bits(gs->pieces[piece]);
    }

    if (white_counts[B] >= 2) {
        total_score.opening += BISHOP_PAIR_BONUS.opening;
        total_score.endgame += BISHOP_PAIR_BONUS.endgame;
    }
    if (black_counts[B] >= 2) {
        total_score.opening -= BISHOP_PAIR_BONUS.opening;
        total_score.endgame -= BISHOP_PAIR_BONUS.endgame;
    }
    
    for (int p1 = P; p1 < K; p1++) {
        if (!white_counts[p1] && !black_counts[p1]) continue;
        for (int p2 = P; p2 < K; p2++) {
            Score bonus = imbalance_table[p1][p2];
            if (bonus.opening == 0 && bonus.endgame == 0) continue;

            int white_term_op = bonus.opening * white_counts[p1] * black_counts[p2];
            int white_term_eg = bonus.endgame * white_counts[p1] * black_counts[p2];
            
            int black_term_op = bonus.opening * black_counts[p1] * white_counts[p2];
            int black_term_eg = bonus.endgame * black_counts[p1] * white_counts[p2];

            total_score.opening += (white_term_op - black_term_op);
            total_score.endgame += (white_term_eg - black_term_eg);
        }
    }

    return total_score;
}


const Score KNIGHT_PAWN_SUPPORT_BONUS = {11, 13};
const Score BISHOP_PAWN_OBSTRUCTION_PENALTY = {-11, -11};
const Score ROOK_OPEN_FILE_BONUS = {48, 20};
const Score ROOK_SEMI_OPEN_FILE_BONUS = {20, 10};
const Score ROOK_TRAPPED_PENALTY = {-44, -13};

const U64 LIGHT_SQUARES = 0x55AA55AA55AA55AAULL;
const U64 DARK_SQUARES = 0xAA55AA55AA55AA55ULL;

extern U64 file_masks[8];

Score evaluate_pieces(const game_state* gs) {
    Score total_score = {0, 0};

    U64 white_pawns = gs->pieces[P];
    U64 black_pawns = gs->pieces[p];
    U64 all_pawns = white_pawns | black_pawns;

    U64 white_knights = gs->pieces[N];
    U64 white_bishops = gs->pieces[B];
    U64 white_rooks = gs->pieces[R];
    U64 black_knights = gs->pieces[n];
    U64 black_bishops = gs->pieces[b];
    U64 black_rooks = gs->pieces[r];

    while (white_knights) {
        int sq = lsb_index(white_knights);
        pop_bit(white_knights, sq);
        U64 support_mask = 0;
        if ((sq % 8) > 0) support_mask |= (1ULL << (sq + 7));
        if ((sq % 8) < 7) support_mask |= (1ULL << (sq + 9));
        if (support_mask & white_pawns) {
            total_score.opening += KNIGHT_PAWN_SUPPORT_BONUS.opening;
            total_score.endgame += KNIGHT_PAWN_SUPPORT_BONUS.endgame;
        }
    }

    while (black_knights) {
        int sq = lsb_index(black_knights);
        pop_bit(black_knights, sq);
        U64 support_mask = 0;
        if ((sq % 8) > 0) support_mask |= (1ULL << (sq - 9));
        if ((sq % 8) < 7) support_mask |= (1ULL << (sq - 7));
        if (support_mask & black_pawns) {
            total_score.opening -= KNIGHT_PAWN_SUPPORT_BONUS.opening;
            total_score.endgame -= KNIGHT_PAWN_SUPPORT_BONUS.endgame;
        }
    }
    
    while (white_bishops) {
        int sq = lsb_index(white_bishops);
        pop_bit(white_bishops, sq);
        U64 obstruction_mask = get_bit(LIGHT_SQUARES, sq) ? LIGHT_SQUARES : DARK_SQUARES;
        int obstruction_count = count_bits(white_pawns & obstruction_mask);
        total_score.opening += obstruction_count * BISHOP_PAWN_OBSTRUCTION_PENALTY.opening;
        total_score.endgame += obstruction_count * BISHOP_PAWN_OBSTRUCTION_PENALTY.endgame;
    }

    while (black_bishops) {
        int sq = lsb_index(black_bishops);
        pop_bit(black_bishops, sq);
        U64 obstruction_mask = get_bit(LIGHT_SQUARES, sq) ? LIGHT_SQUARES : DARK_SQUARES;
        int obstruction_count = count_bits(black_pawns & obstruction_mask);
        total_score.opening -= obstruction_count * BISHOP_PAWN_OBSTRUCTION_PENALTY.opening;
        total_score.endgame -= obstruction_count * BISHOP_PAWN_OBSTRUCTION_PENALTY.endgame;
    }

    while (white_rooks) {
        int sq = lsb_index(white_rooks);
        pop_bit(white_rooks, sq);
        int file = sq % 8;
        if ((all_pawns & file_masks[file]) == 0) {
            total_score.opening += ROOK_OPEN_FILE_BONUS.opening;
            total_score.endgame += ROOK_OPEN_FILE_BONUS.endgame;
        } else if ((white_pawns & file_masks[file]) == 0) {
            total_score.opening += ROOK_SEMI_OPEN_FILE_BONUS.opening;
            total_score.endgame += ROOK_SEMI_OPEN_FILE_BONUS.endgame;
        }
    }
    
    if (get_bit(gs->pieces[K], g1) && get_bit(gs->pieces[R], h1)) {
         total_score.opening += ROOK_TRAPPED_PENALTY.opening;
         total_score.endgame += ROOK_TRAPPED_PENALTY.endgame;
    }
    if (get_bit(gs->pieces[K], c1) && get_bit(gs->pieces[R], a1)) {
         total_score.opening += ROOK_TRAPPED_PENALTY.opening;
         total_score.endgame += ROOK_TRAPPED_PENALTY.endgame;
    }

    while (black_rooks) {
        int sq = lsb_index(black_rooks);
        pop_bit(black_rooks, sq);
        int file = sq % 8;
        if ((all_pawns & file_masks[file]) == 0) {
            total_score.opening -= ROOK_OPEN_FILE_BONUS.opening;
            total_score.endgame -= ROOK_OPEN_FILE_BONUS.endgame;
        } else if ((black_pawns & file_masks[file]) == 0) {
            total_score.opening -= ROOK_SEMI_OPEN_FILE_BONUS.opening;
            total_score.endgame -= ROOK_SEMI_OPEN_FILE_BONUS.endgame;
        }
    }

    if (get_bit(gs->pieces[k], g8) && get_bit(gs->pieces[r], h8)) {
         total_score.opening -= ROOK_TRAPPED_PENALTY.opening;
         total_score.endgame -= ROOK_TRAPPED_PENALTY.endgame;
    }
    if (get_bit(gs->pieces[k], c8) && get_bit(gs->pieces[r], a8)) {
         total_score.opening -= ROOK_TRAPPED_PENALTY.opening;
         total_score.endgame -= ROOK_TRAPPED_PENALTY.endgame;
    }

    return total_score;
}


const Score KNIGHT_MOBILITY_BONUS[9] = {
    {-81, -81}, {-52, -55}, {-11, -29}, { -2, -14}, { 12,   5},
    { 24,  13}, { 33,  23}, {  41,  33}, {  41,  42}
};
const Score BISHOP_MOBILITY_BONUS[14] = {
    {-58, -63}, {-26, -34}, {-11, -15}, { -6,  -6}, { -2,   3},
    {  4,  10}, { 10,  19}, {  16,  27}, {  23,  35}, {  28,  42},
    {  33,  48}, {  38,  56}, {  42,  60}, {  46,  64}
};
const Score ROOK_MOBILITY_BONUS[15] = {
    {-63, -83}, {-30, -38}, {-14, -18}, { -5,   2}, {  4,  11},
    {  9,  22}, {  17,  37}, {  24,  50}, {  30,  62}, {  36,  73},
    {  41,  83}, {  46,  92}, {  50,  98}, {  55, 106}, {  58, 111}
};
const Score QUEEN_MOBILITY_BONUS[28] = {
    {-40, -47}, {-23, -29}, {-11, -13}, { -6,  -3}, { -2,   6},
    {  2,  13}, {   5,  20}, {   9,  26}, {  13,  33}, {  17,  39},
    {  21,  45}, {  25,  51}, {  29,  56}, {  33,  62}, {  36,  67},
    {  40,  72}, {  44,  77}, {  48,  82}, {  52,  87}, {  56,  92},
    {  60,  97}, {  64, 102}, {  68, 107}, {  72, 112}, {  76, 117},
    {  80, 122}, {  85, 127}, {  89, 132}
};

const U64 FILE_A = 0x0101010101010101ULL;
const U64 FILE_H = 0x8080808080808080ULL;

Score evaluate_mobility(const game_state* gs) {
    Score total_score = {0, 0};
    U64 bitboard;
    int move_count;

    U64 white_pawns = gs->pieces[P];
    U64 black_pawns = gs->pieces[p];
    U64 white_occupied = gs->occupied[0];
    U64 black_occupied = gs->occupied[1];
    U64 all_occupied = gs->occupied[both];

    U64 white_pawn_attacks = ((black_pawns >> 7) & ~FILE_H) | ((black_pawns >> 9) & ~FILE_A);
    U64 black_pawn_attacks = ((white_pawns << 7) & ~FILE_A) | ((white_pawns << 9) & ~FILE_H);

    bitboard = gs->pieces[N];
    while(bitboard) {
        int sq = lsb_index(bitboard);
        pop_bit(bitboard, sq);
        U64 attacks = knight_attacks[sq] & ~white_occupied & ~black_pawns & ~white_pawn_attacks;
        move_count = count_bits(attacks);
        total_score.opening += KNIGHT_MOBILITY_BONUS[move_count].opening;
        total_score.endgame += KNIGHT_MOBILITY_BONUS[move_count].endgame;
    }
    bitboard = gs->pieces[n];
    while(bitboard) {
        int sq = lsb_index(bitboard);
        pop_bit(bitboard, sq);
        U64 attacks = knight_attacks[sq] & ~black_occupied & ~white_pawns & ~black_pawn_attacks;
        move_count = count_bits(attacks);
        total_score.opening -= KNIGHT_MOBILITY_BONUS[move_count].opening;
        total_score.endgame -= KNIGHT_MOBILITY_BONUS[move_count].endgame;
    }

    bitboard = gs->pieces[B];
    while(bitboard) {
        int sq = lsb_index(bitboard);
        pop_bit(bitboard, sq);
        U64 attacks = bishop_attacks(sq, all_occupied) & ~white_occupied & ~black_pawns & ~white_pawn_attacks;
        move_count = count_bits(attacks);
        total_score.opening += BISHOP_MOBILITY_BONUS[move_count].opening;
        total_score.endgame += BISHOP_MOBILITY_BONUS[move_count].endgame;
    }
    bitboard = gs->pieces[b];
    while(bitboard) {
        int sq = lsb_index(bitboard);
        pop_bit(bitboard, sq);
        U64 attacks = bishop_attacks(sq, all_occupied) & ~black_occupied & ~white_pawns & ~black_pawn_attacks;
        move_count = count_bits(attacks);
        total_score.opening -= BISHOP_MOBILITY_BONUS[move_count].opening;
        total_score.endgame -= BISHOP_MOBILITY_BONUS[move_count].endgame;
    }

    bitboard = gs->pieces[R];
    while(bitboard) {
        int sq = lsb_index(bitboard);
        pop_bit(bitboard, sq);
        U64 attacks = rook_attacks(sq, all_occupied) & ~white_occupied & ~black_pawns & ~white_pawn_attacks;
        move_count = count_bits(attacks);
        total_score.opening += ROOK_MOBILITY_BONUS[move_count].opening;
        total_score.endgame += ROOK_MOBILITY_BONUS[move_count].endgame;
    }
    bitboard = gs->pieces[r];
    while(bitboard) {
        int sq = lsb_index(bitboard);
        pop_bit(bitboard, sq);
        U64 attacks = rook_attacks(sq, all_occupied) & ~black_occupied & ~white_pawns & ~black_pawn_attacks;
        move_count = count_bits(attacks);
        total_score.opening -= ROOK_MOBILITY_BONUS[move_count].opening;
        total_score.endgame -= ROOK_MOBILITY_BONUS[move_count].endgame;
    }

    bitboard = gs->pieces[Q];
    while(bitboard) {
        int sq = lsb_index(bitboard);
        pop_bit(bitboard, sq);
        U64 attacks = queen_attacks(sq, all_occupied) & ~white_occupied & ~black_pawns & ~white_pawn_attacks;
        move_count = count_bits(attacks);
        total_score.opening += QUEEN_MOBILITY_BONUS[move_count].opening;
        total_score.endgame += QUEEN_MOBILITY_BONUS[move_count].endgame;
    }
    bitboard = gs->pieces[q];
    while(bitboard) {
        int sq = lsb_index(bitboard);
        pop_bit(bitboard, sq);
        U64 attacks = queen_attacks(sq, all_occupied) & ~black_occupied & ~white_pawns & ~black_pawn_attacks;
        move_count = count_bits(attacks);
        total_score.opening -= QUEEN_MOBILITY_BONUS[move_count].opening;
        total_score.endgame -= QUEEN_MOBILITY_BONUS[move_count].endgame;
    }

    return total_score;
}


const Score THREAT_PAWN_ATTACKS_MINOR = {55, 33};
const Score THREAT_PAWN_ATTACKS_MAJOR = {68, 48};
const Score THREAT_BY_MINOR_ON_MAJOR = {33, 20};
const Score THREAT_BY_ROOK_ON_QUEEN = {42, 28};
const Score HANGING_PIECE_PENALTY = {-14, -20};

Score evaluate_threats(const game_state* gs) {
    Score total_score = {0, 0};
    U64 bitboard;

    U64 white_pawns = gs->pieces[P];
    U64 black_pawns = gs->pieces[p];
    U64 white_minors = gs->pieces[N] | gs->pieces[B];
    U64 black_minors = gs->pieces[n] | gs->pieces[b];
    U64 white_majors = gs->pieces[R] | gs->pieces[Q];
    U64 black_majors = gs->pieces[r] | gs->pieces[q];

    U64 white_pawn_attacks = ((white_pawns << 7) & ~FILE_A) | ((white_pawns << 9) & ~FILE_H);
    U64 black_pawn_attacks = ((black_pawns >> 7) & ~FILE_H) | ((black_pawns >> 9) & ~FILE_A);

    int count;

    count = count_bits(white_pawn_attacks & black_minors);
    total_score.opening += count * THREAT_PAWN_ATTACKS_MINOR.opening;
    total_score.endgame += count * THREAT_PAWN_ATTACKS_MINOR.endgame;
    count = count_bits(white_pawn_attacks & black_majors);
    total_score.opening += count * THREAT_PAWN_ATTACKS_MAJOR.opening;
    total_score.endgame += count * THREAT_PAWN_ATTACKS_MAJOR.endgame;
    
    count = count_bits(black_pawn_attacks & white_minors);
    total_score.opening -= count * THREAT_PAWN_ATTACKS_MINOR.opening;
    total_score.endgame -= count * THREAT_PAWN_ATTACKS_MINOR.endgame;
    count = count_bits(black_pawn_attacks & white_majors);
    total_score.opening -= count * THREAT_PAWN_ATTACKS_MAJOR.opening;
    total_score.endgame -= count * THREAT_PAWN_ATTACKS_MAJOR.endgame;

    U64 white_knight_attacks = 0; bitboard = gs->pieces[N]; while(bitboard){ int sq=lsb_index(bitboard); pop_bit(bitboard,sq); white_knight_attacks |= knight_attacks[sq]; }
    U64 white_bishop_attacks = 0; bitboard = gs->pieces[B]; while(bitboard){ int sq=lsb_index(bitboard); pop_bit(bitboard,sq); white_bishop_attacks |= bishop_attacks(sq, gs->occupied[both]); }
    U64 white_rook_attacks = 0; bitboard = gs->pieces[R]; while(bitboard){ int sq=lsb_index(bitboard); pop_bit(bitboard,sq); white_rook_attacks |= rook_attacks(sq, gs->occupied[both]); }
    U64 black_knight_attacks = 0; bitboard = gs->pieces[n]; while(bitboard){ int sq=lsb_index(bitboard); pop_bit(bitboard,sq); black_knight_attacks |= knight_attacks[sq]; }
    U64 black_bishop_attacks = 0; bitboard = gs->pieces[b]; while(bitboard){ int sq=lsb_index(bitboard); pop_bit(bitboard,sq); black_bishop_attacks |= bishop_attacks(sq, gs->occupied[both]); }
    U64 black_rook_attacks = 0; bitboard = gs->pieces[r]; while(bitboard){ int sq=lsb_index(bitboard); pop_bit(bitboard,sq); black_rook_attacks |= rook_attacks(sq, gs->occupied[both]); }

    U64 white_minor_attacks = white_knight_attacks | white_bishop_attacks;
    U64 black_minor_attacks = black_knight_attacks | black_bishop_attacks;

    count = count_bits(white_minor_attacks & black_majors);
    total_score.opening += count * THREAT_BY_MINOR_ON_MAJOR.opening;
    total_score.endgame += count * THREAT_BY_MINOR_ON_MAJOR.endgame;
    count = count_bits(black_minor_attacks & white_majors);
    total_score.opening -= count * THREAT_BY_MINOR_ON_MAJOR.opening;
    total_score.endgame -= count * THREAT_BY_MINOR_ON_MAJOR.endgame;

    count = count_bits(white_rook_attacks & gs->pieces[q]);
    total_score.opening += count * THREAT_BY_ROOK_ON_QUEEN.opening;
    total_score.endgame += count * THREAT_BY_ROOK_ON_QUEEN.endgame;
    count = count_bits(black_rook_attacks & gs->pieces[Q]);
    total_score.opening -= count * THREAT_BY_ROOK_ON_QUEEN.opening;
    total_score.endgame -= count * THREAT_BY_ROOK_ON_QUEEN.endgame;
    
    U64 white_all_attacks = white_pawn_attacks | white_minor_attacks | white_rook_attacks; 
    U64 black_all_attacks = black_pawn_attacks | black_minor_attacks | black_rook_attacks;
    
    bitboard = gs->pieces[Q]; while(bitboard){ int sq=lsb_index(bitboard); pop_bit(bitboard,sq); U64 attacks = bishop_attacks(sq, gs->occupied[both]) | rook_attacks(sq, gs->occupied[both]); white_all_attacks |= attacks; }
    bitboard = gs->pieces[q]; while(bitboard){ int sq=lsb_index(bitboard); pop_bit(bitboard,sq); U64 attacks = bishop_attacks(sq, gs->occupied[both]) | rook_attacks(sq, gs->occupied[both]); black_all_attacks |= attacks; }
    
    count = count_bits((gs->occupied[0] & ~white_pawns) & black_all_attacks & ~white_all_attacks);
    total_score.opening += count * HANGING_PIECE_PENALTY.opening;
    total_score.endgame += count * HANGING_PIECE_PENALTY.endgame;
    count = count_bits((gs->occupied[1] & ~black_pawns) & white_all_attacks & ~black_all_attacks);
    total_score.opening -= count * HANGING_PIECE_PENALTY.opening;
    total_score.endgame -= count * HANGING_PIECE_PENALTY.endgame;

    return total_score;
}


static inline int chebyshev_distance(int sq1, int sq2) {
    int r1 = sq1 / 8;
    int f1 = sq1 % 8;
    int r2 = sq2 / 8;
    int f2 = sq2 % 8;
    int rank_dist = abs(r1 - r2);
    int file_dist = abs(f1 - f2);
    return (rank_dist > file_dist) ? rank_dist : file_dist;
}

Score evaluate_passed_pawns(const game_state* gs) {
    init_pawn_masks();
    Score total_score = {0, 0};

    U64 white_pawns = gs->pieces[P];
    U64 black_pawns = gs->pieces[p];
    U64 white_rooks = gs->pieces[R];
    U64 black_rooks = gs->pieces[r];
    int white_king_sq = lsb_index(gs->pieces[K]);
    int black_king_sq = lsb_index(gs->pieces[k]);

    U64 pawns_copy = white_pawns;
    while(pawns_copy) {
        int sq = lsb_index(pawns_copy);
        pop_bit(pawns_copy, sq);

        if ((passed_pawn_masks[white][sq] & black_pawns) == 0) {
            int rank = sq / 8;
            int file = sq % 8;
            int promo_sq = file;

            Score bonus = PASSED_PAWN_BONUS[rank];
            
            int king_dist = chebyshev_distance(black_king_sq, promo_sq);
            bonus.opening = bonus.opening * (10 + king_dist) / 10;
            bonus.endgame = bonus.endgame * (10 + king_dist) / 10;

            if (get_bit(white_rooks, file_masks[file])) {
                bonus.opening = bonus.opening * 3 / 2;
                bonus.endgame = bonus.endgame * 3 / 2;
            }

            U64 rear_span_mask = passed_pawn_masks[black][sq] ^ passed_pawn_masks[white][sq];
            if (get_bit(black_rooks & file_masks[file], rear_span_mask)) {
                bonus.opening /= 2;
                bonus.endgame /= 2;
            }

            total_score.opening += bonus.opening;
            total_score.endgame += bonus.endgame;
        }
    }

    pawns_copy = black_pawns;
    while(pawns_copy) {
        int sq = lsb_index(pawns_copy);
        pop_bit(pawns_copy, sq);

        if ((passed_pawn_masks[black][sq] & white_pawns) == 0) {
            int rank = 7 - (sq / 8);
            int file = sq % 8;
            int promo_sq = file + 56;
            
            Score bonus = PASSED_PAWN_BONUS[rank];

            int king_dist = chebyshev_distance(white_king_sq, promo_sq);
            bonus.opening = bonus.opening * (10 + king_dist) / 10;
            bonus.endgame = bonus.endgame * (10 + king_dist) / 10;

            if (get_bit(black_rooks, file_masks[file])) {
                bonus.opening = bonus.opening * 3 / 2;
                bonus.endgame = bonus.endgame * 3 / 2;
            }
            
            U64 rear_span_mask = passed_pawn_masks[black][sq] ^ passed_pawn_masks[white][sq];
            if (get_bit(white_rooks & file_masks[file], rear_span_mask)) {
                bonus.opening /= 2;
                bonus.endgame /= 2;
            }

            total_score.opening -= bonus.opening;
            total_score.endgame -= bonus.endgame;
        }
    }

    return total_score;
}


const Score SPACE_BONUS = {7, 0};

const U64 MASK_CDEF = 0x3C3C3C3C3C3C3C3CULL;
const U64 MASK_RANK_5_TO_8 = 0xFFFFFFFF00000000ULL;
const U64 MASK_RANK_1_TO_4 = 0x00000000FFFFFFFFULL;
const U64 WHITE_SPACE_MASK = MASK_CDEF & MASK_RANK_5_TO_8;
const U64 BLACK_SPACE_MASK = MASK_CDEF & MASK_RANK_1_TO_4;

Score evaluate_space(const game_state* gs) {
    Score total_score = {0, 0};
    U64 bitboard;

    if (get_bit(gs->pieces[Q], d1) && get_bit(gs->pieces[P], d2)) {
        U64 black_pawn_attacks = ((gs->pieces[p] >> 7) & ~FILE_H) | ((gs->pieces[p] >> 9) & ~FILE_A);
        int white_bonus_squares = 0;

        bitboard = gs->pieces[N];
        while(bitboard) {
            int sq = lsb_index(bitboard);
            pop_bit(bitboard, sq);
            U64 safe_attacks = knight_attacks[sq] & WHITE_SPACE_MASK & ~black_pawn_attacks;
            white_bonus_squares += count_bits(safe_attacks);
        }

        bitboard = gs->pieces[B];
        while(bitboard) {
            int sq = lsb_index(bitboard);
            pop_bit(bitboard, sq);
            U64 safe_attacks = bishop_attacks(sq, gs->occupied[both]) & WHITE_SPACE_MASK & ~black_pawn_attacks;
            white_bonus_squares += count_bits(safe_attacks);
        }

        bitboard = gs->pieces[R];
        while(bitboard) {
            int sq = lsb_index(bitboard);
            pop_bit(bitboard, sq);
            U64 safe_attacks = rook_attacks(sq, gs->occupied[both]) & WHITE_SPACE_MASK & ~black_pawn_attacks;
            white_bonus_squares += count_bits(safe_attacks);
        }
        
        total_score.opening += white_bonus_squares * SPACE_BONUS.opening;
    }

    if (get_bit(gs->pieces[q], d8) && get_bit(gs->pieces[p], d7)) {
        U64 white_pawn_attacks = ((gs->pieces[P] << 7) & ~FILE_A) | ((gs->pieces[P] << 9) & ~FILE_H);
        int black_bonus_squares = 0;

        bitboard = gs->pieces[n];
        while(bitboard) {
            int sq = lsb_index(bitboard);
            pop_bit(bitboard, sq);
            U64 safe_attacks = knight_attacks[sq] & BLACK_SPACE_MASK & ~white_pawn_attacks;
            black_bonus_squares += count_bits(safe_attacks);
        }

        bitboard = gs->pieces[b];
        while(bitboard) {
            int sq = lsb_index(bitboard);
            pop_bit(bitboard, sq);
            U64 safe_attacks = bishop_attacks(sq, gs->occupied[both]) & BLACK_SPACE_MASK & ~white_pawn_attacks;
            black_bonus_squares += count_bits(safe_attacks);
        }

        bitboard = gs->pieces[r];
        while(bitboard) {
            int sq = lsb_index(bitboard);
            pop_bit(bitboard, sq);
            U64 safe_attacks = rook_attacks(sq, gs->occupied[both]) & BLACK_SPACE_MASK & ~white_pawn_attacks;
            black_bonus_squares += count_bits(safe_attacks);
        }
        
        total_score.opening -= black_bonus_squares * SPACE_BONUS.opening;
    }

    return total_score;
}


const Score PAWN_SHIELD_PENALTY[8] = {
    {-14, -18}, {-14, -18}, { -9, -15}, { -4,  -7},
    {  6,   0}, { 13,   7}, { 20,  14}, { 29,  22}
};

const int ATTACK_WEIGHT[6] = {0, 31, 33, 53, 93, 0};

const Score KING_ATTACK_PENALTY[100] = {
    {0,0},{18,25},{27,38},{36,51},{45,64},{54,77},{63,90},{72,103},{81,116},{90,129},
    {99,142},{108,155},{117,168},{126,181},{135,194},{144,207},{153,220},{162,233},{171,246},{180,259},
    {189,272},{198,285},{207,298},{216,311},{225,324},{234,337},{243,350},{252,363},{261,376},{270,389},
    {279,402},{288,415},{297,428},{306,441},{315,454},{324,467},{333,480},{342,493},{351,506},{360,519},
    {369,532},{378,545},{387,558},{396,571},{405,584},{414,597},{423,610},{432,623},{441,636},{450,649},
    {459,662},{468,675},{477,688},{486,701},{495,714},{504,727},{513,740},{522,753},{531,766},{540,779},
    {549,792},{558,805},{567,818},{576,831},{585,844},{594,857},{603,870},{612,883},{621,896},{630,909},
    {639,922},{648,935},{657,948},{666,961},{675,974},{684,987},{693,1000},{702,1013},{711,1026},{720,1039},
    {729,1052},{738,1065},{747,1078},{756,1091},{765,1104},{774,1117},{783,1130},{792,1143},{801,1156},{810,1169}
};

static inline Score evaluate_king_safety_for_side(color c, const game_state* gs) {
    Score score = {0, 0};

    piece_index friendly_king = (c == white) ? K : k;
    piece_index friendly_pawn = (c == white) ? P : p;
    U64 friendly_pawns = gs->pieces[friendly_pawn];
    int king_sq = lsb_index(gs->pieces[friendly_king]);
    int king_file = king_sq % 8;

    for (int f = king_file - 1; f <= king_file + 1; f++) {
        if (f < 0 || f > 7) continue;

        U64 pawn_on_file = friendly_pawns & file_masks[f];
        int pawn_rank = 0;

        if (pawn_on_file == 0) {
            pawn_rank = 0;
        } else {
            int pawn_sq = (c == white) ? lsb_index(pawn_on_file) : (63 - lsb_index(pawn_on_file));
            pawn_rank = (c == white) ? (pawn_sq / 8) + 1 : 8 - (pawn_sq / 8);
        }
        score.opening += PAWN_SHIELD_PENALTY[pawn_rank].opening;
        score.endgame += PAWN_SHIELD_PENALTY[pawn_rank].endgame;
    }

    int attack_score = 0;
    U64 king_zone = king_attacks[king_sq];

    piece_index start_piece = (c == white) ? p : P;
    piece_index end_piece = (c == white) ? q : Q;
    
    for (piece_index piece = start_piece; piece <= end_piece; piece++) {
        U64 bitboard = gs->pieces[piece];
        while (bitboard) {
            int sq = lsb_index(bitboard);
            pop_bit(bitboard, sq);
            U64 attacks = 0;
            int piece_type = piece % 6;

            if (piece_type == N) attacks = knight_attacks[sq];
            else if (piece_type == B) attacks = bishop_attacks(sq, gs->occupied[both]);
            else if (piece_type == R) attacks = rook_attacks(sq, gs->occupied[both]);
            else if (piece_type == Q) attacks = queen_attacks(sq, gs->occupied[both]);
            
            if (attacks & king_zone) {
                attack_score += ATTACK_WEIGHT[piece_type];
            }
        }
    }
    
    if (attack_score > 99) attack_score = 99;
    
    score.opening += KING_ATTACK_PENALTY[attack_score].opening;
    score.endgame += KING_ATTACK_PENALTY[attack_score].endgame;

    return score;
}

Score evaluate_king(const game_state* gs) {
    Score white_safety = evaluate_king_safety_for_side(white, gs);
    Score black_safety = evaluate_king_safety_for_side(black, gs);

    Score final_score;
    final_score.opening = white_safety.opening - black_safety.opening;
    final_score.endgame = white_safety.endgame - black_safety.endgame;

    return final_score;
}


Score evaluate(const game_state* gs) {
    Score material = count_material(gs);
    Score psqt = evaluate_psqt(gs);
    Score pawns = evaluate_pawns(gs);
    Score imbalance = evaluate_imbalance(gs);
    Score pieces = evaluate_pieces(gs);
    Score mobility = evaluate_mobility(gs);
    Score threats = evaluate_threats(gs);
    Score passed = evaluate_passed_pawns(gs);
    Score space = evaluate_space(gs);
    Score king = evaluate_king(gs);

    Score result = {0, 0};

    result.opening += material.opening;
    result.endgame += material.endgame;
    result.opening += psqt.opening;
    result.endgame += psqt.endgame;
    result.opening += pawns.opening;
    result.endgame += pawns.endgame;
    result.opening += imbalance.opening;
    result.endgame += imbalance.endgame;
    result.opening += pieces.opening;
    result.endgame += pieces.endgame;
    result.opening += mobility.opening;
    result.endgame += mobility.endgame;
    result.opening += threats.opening;
    result.endgame += threats.endgame;
    result.opening += passed.opening;
    result.endgame += passed.endgame;
    result.opening += space.opening;
    result.endgame += space.endgame;
    result.opening += king.opening;
    result.endgame += king.endgame;

    return result;
}


const int phase_weights[6] = {0, 1, 1, 2, 4, 0};

const int TOTAL_PHASE = 24;

int calculate_phase(const game_state* gs) {
    int phase = 0;

    for (piece_index p = N; p <= Q; p++) phase += count_bits(gs->pieces[p]) * phase_weights[p]; 
    for (piece_index p = n; p <= q; p++) phase += count_bits(gs->pieces[p]) * phase_weights[p % 6];

    return (phase > TOTAL_PHASE) ? TOTAL_PHASE : phase;
}

int get_final_evaluation(const game_state* gs) {
    Score score = evaluate(gs);

    int phase = calculate_phase(gs);

    int final_eval = ( (score.opening * phase) + (score.endgame * (TOTAL_PHASE - phase)) ) / TOTAL_PHASE;
    
    return (gs->side == white) ? final_eval : -final_eval;
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------*/

typedef enum { HASH_FLAG_EXACT, HASH_FLAG_ALPHA, HASH_FLAG_BETA } HashFlag;

typedef struct {
    U64 key;
    int depth;
    HashFlag flag;
    int score;
    U16 best_move;
} TTEntry;


TTEntry* transposition_table = NULL;
int tt_size = 0;

void init_transposition_table(int megabytes) {
    tt_size = (megabytes * 1024 * 1024) / sizeof(TTEntry);
    
    if (transposition_table != NULL) {
        free(transposition_table);
    }
    
    transposition_table = (TTEntry*) malloc(tt_size * sizeof(TTEntry));
    
    memset(transposition_table, 0, tt_size * sizeof(TTEntry));
    printf("Transposition table initialized with %d entries.\n", tt_size);
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------*/
int alpha_beta_search(game_state* gs, int depth, int alpha, int beta) {
    
    int index = gs->hash_key % tt_size;
    TTEntry* entry = &transposition_table[index];
    
    if (entry->key == gs->hash_key && entry->depth >= depth) {
        
        if (entry->flag == HASH_FLAG_EXACT) {
            return entry->score;
        }
        if (entry->flag == HASH_FLAG_ALPHA && entry->score > alpha) {
            alpha = entry->score;
        }
        if (entry->flag == HASH_FLAG_BETA && entry->score < beta) {
            beta = entry->score;
        }
        if (alpha >= beta) {
            return entry->score;
        }
    }

    if (depth == 0) {
        return get_final_evaluation(gs);
    }

    moves_struct move_list;
    game_history history;
    history.ply_count = 0;

    generate_moves(gs, &move_list);

    if (move_list.count == 0) {
        
        square_index king_sq = lsb_index(gs->pieces[(gs->side == white) ? K : k]);
        if (is_square_attacked(gs, king_sq, gs->side ^ 1)) {
            return -100000 + gs->fullmove_number; 
        }
        return 0; 
    }

    U16 hash_move = 0;
    if (entry->key == gs->hash_key) {
        hash_move = entry->best_move;
    }


    U16 best_move_found = 0;
    HashFlag hash_flag = HASH_FLAG_ALPHA;
    
    if (hash_move != 0) {

        history.ply_count = 0;
        if (make_move(gs, hash_move, &history)) {
            int score = -alpha_beta_search(gs, depth - 1, -beta, -alpha);
            unmake_move(gs, &history);

            if (score >= beta) {
                entry->key = gs->hash_key; entry->depth = depth; entry->score = beta;
                entry->flag = HASH_FLAG_BETA; entry->best_move = hash_move;
                return beta; 
            }
            if (score > alpha) {
                alpha = score;
                best_move_found = hash_move;
                hash_flag = HASH_FLAG_EXACT;            
            }
        }
    }

    for (int i = 0; i < move_list.count; i++) {
        if (move_list.moves[i] == hash_move) { continue; }
        history.ply_count = 0;


        if (make_move(gs, move_list.moves[i], &history)) {
            int score = -alpha_beta_search(gs, depth - 1, -beta, -alpha);
            unmake_move(gs, &history);

            if (score >= beta) {
                entry->key = gs->hash_key;
                entry->depth = depth;
                entry->score = beta;
                entry->flag = HASH_FLAG_BETA;
                entry->best_move = move_list.moves[i];
                return beta; 
            }
            if (score > alpha) {
                alpha = score;
                best_move_found = move_list.moves[i];
                hash_flag = HASH_FLAG_EXACT;            
            }
        }
    }
    entry->key = gs->hash_key;
    entry->depth = depth;
    entry->score = alpha;
    entry->flag = hash_flag;
    entry->best_move = best_move_found;
    return alpha;
}

U16 search_root(game_state* gs, int depth) {
    U16 best_move = 0;
    int max_score = INT_MIN;

    moves_struct move_list;
    game_history history;
    history.ply_count = 0;

    generate_moves(gs, &move_list);

    for (int i = 0; i < move_list.count; i++) {
        U16 move = move_list.moves[i];
        
        if (make_move(gs, move, &history)) {
            int score = -alpha_beta_search(gs, depth - 1, -INT_MAX, INT_MAX);
            unmake_move(gs, &history);

            if (score > max_score) {
                max_score = score;
                best_move = move;
            }
        }
    }
    return best_move;
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------*/

U16 get_user_move(const game_state* gs) {
    moves_struct move_list;
    generate_moves(gs, &move_list);
    char input_buffer[16];

    while (1) {
        printf("Enter your move (e.g., e2e4 or g7g8q for promotion): ");
        fflush(stdout);

        if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL) {
            continue;
        }

        if (strlen(input_buffer) < 4) {
            printf("Invalid input. Move must be at least 4 characters long.\n");
            continue;
        }

        char file_from_char = input_buffer[0];
        char rank_from_char = input_buffer[1];
        char file_to_char = input_buffer[2];
        char rank_to_char = input_buffer[3];
        char promo_char = (strlen(input_buffer) >= 5) ? tolower(input_buffer[4]) : ' ';

        if (file_from_char < 'a' || file_from_char > 'h' ||
            rank_from_char < '1' || rank_from_char > '8' ||
            file_to_char   < 'a' || file_to_char   > 'h' ||
            rank_to_char   < '1' || rank_to_char   > '8')
        {
            printf("Invalid square format.\n");
            continue;
        }

        int from_file = file_from_char - 'a';
        int from_rank = rank_from_char - '1';
        square_index from_sq = (7 - from_rank) * 8 + from_file;

        int to_file = file_to_char - 'a';
        int to_rank = rank_to_char - '1';
        square_index to_sq = (7 - to_rank) * 8 + to_file;

        for (int i = 0; i < move_list.count; i++) {
            U16 legal_move = move_list.moves[i];
            if (get_move_source(legal_move) == from_sq && get_move_target(legal_move) == to_sq) {
                move_flags flag = get_move_flag(legal_move);

                if (flag == promotion) {
                    promo_pieces promo_type = get_move_promo_piece(legal_move);
                    piece_index promoted_piece = (gs->side == white) ? white_promo_map[promo_type] 
                                                                     : black_promo_map[promo_type];
                    char promoted_piece_char = tolower(piece_ascii[promoted_piece]);

                    if (promoted_piece_char == promo_char) {
                        return legal_move;
                    }
                } else {
                    if (promo_char == ' ' || promo_char == '\n' || promo_char == '\r') {
                        return legal_move;
                    }
                }
            }
        }

        printf("That is not a legal move. Please try again.\n");
    }
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------*/
static inline U64 swap_U64(U64 n) {
    return __builtin_bswap64(n);
}
static inline U16 swap_U16(U16 n) {
    return __builtin_bswap16(n);
}

#define U32 uint32_t
#define MAX_BOOK_MOVES 32
#define MAX_BOOK_ENTRIES 500000

typedef struct {
    U64 key;
    U16 moves[MAX_BOOK_MOVES];
    int num_moves;
} BookEntry;

typedef struct {
    U64 key;
    U16 move;
    U16 weight;
    uint32_t learn;
} RawBookEntry;

BookEntry opening_book[MAX_BOOK_ENTRIES];
int book_size = 0;

// This helper decodes the special Polyglot move format into your engine's U16 format
U16 decode_polyglot_move(U16 poly_move, const game_state* gs) {
    // Polyglot move format:
    // fffrrr fffrrr (from_file, from_rank, to_file, to_rank)
    int from_file = poly_move & 0x7;
    int from_rank = (poly_move >> 3) & 0x7;
    int to_file = (poly_move >> 6) & 0x7;
    int to_rank = (poly_move >> 9) & 0x7;
    int promo_piece_poly = (poly_move >> 12) & 0x7;

    // Convert to your engine's square indices (assuming a8=0, h1=63)
    square_index from_sq = (7 - from_rank) * 8 + from_file;
    square_index to_sq = (7 - to_rank) * 8 + to_file;
    
    // Find the matching legal move in the current position
    moves_struct move_list;
    generate_moves(gs, &move_list);

    for (int i = 0; i < move_list.count; i++) {
        U16 legal_move = move_list.moves[i];
        if (get_move_source(legal_move) == from_sq && get_move_target(legal_move) == to_sq) {
            if (promo_piece_poly != 0) {
                // If it's a promotion in the book move
                if (get_move_flag(legal_move) == promotion) {
                    // Check if the promotion piece matches
                    // Polyglot: 1=N, 2=B, 3=R, 4=Q
                    // Your engine: 0=N, 1=B, 2=R, 3=Q
                    promo_pieces promo_type = get_move_promo_piece(legal_move);
                    if ((promo_piece_poly - 1) == promo_type) {
                        return legal_move;
                    }
                }
            } else {
                // Not a promotion
                return legal_move;
            }
        }
    }
    return 0; // Should not happen if book is valid for the position
}


// The new function to load the binary book file
void load_opening_book(const char* filename) {
    FILE* file = fopen(filename, "rb"); // Open in binary read mode
    if (file == NULL) {
        printf("Opening book '%s' not found.\n", filename);
        return;
    }

    RawBookEntry raw_entry;
    book_size = 0;
    
    // The Polyglot book is sorted. We group moves with the same key together.
    U64 last_key = 0;
    int current_entry_index = -1;

    while (fread(&raw_entry, sizeof(RawBookEntry), 1, file) == 1 && book_size < MAX_BOOK_ENTRIES) {
        U64 key = swap_U64(raw_entry.key); // Byte-swap the key
        
        if (key != last_key) {
            current_entry_index++;
            book_size++;
            opening_book[current_entry_index].key = key;
            opening_book[current_entry_index].num_moves = 0;
            last_key = key;
        }
        
        // Add the move to the current entry, ensuring not to overflow
        if (opening_book[current_entry_index].num_moves < MAX_BOOK_MOVES) {
            opening_book[current_entry_index].moves[opening_book[current_entry_index].num_moves] = swap_U16(raw_entry.move);
            opening_book[current_entry_index].num_moves++;
        }
    }

    printf("Opening book loaded with %d unique positions.\n", book_size);
    fclose(file);
}

U16 probe_opening_book(const game_state* gs) {
    if (book_size == 0) return 0;
    
    U64 current_key = gs->hash_key;
    
    // --- Binary Search for the Book Entry ---
    int low = 0, high = book_size - 1, mid;
    BookEntry* entry = NULL;
    
    while(low <= high) {
        mid = low + (high - low) / 2;
        if (opening_book[mid].key == current_key) {
            entry = &opening_book[mid];
            break;
        }
        if (opening_book[mid].key > current_key) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    // --- End of Binary Search ---

    if (entry != NULL) {
        // Found a matching position in the book!
        // Pick a random move from the list to add variety to openings.
        int random_move_index = rand() % entry->num_moves;
        U16 polyglot_move = entry->moves[random_move_index];
        
        // Decode the Polyglot move into our engine's internal format
        return decode_polyglot_move(polyglot_move, gs);
    }

    return 0; // No move found for this position
}

/* ---------------------------------------------------------------------------------------------------------------------------------------------------------*/

// A helper function to print moves in algebraic notation
void print_move_algebraic(U16 move, color side) {
    square_index from = get_move_source(move);
    square_index to = get_move_target(move);
    move_flags flag = get_move_flag(move);
    
    printf("%s%s", square_ascii[from], square_ascii[to]);
    
    if (flag == promotion) {
        promo_pieces promo_type = get_move_promo_piece(move);
        piece_index promoted_piece = (side == white) ? white_promo_map[promo_type] 
                                                     : black_promo_map[promo_type];
        printf("%c", tolower(piece_ascii[promoted_piece]));
    }
}

// The main game loop for the engine
int main() {
    // --- INITIALIZATION ---
    srand(time(NULL)); // Seed the random number generator
    init_all();
    init_transposition_table(128); // Initialize TT with 128 MB
    load_opening_book("Book.bin"); // Load your downloaded book

    game_state gs;
    parse_fen(start_position, &gs);
    gs.hash_key = generate_hash_key(&gs); // Generate the initial hash key

    // --- GAME HISTORY FOR REPETITION CHECK ---
    // Max game length of 1024 half-moves (512 full moves)
    U64 game_history_log[1024]; 
    int ply = 0;




    // --- SELF-PLAY GAME LOOP ---
    while (1) {
        
        print_board(&gs);
        make_move(&gs, get_user_move(&gs), NULL);

        // --- CHECK FOR GAME OVER CONDITIONS ---
        moves_struct move_list;
        generate_moves(&gs, &move_list);

        
        // --- ENGINE THINKING (WITH ITERATIVE DEEPENING) ---
        char* side_str = (gs.side == white) ? "White" : "Black";
        printf("\n%d. %s to move. Thinking...\n", gs.fullmove_number, side_str);
        
        U16 best_move = probe_opening_book(&gs);
        if (best_move != 0) {printf("Move from opening book: ");}
        else{
            int best_score = 0;
            long start_time = get_time_ms();
            
            // Set search parameters
            int time_limit_ms = 5 * 60 * 1000; // Think for up to 5 mins
            int max_search_depth = 7;  // A practical upper limit for the loop

            // The iterative deepening loop
            for (int current_depth = 1; current_depth <= max_search_depth; current_depth++) {
                U16 move_this_iteration = 0;
                move_this_iteration= search_root(&gs, current_depth);
                
                if (move_this_iteration != 0) {
                    best_move = move_this_iteration;
                }

                long elapsed_time = get_time_ms() - start_time;
                
                printf("info depth %d time %ldms move \n", current_depth, elapsed_time);
                print_move_algebraic(best_move, gs.side);
                printf("\n");
                fflush(stdout);

                if (elapsed_time >= time_limit_ms) {
                    printf("Time limit reached. Playing best move from depth %d.\n", current_depth);
                    break;
                }
                // Optional: Stop if mate is found
                if (abs(best_score) > 90000) {
                    printf("Mate found. Stopping search.\n");
                    break;
                }
            }
        }   
        printf("%s plays: ", side_str);
        print_move_algebraic(best_move, gs.side);
        printf("\n");
        
        // --- MAKE THE MOVE ---
        if (best_move != 0) {
            make_move(&gs, best_move, NULL);
        } else {
            printf("Error: No best move found. Game cannot continue.\n");
            break;
        }
        
        // --- UPDATE HISTORY ---
        game_history_log[ply] = gs.hash_key;
        ply++;
    }

    return 0;
}