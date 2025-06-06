# Constants for file masks (same as your C ULL constants)
NOT_A_FILE = 0xfefefefefefefefe  # All bits set except A-file
NOT_H_FILE = 0x7f7f7f7f7f7f7f7f  # All bits set except H-file
NOT_AB_FILE = 0xfcfcfcfcfcfcfcfc # All bits set except A and B files
NOT_HG_FILE = 0x3f3f3f3f3f3f3f3f # All bits set except H and G files

# Initialize attack tables as lists of 64 zeros
pawn_attacks_white = [0] * 64
pawn_attacks_black = [0] * 64
knight_attacks = [0] * 64
king_attacks = [0] * 64

def init_pawn_attacks():
    """Initializes pawn attack tables."""
    for sq in range(64):
        b = 1 << sq  # Bitboard with only the current square 'sq' set

        # White pawns: move towards smaller square indices (e.g., from rank 2 to rank 8 if a8=0)
        # Attack North-West (sq - 9) and North-East (sq - 7)
        # Masks are applied to the *target* square to clear attacks that wrapped around files.
        pawn_attacks_white[sq] = ((b >> 9) & NOT_H_FILE) | \
                                 ((b >> 7) & NOT_A_FILE)
        
        # Black pawns: move towards larger square indices (e.g., from rank 7 to rank 1 if a8=0)
        # Attack South-West (sq + 7) and South-East (sq + 9)
        pawn_attacks_black[sq] = ((b << 7) & NOT_H_FILE) | \
                                 ((b << 9) & NOT_A_FILE)

def init_knight_attacks():
    """Initializes knight attack table."""
    for sq in range(64):
        b = 1 << sq
        attacks = 0
        # Shifts are relative to square 'sq'.
        # Example: (b >> 17) is a potential target square bitboard.
        # The mask ensures if this target landed on an invalid file due to wrap-around, it's cleared.
        
        attacks |= ((b >> 17) & NOT_H_FILE)  # Up 2, Left 1
        attacks |= ((b >> 15) & NOT_A_FILE)  # Up 2, Right 1
        attacks |= ((b >> 10) & NOT_HG_FILE) # Up 1, Left 2
        attacks |= ((b >>  6) & NOT_AB_FILE) # Up 1, Right 2
        
        attacks |= ((b << 17) & NOT_A_FILE)  # Down 2, Right 1
        attacks |= ((b << 15) & NOT_H_FILE)  # Down 2, Left 1
        attacks |= ((b << 10) & NOT_AB_FILE) # Down 1, Right 2
        attacks |= ((b <<  6) & NOT_HG_FILE) # Down 1, Left 2
        knight_attacks[sq] = attacks

def init_king_attacks():
    """Initializes king attack table."""
    for sq in range(64):
        b = 1 << sq
        attacks = 0
        
        # North/South moves don't wrap around files, so no file mask needed for them.
        # If they go off board, the shift results in 0.
        attacks |= (b >> 8)                  # North (sq - 8)
        attacks |= ((b >> 9) & NOT_H_FILE)   # North-West (sq - 9)
        attacks |= ((b >> 7) & NOT_A_FILE)   # North-East (sq - 7)
        attacks |= ((b >> 1) & NOT_H_FILE)   # West (sq - 1)
        
        attacks |= (b << 8)                  # South (sq + 8)
        attacks |= ((b << 9) & NOT_A_FILE)   # South-East (sq + 9)
        attacks |= ((b << 7) & NOT_H_FILE)   # South-West (sq + 7)
        attacks |= ((b << 1) & NOT_A_FILE)   # East (sq + 1)
        king_attacks[sq] = attacks

def print_hex_table(table_name, table_data, dimensions=1, elements_per_line=4):
    """Prints a table in C-style hex format for U64 arrays."""
    if dimensions == 1:
        print(f"const U64 {table_name}[{len(table_data)}] = {{")
        for i, val in enumerate(table_data):
            if i % elements_per_line == 0:
                print("    ", end="")
            print(f"0x{val:016x}ULL,", end="")
            if i % elements_per_line == (elements_per_line - 1) or i == len(table_data) - 1:
                print()
            else:
                print(" ", end="")
        print("};")
    elif dimensions == 2: # Specifically for pawns [2][64]
        print(f"const U64 {table_name}[2][64] = {{")
        labels = ["white", "black"] # Matches the order in your C code pawn_attacks[white/black]
        for color_idx, color_label in enumerate(labels):
            print(f"  {{ // {color_label.capitalize()} attacks")
            current_table = table_data[color_idx]
            for i, val in enumerate(current_table):
                if i % elements_per_line == 0:
                    print("    ", end="")
                print(f"0x{val:016x}ULL,", end="")
                if i % elements_per_line == (elements_per_line - 1) or i == len(current_table) - 1:
                    print()
                else:
                    print(" ", end="")
            print("  }" + ("," if color_idx == 0 else ""))
        print("};")
    print()

if __name__ == "__main__":
    # Initialize all attack tables
    init_pawn_attacks()
    init_knight_attacks()
    init_king_attacks()

    # Print the tables in C U64 array format
    print_hex_table("pawn_attacks", [pawn_attacks_white, pawn_attacks_black], dimensions=2, elements_per_line=8)
    print_hex_table("knight_attacks", knight_attacks, dimensions=1, elements_per_line=8)
    print_hex_table("king_attacks", king_attacks, dimensions=1, elements_per_line=8)