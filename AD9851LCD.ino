/*
 * Radio Frequency Signal generator.
 *
 * An Arduino Sketch using the TJCTMP24024 TFT LCD display with touchscreen
 * and the AD9851 Direct Digital Synthesis chip.
 *
 * The LCD driver is the ILI9841, so we use the Adafruit library.
 * The Touchscreen driver is the XPT2046, so we use that library, not Adafruit's.
 */
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <AD9851.h>

#define TFT_DC  9               // This pin sends the ILI9841 D/C signal
#define TFT_CS  10              // This pin sends the ILI9841 Chip Select

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS&DC
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

/*
 * This is calibration data for the raw touch data to the screen coordinates.
 * The bare chip sends values in the range 0..4095. These set the useful range.
 * You will need to adjust these values to set the X and Y ranges to 0..320, 240
 */
#define TS_MINX 450
#define TS_MINY 130
#define TS_MAXX 3800
#define TS_MAXY 4000

#define TS_AUTOREPEAT   250     // Auto Repeat time in ms
#define TS_MOTION_THRESHOLD 10  // pixels; we don't use motion

#define TS_CS 8                 // This pin is the Chip Select for the XPT2046
XPT2046_Touchscreen ts(TS_CS, XPT2046_NO_IRQ, XPT2046_FLIP_X|XPT2046_FLIP_Y);

/*
 * To calibrate the frequency of your AD9851 module,
 * set this value to 10000000. Measure the frequency
 * generated, and then change this calibration factor
 * to the frequency you measured.
 */
#define CALIBRATION   9999941   // Set this to the frequency produced when set to 10MHz
#define FREQUENCY_MAX 50000000  // 50MHz max. It won't be a clean signal, so don't go higher!

#define AD9851_CS_PIN  2        // This pin is the Chip Select for the AD9851
AD9851 dds(AD9851_CS_PIN);

unsigned long frequency = 10000000;     // The frequency we want
unsigned long displayed = 0;            // The frequency on the display

inline int iabs(int i) { return i < 0 ? -i : i; }

void setup() {
  Serial.begin(9600);
  tft.begin();            // Start the LCD
  Serial.println(F("AD9851 Touchscreen"));

  // Start the touchscreen
  if (!ts.begin()) {
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
#define TEXT_X        0          // Left side
#define TEXT_Y        ((240+CHAR_ASCENT)/2)   // Text baseline

void loop(void) {
  if (displayed != frequency)
    set_frequency(TEXT_X, TEXT_Y);
  detectTouches();
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
  for (int place = 10; place > 0;) {
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

// A touch or repeat on the screen comes here
void activate(int x, int y)
{
  unsigned long old_frequency = frequency;
  int digit = (TEXT_X+10*CHAR_WIDTH-x) / CHAR_WIDTH + 1;
  if (digit == 4 || digit == 8)
    return;   // Above a ","
  if (digit > 4) digit--;
  if (digit > 7) digit--;
  unsigned long adjustment = 1;
  for (int i = 1; i < digit; i++)
    adjustment *= 10;
  
  if (y < TEXT_Y-CHAR_ASCENT) {
    // Serial.print("above ");
    frequency += adjustment;
  } else if (y > TEXT_Y+CHAR_DESCENT) {
    // Serial.print("below ");
    frequency -= adjustment;
  } else {
    // Serial.print("text ");
  }
  // Serial.println(digit);
  if (frequency > FREQUENCY_MAX || frequency == 0)
    frequency = old_frequency;
}

void touch(int x, int y)
{
//  showXY("touch", x, y);
  activate(x, y);
}

void repeat(int x, int y)
{
//  showXY("repeat", x, y);
  activate(x, y);
}

void motion(int x, int y)
{
//  showXY("motion", x, y);
}

void release(int x, int y)
{
//  showXY("release", x, y);
}

void showXY(const char* why, int x, int y)
{
  Serial.print(why);
  Serial.print("(");
  Serial.print(x);
  Serial.print(',');
  Serial.print(y);
  Serial.println(")");
}

void detectTouches()
{
  static bool touching = false; // Were we touching?
  static int  last_touch = 0;   // millis() as at last touch or auto-repeat
  static int  last_x = 0;
  static int  last_y = 0;
  int         x, y;
  if (ts.touched()) {
    TS_Point p = ts.getPoint();

    // Scale from ~0->4000 to tft.width using the calibration #'s
    x = map(p.x, TS_MINX, TS_MAXX, 0, tft.width());
    y = map(p.y, TS_MINY, TS_MAXY, 0, tft.height());

    if (!touching) {                // Touch just started
      last_touch = millis();
      touching = true;
      touch(x, y);
    } else if (millis()-TS_AUTOREPEAT > last_touch) { // auto-repeat
      last_touch = millis();
      repeat(x, y);
    } else {                        // Not yet time to auto-repeat
      if (iabs(x-last_x) + iabs(y-last_y) < TS_MOTION_THRESHOLD)
        return;                     // Not enough motion to matter
      motion(x, y);
    }
    last_x = x;
    last_y = y;
  } else {
    if (!touching)
      return;
    release(last_x, last_y);
    touching = false;
  }
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
