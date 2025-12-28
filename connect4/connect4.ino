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
#define STATE_PLAYING   0
#define STATE_WIN       1
#define STATE_DRAW      2

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
uint8_t gameState = STATE_PLAYING;
uint8_t cursorCol = 3;  // Start cursor in middle

// Win tracking
uint8_t winPositions[4][2];  // Store winning 4 positions [index][row,col]

// Button state
unsigned long lastButtonPress[COLS];
bool buttonState[COLS];

// Animation timing
unsigned long lastBlinkTime = 0;
bool blinkState = false;

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
  // Clear board
  for (uint8_t r = 0; r < ROWS; r++) {
    for (uint8_t c = 0; c < COLS; c++) {
      board[r][c] = EMPTY;
    }
  }

  currentPlayer = PLAYER1;
  gameState = STATE_PLAYING;
  cursorCol = 3;

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

  // Show cursor on top row if playing
  if (gameState == STATE_PLAYING) {
    uint8_t cursorLed = getLedIndex(ROWS - 1, cursorCol);
    // Only show cursor if top cell is empty
    if (board[ROWS - 1][cursorCol] == EMPTY) {
      // Dim cursor color based on current player
      CRGB cursorColor = (currentPlayer == PLAYER1) ? COLOR_PLAYER1 : COLOR_PLAYER2;
      cursorColor.nscale8(blinkState ? 255 : 80);  // Blink effect
      leds[cursorLed] = cursorColor;
    }
  }

  FastLED.show();
}

// Animate winning positions
void animateWin() {
  unsigned long currentTime = millis();
  if (currentTime - lastBlinkTime >= BLINK_INTERVAL) {
    lastBlinkTime = currentTime;
    blinkState = !blinkState;

    // Update display normally
    updateDisplay();

    // Blink winning positions
    for (uint8_t i = 0; i < 4; i++) {
      uint8_t r = winPositions[i][0];
      uint8_t c = winPositions[i][1];
      uint8_t ledIdx = getLedIndex(r, c);

      if (blinkState) {
        leds[ledIdx] = COLOR_CURSOR;  // Flash white
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

// Check for a win from a position in a direction
bool checkDirection(uint8_t row, uint8_t col, int8_t dRow, int8_t dCol, uint8_t player) {
  uint8_t count = 0;

  for (int8_t i = 0; i < 4; i++) {
    int8_t r = row + i * dRow;
    int8_t c = col + i * dCol;

    if (r < 0 || r >= ROWS || c < 0 || c >= COLS) {
      return false;
    }

    if (board[r][c] == player) {
      winPositions[count][0] = r;
      winPositions[count][1] = c;
      count++;
    } else {
      return false;
    }
  }

  return count == 4;
}

// Check if the current player has won
bool checkWin(uint8_t player) {
  // Check all possible starting positions and directions
  for (uint8_t r = 0; r < ROWS; r++) {
    for (uint8_t c = 0; c < COLS; c++) {
      // Horizontal
      if (c <= COLS - 4 && checkDirection(r, c, 0, 1, player)) return true;
      // Vertical
      if (r <= ROWS - 4 && checkDirection(r, c, 1, 0, player)) return true;
      // Diagonal up-right
      if (r <= ROWS - 4 && c <= COLS - 4 && checkDirection(r, c, 1, 1, player)) return true;
      // Diagonal up-left
      if (r <= ROWS - 4 && c >= 3 && checkDirection(r, c, 1, -1, player)) return true;
    }
  }
  return false;
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
    return true;
  }

  // Check for draw
  if (checkDraw()) {
    gameState = STATE_DRAW;
    return true;
  }

  // Switch player
  currentPlayer = (currentPlayer == PLAYER1) ? PLAYER2 : PLAYER1;

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
  } else {
    // Any button resets the game after win/draw
    resetGame();
  }
}

// Draw animation (alternating colors)
void animateDraw() {
  unsigned long currentTime = millis();
  if (currentTime - lastBlinkTime >= BLINK_INTERVAL * 2) {
    lastBlinkTime = currentTime;
    blinkState = !blinkState;

    for (uint8_t r = 0; r < ROWS; r++) {
      for (uint8_t c = 0; c < COLS; c++) {
        uint8_t ledIdx = getLedIndex(r, c);
        if ((r + c) % 2 == blinkState) {
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

  // Read buttons
  readButtons();

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
  }

  delay(10);  // Small delay to prevent flickering
}
