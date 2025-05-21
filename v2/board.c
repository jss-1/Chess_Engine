/**
 * @file board.c
 * @brief Implementation of Position, FEN parsing, printing, and attack info.
 */

#include "board.h"
#include "attack.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/// Map each square index to its coordinate string.
static const char *SQ_STR[64] = {
    "a1","b1","c1","d1","e1","f1","g1","h1",
    "a2","b2","c2","d2","e2","f2","g2","h2",
    "a3","b3","c3","d3","e3","f3","g3","h3",
    "a4","b4","c4","d4","e4","f4","g4","h4",
    "a5","b5","c5","d5","e5","f5","g5","h5",
    "a6","b6","c6","d6","e6","f6","g6","h6",
    "a7","b7","c7","d7","e7","f7","g7","h7",
    "a8","b8","c8","d8","e8","f8","g8","h8"
};

/// One–letter ASCII for each piece_index
static const char PIECE_CHAR[12] = {
    'P','N','B','R','Q','K',
    'p','n','b','r','q','k'
};

/// Helper: map a FEN char to piece_index
static int fen_char_to_piece(char c) {
    switch (c) {
      case 'P': return P;
      case 'N': return N;
      case 'B': return B;
      case 'R': return R;
      case 'Q': return Q;
      case 'K': return K;
      case 'p': return p;
      case 'n': return n;
      case 'b': return b;
      case 'r': return r;
      case 'q': return q;
      case 'k': return k;
      default:  return no_piece;
    }
}

void init_startpos(Position *pos) {
    parse_fen(pos,
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR "
        "w KQkq -");
}

void parse_fen(Position *pos, const char *fen) {
    // 1) Clear
    memset(pos, 0, sizeof *pos);
    pos->enpassant = no_sq;

    // 2) Piece placement
    int rank = 8, file = 0;
    const char *pc = fen;
    while (*pc && rank > 0) {
        if (*pc == '/') {
            rank--; file = 0;
            pc++;
        } else if (isdigit(*pc)) {
            file += *pc - '0';
            pc++;
        } else if (isalpha(*pc)) {
            int sq = (rank - 1) * 8 + file;
            int pci = fen_char_to_piece(*pc);
            if (pci != no_piece)
                pos->bitboards[pci] |= (1ULL << sq);
            file++;
            pc++;
        } else {
            // hit side-to-move field
            break;
        }
    }

    // 3) Side to move
    while (*pc == ' ') pc++;
    pos->side = (*pc == 'b') ? black : white;
    pc++;
    
    // 4) Castling rights
    pos->castle = no_castle;
    while (*pc == ' ') pc++;
    if (*pc == '-') {
        pc++;
    } else {
        while (*pc && *pc != ' ') {
            switch (*pc) {
              case 'K': pos->castle |= wk; break;
              case 'Q': pos->castle |= wq; break;
              case 'k': pos->castle |= bk; break;
              case 'q': pos->castle |= bq; break;
            }
            pc++;
        }
    }

    // 5) En-passant
    while (*pc == ' ') pc++;
    if (*pc == '-') {
        pos->enpassant = no_sq;
        pc++;
    } else if (isalpha(pc[0]) && isdigit(pc[1])) {
        int f = pc[0] - 'a';
        int r = pc[1] - '1';
        pos->enpassant = r*8 + f;
        pc += 2;
    }

    // 6) Build occupancies
    pos->occupancies[white] = pos->bitboards[P] | pos->bitboards[N]
                           | pos->bitboards[B] | pos->bitboards[R]
                           | pos->bitboards[Q] | pos->bitboards[K];
    pos->occupancies[black] = pos->bitboards[p] | pos->bitboards[n]
                           | pos->bitboards[b] | pos->bitboards[r]
                           | pos->bitboards[q] | pos->bitboards[k];
    pos->occupancies[both]  = pos->occupancies[white]
                            | pos->occupancies[black];

    // 7) Update attack info
    update_attack_info(pos);
}

void print_board(const Position *pos) {
    puts("");
    for (int r = 7; r >= 0; r--) {
        printf("%d  ", r+1);
        for (int f = 0; f < 8; f++) {
            int sq = r*8 + f;
            char piece = '.';
            // find which piece occupies
            for (int pc = P; pc <= k; pc++) {
                if (pos->bitboards[pc] & (1ULL<<sq)) {
                    piece = PIECE_CHAR[pc];
                    break;
                }
            }
            printf("%c ", piece);
        }
        printf("\n");
    }
    puts("   a b c d e f g h\n");
    printf("Side to move: %s\n", pos->side==white?"White":"Black");
    printf("Castling: %c%c%c%c\n",
        (pos->castle & wk)?'K':'-',
        (pos->castle & wq)?'Q':'-',
        (pos->castle & bk)?'k':'-',
        (pos->castle & bq)?'q':'-');
    if (pos->enpassant!=no_sq)
        printf("En-passant: %s\n", SQ_STR[pos->enpassant]);
    else
        printf("En-passant: -\n");
    printf("In check: %s\n\n", pos->in_check?"Yes":"No");
}

int get_king_square(const Position *pos, int side) {
    U64 bb = pos->bitboards[ side==white ? K : k ];
    if (!bb) return no_sq;
    return __builtin_ctzll(bb);
}

// bring in the attack tables from attacks.c
extern U64 pawn_attacks[2][64];
extern U64 knight_attacks[64];
extern U64 king_attacks[64];
// + the slider/magic‐bitboard routines
extern U64 get_bishop_attacks(int sq, U64 occ);
extern U64 get_rook_attacks  (int sq, U64 occ);

void update_attack_info(Position *pos) {
    // Reset
    pos->checkers = 0;
    pos->pinned   = 0;  // TODO: implement real pin detection

    int us = pos->side;
    int them = us^1;
    int ksq = get_king_square(pos, us);
    if (ksq == no_sq) {
        pos->in_check = false;
        return;
    }

    // 1) pawn checks
    U64 pawn_att = pawn_attacks[them][ksq] & pos->bitboards[ them==white?P:p ];
    pos->checkers |= pawn_att;

    // 2) knight checks
    U64 knight_att = knight_attacks[ksq] & pos->bitboards[ them==white?N:n ];
    pos->checkers |= knight_att;

    // 3) bishop/queen diagonal
    U64 diag = get_bishop_attacks(ksq, pos->occupancies[both])
            & (pos->bitboards[them==white?B:b]
             | pos->bitboards[them==white?Q:q]);
    pos->checkers |= diag;

    // 4) rook/queen orthogonal
    U64 ortho = get_rook_attacks(ksq, pos->occupancies[both])
             & (pos->bitboards[them==white?R:r]
              | pos->bitboards[them==white?Q:q]);
    pos->checkers |= ortho;

    pos->in_check = (pos->checkers != 0ULL);
}

void generate_moves(const Position *pos, MoveList *list) {
    list->count = 0;
    // TODO: flesh out:
    //   generate_pawn_moves(pos, list);
    //   generate_knight_moves(pos, list);
    //   generate_bishop_moves(pos, list);
    //   generate_rook_moves(pos, list);
    //   generate_queen_moves(pos, list);
    //   generate_king_moves(pos, list);
}



