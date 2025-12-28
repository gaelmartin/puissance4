# TODO - Connect 4 Arduino Game

## Priority: High

### Bug Fixes
- [ ] Test button debouncing under rapid button presses
- [ ] Verify timer accuracy over long game sessions (millis() rollover after ~50 days)
- [ ] Test behavior when multiple buttons pressed simultaneously
- [ ] Validate win detection for all edge cases (corners, multiple overlapping wins)

### Critical Features
- [ ] Add power-saving mode (dim LEDs after inactivity)
- [ ] Implement sound/buzzer support for game events
- [ ] Add EEPROM support to persist high scores across power cycles
- [ ] Create fail-safe for LED power draw (prevent brownout)

## Priority: Medium

### Enhancements
- [ ] Add difficulty level display using different LED patterns
- [ ] Implement undo/redo functionality (last move)
- [ ] Add "best of N" match options (currently hardcoded to 7 points)
- [ ] Create animation library for custom win celebrations
- [ ] Add AI opponent mode (single player vs computer)
- [ ] Implement different color schemes/themes

### UI/UX Improvements
- [ ] Add "thinking" animation during auto-drop countdown
- [ ] Show remaining time as LED bar graph
- [ ] Add column preview (show where piece will land before dropping)
- [ ] Implement smooth color transitions instead of hard switches
- [ ] Add startup configuration menu (brightness, colors, etc.)

### Code Quality
- [ ] Refactor large functions into smaller, testable units
- [ ] Add function documentation (Doxygen style)
- [ ] Create unit tests for game logic functions
- [ ] Optimize memory usage (check SRAM usage)
- [ ] Add debug mode with verbose serial output
- [ ] Create configuration struct instead of multiple #defines

## Priority: Low

### Nice to Have
- [ ] Add Bluetooth/WiFi support for remote play
- [ ] Create mobile app for game control
- [ ] Implement tournament mode (multiple players, brackets)
- [ ] Add replay functionality (save and playback games)
- [ ] Create statistics tracking (win rate, average time per move, etc.)
- [ ] Add special game modes:
  - [ ] Gravity mode (pieces can "fall" sideways)
  - [ ] Power-ups (remove opponent piece, extra turn, etc.)
  - [ ] Obstacles (some cells blocked)
  - [ ] Timed matches (total game time limit)

### Documentation
- [ ] Create wiring diagrams (Fritzing)
- [ ] Add photos/videos of working project
- [ ] Write beginner-friendly assembly guide
- [ ] Create 3D printable enclosure design
- [ ] Add PCB design files for custom board
- [ ] Write article/tutorial for maker blogs

### Hardware Expansions
- [ ] Design PCB shield for Arduino Nano
- [ ] Add 7-segment displays for score
- [ ] Implement WS2812B alternatives (APA102, etc.)
- [ ] Support different grid sizes (8x8, 9x7, etc.)
- [ ] Add RGB button backlighting
- [ ] Create portable battery-powered version

## Completed Features

- [x] Basic Connect 4 game logic
- [x] LED display with zigzag pattern support
- [x] Multi-mode difficulty system with timers
- [x] Score tracking and grand winner detection
- [x] Win animation with green blinking
- [x] Draw detection and animation
- [x] Button 4 hold for score display
- [x] Timer pause during score view
- [x] Auto-drop on timeout
- [x] Dynamic blink speed based on remaining time
- [x] Mode selection at game start
- [x] Loser starts next round
- [x] Multiple Connect-4 scoring
- [x] Serial debug output

## Known Issues

### Minor Issues
- Comment in line 437 says "//test" - remove or document purpose
- No upper limit validation on brightness setting
- Mode selection timeout might feel too short (3s)
- No visual feedback when column is full

### Potential Improvements
- `checkWin()` could be optimized (currently checks entire board every time)
- Button 4 blocking logic is complex - consider simplification
- Magic numbers in code could be replaced with named constants
- No protection against corrupted board state

## Future Ideas

### Advanced Features
- [ ] Network multiplayer (ESP32/ESP8266)
- [ ] Voice control integration
- [ ] Gesture control (ultrasonic sensors)
- [ ] AR companion app (scan board to see stats)
- [ ] LED animations library (fireworks, waves, etc.)
- [ ] Custom game rules editor
- [ ] Achievement system
- [ ] Daily challenges

### Different Game Modes
- [ ] Connect 5 or Connect 3
- [ ] Reverse mode (lose if you connect 4)
- [ ] Team mode (4 players, 2 teams)
- [ ] Speed mode (both players play simultaneously)
- [ ] Mystery mode (random effects each turn)

## Testing Checklist

Before any release:
- [ ] Test all 7 difficulty modes
- [ ] Verify score tracking accuracy
- [ ] Test grand winner detection at exactly 7 points
- [ ] Test button 4 hold functionality
- [ ] Verify timer pause/resume works correctly
- [ ] Test all win conditions (H, V, D-right, D-left)
- [ ] Test draw condition (full board)
- [ ] Test mode selection timeout
- [ ] Test rapid button pressing (debounce)
- [ ] Verify LED brightness is safe for eyes
- [ ] Test power consumption under full LEDs
- [ ] Verify serial output is helpful

## Development Environment

### Recommended Tools
- Arduino IDE 2.x or PlatformIO
- Serial monitor for debugging
- Logic analyzer for button testing
- Multimeter for power testing
- WS2812B LED testing rig

### Code Standards
- Use consistent indentation (2 spaces)
- Comment complex logic
- Keep functions under 50 lines when possible
- Use descriptive variable names
- Follow Arduino naming conventions
