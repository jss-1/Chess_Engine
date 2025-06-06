#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/time.h>
#include <x86intrin.h>

#define U64 uint64_t
#define U16 uint16_t

#define empty_board "8/8/8/8/8/8/8/8 b - - "
#define start_position "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
#define tricky_position "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 "

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
} game_state;

#define set_bit(bitboard, square) ((bitboard) |= (1ULL << (square)))
#define get_bit(bitboard, square) ((bitboard) & (1ULL << (square)))
#define pop_bit(bitboard, square) ((bitboard) &= ~(1ULL << (square)))
#define count_bits(bitboard) (__builtin_popcountll(bitboard))
#define lsb_index(bitboard) (bitboard == 0 ? no_sq : __builtin_ctzll(bitboard))

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
    init_slider_masks();
    init_magic_bitboards();
}

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
        undo->captured_piece = (flag == enpassant) ? (gs->side == white ? p : P) : captured_piece;
    }

    gs->board[to] = piece_to_move;
    gs->board[from] = no_piece;
    pop_bit(gs->pieces[piece_to_move], from);
    set_bit(gs->pieces[piece_to_move], to);

    gs->halfmove_clock++;
    if (piece_to_move == P || piece_to_move == p) gs->halfmove_clock = 0;
    
    if (captured_piece != no_piece) {
        pop_bit(gs->pieces[captured_piece], to);
        gs->halfmove_clock = 0;
    }
    
    gs->en_passant_square = no_sq;

    if(flag == promotion) {
        piece_index promoted_piece = (gs->side == white) ? white_promo_map[promo_type] : black_promo_map[promo_type];
        pop_bit(gs->pieces[piece_to_move], to);
        set_bit(gs->pieces[promoted_piece], to);
        gs->board[to] = promoted_piece;
    } else if (flag == enpassant) {
        square_index captured_pawn_sq = (gs->side == white) ? to + 8 : to - 8;
        pop_bit(gs->pieces[(gs->side == white) ? p : P], captured_pawn_sq);
        gs->board[captured_pawn_sq] = no_piece;
        gs->halfmove_clock = 0;
    } else if (flag == castling) {
        switch(to) {
            case g1: pop_bit(gs->pieces[R], h1); set_bit(gs->pieces[R], f1); gs->board[h1] = no_piece; gs->board[f1] = R; break;
            case c1: pop_bit(gs->pieces[R], a1); set_bit(gs->pieces[R], d1); gs->board[a1] = no_piece; gs->board[d1] = R; break;
            case g8: pop_bit(gs->pieces[r], h8); set_bit(gs->pieces[r], f8); gs->board[h8] = no_piece; gs->board[f8] = r; break;
            case c8: pop_bit(gs->pieces[r], a8); set_bit(gs->pieces[r], d8); gs->board[a8] = no_piece; gs->board[d8] = r; break;
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

void initialize_start_position(game_state* restrict gs) {
    parse_fen(start_position, gs);
}

int main() {
    init_all();
    game_state gs;
    initialize_start_position(&gs);
    
    int run_uci_mode = 0;
    if (run_uci_mode) {
    } else {
        print_board(&gs);
        for(int d = 1; d <= 10; d++) {
            game_state test_gs = gs;
            perft_test(&test_gs, d);
        }
    }

    return 0;
}
