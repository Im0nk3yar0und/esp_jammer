/*
 * Modified version of the original code by dkyazzentwatwa (https://github.com/dkyazzentwatwa/cypher-jammer).
 * All credits for the base functionality go to the original author.
 * 
 * Thank you to the original author for their work!
 */

#include "RF24.h"               // RadioHead RF24 library for NRF24L01+ wireless transceiver
#include <SPI.h>                // SPI communication library (required for RF24 and other SPI devices)
#include <ezButton.h>           // Debounced button input library for handling switch/button events
#include "esp_bt.h"             // ESP32 Bluetooth stack control (enable/disable Bluetooth functionality)
#include "esp_wifi.h"           // ESP32 Wi-Fi stack control (enable/disable Wi-Fi functionality)

#include <Wire.h>               // I2C communication library
#include <Adafruit_GFX.h>       // Core graphics library for displays
#include <Adafruit_SSD1306.h>   // SSD1306 OLED display driver




// OLED display dimensions (in pixels)
#define SCREEN_WIDTH 128        // Width of the OLED display
#define SCREEN_HEIGHT 64        // Height of the OLED display


// SSD1306 display configuration
#define OLED_RESET -1           // Reset pin (-1 means share Arduino reset pin)
#define SCREEN_ADDRESS 0x3C     // I2C address (0x3C for 128x32, 0x3D for 128x64)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); // Initialize OLED object



// Bitmap dimensions for the custom logo
#define LOGO_HEIGHT 16          // Logo height (in pixels)
#define LOGO_WIDTH 16           // Logo width (in pixels)

// Binary representation of the logo (16x16 pixels)
static const unsigned char PROGMEM logo_bmp[] =
{ 0b00000000, 0b11000000,
  0b00000001, 0b11000000,
  0b00000001, 0b11000000,
  0b00000011, 0b11100000,
  0b11110011, 0b11100000,
  0b11111110, 0b11111000,
  0b01111110, 0b11111111,
  0b00110011, 0b10011111,
  0b00011111, 0b11111100,
  0b00001101, 0b01110000,
  0b00011011, 0b10100000,
  0b00111111, 0b11100000,
  0b00111111, 0b11110000,
  0b01111100, 0b11110000,
  0b01110000, 0b01110000,
  0b00000000, 0b00110000 };




/* Defining Pins & Variables
 * HSPI=SCK = 18, MISO = 19, MOSI = 23, CS = 15 , CE = 16
 * VSPI=SCK = 14, MISO = 12, MOSI = 13 ,CS = 25 ,CE = 26
 */

// Define HSPI pins
#define hp_sck  18
#define hp_miso 19
#define hp_mosi 23
#define hp_cs   15
#define hp_ce   16

// Define VSPI pins
#define vp_sck  14
#define vp_miso 12
#define vp_mosi 13
#define vp_cs   25
#define vp_ce   26


// sp: Pointer for VSPI (Virtual SPI).
// hp: Pointer for HSPI (Hardware SPI).
SPIClass *sp = nullptr;
SPIClass *hp = nullptr;


// Creates two RF24 objects for two SPI buses (HSPI & VSPI).
// Both are set to 16MHz SPI speed for fast communication.
RF24 radio(hp_ce, hp_cs, 16000000);             //HSPI CAN SET SPI SPEED TO 16000000 BY DEFAULT ITS 10000000
RF24 radio1(vp_ce, vp_cs, 16000000);            //VSPI CAN SET SPI SPEED TO 16000000 BY DEFAULT ITS 10000000






/* flag and flagv are variables used to control the direction of channel sweeping.
 * They act as switches (flags) to determine whether the channel values should increase or decrease.
 * If flag == 0, the channel (ch) increases.
 * If flag == 1, the channel (ch) decreases.
 * When the channel reaches a limit (79 or 2), flag is flipped (0 → 1 or 1 → 0).
 */
unsigned int flag = 0;   // HSPI - Flag variable to keep track of direction
unsigned int flagv = 0;  // VSPI - Flag variable to keep track of direction


/*
 * nRF24L01 Channel Distribution (2.4 GHz)
 * ---------------------------------------
 * - Total Channels: 125 (0-124)
 * - Frequency Range: 2400 MHz - 2525 MHz
 * - Channel Spacing: 1 MHz
 * - Default Usable Range: 0-79 (2400-2479 MHz) to avoid Wi-Fi interference
 *
 * Wi-Fi (802.11b/g/n) Channels:
 * -----------------------------
 * - Channels: 1-14
 * - Frequency Range: 2412 MHz - 2484 MHz
 * - Each channel is 20 MHz wide (overlapping)
 * - Non-overlapping channels: 1 (2412 MHz), 6 (2437 MHz), 11 (2462 MHz)
 * - Channels 12-14 used in EU/Japan only
 *
 * Bluetooth (2.4 GHz)
 * -------------------
 * - Frequency Range: 2402 MHz - 2480 MHz
 * - Uses Adaptive Frequency Hopping (AFH)
 * - 79 Channels (1 MHz apart)
 * - Channels: 0-78 (2402-2480 MHz)
 *
 * Drone FPV (First Person View) Channels:
 * ---------------------------------------
 * - Frequency Range: 2400 MHz - 2483 MHz
 * - Typically uses 8-40 MHz wide channels
 * - Common FPV Channels: 2410 MHz, 2430 MHz, 2450 MHz, 2470 MHz, 2490 MHz
 *
 */

int ch = 45;                  // Variable to store value of ch
int ch1 = 45;                 // Variable to store value of ch


// - Pin 27 is the GPIO connected to the physical button/switch.
ezButton toggleSwitch(27);


/* Adjusts Radio Channels
 * Increases/decreases radio channels dynamically.
 * If flag or flagv is 0, channels increase; otherwise, they decrease.
 * Prevents out-of-range channel values (limits: 2 to 79).
 * Updates both radio modules with new channel values.
 */


// gradual channel change
void gradual_ch() {
  if (flagv == 0) {  // If flag is 0, increment ch by 4 and ch1 by 1

    ch1 += 4;
  } else {           // If flag is not 0, decrement ch by 4 and ch1 by 1

    ch1 -= 4;
  }

  if (flag == 0) {  // If flag is 0, increment ch by 4 and ch1 by 1
    ch += 2;

  } else {          // If flag is not 0, decrement ch by 4 and ch1 by 1
    ch -= 2;
  }

  // Check if ch1 is greater than 79 and flag is 0
  if ((ch1 > 79) && (flagv == 0)) {
    flagv = 1;                             // Set flag to 1 to change direction
  } else if ((ch1 < 2) && (flagv == 1)) {  // Check if ch1 is less than 2 and flag is 1
    flagv = 0;                             // Set flag to 0 to change direction
  }

  // Check if ch is greater than 79 and flag is 0
  if ((ch > 79) && (flag == 0)) {
    flag = 1;                             // Set flag to 1 to change direction
  } else if ((ch < 2) && (flag == 1)) {   // Check if ch is less than 2 and flag is 1
    flag = 0;                             // Set flag to 0 to change direction
  }


  radio.setChannel(ch);
  radio1.setChannel(ch1);


  // Print to serial
  // Serial.print("SP:");
  // Serial.print(ch1);
  // Serial.print("\tHP:");
  // Serial.println(ch);
}



/* Function: random_ch() - Sets Random Channels
 * Assigns a random channel (0-79) to each radio module.
 * Delays for a random time (0-60 microseconds) to add randomness.
 */


void random_ch() {

  int randomChannel1 = random(80);
  int randomChannel2 = random(80);

  radio1.setChannel(randomChannel1);
  radio.setChannel(randomChannel2);

  // Serial.print("Radio1 Channel: ");
  // Serial.println(randomChannel1);

  // Serial.print("Radio Channel: ");
  // Serial.println(randomChannel2);

  delayMicroseconds(random(60));  //////REMOVE IF SLOW
}





// Initializes ESP32 & Radios
void setup() {

  // Start serial communication
  Serial.begin(115200);

  // Initialize the OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));  // Error message if display fails
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000);          // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();

  // Draw a single pixel in white
  display.drawPixel(10, 10, SSD1306_WHITE);

  // Show the display buffer on the screen. You MUST call display() after
  // drawing commands to make them visible on screen!
  display.display();
  delay(2000);
  display.clearDisplay();
  display.display();
  delay(2000);

  testdrawstyles();    // Draw 'stylized' characters

  
  // Draw a single pixel in white
  delay(2000);                        // Pause for 2 seconds
  display.clearDisplay();             // Clear the buffer
  display.drawPixel(10, 10, SSD1306_WHITE);
  display.display();








  Serial.println("Jammer started ....");

  // Disables Bluetooth & WiFi to reduce power consumption.
  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();
  esp_wifi_disconnect();
  Serial.println("Disable Bluetooth & WiFi to reduce power consumption. DONE");

  // Debounce time of 50ms to prevent unwanted button presses.
  toggleSwitch.setDebounceTime(50);


  // Calls functions to initialize HSPI & VSPI radios.
  initHP();
  initSP();
}


// Creates & starts a new VSPI instance.
void initSP() {
  sp = new SPIClass(VSPI);
  sp->begin(vp_sck, vp_miso, vp_mosi, vp_cs);

  if (radio1.begin(sp)) {
    Serial.println("SP Started !!!");
    radio1.setAutoAck(false);                     // Disables auto acknowledgment.
    radio1.stopListening();                       // Stops listening (acts as a transmitter).
    radio1.setRetries(0, 0);                      
    radio1.setPALevel(RF24_PA_MAX, true);         // Sets maximum power level.    
    radio1.setDataRate(RF24_2MBPS);               // Uses 2Mbps data rate for fast transmission.
    radio1.setCRCLength(RF24_CRC_DISABLED);       // Disables CRC checking for speed.
    radio1.printPrettyDetails();
    radio1.startConstCarrier(RF24_PA_MAX, ch1);   // Starts constant carrier mode on ch1.
  } else {
    Serial.println("SP couldn't start !!!");
  }
}



// Creates & starts a new HSPI instance.
void initHP() {
  hp = new SPIClass(HSPI);
  hp->begin(hp_sck, hp_miso, hp_mosi, hp_cs);
  
  if (radio.begin(hp)) {
    Serial.println("HP Started !!!");
    radio.setAutoAck(false);                      // Disables auto acknowledgment.
    radio.stopListening();                        // Stops listening (acts as a transmitter).
    radio.setRetries(0, 0);
    radio.setPALevel(RF24_PA_MAX, true);          // Sets maximum power level.
    radio.setDataRate(RF24_2MBPS);                // Uses 2Mbps data rate for fast transmission.
    radio.setCRCLength(RF24_CRC_DISABLED);        // Disables CRC checking for speed.
    radio.printPrettyDetails();
    radio.startConstCarrier(RF24_PA_MAX, ch);     // Starts constant carrier mode on ch1.
  } else {
    Serial.println("HP couldn't start !!!");
  }
}



void draw_gradual(void) {
  display.clearDisplay();              // Clear the display buffer (erases previous content)

  // Initial display settings
  display.setTextSize(1);              // Set text size to normal (1:1 pixel scale)
  display.setTextColor(SSD1306_WHITE); // Set text color to white

  // First text line
  display.setCursor(0, 0);             // Position cursor at top-left corner (x=0, y=0)
  display.println(F("Jamming mode:")); // Print static text (stored in flash with F()))

  // Add vertical spacing
  display.setCursor(0, 20);            // Move cursor down 20 pixels (x=0, y=20)
  display.setTextSize(2);              // Increase text size to 2X
  display.setTextColor(SSD1306_WHITE); // Maintain white text color
  display.println("GRADUAL");          // Print mode label

  display.display();                   // Push buffer content to physical OLED display
}




void draw_random(void) {
  // Clear the display buffer (removes all previously drawn content)
  display.clearDisplay();

  // Configure basic text settings
  display.setTextSize(1);              // Set standard text size (6x8 pixels per character)
  display.setTextColor(SSD1306_WHITE); // Set text color to white (visible on OLED's black background)

  // Display header text
  display.setCursor(0, 0);             // Position cursor at top-left corner (x=0, y=0)
  display.println(F("Jamming mode:")); // Print constant string (stored in flash memory to save RAM)

  // Display mode label with different formatting
  display.setCursor(0, 20);            // Move cursor down 20 pixels from top (x=0, y=20)
  display.setTextSize(2);              // Set larger text size (approx 12x16 pixels per character)
  display.setTextColor(SSD1306_WHITE); // Maintain white text color
  display.println("RANDOM");           // Print the current jamming mode label

  // Update the physical display with the prepared buffer
  display.display();
}





void testdrawstyles(void) {
  // Initialize display by clearing the previous frame buffer
  display.clearDisplay();

  // Configure default text settings
  display.setTextSize(1);              // Standard character size (6x8 pixels)
  display.setTextColor(SSD1306_WHITE); // White text on black background

  // First text line - Basic text demonstration
  display.setCursor(0, 0);             // Position at top-left (x=0,y=0)
  display.println(F("Hello, world!")); // Store string in flash memory (PROGMEM)

  // Second text line - Inverted text demonstration
  display.setCursor(0, 20);            // Move down 20 pixels (x=0,y=20)
  // Inverted color (black text on white background):
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.println(3.141592);           // Print floating point number

  // Third text line - Large text with hexadecimal value
  display.setCursor(0, 40);            // Move down 40 pixels total (x=0,y=40)
  display.setTextSize(2);              // Double-size characters (~12x16 pixels)
  display.setTextColor(SSD1306_WHITE); // Revert to white-on-black
  display.print(F("0x"));              // Print hex prefix (stored in flash)
  display.println(0xDEADBEEF, HEX);    // Print value in hexadecimal format

  // Render the frame buffer to the physical display
  display.display();
  
  // Pause for visibility (2000ms = 2 seconds)
  delay(2000);
}





void testdrawbitmap(void) {
  // Clear the display buffer to prepare for new drawing
  display.clearDisplay();

  // Draw a bitmap image centered on the display
  display.drawBitmap(
    (display.width()  - LOGO_WIDTH ) / 2,  // X position: centered horizontally
    (display.height() - LOGO_HEIGHT) / 2, // Y position: centered vertically
    logo_bmp,                             // Bitmap array (defined elsewhere)
    LOGO_WIDTH,                           // Width of bitmap in pixels
    LOGO_HEIGHT,                          // Height of bitmap in pixels
    1);                                   // Color (1=white, 0=black)

  // Send the buffer to the physical display
  display.display();
  
  // Keep the image visible for 1 second (1000ms)
  delay(1000);
}





// Boolean flag to track the current mode (false = gradual_ch, true = random_ch)
bool isRandomMode = false; // Initially set to gradual channel change mode

void loop() {
  // Must call the loop() function of ezButton to update the button state
  toggleSwitch.loop();  

  /*
   * Toggle mechanism:
   * - If the button is pressed, switch the mode (true ↔ false).
   * - If switching to true, enable random_ch().
   * - If switching to false, enable gradual_ch().
   * - The mode remains unchanged until the button is pressed again.
   */
  if (toggleSwitch.isPressed()) {
    isRandomMode = !isRandomMode; // Toggle between random and gradual mode

    // Print the current mode to the Serial Monitor
    if (isRandomMode) {
      Serial.println("Switched to RANDOM jamming mode!");
      draw_random();
    } else {
      Serial.println("Switched to GRADUAL jamming mode!");
      draw_gradual();
    }
  }

  /*
   * Execute the corresponding function based on the current mode:
   * - If isRandomMode == true → Call random_ch() for random channel switching.
   * - If isRandomMode == false → Call gradual_ch() for smooth channel changes.
   */
  if (isRandomMode) {
    random_ch();
  } else {
    gradual_ch();
  }
}
