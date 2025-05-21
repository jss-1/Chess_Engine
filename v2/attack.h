/**
 * @file attack.h
 * @brief Magic‐bitboard attack tables using BMI2/PEXT only.
 */

#ifndef ATTACK_H
#define ATTACK_H

#include <stdint.h>
#include <immintrin.h>    // for _pext_u64

/// force PEXT‐only path
#define USE_PEXT 1

#include "board.h"        // for U64, square_index, color, etc.

/** Pawn attack masks: [color][square] → bitboard of pawn captures */
extern U64 pawn_attacks[2][64];
/** Knight attack masks: [square] → bitboard of knight jumps */
extern U64 knight_attacks[64];
/** King attack masks: [square] → bitboard of king moves */
extern U64 king_attacks[64];

/** Bishop occupancy masks: [square] → slider mask */
extern U64 mask_bishop[64];
/** Rook occupancy masks: [square] → slider mask */
extern U64 mask_rook[64];





/**
 * @struct Magic
 * @brief Magic bitboard helper for sliding-piece attacks.
 *
 * Each sliding-piece square gets a Magic entry that holds:
 *  - mask:    the occupancy mask for that square (see mask_bishop/rook)
 *  - table:   pointer to the precomputed attack table for that square
 */
typedef struct {
    U64    mask;   /**< Occupancy mask for blocker extraction */
    U64   *table;  /**< Base pointer into the attack table entries */
} Magic;

/**
 * @brief Per-square magic data for bishop (diagonal) attacks.
 *
 * bishopM[sq] holds the Magic struct for square @p sq, enabling
 * fast PEXT-based bishop attack lookups.
 */
extern Magic bishopM[64];

/**
 * @brief Per-square magic data for rook (orthogonal) attacks.
 *
 * rookM[sq] holds the Magic struct for square @p sq, enabling
 * fast PEXT-based rook attack lookups.
 */
extern Magic rookM[64];


/**
 * @brief Fill pawn_attacks, knight_attacks & king_attacks.
 */
void init_leaper_attacks(void);

/**
 * @brief Fill mask_bishop and mask_rook arrays.
 */
void init_slider_masks(void);

/**
 * @brief Build PEXT‐based magic tables for bishops & rooks.
 */
void init_magic_tables(void);

/**
 * @brief Convenience initializer for _all_ attack tables.
 */
static inline void init_attacks(void) {
    init_leaper_attacks();
    init_slider_masks();
    init_magic_tables();
}

/**
 * @brief Get bishop (or diagonal‐queen) attacks via PEXT.
 * @param sq  index 0..63
 * @param occ occupancy bitboard
 */
static inline U64 get_bishop_attacks(int sq, U64 occ) {
    Magic *m = &bishopM[sq];
    return m->table[_pext_u64(occ & m->mask, m->mask)];
}

/**
 * @brief Get rook (or orthogonal‐queen) attacks via PEXT.
 * @param sq  index 0..63
 * @param occ occupancy bitboard
 */
static inline U64 get_rook_attacks(int sq, U64 occ) {

    Magic *m = &rookM[sq];
    return m->table[_pext_u64(occ & m->mask, m->mask)];
}

/**
 * @brief Queen attacks = bishop ∪ rook attacks.
 */
static inline U64 get_queen_attacks(int sq, U64 occ) {
    return get_bishop_attacks(sq, occ) | get_rook_attacks(sq, occ);
}

#endif // ATTACK_H
