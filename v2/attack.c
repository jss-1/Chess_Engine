/**
 * @file attack.c
 * @brief Precompute pawn, knight, king and sliding‐piece attack tables using BMI2/PEXT.
 */

#include "attack.h"
#include <immintrin.h>  /* for _pext_u64 */
#include <string.h>     /* for memset */

/** File‐rank exclusion masks to avoid wrap‐around on shifts. */
static const U64 not_a_file  = 0xFEFEFEFEFEFEFEFEULL;
static const U64 not_h_file  = 0x7F7F7F7F7F7F7F7FULL;
static const U64 not_hg_file = 0x3F3F3F3F3F3F3F3FULL;
static const U64 not_ab_file = 0xFCFCFCFCFCFCFCFCULL;

/*————————————————————————————————————————————————————————————————————————*/
/** Pawn attack lookup: pawn_attacks[color][square] */
U64 pawn_attacks[2][64];
/** Knight attack lookup: knight_attacks[square] */
U64 knight_attacks[64];
/** King attack lookup: king_attacks[square] */
U64 king_attacks[64];

/** Occupancy masks for bishop rays (file edges removed) */
U64 mask_bishop[64];
/** Occupancy masks for rook rays (rank edges removed) */
U64 mask_rook[64];

/** Magic‐bitboard descriptor for bishops */
Magic bishopM[64];
/** Magic‐bitboard descriptor for rooks */
Magic rookM[64];

/** Flat attack table for all bishop squares */
static U64 bishopTable[0x1480];
/** Flat attack table for all rook squares */
static U64 rookTable  [0x19000];

/*————————————————————————————————————————————————————————————————————————*/
/**
 * @brief Set bit `sq` in bitboard `*bb`.
 */
static inline void set_bit(U64 *bb, int sq) {
    *bb |= (1ULL << sq);
}

/**
 * @brief Clear bit `sq` in bitboard `*bb`.
 */
static inline void pop_bit(U64 *bb, int sq) {
    *bb &= ~(1ULL << sq);
}

/**
 * @brief Count bits in `b`.
 */
static inline int count_bits(U64 b) {
    return __builtin_popcountll(b);
}

/**
 * @brief Return index of least‐significant 1‐bit in `b`.
 */
static inline int get_ls1b_index(U64 b) {
    return __builtin_ctzll(b);
}

/*————————————————————————————————————————————————————————————————————————*/
/**
 * @brief Compute pawn attack mask for one square.
 * @param sq    Source square (0..63).
 * @param side  white=0 or black=1.
 * @return      Bitboard of squares attacked by a pawn on `sq`.
 */
static U64 mask_pawn_attacks_one(int sq, int side) {
    U64 b = 1ULL << sq, attacks = 0ULL;
    if (side == white) {
        if ((b >> 7) & not_a_file) attacks |= (b >> 7);
        if ((b >> 9) & not_h_file) attacks |= (b >> 9);
    } else {
        if ((b << 7) & not_h_file) attacks |= (b << 7);
        if ((b << 9) & not_a_file) attacks |= (b << 9);
    }
    return attacks;
}

/**
 * @brief Compute knight attack mask for one square.
 * @param sq  Source square (0..63).
 * @return    Bitboard of squares attacked by a knight on `sq`.
 */
static U64 mask_knight_attacks_one(int sq) {
    U64 b = 1ULL << sq, a = 0ULL;
    if ((b >> 17) & not_h_file)  a |= b >> 17;
    if ((b >> 15) & not_a_file)  a |= b >> 15;
    if ((b >> 10) & not_hg_file) a |= b >> 10;
    if ((b >>  6) & not_ab_file) a |= b >>  6;
    if ((b << 17) & not_a_file)  a |= b << 17;
    if ((b << 15) & not_h_file)  a |= b << 15;
    if ((b << 10) & not_ab_file) a |= b << 10;
    if ((b <<  6) & not_hg_file) a |= b <<  6;
    return a;
}

/**
 * @brief Compute king attack mask for one square.
 * @param sq  Source square (0..63).
 * @return    Bitboard of squares attacked by a king on `sq`.
 */
static U64 mask_king_attacks_one(int sq) {
    U64 b = 1ULL << sq, a = 0ULL;
    if (b >> 8)                 a |= b >> 8;
    if ((b >> 9) & not_h_file)  a |= b >> 9;
    if ((b >> 7) & not_a_file)  a |= b >> 7;
    if ((b >> 1) & not_h_file)  a |= b >> 1;
    if (b << 8)                 a |= b << 8;
    if ((b << 9) & not_a_file)  a |= b << 9;
    if ((b << 7) & not_h_file)  a |= b << 7;
    if ((b << 1) & not_a_file)  a |= b << 1;
    return a;
}

/**
 * @brief Build occupancy subset for a given index.
 * @param index  Subset index (0..(1<<bits)-1).
 * @param bits   Number of relevant bits in `mask`.
 * @param mask   Bitboard of relevant blocker bits.
 * @return       Occupancy bitboard for this subset.
 */
static U64 set_occupancy(int index, int bits, U64 mask) {
    U64 occ = 0ULL;
    for (int i = 0; i < bits; i++) {
        int sq = get_ls1b_index(mask);
        mask &= mask - 1;
        if (index & (1 << i))
            occ |= (1ULL << sq);
    }
    return occ;
}

/**
 * @brief Generate bishop attacks on‐the‐fly for `blockers`.
 * @param sq        Source square (0..63).
 * @param blockers  Bitboard of all occupied squares.
 * @return          Bitboard of bishop attacks.
 */
static U64 bishop_on_the_fly(int sq, U64 blockers) {
    U64 attacks = 0ULL;
    int r = sq >> 3, f = sq & 7;
    for (int rr = r + 1, ff = f + 1; rr <= 7 && ff <= 7; rr++, ff++) {
        U64 b = 1ULL << (rr * 8 + ff);
        attacks |= b; if (b & blockers) break;
    }
    for (int rr = r + 1, ff = f - 1; rr <= 7 && ff >= 0; rr++, ff--) {
        U64 b = 1ULL << (rr * 8 + ff);
        attacks |= b; if (b & blockers) break;
    }
    for (int rr = r - 1, ff = f + 1; rr >= 0 && ff <= 7; rr--, ff++) {
        U64 b = 1ULL << (rr * 8 + ff);
        attacks |= b; if (b & blockers) break;
    }
    for (int rr = r - 1, ff = f - 1; rr >= 0 && ff >= 0; rr--, ff--) {
        U64 b = 1ULL << (rr * 8 + ff);
        attacks |= b; if (b & blockers) break;
    }
    return attacks;
}

/**
 * @brief Generate rook attacks on‐the‐fly for `blockers`.
 * @param sq        Source square (0..63).
 * @param blockers  Bitboard of all occupied squares.
 * @return          Bitboard of rook attacks.
 */
static U64 rook_on_the_fly(int sq, U64 blockers) {
    U64 attacks = 0ULL;
    int r = sq >> 3, f = sq & 7;
    for (int rr = r + 1; rr <= 7; rr++) {
        U64 b = 1ULL << (rr * 8 + f);
        attacks |= b; if (b & blockers) break;
    }
    for (int rr = r - 1; rr >= 0; rr--) {
        U64 b = 1ULL << (rr * 8 + f);
        attacks |= b; if (b & blockers) break;
    }
    for (int ff = f + 1; ff <= 7; ff++) {
        U64 b = 1ULL << (r * 8 + ff);
        attacks |= b; if (b & blockers) break;
    }
    for (int ff = f - 1; ff >= 0; ff--) {
        U64 b = 1ULL << (r * 8 + ff);
        attacks |= b; if (b & blockers) break;
    }
    return attacks;
}

/**
 * @brief Initialize pawn, knight, and king attack tables.
 */
static void init_leaper_attacks(void) {
    for (int sq = 0; sq < 64; ++sq) {
        pawn_attacks[white][sq] = mask_pawn_attacks_one(sq, white);
        pawn_attacks[black][sq] = mask_pawn_attacks_one(sq, black);
        knight_attacks[sq]      = mask_knight_attacks_one(sq);
        king_attacks[sq]        = mask_king_attacks_one(sq);
    }
}

/**
 * @brief Initialize occupancy masks for bishop and rook rays.
 */
static void init_slider_masks(void) {
    for (int sq = 0; sq < 64; ++sq) {
        mask_bishop[sq] = bishop_on_the_fly(sq, 0ULL);
        mask_rook  [sq] = rook_on_the_fly  (sq, 0ULL);
    }
}

/**
 * @brief Build PEXT‐based magic tables for bishop & rook.
 */
static void init_magic_tables(void) {
    int bishopOfs = 0, rookOfs = 0;
    for (int sq = 0; sq < 64; ++sq) {
        /* ─ Bishop ───────────────────────────────────────────────────── */
        {
            Magic *m = &bishopM[sq];
            m->mask  = mask_bishop[sq];
            m->table = bishopTable + bishopOfs;
            int bits = count_bits(m->mask), n = 1 << bits;
            for (int i = 0; i < n; i++) {
                U64 occ = set_occupancy(i, bits, m->mask);
                unsigned idx = (unsigned)_pext_u64(occ, m->mask);
                m->table[idx] = bishop_on_the_fly(sq, occ);
            }
            bishopOfs += n;
        }
        /* ─ Rook ──────────────────────────────────────────────────────── */
        {
            Magic *m = &rookM[sq];
            m->mask  = mask_rook[sq];
            m->table = rookTable + rookOfs;
            int bits = count_bits(m->mask), n = 1 << bits;
            for (int i = 0; i < n; i++) {
                U64 occ = set_occupancy(i, bits, m->mask);
                unsigned idx = (unsigned)_pext_u64(occ, m->mask);
                m->table[idx] = rook_on_the_fly(sq, occ);
            }
            rookOfs += n;
        }
    }
}

/**
 * @brief Public entry: initialize all attack tables. Must be called once at startup.
 */
void init_attacks(void) {
    init_leaper_attacks();
    init_slider_masks();
    init_magic_tables();
}
