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
#define STATE_PLAYING    0
#define STATE_WIN        1
#define STATE_DRAW       2
#define STATE_SCORE      3
#define STATE_GRAND_WIN  4
#define STATE_MENU_SCORE 5

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
#define COMBO_HOLD_TIME 1000  // Time to hold buttons 2+4 for menu

// LED array
CRGB leds[NUM_LEDS];

// Game board (0=empty, 1=player1, 2=player2)
uint8_t board[ROWS][COLS];

// Game state
uint8_t currentPlayer = PLAYER1;
uint8_t gameState = STATE_PLAYING;
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

// Combo button tracking (buttons 2 and 4 = indices 1 and 3)
unsigned long comboStartTime = 0;
bool comboActive = false;
uint8_t previousGameState = STATE_PLAYING;  // To restore after menu score

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

  currentPlayer = PLAYER1;
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
    CRGB cursorColor = (currentPlayer == PLAYER1) ? COLOR_PLAYER1 : COLOR_PLAYER2;
    cursorColor.nscale8(blinkState ? 150 : 60);  // Blink effect (max 150)

    for (uint8_t c = 0; c < COLS; c++) {
      // Show on top row if column is not full
      if (board[ROWS - 1][c] == EMPTY) {
        uint8_t ledIdx = getLedIndex(ROWS - 1, c);
        leds[ledIdx] = cursorColor;
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
  if (gameState == STATE_PLAYING) {
    // Move cursor to this column and drop piece
    cursorCol = col;
    dropPiece(col);
    updateDisplay();
  } else if (gameState == STATE_GRAND_WIN) {
    // Reset everything for a new match
    scorePlayer1 = 0;
    scorePlayer2 = 0;
    Serial.println("Nouvelle partie!");
    Serial.println("Score reinitialise.");
    resetGame();
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

// Check for combo button press (buttons 2 and 4 held for 1 second)
void checkComboButtons() {
  bool btn2 = !digitalRead(BUTTON_START_PIN + 1);  // Button 2 (index 1)
  bool btn4 = !digitalRead(BUTTON_START_PIN + 3);  // Button 4 (index 3)

  if (btn2 && btn4) {
    if (!comboActive) {
      // Start tracking combo
      comboActive = true;
      comboStartTime = millis();
    } else if (millis() - comboStartTime >= COMBO_HOLD_TIME) {
      // Combo held long enough
      if (gameState == STATE_MENU_SCORE) {
        // Already showing score, reset the match
        scorePlayer1 = 0;
        scorePlayer2 = 0;
        Serial.println("Match reinitialise!");
        Serial.println("Score: 0 - 0");
        resetGame();
      } else {
        // Show score menu
        previousGameState = gameState;
        gameState = STATE_MENU_SCORE;
        Serial.println("Affichage du score...");
      }
      // Reset combo to prevent repeated triggers
      comboActive = false;
      comboStartTime = millis();  // Prevent immediate re-trigger
    }
  } else {
    // Buttons released
    comboActive = false;
  }
}

// Display score in menu mode (same display as STATE_SCORE but no auto-exit)
void displayMenuScore() {
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

  // Start game
  resetGame();

  // Startup animation
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Green;
    FastLED.show();
    delay(30);
    leds[i] = COLOR_OFF;
  }

  updateDisplay();
}

void loop() {
  unsigned long currentTime = millis();

  // Handle blink timing for cursor
  if (currentTime - lastBlinkTime >= BLINK_INTERVAL) {
    lastBlinkTime = currentTime;
    blinkState = !blinkState;
  }

  // Check for combo button press (buttons 2+4)
  checkComboButtons();

  // Read buttons (only if not in menu score mode)
  if (gameState != STATE_MENU_SCORE) {
    readButtons();
  }

  // Update display based on game state
  switch (gameState) {
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
    case STATE_MENU_SCORE:
      displayMenuScore();
      break;
  }

  delay(10);  // Small delay to prevent flickering
}
