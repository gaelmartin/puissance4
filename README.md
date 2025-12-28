# Connect 4 - Arduino LED Game

A fully-featured Connect 4 game implementation for Arduino Nano with WS2812B LED strip display and physical button controls.

## Hardware Requirements

### Components
- **Arduino Nano** (or compatible board)
- **WS2812B LED Strip** - 42 LEDs configured as 6x7 grid
- **7 Push Buttons** - One for each column
- **Power Supply** - Adequate for LED strip (5V, recommend 2A+)
- **Resistors** - Pull-up resistors for buttons (if not using internal pull-ups)

### Pin Configuration
- **Pin 2** - LED strip data (WS2812B)
- **Pins 3-9** - Column buttons (1-7, active LOW with internal pull-up)

### LED Layout
The LEDs are arranged in a zigzag pattern:
```
41-40-39-38-37-36-35  (row 5, top)
28-29-30-31-32-33-34  (row 4)
27-26-25-24-23-22-21  (row 3)
14-15-16-17-18-19-20  (row 2)
13-12-11-10-09-08-07  (row 1)
00-01-02-03-04-05-06  (row 0, bottom)
```

## Features

### Game Modes
The game supports 7 different difficulty modes with varying time limits:

| Mode | Time Limit | Description |
|------|------------|-------------|
| 0    | No limit   | Classic mode - take your time |
| 1    | 10 seconds | Casual play |
| 2    | 8 seconds  | Moderate pace |
| 3    | 6 seconds  | Quick thinking |
| 4    | 4 seconds  | Fast-paced |
| 5    | 3 seconds  | Very fast |
| 6    | 2 seconds  | Extreme challenge |

**Mode Selection:**
- At game start, press any button (1-7) to select mode 0-6
- Selected mode blinks for 2 seconds to confirm
- If no button pressed within 3 seconds, defaults to mode 0

### Gameplay

#### Controls
- **Press any button (1-7)** - Drop a piece in that column
- **Hold button 4 (middle) for 2 seconds** - Display current score (timer pauses)

#### Turn Timer
- In timed modes, blink speed increases as time runs out:
  - Normal blink: >50% time remaining
  - Fast blink: 50-25% time remaining
  - Faster blink: 25-10% time remaining
  - Urgent blink: <10% time remaining
- If time expires, a random available column is chosen automatically

#### Visual Indicators
- **Red LEDs** - Player 1 pieces
- **Yellow LEDs** - Player 2 pieces
- **Blinking top row** - Available columns (dimmed player color)
- **Green flash** - Winning positions
- **Alternating colors** - Draw game

### Scoring System
- Each Connect-4 = **1 point**
- Multiple Connect-4s in one move = **multiple points**
- First player to reach **7 points** wins the grand match
- Score display shows:
  - Bottom 3 rows (red) = Player 1 score
  - Top 3 rows (yellow) = Player 2 score

### Game States

1. **Mode Selection** - Choose difficulty at start
2. **Playing** - Active gameplay
3. **Win** - Winning positions blink green for 3 seconds
4. **Draw** - Board full, alternating color animation
5. **Score Display** - Shows current match score for 3 seconds
6. **Grand Win** - A player reached 7 points, entire board blinks

## Installation

### Required Libraries
```cpp
#include <FastLED.h>
```

Install via Arduino IDE:
1. Open Arduino IDE
2. Go to **Sketch > Include Library > Manage Libraries**
3. Search for "FastLED"
4. Install latest version

### Upload Instructions
1. Connect Arduino Nano to computer via USB
2. Open `connect4/connect4.ino` in Arduino IDE
3. Select **Tools > Board > Arduino Nano**
4. Select **Tools > Processor > ATmega328P (Old Bootloader)** if using clone
5. Select correct **Port**
6. Click **Upload**

### Hardware Setup
1. Connect WS2812B LED strip data pin to Arduino pin 2
2. Connect LED strip power (5V and GND) to appropriate power supply
3. Connect 7 push buttons between pins 3-9 and GND (internal pull-ups enabled)
4. Ensure common ground between Arduino and LED power supply

## Configuration

### Adjustable Parameters
Edit these defines in `connect4.ino`:

```cpp
#define POINTS_TO_WIN   7      // Points needed to win match
#define BLINK_INTERVAL  300    // Cursor blink speed (ms)
#define DROP_DELAY      80     // Drop animation speed (ms)
#define SCORE_DISPLAY_TIME 3000  // Score display duration (ms)
#define WIN_DISPLAY_TIME   3000  // Win animation duration (ms)
```

### LED Brightness
Adjust in `setup()`:
```cpp
FastLED.setBrightness(200);  // 0-255, default 200
```

### Time Limits
Modify `modeTimes` array:
```cpp
const uint16_t modeTimes[7] = {0, 10000, 8000, 6000, 4000, 3000, 2000};
```

## Game Rules

1. Players take turns dropping pieces in columns
2. Pieces fall to the lowest available position
3. First to connect 4 pieces horizontally, vertically, or diagonally wins
4. Multiple Connect-4s in one move score multiple points
5. Loser of previous round starts next round
6. First to 7 points wins the grand match
7. After grand win, score resets and returns to mode selection

## Serial Monitor

Connect at **9600 baud** to see game events:
- Mode selection confirmations
- Win announcements with point counts
- Current score updates
- Time-out notifications
- Grand winner announcements

## Troubleshooting

### LEDs not lighting up
- Check power supply is adequate (5V, 2A+ recommended)
- Verify data pin connection to pin 2
- Check LED strip type (should be WS2812B/GRB)
- Common ground between Arduino and LED power

### Buttons not responding
- Verify button pins 3-9 are connected to GND when pressed
- Check debounce delay (default 50ms)
- Test with Serial monitor to see button presses

### Wrong LED positions
- Verify LED strip is connected starting at LED 0
- Check zigzag pattern matches your physical layout
- May need to adjust `getLedIndex()` function

### Colors incorrect
- Change color order in `FastLED.addLeds()`:
```cpp
FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);  // Try RGB or BGR
```

## License

See LICENSE file for details.

## Credits

Arduino Connect 4 LED Game
Hardware: Arduino Nano + WS2812B LED Strip
