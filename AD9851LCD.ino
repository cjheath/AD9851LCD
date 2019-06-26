/*
 * Radio Frequency Signal generator.
 *
 * An Arduino Sketch using:
 * - the TJCTMP24024 TFT LCD display with touchscreen
 * - the AD9851 Direct Digital Synthesis chip.
 *
 * The LCD driver is the ILI9341, so we use the Adafruit library.
 * The Touchscreen driver is the XPT2046, so we use my library, not Adafruit's:
 * https://github.com/cjheath/XPT2046_Touchscreen
 * The AD9851 is driven by a library of mine: https://github.com/cjheath/AD9851
 */
#include <SPI.h>                        // Hardware SPI
#include <Adafruit_GFX.h>               // Standard Adafruit graphics
#include <Adafruit_ILI9341.h>           // Standard Adafruit ILI9341 driver
#include <XPT2046_Touchscreen.h>        // My XPT2046 driver
#include <AD9851.h>                     // My AD9851 driver
#include <TSEvents.h>                   // Touchscreen event processing

// Use hardware SPI (on Uno, #13, #12, #11) and these for CS&DC:
#define TFT_DC  6                       // Define your pin for the ILI9341 D/C signal
#define TFT_CS  3                       // Define your pin for the ILI9341 Chip Select
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

/*
 * Software driver (based on a template) for the touchscreen events
 */
// our handlers for touchscreen events are below
void	dragTo(int digit, int x, int y);

#define TS_CS 8                         // Define your Chip Select pin for the XPT2046
class TouchscreenEvents
: public TSEvents<XPT2046_Touchscreen>	// We're going to use an XPT2046_Touchscreen here
{
public:
  TouchscreenEvents()
  : TSEvents<XPT2046_Touchscreen>(	// Here's where we construct the hardware driver:
    XPT2046_Touchscreen(TS_CS, XPT2046_NO_IRQ, XPT2046_FLIP_X|XPT2046_FLIP_Y)
  )
  {}
  void	touch(int x, int y);
  void  repeat(int x, int y) { touch(x, y); }
  void	dragTo(int capture_id, int x, int y);
};
TouchscreenEvents	tsev;		// This object encapsulates the touchscreen and event detection

/*
 * Calibration is offset in parts-per-billion error (positive if the frequency is high, negative if low)
 * To calibrate the frequency of your AD9851 module, set this value to 0.
 * Measure the frequency generated, and then change this calibration factor
 */
#define CALIBRATION		0L           // Set this to the correct value for calibration.
#define AD9851_FQ_UD_PIN	10
#define AD9851_RESET_PIN	9
#define AD9851_REFERENCE_FREQ	180000000L	// Default 3rd template argument (x6 multiplier engaged)
// And MOSI=11, SCK=13

class MyAD9851 : public AD9851<AD9851_RESET_PIN, AD9851_FQ_UD_PIN> {};
MyAD9851 dds;

#define FREQUENCY_MAX 70000000          // 70MHz max, to match the filter

unsigned long frequency = 10000000;     // The frequency we want
unsigned long displayed = 0;            // The frequency on the display

void setup() {
  Serial.begin(19200);
  tft.begin();            // Start the LCD
  Serial.println(F("AD9851 Touchscreen"));

  // Start the touchscreen
  if (!tsev.begin()) {
    Serial.println("Couldn't start touchscreen");
    while (1);
  }
  // diagnostics();

  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);
  if (CALIBRATION != 0)
  	dds.setClock(CALIBRATION);
}

/* Geometry of the font */
#define FONT_WIDTH    6                                 // Spacing in pixels
#define FONT_HEIGHT   8                                 // Total height of a character
#define	FONT_DESCENT  1					// Pixels below baseline

/* Geometry of main numeric display text */
#define	CHAR_MAG      5					// Multiply the font 5x
#define	CHAR_CONTRACT 2					// Pixels to contract width per magnified char
#define CHAR_WIDTH    (FONT_WIDTH*CHAR_MAG-CHAR_CONTRACT) // Spacing in pixels
#define CHAR_HEIGHT   (FONT_HEIGHT*CHAR_MAG)            // Total height of a character
#define CHAR_DESCENT  (FONT_DESCENT*CHAR_MAG)           // Descent below the baseline
#define CHAR_ASCENT   (CHAR_HEIGHT-CHAR_DESCENT)        // Ascent above the baseline

/* Positioning of the frequency display */
#define TEXT_X        0                                 // Left side
#define TEXT_Y        ((240+CHAR_ASCENT)/2)             // Text baseline

void show_number(long number, int indent, int baseline, int size = CHAR_MAG, int foreground = ILI9341_CYAN, int background = ILI9341_DARKGREEN);

void loop(void) {
  if (displayed != frequency)
    change_frequency();
  tsev.detect();
}

void change_frequency()
{
  // Change the DDS
  uint32_t delta = dds.frequencyDelta(frequency);
  dds.setDelta(delta);

  // Display the change
  show_number(frequency, TEXT_X, TEXT_Y);

  // Record the change
  displayed = frequency;

  // Show a diagnostic
  Serial.print("Frequency set to ");
  Serial.print(frequency);
  Serial.print(", delta ");
  Serial.println(delta);
}

/*
 * Text display for numbers. Ten characters, displays up to 99,999,999
 * Draws one character at a time to control the density.
 */
void show_number(long number, int indent, int baseline, int size = CHAR_MAG, int foreground = ILI9341_CYAN, int background = ILI9341_DARKGREEN)
{
  tft.setTextSize(size);		// 30x40 pixels (five times 6x8)
  tft.setTextColor(foreground);		// Background is not defined so it is transparent
#define TEXT_PAD	5
  tft.fillRect(indent, baseline-CHAR_ASCENT-TEXT_PAD, 10*CHAR_WIDTH, CHAR_ASCENT+CHAR_DESCENT+TEXT_PAD*2, background);
  unsigned long n = number;
  for (int place = 10; place > 0;) {    // Derive the digits from the right to left
    tft.setCursor(indent+CHAR_WIDTH*--place, baseline-CHAR_HEIGHT+CHAR_DESCENT);
    if (n)
      tft.print((char)('0'+n%10));
    n /= 10;
    if (n && (place == 7 || place == 3)) {
      tft.setCursor(indent+CHAR_WIDTH*--place, baseline-CHAR_HEIGHT+CHAR_DESCENT);
      tft.print(',');		// These commas are really ugly.
    }
  }
  // tft.drawLine(indent, baseline, indent+10*CHAR_WIDTH, baseline, ILI9341_RED); // Draw the text baseline
}

void applyFrequency(unsigned long new_frequency)
{
  if (new_frequency <= FREQUENCY_MAX && new_frequency > 0)
    frequency = new_frequency;
}

unsigned long dragAdjustment;   // The scaled value of the drag adjustment
unsigned long increment;        // The unit value of the digit we're adjusting.

// A touch or repeat on the screen comes here. Figure out what digit to adjust.
void
TouchscreenEvents::touch(int x, int y)
{
  unsigned long new_frequency = frequency;
  int digit = (TEXT_X+10*CHAR_WIDTH-x) / CHAR_WIDTH + 1;
  if (digit == 4 || digit == 8)
    return;   // Above a ","
  if (digit > 4) digit--;
  if (digit > 7) digit--;
  increment = 1;
  for (int i = 1; i < digit; i++)
    increment *= 10;

  if (y < TEXT_Y-CHAR_ASCENT) {         // Step up
    Serial.print("raise ");
    new_frequency += increment;
  } else if (y > TEXT_Y+CHAR_DESCENT) { // Step down
    Serial.print("lower ");
    new_frequency -= increment;
  } else {                              // Drag this digit up or down
    Serial.print("drag ");
    tsev.dragCapture(digit);                 // Start dragging on this digit
    dragAdjustment = 0;
  }
  Serial.println(digit);
  applyFrequency(new_frequency);
}

// We set a dragCapture on a digit, and have moved from there to (x, y)
void TouchscreenEvents::dragTo(int digit, int x, int y)
{
  unsigned long new_frequency = frequency;
  int   units;

  units = (TEXT_Y-CHAR_ASCENT/2-y) / 10;// Each ten pixels is one unit
  new_frequency -= dragAdjustment;      // Cancel previous adjustment
  dragAdjustment = units*increment;     // Calculate new adjustment
  new_frequency += dragAdjustment;      // And make it
  applyFrequency(new_frequency);

  Serial.print("drag digit ");
  Serial.print(digit);
  Serial.print(" to ");
  Serial.println(frequency);            // Show the actual frequency, in case it was bad
}

#if 0
void diagnostics()
{
  // read diagnostics (optional but can help debug problems)
  uint8_t x = tft.readcommand8(ILI9341_RDMODE);
  Serial.print("Display Power Mode: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDMADCTL);
  Serial.print("MADCTL Mode: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDPIXFMT);
  Serial.print("Pixel Format: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDIMGFMT);
  Serial.print("Image Format: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDSELFDIAG);
  Serial.print("Self Diagnostic: 0x"); Serial.println(x, HEX);
}
#endif
