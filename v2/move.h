/**
 * @file moves.h
 * @brief Defines move representation, move list structure, and move generation interfaces.
 */

#ifndef MOVES_H
#define MOVES_H

#include <stdint.h>
#include "board.h"

/**
 * @typedef move_t
 * @brief 32-bit move encoding:
 *        - bits 0-5   : from square (0-63)
 *        - bits 6-11  : to square (0-63)
 *        - bits 12-15 : moved piece (0=none, see board.h piece codes)
 *        - bits 16-19 : promotion piece (0 if no promotion)
 *        - bits 20-23 : special flags (capture, en-passant, castling, double pawn)
 */
typedef uint32_t move_t;

/** @name Bit shifts for move fields
 * @{ */
enum {
    FROM   = 0,  /**< Shift for 'from' square */
    TO     = 6,  /**< Shift for 'to' square */
    PIECE  = 12, /**< Shift for moved piece */
    PROMO  = 16, /**< Shift for promotion piece */
    FLAG   = 20  /**< Shift for move flags */
};
/** @} */

/** @name Masks for move fields
 * @{ */
enum {
    MOVE_MASK_6       = 0x3F, /**< Mask for 6-bit fields */
    MOVE_MASK_4       = 0x0F  /**< Mask for 4-bit fields */
};
/** @} */

/**
 * @brief Extract the 'from' square from a move.
 * @param m Encoded move
 * @return Square index (0-63)
 */
#define MOVE_FROM(m)    (((m) >> MOVE_SHIFT_FROM) & MOVE_MASK_6)

/**
 * @brief Extract the 'to' square from a move.
 * @param m Encoded move
 * @return Square index (0-63)
 */
#define MOVE_TO(m)      (((m) >> MOVE_SHIFT_TO)   & MOVE_MASK_6)

/**
 * @brief Extract the moved piece code from a move.
 * @param m Encoded move
 * @return Piece code (see board.h)
 */
#define MOVE_PIECE(m)   (((m) >> MOVE_SHIFT_PIECE)& MOVE_MASK_4)

/**
 * @brief Extract the promotion piece code from a move.
 * @param m Encoded move
 * @return Promotion piece code (0 if no promotion)
 */
#define MOVE_PROMO(m)   (((m) >> MOVE_SHIFT_PROMO)& MOVE_MASK_4)

/**
 * @brief Extract special flags from a move.
 * @param m Encoded move
 * @return Flags bitmask
 */
#define MOVE_FLAGS(m)   (((m) >> MOVE_SHIFT_FLAG) & MOVE_MASK_4)

/** @enum Move flags
 *  @brief Special move types encoded in bits 20-23
 */
enum {
    none         = 0, /**< No special flag */
    CAPTURE      = 1, /**< Capture */
    DOUBLE_PAWN  = 2, /**< Two-square pawn advance */
    EN_PASSANT   = 4, /**< En-passant capture */
    CASTLING     = 8  /**< King castling */
};

/**
 * @brief Construct a move from components.
 * @param from  Source square (0-63)
 * @param to    Destination square (0-63)
 * @param piece Moved piece code
 * @param promo Promotion piece code (0 if none)
 * @param flags Special move flags
 * @return Encoded move_t
 */
static inline move_t make_move(int from, int to, int piece, int promo, int flags) {
    return ((uint32_t)(from & MOVE_MASK_6)   << MOVE_SHIFT_FROM)  |
           ((uint32_t)(to   & MOVE_MASK_6)   << MOVE_SHIFT_TO)    |
           ((uint32_t)(piece& MOVE_MASK_4)   << MOVE_SHIFT_PIECE) |
           ((uint32_t)(promo& MOVE_MASK_4)   << MOVE_SHIFT_PROMO) |
           ((uint32_t)(flags& MOVE_MASK_4)   << MOVE_SHIFT_FLAG);
}

/**
 * @def MAX_MOVES
 * @brief Maximum number of moves in a move list
 */
#define MAX_MOVES 256

/**
 * @struct move_list
 * @brief Container for generated moves.
 */
typedef struct {
    move_t moves[MAX_MOVES]; /**< Array of moves */
    int    count;            /**< Number of moves in the list */
} move_list;

/**
 * @brief Generate all legal moves for the given position.
 * @param pos Current board position
 * @param list Output move list (must be pre-allocated)
 */
void generate_moves(const position *pos, move_list *list);

/**
 * @brief Convert a move to a UCI string.
 * @param move Encoded move
 * @param buf  Buffer with at least 6 bytes
 * @return Pointer to buf containing the NUL-terminated string
 */
char *move_to_string(move_t move, char *buf);

/**
 * @brief Parse a UCI move string in the context of the position.
 * @param str   UCI move (e.g., "e2e4", "e7e8q")
 * @param pos   Current position for legality/context
 * @return Encoded move_t, or 0 if invalid
 */
move_t parse_move(const char *str, const position *pos);

#endif // MOVES_H
