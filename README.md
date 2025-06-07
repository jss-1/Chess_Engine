# Chess Engine Project

This repository hosts a chess engine project developed in two distinct versions:

*   **v1/**: A user-friendly version written in Python with a Pygame GUI.
*   **v2/**: A performance-oriented version written in C, utilizing advanced bitboard techniques.

Both versions aim to provide a functional chess-playing experience, each with different strengths and features.

## v1: Python Chess Engine with Pygame GUI

This version is developed in Python and utilizes the Pygame library for a graphical user interface. It's designed for ease of use and understanding.

### Features:
*   **Graphical User Interface:** Play chess on a visual board.
*   **Human vs. Human:** Allows two players to play against each other.
*   **Human vs. AI:** Play against a computer opponent.
    *   The AI uses a NegaMax algorithm with alpha-beta pruning for move selection.
*   **Move Management:**
    *   Undo previous moves (press 'z').
    *   Reset the game to the initial state (press 'r').
*   **Move Log:** Displays the moves made during the game.

### Running v1:
1.  **Prerequisites:** Ensure you have Python and Pygame installed.
    *   Pygame can typically be installed via pip: `pip install pygame`
2.  **Navigate to the directory:** `cd v1`
3.  **Run the main script:** `python ChessMain.py`

## v2: High-Performance C Chess Engine

This version is written in C with a focus on performance and efficient computation, primarily using bitboards.

### Features:
*   **Bitboard Representation:** The board state and piece movements are managed using 64-bit integers (bitboards) for speed.
*   **FEN Notation Parsing:** Initialize board positions using Forsyth-Edwards Notation (FEN).
*   **Advanced Attack Generation:**
    *   Uses pre-calculated attack tables for non-sliding pieces (pawns, knights, kings).
    *   Employs "magic bitboards" with Parallel Bit Extract (PEXT) instructions (BMI2 instruction set) for highly efficient generation of sliding piece attacks (bishops, rooks, queens).
*   **Move Generation & Validation:** Includes robust move generation for all pieces, covering standard moves, promotions, en passant, and castling.
*   **Perft Testing:** Integrated Performance Test (`perft`) to verify the correctness and speed of the move generator.
*   **Console Output:** Prints the board state to the console using Unicode chess characters.
*   **UCI (Universal Chess Interface) Capable:** Designed with UCI compatibility in mind, though full UCI protocol implementation might be ongoing.

### Compiling and Running v2:
1.  **Prerequisites:** You'll need a C compiler that supports the BMI2 instruction set (e.g., GCC version 4.7+ or Clang 3.2+).
2.  **Navigate to the directory:** `cd v2`
3.  **Compile the source code:**
    ```bash
    gcc -o chess_engine game_pext.c -O3 -march=native
    ```
    *   `-O3` enables high optimization.
    *   `-march=native` enables optimizations for the specific architecture of your machine, including BMI2 if available. If compiling for a different machine, you might need a more specific flag (e.g., `-mbmi2`).
4.  **Run the engine:**
    ```bash
    ./chess_engine
    ```
    Currently, this will run a `perft` test.

## Future Work / Development

Potential areas for future development include:

*   **v1 (Python):**
    *   Improving the AI further (e.g., more advanced evaluation, search depth).
    *   Adding features like game saving/loading.
    *   Refining the GUI.
*   **v2 (C):**
    *   Fully implementing the UCI protocol for compatibility with standard chess GUIs.
    *   Developing a search algorithm (e.g., Alpha-Beta, NegaMax) to create a playable AI.
    *   Implementing a more sophisticated board evaluation function.
    *   Adding support for time control.
*   **General:**
    *   Creating a more comprehensive test suite.

## Structure

*   **/v1/**: Contains the Python version of the chess engine with a Pygame GUI.
    *   `ChessMain.py`: Main script to run the game.
    *   `ChessEngine.py`: Core game logic and rules.
    *   `ChessAI.py`: AI opponent logic.
    *   `images/`: Contains the images for the chess pieces.
*   **/v2/**: Contains the C version of the chess engine.
    *   `game_pext.c`: Main source code for the C engine, including bitboard logic, move generation, and FEN parsing.
    *   `leaper_attack_tables.py`: Python script used to generate pre-calculated attack tables for pawns, knights, and kings for the C engine.
