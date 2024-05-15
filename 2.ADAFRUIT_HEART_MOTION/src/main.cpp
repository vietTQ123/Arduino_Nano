#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <MAX30100_PulseOximeter.h>
#include <Fonts/DSEG7_Classic_Mini_Bold_25.h>
#include <Fonts/SFProText_Semibold12pt7b.h>
#include <SPI.h>
#include <Wire.h>

#define TFT_CS 10
#define TFT_RST 12
#define TFT_DC 8
#define TFT_SCK 13
#define TFT_MOSI 11

// The following settings adjust various factors of the display
#define SCALING 12                                                  // Scale height of trace, reduce value to make trace height
// bigger, increase to make smaller
#define TRACE_SPEED 0.5                                             // Speed of trace across screen, higher=faster   
#define TRACE_MIDDLE_Y_POSITION 41                                  // y pos on screen of approx middle of trace
#define TRACE_HEIGHT 64                                             // Max height of trace in pixels    
#define HALF_TRACE_HEIGHT TRACE_HEIGHT/2                            // half Max height of trace in pixels (the trace amplitude)    
#define TRACE_MIN_Y TRACE_MIDDLE_Y_POSITION-HALF_TRACE_HEIGHT+1     // Min Y pos of trace, calculated from above values
#define TRACE_MAX_Y TRACE_MIDDLE_Y_POSITION+HALF_TRACE_HEIGHT-1     // Max Y pos of trace, calculated from above values

// Recommended settings for the MAX30100, DO NOT CHANGE!!!!,  refer to the datasheet for further info
#define SAMPLING_RATE                       MAX30100_SAMPRATE_100HZ       // Max sample rate
#define IR_LED_CURRENT                      MAX30100_LED_CURR_50MA        // The LEDs currents must be set to a level that 
#define RED_LED_CURRENT                     MAX30100_LED_CURR_27_1MA      // avoids clipping and maximises the dynamic range
#define PULSE_WIDTH                         MAX30100_SPC_PW_1600US_16BITS // The pulse width of the LEDs driving determines
#define HIGHRES_MODE                        true                          // the resolution of the ADC

#define X1_GRAPH 10
#define X2_GRAPH 138

#define X1_BMP_O2 25


#define REPORTING_PERIOD_MS 3000

PulseOximeter pox;
MAX30100 sensor;            // Raw Data

uint32_t tsLastReport = 0;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

uint8_t BPM, O2;                            // BPM and O2 values
int16_t Diff = 0;                           // The difference between the Infra Red (IR) and Red LED raw results
uint16_t ir, red;                           // raw results returned in these
static float lastx = X1_GRAPH+1;                     // Last x position of trace
static int lasty = TRACE_MIDDLE_Y_POSITION; // Last y position of trace, default to middle
static float x = X1_GRAPH+1;                         // current x position of trace
int32_t y;   
static int32_t SensorOffset = 10000;                                  // current y position of trace

void onBeatDetected()
{
  Serial.println("Beat!");
}

void setup()
{
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST7735_BLACK);
  tft.setRotation(3);

  Serial.begin(9600);

  Serial.print("Initializing pulse oximeter..");

  if (!sensor.begin()) {
    tft.print("Could not initialise MAX30100");
    for (;;);                                               // End program in permanent loop
  }
  sensor.setMode(MAX30100_MODE_SPO2_HR);
  sensor.setLedsCurrent(IR_LED_CURRENT, RED_LED_CURRENT);
  sensor.setLedsPulseWidth(PULSE_WIDTH);
  sensor.setSamplingRate(SAMPLING_RATE);
  sensor.setHighresModeEnabled(HIGHRES_MODE);


  if (!pox.begin())
  {
    Serial.println("FAILED");
    for (;;);
  } else {
    Serial.println("SUCCESS");
  }
  pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
  pox.setOnBeatDetectedCallback(onBeatDetected);

  tft.setTextSize(2);
  tft.setCursor(X1_BMP_O2, 86);
  tft.print("BPM   O");
  tft.setCursor(X1_BMP_O2+92, 86);
  tft.print("%");
  tft.setTextSize(1);
  tft.setCursor(X1_BMP_O2+84, 94);
  tft.print("2");                            // The small subscriper 2 of O2
  tft.setCursor(X1_GRAPH+1, 0);
  tft.print("XTronical Health Care");
  tft.setTextSize(2);
  tft.drawRect(X1_GRAPH, TRACE_MIN_Y - 1, X2_GRAPH, TRACE_HEIGHT + 2, ST7735_BLUE);

}

void loop()
{


  pox.update();

  sensor.update();                            // request raw data from sensor
  if (sensor.getRawValues(&ir, &red))         // If raw data available for IR and Red
  {
    if (red < 1000)                           // No pulse
      y = TRACE_MIDDLE_Y_POSITION;            // Set Y to default flat line in middle
    else
    {
      // Plot our new point
      Diff = (ir - red);                      // Get raw difference between the 2 LEDS
      Diff = Diff - SensorOffset;             // Adjust the baseline of raw values by removing the offset (moves into a good range for scaling)
      Diff = Diff / SCALING;                  // Scale the difference so that it appears at a good height on screen

      // If the Min or max are off screen then we need to alter the SensorOffset, this should bring it nicely on screen
      if (Diff < -HALF_TRACE_HEIGHT)
        SensorOffset += (SCALING * (abs(Diff) - 32));
      if (Diff > HALF_TRACE_HEIGHT)
        SensorOffset += (SCALING * (abs(Diff) - 32));

      y = Diff + (TRACE_MIDDLE_Y_POSITION - HALF_TRACE_HEIGHT);   // These two lines move Y pos of trace to approx middle of display area
      y += TRACE_HEIGHT / 4;
    }

    if (y > TRACE_MAX_Y) y = TRACE_MAX_Y;                         // If going beyond trace box area then crop the trace
    if (y < TRACE_MIN_Y) y = TRACE_MIN_Y;                         // so it stays within
    tft.drawLine(lastx, lasty, x, y, ST7735_YELLOW);              // Plot the next part of the trace
    lasty = y;                                                    // Save where the last Y pos was
    lastx = x;                                                    // Save where the last X pos was
    x += TRACE_SPEED;                                             // Move trace along the display
    if (x > X2_GRAPH-2)                                                  // If reached end of display then reset to statt
    {
      tft.fillRect(X1_GRAPH+1, TRACE_MIN_Y, X2_GRAPH-2, TRACE_HEIGHT, ST7735_BLACK); // Blank trace display area
      x = X1_GRAPH+1;                                                      // Back to start
      lastx = x;
    }
  }

  if (millis() - tsLastReport > REPORTING_PERIOD_MS)
  {
    BPM = round(pox.getHeartRate());
    tft.fillRect(0, 104, 128, 16, ST7735_BLACK);                // Clear the old values
    BPM = round(pox.getHeartRate());                            // Get BPM
    if ((BPM < 60) | (BPM > 110))
    {
      tft.setTextColor(ST7735_RED);    // If too low or high for a resting heart rate then display in red
    }
    else
    {
      tft.setTextColor(ST7735_GREEN);                           // else display in green
    }
    tft.setCursor(X1_BMP_O2, 104);                                      // Put BPM at this position
    tft.print(BPM);                                             // print BPM to screen


    O2 = pox.getSpO2();                                         // Get the O2
    if (O2 < 94)
    {
      tft.setTextColor(ST7735_RED);
    }                                                // If too low then display in red
    else
    {
      tft.setTextColor(ST7735_GREEN);
    }
    // else green
    tft.setCursor(X1_BMP_O2+72, 104);                                     // Set print position for the O2 value
    tft.print(O2);

    tsLastReport = millis();
  }
}