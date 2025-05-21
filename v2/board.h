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
 *        - bits 16-19 : capture piece code (0 if no capture)
 *        - bits 20-23 : promotion piece code (0 if no promotion)
 *        - bits 24-27 : special flags (double pawn, en-passant, castling)
 */
typedef uint32_t move_t;

/** @name bit shifts for move fields
 * @{ */
enum {
    from    = 0,  /**< shift for 'from' square */
    to      = 6,  /**< shift for 'to' square */
    piece   = 12, /**< shift for moved piece */
    capture = 16, /**< shift for capture piece */
    promo   = 20, /**< shift for promotion piece */
    flag    = 24  /**< shift for move flags */
};
/** @} */

/** @name masks for move fields
 * @{ */
enum {
    move_mask_6 = 0x3f, /**< mask for 6-bit fields */
    move_mask_4 = 0x0f  /**< mask for 4-bit fields */
};
/** @} */

/**
 * @brief extract the 'from' square from a move
 * @param m encoded move
 * @return square index (0-63)
 */
#define move_from(m)    (((m) >> from)    & move_mask_6)

/**
 * @brief extract the 'to' square from a move
 * @param m encoded move
 * @return square index (0-63)
 */
#define move_to(m)      (((m) >> to)      & move_mask_6)

/**
 * @brief extract the moved piece code from a move
 * @param m encoded move
 * @return piece code (see board.h)
 */
#define move_piece(m)   (((m) >> piece)   & move_mask_4)

/**
 * @brief extract the captured piece code from a move
 * @param m encoded move
 * @return capture piece code (0 if no capture)
 */
#define move_capture(m) (((m) >> capture) & move_mask_4)

/**
 * @brief extract the promotion piece code from a move
 * @param m encoded move
 * @return promotion piece code (0 if no promotion)
 */
#define move_promo(m)   (((m) >> promo)   & move_mask_4)

/**
 * @brief extract special flags from a move
 * @param m encoded move
 * @return flags bitmask
 */
#define move_flags(m)   (((m) >> flag)    & move_mask_4)

/** @enum move flags
 *  @brief special move types encoded in bits 24-27
 */
enum {
    move_flag_none         = 0, /**< no special flag */
    move_flag_double_pawn  = 1, /**< two-square pawn advance */
    move_flag_en_passant   = 2, /**< en-passant capture */
    move_flag_castling     = 4  /**< king castling */
};

/**
 * @brief construct a move from components
 * @param from    source square (0-63)
 * @param to      destination square (0-63)
 * @param piece   moved piece code
 * @param capture captured piece code (0 if none)
 * @param promo   promotion piece code (0 if none)
 * @param flags   special move flags
 * @return encoded move_t
 */
static inline move_t make_move(int from, int to, int piece, int capture, int promo, int flags) {
    return ((uint32_t)(from    & move_mask_6) << from)    |
           ((uint32_t)(to      & move_mask_6) << to)      |
           ((uint32_t)(piece   & move_mask_4) << piece)   |
           ((uint32_t)(capture & move_mask_4) << capture) |
           ((uint32_t)(promo   & move_mask_4) << promo)   |
           ((uint32_t)(flags   & move_mask_4) << flag);
}

/**
 * @def max_moves
 * @brief maximum number of moves in a move list
 */
#define max_moves 256

/**
 * @struct move_list
 * @brief container for generated moves
 */
typedef struct {
    move_t moves[max_moves]; /**< array of moves */
    int    count;            /**< number of moves in the list */
} move_list;

/**
 * @brief generate all legal moves for the given position
 * @param pos   current board position
 * @param list  output move list (must be pre-allocated)
 */
void generate_moves(const position *pos, move_list *list);

/**
 * @brief convert a move to a UCI string
 * @param move encoded move
 * @param buf  buffer with at least 6 bytes
 * @return pointer to buf containing the NUL-terminated string
 */
char *move_to_string(move_t move, char *buf);

/**
 * @brief parse a UCI move string in the context of the position
 * @param str   UCI move (e.g., "e2e4", "e7e8q")
 * @param pos   current position for legality/context
 * @return encoded move_t, or 0 if invalid
 */
move_t parse_move(const char *str, const position *pos);

#endif // MOVES_H
