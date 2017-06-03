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
#define TFT_DC  9                       // Define your pin for the ILI9341 D/C signal
#define TFT_CS  10                      // Define your pin for the ILI9341 Chip Select
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

/*
 * Software driver (based on a template) for the touchscreen events
 */
// our handlers for touchscreen events are below
void	activate(int x, int y);
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
  void	touch(int x, int y) { activate(x, y); }
  void  repeat(int x, int y) { activate(x, y); }
  void	dragTo(int capture_id, int x, int y) { ::dragTo(capture_id, x, y); }
};
TouchscreenEvents	tsev;		// This object encapsulates the touchscreen and event detection

/*
 * To calibrate the frequency of your AD9851 module,
 * set this value to 10000000. Measure the frequency
 * generated, and then change this calibration factor
 * to the frequency you measured.
 */
#define CALIBRATION		9999953           // Set this to the correct value for calibration.
#define AD9851_FQ_UD_PIN	2
#define AD9851_RESET_PIN	3
// And MOSI=11, SCK=13

class MyAD9851 : public AD9851<AD9851_RESET_PIN, AD9851_FQ_UD_PIN, CALIBRATION> {};
MyAD9851 dds;

#define FREQUENCY_MAX 30000000          // 30MHz max. It won't be a clean signal, so don't go higher!

unsigned long frequency = 10000000;     // The frequency we want
unsigned long displayed = 0;            // The frequency on the display

void setup() {
  Serial.begin(9600);
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
}

/* Geometry of our chosen font */
#define CHAR_WIDTH    28                                // Spacing in pixels
#define CHAR_HEIGHT   40                                // Total height of a character
#define CHAR_DESCENT  5                                 // Descent below the baseline
#define CHAR_ASCENT   (CHAR_HEIGHT-CHAR_DESCENT)        // Ascent above the baseline

/* Positioning of the frequency display */
#define TEXT_X        0                                 // Left side
#define TEXT_Y        ((240+CHAR_ASCENT)/2)             // Text baseline

void loop(void) {
  if (displayed != frequency)
    set_frequency(TEXT_X, TEXT_Y);
  tsev.detect();
}

/*
 * Text display for the frequency.
 * We draw one character at a time, so we know where each is.
 */
void set_frequency(int x, int y)
{
  dds.setFrequency(frequency*10000000ULL/CALIBRATION);

  tft.setTextSize(5);                   // 30x40 pixels (five times 6x8)
  tft.setTextColor(ILI9341_CYAN);       // Background is not defined so it is transparent
  tft.fillRect(x, y-CHAR_ASCENT-10, 10*CHAR_WIDTH, CHAR_ASCENT+CHAR_DESCENT+15, ILI9341_DARKGREEN);
  unsigned long f = frequency;
  for (int place = 10; place > 0;) {    // Derive the digits from the right to left
    tft.setCursor(x+CHAR_WIDTH*--place, y-CHAR_HEIGHT+CHAR_DESCENT);
    if (f)
      tft.print((char)('0'+f%10));
    f /= 10;
    if (f && (place == 7 || place == 3)) {
      tft.setCursor(x+CHAR_WIDTH*--place, y-CHAR_HEIGHT+CHAR_DESCENT);
      tft.print(',');
    }
  }
  // tft.drawLine(x, y, x+10*CHAR_WIDTH, y, ILI9341_RED); // Draw the text baseline
  Serial.print("Frequency set to ");
  Serial.println(frequency);
  displayed = frequency;
}

void applyFrequency(unsigned long new_frequency)
{
  if (new_frequency <= FREQUENCY_MAX && new_frequency > 0)
    frequency = new_frequency;
}

unsigned long dragAdjustment;   // The scaled value of the drag adjustment
unsigned long increment;        // The unit value of the digit we're adjusting.

// A touch or repeat on the screen comes here. Figure out what digit to adjust.
void activate(int x, int y)
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
    // Serial.print("raise ");
    new_frequency += increment;
  } else if (y > TEXT_Y+CHAR_DESCENT) { // Step down
    // Serial.print("lower ");
    new_frequency -= increment;
  } else {                              // Drag this digit up or down
    // Serial.print("drag ");
    tsev.dragCapture(digit);                 // Start dragging on this digit
    dragAdjustment = 0;
  }
  // Serial.println(digit);
  applyFrequency(new_frequency);
}

// We set a dragCapture on a digit, and have moved from there to (x, y)
void dragTo(int digit, int x, int y)
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
