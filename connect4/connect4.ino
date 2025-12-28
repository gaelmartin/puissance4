/*
 * Connect 4 Game for Arduino Nano
 *
 * Hardware:
 * - BTF LED ribbon (WS2812B) on PIN 2, 42 LEDs in 6x7 grid
 * - 7 push buttons on PINs 3-9 (one per column)
 *
 * LED Layout (zigzag pattern):
 * 41-40-39-38-37-36-35  (row 5, top)
 * 28-29-30-31-32-33-34  (row 4)
 * 27-26-25-24-23-22-21  (row 3)
 * 14-15-16-17-18-19-20  (row 2)
 * 13-12-11-10-09-08-07  (row 1)
 * 00-01-02-03-04-05-06  (row 0, bottom)
 */

#include <FastLED.h>

// Pin definitions
#define LED_PIN     2
#define NUM_LEDS    42
#define BUTTON_START_PIN 3  // Buttons on pins 3-9

// Grid dimensions
#define COLS 7
#define ROWS 6

// Game states
#define STATE_MODE_SELECT    0
#define STATE_AI_LEVEL_SELECT 1
#define STATE_PLAYING        2
#define STATE_WIN            3
#define STATE_DRAW           4
#define STATE_SCORE          5
#define STATE_GRAND_WIN      6

// Points to win the match
#define POINTS_TO_WIN   7

// Cell states
#define EMPTY   0
#define PLAYER1 1
#define PLAYER2 2

// Colors
#define COLOR_OFF      CRGB::Black
#define COLOR_PLAYER1  CRGB::Red
#define COLOR_PLAYER2  CRGB::Yellow
#define COLOR_CURSOR   CRGB::White

// Timing
#define DEBOUNCE_DELAY 50
#define BLINK_INTERVAL 300
#define DROP_DELAY     80

// LED array
CRGB leds[NUM_LEDS];

// Game board (0=empty, 1=player1, 2=player2)
uint8_t board[ROWS][COLS];

// Game state
uint8_t currentPlayer = PLAYER1;
uint8_t gameState = STATE_MODE_SELECT;
uint8_t cursorCol = 3;  // Start cursor in middle

// Win tracking - use a grid to mark all winning positions
bool winningCells[ROWS][COLS];
uint8_t winner = EMPTY;

// Score tracking
uint16_t scorePlayer1 = 0;
uint16_t scorePlayer2 = 0;

// Button state
unsigned long lastButtonPress[COLS];
bool buttonState[COLS];

// Animation timing
unsigned long lastBlinkTime = 0;
unsigned long lastWinBlinkTime = 0;
unsigned long lastDrawBlinkTime = 0;
unsigned long scoreDisplayStart = 0;
unsigned long winAnimationStart = 0;
bool blinkState = false;
bool winBlinkState = false;
bool drawBlinkState = false;

// Display durations
#define SCORE_DISPLAY_TIME 3000
#define WIN_DISPLAY_TIME   3000
#define SCORE_HOLD_TIME    2000  // Time to hold button 4 to show score

// Button 4 hold tracking for score display
unsigned long button4HoldStart = 0;
bool showingHoldScore = false;
bool button4WasHeldForScore = false;  // Track if button was held long enough for score
bool button4Blocked = false;  // Block button 4 after state transitions

// Game mode / difficulty settings
#define MODE_SELECT_TIMEOUT 3000  // 3 seconds to choose mode
#define MODE_CONFIRM_TIME   2000  // 2 seconds to confirm mode selection (blinking)
uint8_t gameMode = 0;  // 0-6, current difficulty level
unsigned long modeSelectStart = 0;  // When mode selection started
unsigned long modeConfirmStart = 0;  // When mode was selected (for confirmation display)
bool modeConfirmed = false;  // Whether a mode has been selected

// AI opponent mode
bool aiMode = false;  // true if playing against computer
uint8_t aiLevel = 1;  // AI difficulty level: 1-7 (très facile à expert)
#define AI_MOVE_DELAY 800  // Delay before AI makes a move (ms)

// Time limits for each mode (in milliseconds), 0 = no limit
const uint16_t modeTimes[7] = {0, 10000, 8000, 6000, 4000, 3000, 2000};

// Turn timer
unsigned long turnStartTime = 0;  // When current player's turn started
unsigned long scorePauseStart = 0;  // When score display started (to pause timer)

// Blue color for mode display
#define COLOR_MODE CRGB::Blue

// Blink speed settings
#define BLINK_NORMAL    300   // Normal blink interval
#define BLINK_FAST      150   // Fast blink when 50% time left
#define BLINK_FASTER    80    // Faster blink when 25% time left
#define BLINK_URGENT    40    // Urgent blink when 10% time left

// AI timing
unsigned long aiMoveTime = 0;  // When AI should make its move
bool aiThinking = false;  // AI is thinking about its move

// Convert grid position (row, col) to LED index
// Handles the zigzag pattern
uint8_t getLedIndex(uint8_t row, uint8_t col) {
  if (row % 2 == 0) {
    // Even rows: left to right (0, 2, 4)
    return row * COLS + col;
  } else {
    // Odd rows: right to left (1, 3, 5)
    return row * COLS + (COLS - 1 - col);
  }
}

// Initialize the game
void resetGame() {
  // Clear board and winning cells
  for (uint8_t r = 0; r < ROWS; r++) {
    for (uint8_t c = 0; c < COLS; c++) {
      board[r][c] = EMPTY;
      winningCells[r][c] = false;
    }
  }

  // Loser starts next game (if there was a winner)
  // If draw, keep same player
  if (winner != EMPTY) {
    currentPlayer = (winner == PLAYER1) ? PLAYER2 : PLAYER1;
  }
  // On first game or after grand win reset, currentPlayer stays as is

  gameState = STATE_PLAYING;
  cursorCol = 3;
  winner = EMPTY;

  updateDisplay();
}

// Get color for a cell
CRGB getCellColor(uint8_t cellState) {
  switch (cellState) {
    case PLAYER1: return COLOR_PLAYER1;
    case PLAYER2: return COLOR_PLAYER2;
    default:      return COLOR_OFF;
  }
}

// Calculate blink interval based on remaining time
uint16_t getBlinkInterval() {
  if (gameMode == 0 || gameState != STATE_PLAYING) {
    return BLINK_NORMAL;  // No timer, normal blink
  }

  unsigned long elapsed = millis() - turnStartTime;
  uint16_t timeLimit = modeTimes[gameMode];

  if (elapsed >= timeLimit) {
    return BLINK_URGENT;  // Time's up!
  }

  unsigned long remaining = timeLimit - elapsed;
  uint16_t percent = (remaining * 100) / timeLimit;

  if (percent <= 10) {
    return BLINK_URGENT;   // Last 10%
  } else if (percent <= 25) {
    return BLINK_FASTER;   // Last 25%
  } else if (percent <= 50) {
    return BLINK_FAST;     // Last 50%
  } else {
    return BLINK_NORMAL;   // More than 50% left
  }
}

// Update the LED display
void updateDisplay() {
  // Set all LEDs based on board state
  for (uint8_t r = 0; r < ROWS; r++) {
    for (uint8_t c = 0; c < COLS; c++) {
      uint8_t ledIdx = getLedIndex(r, c);
      leds[ledIdx] = getCellColor(board[r][c]);
    }
  }

  // Show blinking indicator on all playable columns (top row)
  if (gameState == STATE_PLAYING) {
    for (uint8_t c = 0; c < COLS; c++) {
      // Show on top row if column is not full
      if (board[ROWS - 1][c] == EMPTY) {
        uint8_t ledIdx = getLedIndex(ROWS - 1, c);
        if (blinkState) {
          // Dim player color when blinking on
          CRGB cursorColor = (currentPlayer == PLAYER1) ? COLOR_PLAYER1 : COLOR_PLAYER2;
          cursorColor.nscale8(100);  // Dimmer than static pieces
          leds[ledIdx] = cursorColor;
        } else {
          // Off when blinking off
          leds[ledIdx] = COLOR_OFF;
        }
      }
    }
  }

  FastLED.show();
}

// Animate winning positions
void animateWin() {
  unsigned long currentTime = millis();

  // Check if display time is over - auto transition to score
  if (currentTime - winAnimationStart >= WIN_DISPLAY_TIME) {
    // Check for grand winner
    if (scorePlayer1 >= POINTS_TO_WIN) {
      gameState = STATE_GRAND_WIN;
      winner = PLAYER1;
      Serial.println("*** JOUEUR 1 GAGNE LA PARTIE! ***");
    } else if (scorePlayer2 >= POINTS_TO_WIN) {
      gameState = STATE_GRAND_WIN;
      winner = PLAYER2;
      Serial.println("*** JOUEUR 2 GAGNE LA PARTIE! ***");
    } else {
      gameState = STATE_SCORE;
      scoreDisplayStart = millis();
    }
    return;
  }

  if (currentTime - lastWinBlinkTime >= BLINK_INTERVAL) {
    lastWinBlinkTime = currentTime;
    winBlinkState = !winBlinkState;

    CRGB winnerColor = (winner == PLAYER1) ? COLOR_PLAYER1 : COLOR_PLAYER2;

    // Set all LEDs based on board state, blink winning cells
    for (uint8_t r = 0; r < ROWS; r++) {
      for (uint8_t c = 0; c < COLS; c++) {
        uint8_t ledIdx = getLedIndex(r, c);
        if (winningCells[r][c]) {
          // Blink winning positions between green and player color
          if (winBlinkState) {
            leds[ledIdx] = CRGB::Green;
          } else {
            leds[ledIdx] = winnerColor;
          }
        } else {
          leds[ledIdx] = getCellColor(board[r][c]);
        }
      }
    }
    FastLED.show();
  }
}

// Find the lowest empty row in a column (-1 if full)
int8_t findEmptyRow(uint8_t col) {
  for (uint8_t r = 0; r < ROWS; r++) {
    if (board[r][col] == EMPTY) {
      return r;
    }
  }
  return -1;
}

// Drop animation
void animateDrop(uint8_t col, uint8_t targetRow, uint8_t player) {
  CRGB color = getCellColor(player);

  for (int8_t r = ROWS - 1; r >= (int8_t)targetRow; r--) {
    // Light up current position
    uint8_t ledIdx = getLedIndex(r, col);
    leds[ledIdx] = color;
    FastLED.show();
    delay(DROP_DELAY);

    // Turn off if not target
    if (r > (int8_t)targetRow) {
      leds[ledIdx] = COLOR_OFF;
    }
  }
}

// Mark a line of 4 as winning
void markWinningLine(uint8_t row, uint8_t col, int8_t dRow, int8_t dCol) {
  for (int8_t i = 0; i < 4; i++) {
    int8_t r = row + i * dRow;
    int8_t c = col + i * dCol;
    winningCells[r][c] = true;
  }
}

// Check for a win from a position in a direction
// Returns true only if at least 3 cells are not already marked as winning
bool checkDirection(uint8_t row, uint8_t col, int8_t dRow, int8_t dCol, uint8_t player) {
  uint8_t alreadyMarked = 0;

  for (int8_t i = 0; i < 4; i++) {
    int8_t r = row + i * dRow;
    int8_t c = col + i * dCol;

    if (r < 0 || r >= ROWS || c < 0 || c >= COLS) {
      return false;
    }

    if (board[r][c] != player) {
      return false;
    }

    if (winningCells[r][c]) {
      alreadyMarked++;
    }
  }

  // Only count as new connect-4 if at most 1 cell already marked
  return alreadyMarked <= 1;
}

// Check if the current player has won and mark ALL winning lines
bool checkWin(uint8_t player) {
  uint8_t winCount = 0;

  // Clear winning cells first
  for (uint8_t r = 0; r < ROWS; r++) {
    for (uint8_t c = 0; c < COLS; c++) {
      winningCells[r][c] = false;
    }
  }

  // Check all possible starting positions and directions
  for (uint8_t r = 0; r < ROWS; r++) {
    for (uint8_t c = 0; c < COLS; c++) {
      // Horizontal
      if (c <= COLS - 4 && checkDirection(r, c, 0, 1, player)) {
        markWinningLine(r, c, 0, 1);
        winCount++;
      }
      // Vertical
      if (r <= ROWS - 4 && checkDirection(r, c, 1, 0, player)) {
        markWinningLine(r, c, 1, 0);
        winCount++;
      }
      // Diagonal up-right
      if (r <= ROWS - 4 && c <= COLS - 4 && checkDirection(r, c, 1, 1, player)) {
        markWinningLine(r, c, 1, 1);
        winCount++;
      }
      // Diagonal up-left
      if (r <= ROWS - 4 && c >= 3 && checkDirection(r, c, 1, -1, player)) {
        markWinningLine(r, c, 1, -1);
        winCount++;
      }
    }
  }

  if (winCount > 0) {
    winner = player;

    // Add points to score
    if (player == PLAYER1) {
      scorePlayer1 += winCount;
    } else {
      scorePlayer2 += winCount;
    }

    Serial.print("Joueur ");
    Serial.print(player);
    Serial.print(" gagne avec ");
    Serial.print(winCount);
    Serial.println(" puissance(s) quatre!");
    Serial.print("Score: Joueur 1 = ");
    Serial.print(scorePlayer1);
    Serial.print(" | Joueur 2 = ");
    Serial.println(scorePlayer2);
  }

  return winCount > 0;
}

// Check if board is full (draw)
bool checkDraw() {
  for (uint8_t c = 0; c < COLS; c++) {
    if (board[ROWS - 1][c] == EMPTY) {
      return false;
    }
  }
  return true;
}

// AI: Count threats in a direction for a player
uint8_t countInDirection(uint8_t row, uint8_t col, int8_t dRow, int8_t dCol, uint8_t player) {
  uint8_t count = 0;
  int8_t r = row;
  int8_t c = col;

  while (r >= 0 && r < ROWS && c >= 0 && c < COLS && board[r][c] == player) {
    count++;
    r += dRow;
    c += dCol;
  }

  return count;
}

// AI: Check if placing at position creates a win (without modifying winningCells permanently)
bool aiCheckWin(uint8_t row, uint8_t col, uint8_t player) {
  // Check horizontal
  for (int8_t startCol = max(0, (int8_t)col - 3); startCol <= min((int8_t)COLS - 4, (int8_t)col); startCol++) {
    bool found = true;
    for (int8_t i = 0; i < 4; i++) {
      if (board[row][startCol + i] != player) {
        found = false;
        break;
      }
    }
    if (found) return true;
  }

  // Check vertical
  if (row >= 3) {
    bool found = true;
    for (int8_t i = 0; i < 4; i++) {
      if (board[row - i][col] != player) {
        found = false;
        break;
      }
    }
    if (found) return true;
  }

  // Check diagonal up-right
  for (int8_t offset = -3; offset <= 0; offset++) {
    int8_t startRow = row + offset;
    int8_t startCol = col + offset;
    if (startRow >= 0 && startRow <= ROWS - 4 && startCol >= 0 && startCol <= COLS - 4) {
      bool found = true;
      for (int8_t i = 0; i < 4; i++) {
        if (board[startRow + i][startCol + i] != player) {
          found = false;
          break;
        }
      }
      if (found) return true;
    }
  }

  // Check diagonal up-left
  for (int8_t offset = -3; offset <= 0; offset++) {
    int8_t startRow = row + offset;
    int8_t startCol = col - offset;
    if (startRow >= 0 && startRow <= ROWS - 4 && startCol >= 3 && startCol < COLS) {
      bool found = true;
      for (int8_t i = 0; i < 4; i++) {
        if (board[startRow + i][startCol - i] != player) {
          found = false;
          break;
        }
      }
      if (found) return true;
    }
  }

  return false;
}

// AI: Evaluate a column position for potential wins/blocks
// Returns score: higher is better
int16_t evaluatePosition(uint8_t col, uint8_t player) {
  int8_t row = findEmptyRow(col);
  if (row < 0) return -1000;  // Column full

  int16_t score = 0;
  uint8_t opponent = (player == PLAYER1) ? PLAYER2 : PLAYER1;

  // Temporarily place piece
  board[row][col] = player;

  // NIVEAU 1+: Check if this wins the game (everyone should take a winning move!)
  if (aiLevel >= 1) {
    if (aiCheckWin(row, col, player)) {
      board[row][col] = EMPTY;
      return 1000;  // Winning move!
    }
  }

  // NIVEAU 2+: Prefer center column slightly
  if (aiLevel >= 2) {
    score += (3 - abs((int8_t)col - 3)) * 2;
  }

  // NIVEAU 3+: Check if this blocks opponent's win
  if (aiLevel >= 3) {
    board[row][col] = opponent;
    if (aiCheckWin(row, col, opponent)) {
      score += 500;  // Must block!
    }
    board[row][col] = player;
  }

  // NIVEAU 4+: Check for 2-in-a-row opportunities (build on existing pieces)
  if (aiLevel >= 4) {
    // Horizontal
    uint8_t leftCount = countInDirection(row, col, 0, -1, player);
    uint8_t rightCount = countInDirection(row, col, 0, 1, player);
    if (leftCount + rightCount - 1 >= 2) score += 20;

    // Vertical
    uint8_t downCount = countInDirection(row, col, -1, 0, player);
    if (downCount >= 2) score += 20;

    // Diagonal down-right
    uint8_t diagDR1 = countInDirection(row, col, -1, -1, player);
    uint8_t diagDR2 = countInDirection(row, col, 1, 1, player);
    if (diagDR1 + diagDR2 - 1 >= 2) score += 20;

    // Diagonal down-left
    uint8_t diagDL1 = countInDirection(row, col, -1, 1, player);
    uint8_t diagDL2 = countInDirection(row, col, 1, -1, player);
    if (diagDL1 + diagDL2 - 1 >= 2) score += 20;
  }

  // NIVEAU 5+: Check for 3-in-a-row opportunities
  if (aiLevel >= 5) {
    // Horizontal
    uint8_t leftCount = countInDirection(row, col, 0, -1, player);
    uint8_t rightCount = countInDirection(row, col, 0, 1, player);
    if (leftCount + rightCount - 1 >= 3) score += 50;

    // Vertical
    uint8_t downCount = countInDirection(row, col, -1, 0, player);
    if (downCount >= 3) score += 50;

    // Diagonal down-right
    uint8_t diagDR1 = countInDirection(row, col, -1, -1, player);
    uint8_t diagDR2 = countInDirection(row, col, 1, 1, player);
    if (diagDR1 + diagDR2 - 1 >= 3) score += 50;

    // Diagonal down-left
    uint8_t diagDL1 = countInDirection(row, col, -1, 1, player);
    uint8_t diagDL2 = countInDirection(row, col, 1, -1, player);
    if (diagDL1 + diagDL2 - 1 >= 3) score += 50;
  }

  // NIVEAU 6+: Block opponent's 3-in-a-row and prefer stable positions
  if (aiLevel >= 6) {
    if (row > 0) score += 5;

    // Block opponent's 3-in-a-row
    board[row][col] = opponent;
    uint8_t oppLeftCount = countInDirection(row, col, 0, -1, opponent);
    uint8_t oppRightCount = countInDirection(row, col, 0, 1, opponent);
    if (oppLeftCount + oppRightCount - 1 >= 3) score += 40;

    uint8_t oppDownCount = countInDirection(row, col, -1, 0, opponent);
    if (oppDownCount >= 3) score += 40;

    board[row][col] = player;
  }

  // NIVEAU 7 (EXPERT): Set traps - create forks
  if (aiLevel >= 7 && row < ROWS - 1) {
    // Check if this move creates a "fork" (two ways to win)
    uint8_t winningThreats = 0;

    // Simulate placing another piece on top
    board[row + 1][col] = player;

    // Check all positions for potential wins
    for (int8_t c = 0; c < COLS; c++) {
      int8_t r = findEmptyRow(c);
      if (r >= 0) {
        board[r][c] = player;
        if (aiCheckWin(r, c, player)) {
          winningThreats++;
        }
        board[r][c] = EMPTY;
      }
    }

    board[row + 1][col] = EMPTY;

    // If this creates multiple winning opportunities, it's a trap!
    if (winningThreats >= 2) {
      score += 200;  // High value for setting a trap
    }

    // Also check if opponent could create a trap here
    board[row][col] = opponent;
    if (row < ROWS - 1) {
      board[row + 1][col] = opponent;
      uint8_t opponentThreats = 0;

      for (int8_t c = 0; c < COLS; c++) {
        int8_t r = findEmptyRow(c);
        if (r >= 0) {
          board[r][c] = opponent;
          if (aiCheckWin(r, c, opponent)) {
            opponentThreats++;
          }
          board[r][c] = EMPTY;
        }
      }

      board[row + 1][col] = EMPTY;

      // Block opponent's trap setup
      if (opponentThreats >= 2) {
        score += 150;
      }
    }
  }

  // Remove temporary piece
  board[row][col] = EMPTY;

  return score;
}

// AI: Choose best column to play
uint8_t aiChooseColumn() {
  int16_t bestScore = -2000;
  uint8_t bestCol = 3;  // Default to center
  uint8_t bestCols[COLS];
  uint8_t bestCount = 0;

  // Evaluate all columns
  for (uint8_t c = 0; c < COLS; c++) {
    if (findEmptyRow(c) >= 0) {
      int16_t score = evaluatePosition(c, PLAYER2);  // AI is always PLAYER2

      if (score > bestScore) {
        bestScore = score;
        bestCol = c;
        bestCount = 1;
        bestCols[0] = c;
      } else if (score == bestScore) {
        bestCols[bestCount++] = c;
      }
    }
  }

  // If multiple columns have same score, pick randomly
  if (bestCount > 1) {
    bestCol = bestCols[random(bestCount)];
  }

  return bestCol;
}

// AI: Make a move
void aiMakeMove() {
  if (currentPlayer != PLAYER2 || !aiMode || gameState != STATE_PLAYING) {
    aiThinking = false;
    return;
  }

  uint8_t col = aiChooseColumn();

  Serial.print("AI joue colonne: ");
  Serial.println(col + 1);

  dropPiece(col);
  turnStartTime = millis();
  updateDisplay();
  aiThinking = false;
}

// Drop a piece in a column
bool dropPiece(uint8_t col) {
  int8_t row = findEmptyRow(col);
  if (row < 0) {
    return false;  // Column full
  }

  // Animate the drop
  animateDrop(col, row, currentPlayer);

  // Place the piece
  board[row][col] = currentPlayer;

  // Check for win
  if (checkWin(currentPlayer)) {
    gameState = STATE_WIN;
    winAnimationStart = millis();
    return true;
  }

  // Check for draw
  if (checkDraw()) {
    gameState = STATE_DRAW;
    winAnimationStart = millis();
    return true;
  }

  // Switch player
  currentPlayer = (currentPlayer == PLAYER1) ? PLAYER2 : PLAYER1;//test

  return true;
}

// Read buttons with debouncing
void readButtons() {
  unsigned long currentTime = millis();

  for (uint8_t i = 0; i < COLS; i++) {
    uint8_t pin = BUTTON_START_PIN + i;
    bool reading = !digitalRead(pin);  // Active LOW (pulled up)

    if (reading && !buttonState[i]) {
      // Button just pressed
      if (currentTime - lastButtonPress[i] > DEBOUNCE_DELAY) {
        lastButtonPress[i] = currentTime;
        buttonState[i] = true;

        // Handle button press
        handleButtonPress(i);
      }
    } else if (!reading) {
      buttonState[i] = false;
    }
  }
}

// Handle a button press
void handleButtonPress(uint8_t col) {
  if (gameState == STATE_MODE_SELECT) {
    // Select mode based on button (0-6)
    if (!modeConfirmed) {
      selectMode(col);
    }
  } else if (gameState == STATE_AI_LEVEL_SELECT) {
    // Select AI level based on button (0-6)
    if (!modeConfirmed) {
      selectAILevel(col);
    }
  } else if (gameState == STATE_PLAYING) {
    // In AI mode, only allow human player (PLAYER1) to play
    if (aiMode && currentPlayer == PLAYER2) {
      return;  // Block input during AI turn
    }

    // Move cursor to this column and drop piece
    cursorCol = col;
    dropPiece(col);
    turnStartTime = millis();  // Reset timer for next player
    updateDisplay();
  } else if (gameState == STATE_GRAND_WIN) {
    // Reset everything for a new match
    scorePlayer1 = 0;
    scorePlayer2 = 0;
    currentPlayer = PLAYER1;  // Reset to player 1 for new big game
    winner = EMPTY;
    // Clear board
    for (uint8_t r = 0; r < ROWS; r++) {
      for (uint8_t c = 0; c < COLS; c++) {
        board[r][c] = EMPTY;
        winningCells[r][c] = false;
      }
    }
    Serial.println("Nouvelle partie!");
    Serial.println("Score reinitialise.");

    // Return to mode selection (or AI level selection if in AI mode)
    if (aiMode) {
      Serial.println("Retour a la selection de niveau AI...");
      startAILevelSelection();
    } else {
      startModeSelection();
    }
  } else if (gameState == STATE_WIN || gameState == STATE_DRAW) {
    // Check for grand winner immediately
    if (scorePlayer1 >= POINTS_TO_WIN) {
      gameState = STATE_GRAND_WIN;
      winner = PLAYER1;
      Serial.println("*** JOUEUR 1 GAGNE LA PARTIE! ***");
    } else if (scorePlayer2 >= POINTS_TO_WIN) {
      gameState = STATE_GRAND_WIN;
      winner = PLAYER2;
      Serial.println("*** JOUEUR 2 GAGNE LA PARTIE! ***");
    } else {
      // Show score display
      gameState = STATE_SCORE;
      scoreDisplayStart = millis();
    }
  } else if (gameState == STATE_SCORE) {
    // Skip score display, start new game
    resetGame();
    turnStartTime = millis();  // Start timer for new round
  }
}

// Display score as lines on the grid
void displayScore() {
  // Clear all LEDs
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = COLOR_OFF;
  }

  // Display Player 1 score on bottom rows (rows 0, 1, 2)
  // Each point = one column of 3 LEDs, fill from left to right
  uint8_t p1Points = min(scorePlayer1, (uint16_t)COLS);  // Max 7 points
  for (uint8_t col = 0; col < p1Points; col++) {
    for (uint8_t row = 0; row < 3; row++) {
      uint8_t ledIdx = getLedIndex(row, col);
      leds[ledIdx] = COLOR_PLAYER1;
    }
  }

  // Display Player 2 score on top rows (rows 3, 4, 5)
  // Each point = one column of 3 LEDs, fill from left to right
  uint8_t p2Points = min(scorePlayer2, (uint16_t)COLS);  // Max 7 points
  for (uint8_t col = 0; col < p2Points; col++) {
    for (uint8_t row = 3; row < 6; row++) {
      uint8_t ledIdx = getLedIndex(row, col);
      leds[ledIdx] = COLOR_PLAYER2;
    }
  }

  FastLED.show();

  // Check if score display time is over
  if (millis() - scoreDisplayStart >= SCORE_DISPLAY_TIME) {
    resetGame();
    turnStartTime = millis();  // Reset timer for new round
  }
}

// Display AI level selection screen
void displayAILevelSelection(uint8_t level, bool blinkOff) {
  // Clear all LEDs
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = COLOR_OFF;
  }

  // Light up LEDs based on AI level - triangular pattern
  // Same as mode selection but with purple color
  for (uint8_t col = 0; col <= level; col++) {
    uint8_t numRows;
    if (col <= 1) {
      numRows = 1;  // col 0 and 1 always have 1 LED
    } else {
      numRows = col;  // col 2 = 2, col 3 = 3, etc.
    }
    if (numRows > ROWS) numRows = ROWS;
    for (uint8_t row = 0; row < numRows; row++) {
      uint8_t ledIdx = getLedIndex(row, col);
      if (level == 0) {
        // Level 1 (index 0): purple LED, blinks
        if (!blinkOff) {
          leds[ledIdx] = CRGB::Purple;
        }
      } else {
        // Level > 1: col 0 is always purple (no blink), rest is purple (blinks)
        if (col == 0) {
          leds[ledIdx] = CRGB::Purple;  // Always on, no blink
        } else if (!blinkOff) {
          leds[ledIdx] = CRGB::Purple;  // Purple, blinks
        }
      }
    }
  }

  FastLED.show();
}

// Start AI level selection process
void startAILevelSelection() {
  gameState = STATE_AI_LEVEL_SELECT;
  modeSelectStart = millis();
  modeConfirmed = false;
  aiLevel = 1;  // Default to level 1
  displayAILevelSelection(0, false);
}

// Handle AI level selection timeout and confirmation
void handleAILevelSelection() {
  unsigned long currentTime = millis();

  if (modeConfirmed) {
    // Level has been selected, blink for confirmation time
    if (currentTime - modeConfirmStart >= MODE_CONFIRM_TIME) {
      // Start the game
      gameState = STATE_PLAYING;
      turnStartTime = millis();
      blinkState = false;
      updateDisplay();
      Serial.print("Niveau AI ");
      Serial.print(aiLevel);
      Serial.println(" selectionne - Demarrage du jeu!");
    } else {
      // Blink the level display during confirmation
      displayAILevelSelection(aiLevel - 1, !blinkState);
    }
  } else {
    // Check for timeout - default to level 1
    if (currentTime - modeSelectStart >= MODE_SELECT_TIMEOUT) {
      aiLevel = 1;
      modeConfirmed = true;
      modeConfirmStart = millis();
      displayAILevelSelection(0, false);
    }
  }
}

// Select an AI level (called when button pressed during level selection)
void selectAILevel(uint8_t level) {
  if (level <= 6) {  // 0-6 = levels 1-7
    aiLevel = level + 1;
    modeConfirmed = true;
    modeConfirmStart = millis();
    displayAILevelSelection(level, false);
    Serial.print("Niveau AI ");
    Serial.print(aiLevel);
    Serial.print(" - ");
    switch(aiLevel) {
      case 1: Serial.println("Tres Facile"); break;
      case 2: Serial.println("Facile"); break;
      case 3: Serial.println("Moyen"); break;
      case 4: Serial.println("Peu Difficile"); break;
      case 5: Serial.println("Difficile"); break;
      case 6: Serial.println("Tres Difficile"); break;
      case 7: Serial.println("Expert"); break;
    }
  }
}

// Display mode selection screen
void displayModeSelection(uint8_t mode, bool blinkOff) {
  // Clear all LEDs
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = COLOR_OFF;
  }

  // Light up LEDs based on mode - triangular pattern starting from col 2
  // Mode 0: col 0 = 1 LED (white)
  // Mode 1: col 0 = 1 LED, col 1 = 1 LED
  // Mode 2: col 0 = 1 LED, col 1 = 1 LED, col 2 = 2 LEDs
  // Mode 3: col 0 = 1 LED, col 1 = 1 LED, col 2 = 2 LEDs, col 3 = 3 LEDs
  // etc.
  for (uint8_t col = 0; col <= mode; col++) {
    uint8_t numRows;
    if (col <= 1) {
      numRows = 1;  // col 0 and 1 always have 1 LED
    } else {
      numRows = col;  // col 2 = 2, col 3 = 3, etc.
    }
    if (numRows > ROWS) numRows = ROWS;
    for (uint8_t row = 0; row < numRows; row++) {
      uint8_t ledIdx = getLedIndex(row, col);
      if (mode == 0) {
        // Mode 0: white LED, blinks
        if (!blinkOff) {
          leds[ledIdx] = COLOR_CURSOR;
        }
      } else {
        // Mode > 0: col 0 is always white (no blink), rest is blue (blinks)
        if (col == 0) {
          leds[ledIdx] = COLOR_CURSOR;  // Always on, no blink
        } else if (!blinkOff) {
          leds[ledIdx] = COLOR_MODE;  // Blue, blinks
        }
      }
    }
  }

  FastLED.show();
}

// Start mode selection process
void startModeSelection() {
  gameState = STATE_MODE_SELECT;
  modeSelectStart = millis();
  modeConfirmed = false;
  gameMode = 0;  // Default to mode 0
  displayModeSelection(0, false);
}

// Handle mode selection timeout and confirmation
void handleModeSelection() {
  unsigned long currentTime = millis();

  if (modeConfirmed) {
    // Mode has been selected, blink for confirmation time
    if (currentTime - modeConfirmStart >= MODE_CONFIRM_TIME) {
      // Start the game
      gameState = STATE_PLAYING;
      turnStartTime = millis();
      blinkState = false;
      updateDisplay();
    } else {
      // Blink the mode display during confirmation
      displayModeSelection(gameMode, !blinkState);
    }
  } else {
    // Check for timeout - default to mode 0
    if (currentTime - modeSelectStart >= MODE_SELECT_TIMEOUT) {
      gameMode = 0;
      modeConfirmed = true;
      modeConfirmStart = millis();
      displayModeSelection(0, false);
    }
  }
}

// Select a mode (called when button pressed during mode selection)
void selectMode(uint8_t mode) {
  if (mode <= 6) {
    gameMode = mode;
    // Normal 2-player mode only
    aiMode = false;
    modeConfirmed = true;
    modeConfirmStart = millis();
    displayModeSelection(mode, false);
    Serial.print("Mode ");
    Serial.print(mode);
    Serial.print(" selectionne - ");
    if (mode == 0) {
      Serial.println("Pas de limite de temps");
    } else {
      Serial.print("Limite: ");
      Serial.print(modeTimes[mode] / 1000);
      Serial.println(" secondes");
    }
  }
}

// Check turn timer and auto-drop if expired
void checkTurnTimer() {
  if (gameMode == 0 || gameState != STATE_PLAYING || showingHoldScore) return;  // No timer in mode 0 or when showing score

  unsigned long currentTime = millis();
  uint16_t timeLimit = modeTimes[gameMode];

  if (currentTime - turnStartTime >= timeLimit) {
    // Time expired - choose random free column
    uint8_t freeColumns[COLS];
    uint8_t freeCount = 0;

    for (uint8_t c = 0; c < COLS; c++) {
      if (board[ROWS - 1][c] == EMPTY) {
        freeColumns[freeCount++] = c;
      }
    }

    if (freeCount > 0) {
      // Pick random column
      uint8_t randomCol = freeColumns[random(freeCount)];
      Serial.print("Temps ecoule! Colonne aleatoire: ");
      Serial.println(randomCol + 1);
      dropPiece(randomCol);
      turnStartTime = millis();  // Reset timer for next player
      updateDisplay();
    }
  }
}

// Grand winner animation
void animateGrandWin() {
  unsigned long currentTime = millis();
  if (currentTime - lastWinBlinkTime >= BLINK_INTERVAL / 2) {
    lastWinBlinkTime = currentTime;
    winBlinkState = !winBlinkState;

    CRGB winnerColor = (winner == PLAYER1) ? COLOR_PLAYER1 : COLOR_PLAYER2;

    // Fill entire grid with winner color, blinking
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      if (winBlinkState) {
        leds[i] = winnerColor;
      } else {
        leds[i] = CRGB::Green;
      }
    }
    FastLED.show();
  }
}

// Check if button 4 is held to show score (only during play)
// Returns true if button 4 should be blocked from normal processing
bool checkButton4Hold() {
  bool btn4Pressed = !digitalRead(BUTTON_START_PIN + 3);  // Button 4 (index 3, pin 6)

  // If button 4 is blocked (after returning from non-playing state), wait for release
  if (button4Blocked) {
    if (!btn4Pressed) {
      button4Blocked = false;
    }
    return true;  // Keep blocking until released
  }

  if (gameState == STATE_PLAYING) {
    if (btn4Pressed) {
      if (button4HoldStart == 0) {
        // Start tracking hold time
        button4HoldStart = millis();
        button4WasHeldForScore = false;
      } else if (millis() - button4HoldStart >= SCORE_HOLD_TIME) {
        // Held long enough, show score
        if (!showingHoldScore) {
          // Just started showing score, record pause time
          showingHoldScore = true;
          scorePauseStart = millis();
        }
        button4WasHeldForScore = true;
      }
      // Block button 4 processing while it's pressed in playing state
      return true;
    } else {
      // Button released in playing state
      if (showingHoldScore) {
        // Was showing score, go back to playing
        // Adjust turn start time to account for pause duration
        unsigned long pauseDuration = millis() - scorePauseStart;
        turnStartTime += pauseDuration;
        showingHoldScore = false;
        updateDisplay();
      } else if (button4HoldStart > 0 && !button4WasHeldForScore) {
        // Was pressed but released before 2 seconds - drop piece now
        dropPiece(3);  // Column 4 (index 3)
        updateDisplay();
      }
      button4HoldStart = 0;
      button4WasHeldForScore = false;
      return false;
    }
  } else {
    // Not in playing state - let button 4 work normally for state transitions
    // But set blocked flag so it won't drop a piece when we return to playing
    if (btn4Pressed) {
      button4Blocked = true;
    }
    button4HoldStart = 0;
    button4WasHeldForScore = false;
    showingHoldScore = false;
    // Return false to allow normal button processing in non-playing states
    return false;
  }
}

// Display score while holding button (same as displayScore but no auto-exit)
void displayHoldScore() {
  // Clear all LEDs
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = COLOR_OFF;
  }

  // Display Player 1 score on bottom rows (rows 0, 1, 2)
  uint8_t p1Points = min(scorePlayer1, (uint16_t)COLS);
  for (uint8_t col = 0; col < p1Points; col++) {
    for (uint8_t row = 0; row < 3; row++) {
      uint8_t ledIdx = getLedIndex(row, col);
      leds[ledIdx] = COLOR_PLAYER1;
    }
  }

  // Display Player 2 score on top rows (rows 3, 4, 5)
  uint8_t p2Points = min(scorePlayer2, (uint16_t)COLS);
  for (uint8_t col = 0; col < p2Points; col++) {
    for (uint8_t row = 3; row < 6; row++) {
      uint8_t ledIdx = getLedIndex(row, col);
      leds[ledIdx] = COLOR_PLAYER2;
    }
  }

  FastLED.show();
}

// Draw animation (alternating colors)
void animateDraw() {
  unsigned long currentTime = millis();

  // Check if display time is over - auto transition to score
  if (currentTime - winAnimationStart >= WIN_DISPLAY_TIME) {
    gameState = STATE_SCORE;
    scoreDisplayStart = millis();
    return;
  }

  if (currentTime - lastDrawBlinkTime >= BLINK_INTERVAL * 2) {
    lastDrawBlinkTime = currentTime;
    drawBlinkState = !drawBlinkState;

    for (uint8_t r = 0; r < ROWS; r++) {
      for (uint8_t c = 0; c < COLS; c++) {
        uint8_t ledIdx = getLedIndex(r, c);
        if ((r + c) % 2 == drawBlinkState) {
          leds[ledIdx] = COLOR_PLAYER1;
        } else {
          leds[ledIdx] = COLOR_PLAYER2;
        }
      }
    }
    FastLED.show();
  }
}

void setup() {
  // Initialize Serial
  Serial.begin(9600);
  Serial.println("Connect 4 - Pret!");

  // Initialize FastLED
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(200);  // Adjust brightness (0-255)

  // Initialize button pins with internal pull-up
  for (uint8_t i = 0; i < COLS; i++) {
    pinMode(BUTTON_START_PIN + i, INPUT_PULLUP);
    lastButtonPress[i] = 0;
    buttonState[i] = false;
  }

  // Initialize random seed
  randomSeed(analogRead(0));

  // Startup animation - alternate yellow and red (same for both modes)
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = (i % 2 == 0) ? COLOR_PLAYER2 : COLOR_PLAYER1;  // Yellow/Red alternating
    FastLED.show();
    delay(30);
    leds[i] = COLOR_OFF;
  }

  // Clear all LEDs and reset blink state
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = COLOR_OFF;
  }
  blinkState = false;
  FastLED.show();

  // Check if any button (1-7) is pressed at startup for AI mode
  delay(100);  // Small delay to stabilize button reading
  bool anyButtonPressed = false;
  for (uint8_t i = 0; i < COLS; i++) {
    if (!digitalRead(BUTTON_START_PIN + i)) {
      anyButtonPressed = true;
      break;
    }
  }

  if (anyButtonPressed) {
    // AI mode selected
    aiMode = true;
    Serial.println("*** MODE SOLO (AI) SELECTIONNE ***");

    // Wait for all buttons to be released before starting level selection
    bool stillPressed = true;
    while (stillPressed) {
      stillPressed = false;
      for (uint8_t i = 0; i < COLS; i++) {
        if (!digitalRead(BUTTON_START_PIN + i)) {
          stillPressed = true;
          break;
        }
      }
      delay(10);
    }
    delay(100);  // Extra delay after release

    // Start AI level selection
    startAILevelSelection();
  } else {
    // Normal 2-player mode
    aiMode = false;

    // Start with mode selection
    startModeSelection();
  }
}

void loop() {
  unsigned long currentTime = millis();

  // Handle blink timing for cursor (dynamic speed based on timer)
  uint16_t currentBlinkInterval = getBlinkInterval();
  if (currentTime - lastBlinkTime >= currentBlinkInterval) {
    lastBlinkTime = currentTime;
    blinkState = !blinkState;
  }

  // Check button 4 hold for score display (must be before readButtons)
  bool button4Held = checkButton4Hold();

  // Read buttons, but skip button 4 if it's being held for score check
  if (!button4Held) {
    readButtons();
  } else {
    // Still read other buttons, just not button 4
    for (uint8_t i = 0; i < COLS; i++) {
      if (i == 3) continue;  // Skip button 4
      uint8_t pin = BUTTON_START_PIN + i;
      bool reading = !digitalRead(pin);
      if (reading && !buttonState[i]) {
        if (currentTime - lastButtonPress[i] > DEBOUNCE_DELAY) {
          lastButtonPress[i] = currentTime;
          buttonState[i] = true;
          handleButtonPress(i);
        }
      } else if (!reading) {
        buttonState[i] = false;
      }
    }
  }

  // Check turn timer for auto-drop
  checkTurnTimer();

  // AI turn handling
  if (aiMode && currentPlayer == PLAYER2 && gameState == STATE_PLAYING && !aiThinking) {
    // Start AI thinking
    aiThinking = true;
    aiMoveTime = millis() + AI_MOVE_DELAY;
  }

  if (aiThinking && millis() >= aiMoveTime) {
    aiMakeMove();
  }

  // Update display based on game state
  if (showingHoldScore) {
    displayHoldScore();
  } else {
    switch (gameState) {
      case STATE_MODE_SELECT:
        handleModeSelection();
        break;
      case STATE_AI_LEVEL_SELECT:
        handleAILevelSelection();
        break;
      case STATE_PLAYING:
        updateDisplay();
        break;
      case STATE_WIN:
        animateWin();
        break;
      case STATE_DRAW:
        animateDraw();
        break;
      case STATE_SCORE:
        displayScore();
        break;
      case STATE_GRAND_WIN:
        animateGrandWin();
        break;
    }
  }

  delay(10);  // Small delay to prevent flickering
}
