/********************************************************************
 *  Tiny Chess Engine – a8 = 0 indexing, optional BMI2/PEXT         *
 *                                                                  *
 *  • If compiler defines __BMI2__ -> use pext                      *
 *  • else run PRNG loop to find collision-free magics at start-up  *
 ********************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#if defined(__BMI2__) /* GCC/Clang set this when -march has bmi2 */
#include <immintrin.h>
#define USE_PEXT 1
#else
#define USE_PEXT 0
#endif

typedef uint64_t U64;

/*──────────────── Square & colour enums (your a8 = 0 order) ─────────────*/
enum
{ /* 0 .. 63 */
  a8,
  b8,
  c8,
  d8,
  e8,
  f8,
  g8,
  h8,
  a7,
  b7,
  c7,
  d7,
  e7,
  f7,
  g7,
  h7,
  a6,
  b6,
  c6,
  d6,
  e6,
  f6,
  g6,
  h6,
  a5,
  b5,
  c5,
  d5,
  e5,
  f5,
  g5,
  h5,
  a4,
  b4,
  c4,
  d4,
  e4,
  f4,
  g4,
  h4,
  a3,
  b3,
  c3,
  d3,
  e3,
  f3,
  g3,
  h3,
  a2,
  b2,
  c2,
  d2,
  e2,
  f2,
  g2,
  h2,
  a1,
  b1,
  c1,
  d1,
  e1,
  f1,
  g1,
  h1,
  no_sq
};

// square to file/rank
enum
{
    white,
    black,
    both
};

// square to file and rank
const char *square_to_coordinates[] = {
    "a8",
    "b8",
    "c8",
    "d8",
    "e8",
    "f8",
    "g8",
    "h8",
    "a7",
    "b7",
    "c7",
    "d7",
    "e7",
    "f7",
    "g7",
    "h7",
    "a6",
    "b6",
    "c6",
    "d6",
    "e6",
    "f6",
    "g6",
    "h6",
    "a5",
    "b5",
    "c5",
    "d5",
    "e5",
    "f5",
    "g5",
    "h5",
    "a4",
    "b4",
    "c4",
    "d4",
    "e4",
    "f4",
    "g4",
    "h4",
    "a3",
    "b3",
    "c3",
    "d3",
    "e3",
    "f3",
    "g3",
    "h3",
    "a2",
    "b2",
    "c2",
    "d2",
    "e2",
    "f2",
    "g2",
    "h2",
    "a1",
    "b1",
    "c1",
    "d1",
    "e1",
    "f1",
    "g1",
    "h1",
};

// Casting side
enum
{
    wk = 1,
    wq = 2,
    bk = 4,
    bq = 8
};

// encode piece types
enum
{
    P = 0,
    N,
    B,
    R,
    Q,
    K,
    p,
    n,
    b,
    r,
    q,
    k
};

// bishop/rook
enum
{
    BISHOP = 0,
    ROOK = 1
};

// ASCII piece symbols
const char ascii_pieces[] = {'P', 'N', 'B', 'R', 'Q', 'K', 'p', 'n', 'b', 'r', 'q', 'k'};

// unicode piece symbols
const char *unicode_pieces[12] = {

    "\xE2\x99\x9F", // ♟ Black Pawn
    "\xE2\x99\x9E", // ♞ Black Knight
    "\xE2\x99\x9D", // ♝ Black Bishop
    "\xE2\x99\x9C", // ♜ Black Rook
    "\xE2\x99\x9B", // ♛ Black Queen
    "\xE2\x99\x9A", // ♚ Black King
    "\xE2\x99\x99", // ♙ White Pawn
    "\xE2\x99\x98", // ♘ White Knight
    "\xE2\x99\x97", // ♗ White Bishop
    "\xE2\x99\x96", // ♖ White Rook
    "\xE2\x99\x95", // ♕ White Queen
    "\xE2\x99\x94" // ♔ White King
};

// ASCII to encoded values
int char_pieces[256] = {
    ['P'] = P,
    ['N'] = N,
    ['B'] = B,
    ['R'] = R,
    ['Q'] = Q,
    ['K'] = K,
    ['p'] = p,
    ['n'] = n,
    ['b'] = b,
    ['r'] = r,
    ['q'] = q,
    ['k'] = k,
};

/*----------------------------------------------------- State Bitboards ----------------------------------------*/
// piece bitboards
U64 bitboards[12];

// occupancies
U64 occupancies[3]; // 0: all, 1: white, 2: black

// side to move
int side = -1;

// en passant square
int enpassant;

// castling rights
int castle;

/*-------------------------------------------------------- FEN Debug Positions-----------------------------------------------*/
#define empty_board "8/8/8/8/8/8/8/8 w - - "
#define start_position "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
#define tricky_position "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 "
#define killer_position "rnbqkb1r/pp1p1pPp/8/2p1pP2/1P1P4/3P3P/P1P1P3/RNBQKBNR w KQkq e6 0 1"
#define cmk_position "r2q1rk1/ppp2ppp/2n1bn2/2b1p3/3pP3/3P1NPP/PPP1NPB1/R1BQ1RK1 b - - 0 9 "

/*──────────────── Bit helpers ───────────────────────────────────────────*/
static inline void set_bit(U64 *bb, int sq) { *bb |= 1ULL << sq; }
static inline int get_bit(U64 bb, int sq) { return (bb >> sq) & 1ULL; }
static inline void pop_bit(U64 *bb, int sq) { *bb &= ~1ULL << sq; }

#define count_bit(b) __builtin_popcountll(b)
#define get_ls1b_index(b) __builtin_ctzll(b)

/* ────────────── Debug printer (add anywhere above main) ────────────── */
static void print_bitboard(U64 bb)
{
    puts("");
    for (int r = 0; r < 8; ++r)
    {
        printf("%d  ", 8 - r);
        for (int f = 0; f < 8; ++f)
            printf("%d ", get_bit(bb, r * 8 + f) ? 1 : 0);
        putchar('\n');
    }
    puts("   a b c d e f g h\n");
    printf("Bitboard: 0x%016llx\n", (unsigned long long)bb);
}

/* ------------------------ Print game state with unicode pieces ------------------------ */
static char get_piece_at_square(int sq)
{
    for (int i = 0; i < 12; ++i)
        if (get_bit(bitboards[i], sq))
            return ascii_pieces[i];
    return 0;
}

static void print_board(void)
{
    puts("");
    for (int r = 0; r < 8; ++r)
    {
        printf("%d  ", 8 - r);
        for (int f = 0; f < 8; ++f)
        {
            int sq = r * 8 + f;
            char piece_char = get_piece_at_square(sq);
            if (piece_char)
            {
                int index = char_pieces[(unsigned char)piece_char];
                printf(" %s ", unicode_pieces[index]);
            }
            else
            {
                printf(" . ");
            }
        }
        putchar('\n');
    }
    puts("    a  b  c  d  e  f  g  h \n");

    printf("Side to move: %s\n", side == white ? "White" : "Black");
}

/*──────────────── File masks ────────────────────────────────────────────*/
static const U64 not_a_file = 0xFEFEFEFEFEFEFEFEULL;
static const U64 not_h_file = 0x7F7F7F7F7F7F7F7FULL;
static const U64 not_hg_file = 0x3F3F3F3F3F3F3F3FULL;
static const U64 not_ab_file = 0xFDFDFDFDFDFDFDFDULL;

/*──────────────── Leaper attack tables ─────────────────────────────────*/
static U64 pawn_attacks[2][64], knight_attacks[64], king_attacks[64];

static U64 mask_pawn_attacks(int sq, int side)
{
    U64 b = 1ULL << sq, a = 0;
    side ? 0 : 0;
    if (side == white)
    {
        if ((b >> 7) & not_a_file)
            a |= b >> 7;
        if ((b >> 9) & not_h_file)
            a |= b >> 9;
    }
    else
    {
        if ((b << 7) & not_h_file)
            a |= b << 7;
        if ((b << 9) & not_a_file)
            a |= b << 9;
    }
    return a;
}
static U64 mask_knight_attacks(int sq)
{
    U64 b = 1ULL << sq, a = 0;
    if ((b >> 17) & not_h_file)
        a |= b >> 17;
    if ((b >> 15) & not_a_file)
        a |= b >> 15;
    if ((b >> 10) & not_hg_file)
        a |= b >> 10;
    if ((b >> 6) & not_ab_file)
        a |= b >> 6;
    if ((b << 17) & not_a_file)
        a |= b << 17;
    if ((b << 15) & not_h_file)
        a |= b << 15;
    if ((b << 10) & not_ab_file)
        a |= b << 10;
    if ((b << 6) & not_hg_file)
        a |= b << 6;
    return a;
}
static U64 mask_king_attacks(int sq)
{
    U64 b = 1ULL << sq, a = 0;
    if ((b >> 1) & not_h_file)
        a |= b >> 1;
    a |= b >> 8;
    if ((b >> 7) & not_a_file)
        a |= b >> 7;
    if ((b >> 9) & not_h_file)
        a |= b >> 9;
    if ((b << 1) & not_a_file)
        a |= b << 1;
    a |= b << 8;
    if ((b << 7) & not_h_file)
        a |= b << 7;
    if ((b << 9) & not_a_file)
        a |= b << 9;
    return a;
}

/*──────────────── Sliding masks (occupied-independent) ─────────────────*/
static U64 mask_bishop[64], mask_rook[64];

static U64 mask_bishop_attacks(int sq)
{
    U64 a = 0;
    int tr = sq / 8, tf = sq & 7, r, f;
    for (r = tr + 1, f = tf + 1; r <= 6 && f <= 6; r++, f++)
        a |= 1ULL << (r * 8 + f);
    for (r = tr - 1, f = tf + 1; r >= 1 && f <= 6; r--, f++)
        a |= 1ULL << (r * 8 + f);
    for (r = tr + 1, f = tf - 1; r <= 6 && f >= 1; r++, f--)
        a |= 1ULL << (r * 8 + f);
    for (r = tr - 1, f = tf - 1; r >= 1 && f >= 1; r--, f--)
        a |= 1ULL << (r * 8 + f);
    return a;
}
static U64 mask_rook_attacks(int sq)
{
    U64 a = 0;
    int tr = sq / 8, tf = sq & 7, r, f;
    for (r = tr + 1; r <= 6; r++)
        a |= 1ULL << (r * 8 + tf);
    for (r = tr - 1; r >= 1; r--)
        a |= 1ULL << (r * 8 + tf);
    for (f = tf + 1; f <= 6; f++)
        a |= 1ULL << (tr * 8 + f);
    for (f = tf - 1; f >= 1; f--)
        a |= 1ULL << (tr * 8 + f);
    return a;
}

/*──────────────── On-the-fly generator (needed for PRNG path) ───────────*/
static U64 bishop_on_the_fly(int sq, U64 blk)
{
    U64 a = 0;
    int tr = sq / 8, tf = sq & 7, r, f;
    for (r = tr + 1, f = tf + 1; r <= 7 && f <= 7; r++, f++)
    {
        U64 b = 1ULL << (r * 8 + f);
        a |= b;
        if (b & blk)
            break;
    }
    for (r = tr - 1, f = tf + 1; r >= 0 && f <= 7; r--, f++)
    {
        U64 b = 1ULL << (r * 8 + f);
        a |= b;
        if (b & blk)
            break;
    }
    for (r = tr + 1, f = tf - 1; r <= 7 && f >= 0; r++, f--)
    {
        U64 b = 1ULL << (r * 8 + f);
        a |= b;
        if (b & blk)
            break;
    }
    for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--)
    {
        U64 b = 1ULL << (r * 8 + f);
        a |= b;
        if (b & blk)
            break;
    }
    return a;
}
static U64 rook_on_the_fly(int sq, U64 blk)
{
    U64 a = 0;
    int tr = sq / 8, tf = sq & 7, r, f;
    for (r = tr + 1; r <= 7; r++)
    {
        U64 b = 1ULL << (r * 8 + tf);
        a |= b;
        if (b & blk)
            break;
    }
    for (r = tr - 1; r >= 0; r--)
    {
        U64 b = 1ULL << (r * 8 + tf);
        a |= b;
        if (b & blk)
            break;
    }
    for (f = tf + 1; f <= 7; f++)
    {
        U64 b = 1ULL << (tr * 8 + f);
        a |= b;
        if (b & blk)
            break;
    }
    for (f = tf - 1; f >= 0; f--)
    {
        U64 b = 1ULL << (tr * 8 + f);
        a |= b;
        if (b & blk)
            break;
    }
    return a;
}

/*──────────────── PRNG (simple xorshift64*) ────────────────────────────*/
static U64 rng_state = 0x123456789abcdefULL;
static inline U64 rng_rand(void)
{
    U64 x = rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rng_state = x;
    return x * 0x2545F4914F6CDD1DULL;
}
/* generate a sparse random number (Stockfish trick) */
static inline U64 rng_sparse(void)
{
    return rng_rand() & rng_rand() & rng_rand();
}

/*──────────────── Magic structures / tables ───────────────────────────*/
typedef struct
{
    U64 mask, magic;
    uint8_t shift;
    U64 *table;
} Magic;
static Magic bishopM[64], rookM[64]; /* filled at init */
static U64 bishopTable[0x1480];      /* == 1664  */
static U64 rookTable[0x19000];       /* == 102400 */

/* helper to enumerate occupancy subsets with carry-rippler */
static U64 set_occupancy(int index, int bits, U64 mask)
{
    U64 occ = 0;
    for (int i = 0; i < bits; i++)
    {
        int sq = get_ls1b_index(mask);
        mask &= mask - 1;
        if (index & (1 << i))
            occ |= 1ULL << sq;
    }
    return occ;
}

/*──────────────── Build tables – 2 paths ──────────────────────────────*/
static void init_magic_tables(void)
{

#if USE_PEXT
    /* give each square its own segment ---------------------------- */
    int bishopOfs = 0, rookOfs = 0;

    for (int sq = 0; sq < 64; ++sq)
    {

        /* bishop */
        U64 mask = mask_bishop[sq];
        int bits = count_bit(mask);
        int n = 1 << bits;

        bishopM[sq].mask = mask;
        bishopM[sq].table = bishopTable + bishopOfs;
        bishopM[sq].shift = 0; /* unused */

        for (int i = 0; i < n; ++i)
        {
            U64 occ = set_occupancy(i, bits, mask);
            bishopM[sq].table[_pext_u64(occ, mask)] =
                bishop_on_the_fly(sq, occ);
        }
        bishopOfs += n;

        /* rook (same idea) */
        mask = mask_rook[sq];
        bits = count_bit(mask);
        n = 1 << bits;

        rookM[sq].mask = mask;
        rookM[sq].table = rookTable + rookOfs;
        rookM[sq].shift = 0;

        for (int i = 0; i < n; ++i)
        {
            U64 occ = set_occupancy(i, bits, mask);
            rookM[sq].table[_pext_u64(occ, mask)] =
                rook_on_the_fly(sq, occ);
        }
        rookOfs += n;
    }

#else /*------------------------- Random-magic path --------------------*/

    for (int sq = 0, bishopOfs = 0, rookOfs = 0; sq < 64; sq++)
    {
        /* ------- Bishops ------- */
        Magic *m = &bishopM[sq];
        m->mask = mask_bishop[sq];
        m->shift = 64 - count_bit(m->mask);
        m->table = bishopTable + bishopOfs;
        int bits = count_bit(m->mask), n = 1 << bits;

        /* build reference attacks for every occupancy */
        U64 occArr[512], refArr[512];
        for (int i = 0; i < n; i++)
        {
            occArr[i] = set_occupancy(i, bits, m->mask);
            refArr[i] = bishop_on_the_fly(sq, occArr[i]);
        }

        /* search magic */
        for (;;)
        {
            U64 magic = rng_sparse();
            if (count_bit((magic * m->mask) >> 56) < 6)
                continue; /* density test */

            /* clear table area */
            memset(m->table, 0, n * sizeof(U64));
            int ok = 1;
            for (int i = 0; i < n; i++)
            {
                unsigned idx = (unsigned)((occArr[i] * magic) >> m->shift);
                if (m->table[idx] == 0)
                    m->table[idx] = refArr[i];
                else if (m->table[idx] != refArr[i])
                {
                    ok = 0;
                    break;
                }
            }
            if (ok)
            {
                m->magic = magic;
                break;
            }
        }
        bishopOfs += n;

        /* ------- Rooks ------- */
        m = &rookM[sq];
        m->mask = mask_rook[sq];
        m->shift = 64 - count_bit(m->mask);
        m->table = rookTable + rookOfs;
        bits = count_bit(m->mask);
        n = 1 << bits;

        U64 occArrR[4096], refArrR[4096];
        for (int i = 0; i < n; i++)
        {
            occArrR[i] = set_occupancy(i, bits, m->mask);
            refArrR[i] = rook_on_the_fly(sq, occArrR[i]);
        }
        for (;;)
        {
            U64 magic = rng_sparse();
            if (count_bit((magic * m->mask) >> 56) < 6)
                continue;
            memset(m->table, 0, n * sizeof(U64));
            int ok = 1;
            for (int i = 0; i < n; i++)
            {
                unsigned idx = (unsigned)((occArrR[i] * magic) >> m->shift);
                if (m->table[idx] == 0)
                    m->table[idx] = refArrR[i];
                else if (m->table[idx] != refArrR[i])
                {
                    ok = 0;
                    break;
                }
            }
            if (ok)
            {
                m->magic = magic;
                break;
            }
        }
        rookOfs += n;
    }
#endif
}

/*──────────────── Fast runtime look-ups ───────────────────────────────*/
static inline U64 get_bishop_attacks(int sq, U64 occ)
{
#if USE_PEXT
    return bishopM[sq].table[_pext_u64(occ, bishopM[sq].mask)];
#else
    occ = (occ & bishopM[sq].mask) * bishopM[sq].magic >> bishopM[sq].shift;
    return bishopM[sq].table[occ];
#endif
}
static inline U64 get_rook_attacks(int sq, U64 occ)
{
#if USE_PEXT
    return rookM[sq].table[_pext_u64(occ, rookM[sq].mask)];
#else
    occ = (occ & rookM[sq].mask) * rookM[sq].magic >> rookM[sq].shift;
    return rookM[sq].table[occ];
#endif
}

/*──────────────── Init helpers ────────────────────────────────────────*/
static void init_leaper(void)
{
    for (int sq = 0; sq < 64; sq++)
    {
        pawn_attacks[white][sq] = mask_pawn_attacks(sq, white);
        pawn_attacks[black][sq] = mask_pawn_attacks(sq, black);
        knight_attacks[sq] = mask_knight_attacks(sq);
        king_attacks[sq] = mask_king_attacks(sq);
    }
}
static void init_slider_masks(void)
{
    for (int sq = 0; sq < 64; sq++)
    {
        mask_bishop[sq] = mask_bishop_attacks(sq);
        mask_rook[sq] = mask_rook_attacks(sq);
    }
}

/*──────────────── Init function ───────────────────────────────────────*/
static inline void init_attacks_helper()
{
    init_leaper();
    init_slider_masks();
    init_magic_tables();
}



/* --------------------------------------------------- Parsing FEN ---------------------------------------*/
/* Parses a FEN string into bitboards, occupancies, side, castle rights, en-passant */
static void parse_fen(const char *fen)
{
    /* 1. clear everything */
    memset(bitboards,   0, sizeof(bitboards));
    memset(occupancies, 0, sizeof(occupancies));
    side      = white;
    castle    = 0;
    enpassant = -1;

    /* 2. piece placement */
    int sq = a8;
    while (*fen && *fen != ' ')
    {
        char c = *fen++;

        if (c == '/')
            continue;

        if (c >= '1' && c <= '8')
            sq += c - '0';                     /* skip empty squares */
        else
        {
            int idx = char_pieces[(unsigned char)c];
            set_bit(&bitboards[idx], sq);
            ++sq;
        }
    }

    /* 3. occupancies */
    occupancies[white] = bitboards[P] | bitboards[N] | bitboards[B] |
                         bitboards[R] | bitboards[Q] | bitboards[K];
    occupancies[black] = bitboards[p] | bitboards[n] | bitboards[b] |
                         bitboards[r] | bitboards[q] | bitboards[k];
    occupancies[both]  = occupancies[white] | occupancies[black];

    /* 4. side to move */
    if (*fen) ++fen;
    side = (*fen == 'w') ? white : black;

    /* 5. castling rights */
    fen += 2;                             /* skip “ w ” / “ b ” and space */
    while (*fen && *fen != ' ')
    {
        switch (*fen++)
        {
            case 'K': castle |= wk; break;
            case 'Q': castle |= wq; break;
            case 'k': castle |= bk; break;
            case 'q': castle |= bq; break;
            case '-': break;
        }
    }

    /* 6. en-passant square */
    if (*fen) ++fen;
    if (*fen != '-')
    {
        int file = fen[0] - 'a';
        int rank = fen[1] - '1';
        enpassant = rank * 8 + file;
    }
    else
        enpassant = -1;
}
static inline void init_boards()
{
    /* clear everything */
    memset(bitboards,   0, sizeof(bitboards));
    memset(occupancies, 0, sizeof(occupancies));
    side      = white;
    castle    = 0;
    enpassant = -1;

    /* set up the start position */
    parse_fen(start_position);
}

/*──────────────── Driver ───────────────────────────────────────*/
int main(void)
{
    puts("Bitboard engine – "
#if USE_PEXT
         "BMI2/PEXT path"
#else
         "Random-magic path"
#endif
    );

    init_attacks_helper();
    init_boards(); // Initialize to the start position first
    print_bitboard(bitboards[P]);
    print_bitboard(occupancies[white]);
    print_board();
    // Parse FEN string
    parse_fen(start_position);

    print_bitboard(bitboards[P]);
    print_bitboard(occupancies[white]);
    // Print Board with chess pieces
    print_board();

    return 0;
}