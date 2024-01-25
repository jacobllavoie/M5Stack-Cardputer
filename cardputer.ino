#include <M5Unified.h>
#include <M5Cardputer.h>
#include <Button.h>
#include <IRremote.hpp>
#include <M5Unified.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <ArduinoOTA.h>
#include "Unit_Encoder.h"
#include "config.h"

// Constants
const long gmtOffset_sec = -28800;  // Adjust for your time zone
const int daylightOffset_sec = 3600; // For daylight saving time
const unsigned long REFRESH_INTERVAL = 1000;
const int buttondebounceDelay = 50; // Adjust as needed
const int encoderdebounceDelay = 500;

// Screen coordinates (adjust as needed)
int timeX = 0;
int timeY = 0;
int dateX = 120;
int dateY = 2;

// Menu items
// const char* menuItems[] = {"Apps", "Settings", "Power"};
// const int numMenuItems = sizeof(menuItems) / sizeof(menuItems[0]);
struct MenuItem {
  const char* name;
  int level; // 0 for main menu items, 1 for submenu items, etc.
  bool selected; // Add this line if it's missing
};

MenuItem menuItems[] = {
//  {"Apps", 0},
  {"Settings", 0},
    {"Display", 1},
      {"Brightness", 2},
      {"Timeout", 2},
    {"Network", 1},
  {"Power", 0},
    {"Shutdown", 1}, // Submenu item for Power
    {"Reboot", 1}   // Submenu item for Power
};

// Menu state and timing
const int MENU_IDLE_TIMEOUT = 5000;  // Milliseconds

// Networking and time synchronization
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", gmtOffset_sec, daylightOffset_sec);

// Variables
int currentMenuItemIndex = 0;   // Index of the currently selected item
bool inMenuMode = false;   // Flag indicating menu mode
int previousMenuItem = 0;  // Stores previous menu item before entering menu mode
unsigned long menuIdleTimer = 0;  // Timer for menu idle timeout
String selectedPowerOption = "";  // Stores selected power option
unsigned long previousMillis = 0;
unsigned long lastTimeUpdate = 0;
Unit_Encoder sensor; // Initialize the encoder
signed short int encoder_last_value = 0;
unsigned long selectionHighlightDuration = 0;
int currentMenuLevel = 0; // Initialize to 0 for the main menu
int numMenuItems = sizeof(menuItems) / sizeof(menuItems[0]);
bool previousButtonState = false;
unsigned long previousButtonMillis = 0;
bool shouldRedrawMenu = false; // Initial value set to false

// Global flag for redrawing
// bool shouldRedrawMenu = true;

// Button for menu navigation
Button BtnA(0);

void setup() {
  // Serial communication
  Serial.begin(115200);

  // M5Cardputer initialization
  auto cfg = M5.config();
  M5.begin(cfg);
  M5Cardputer.begin(cfg, true);
  M5.Lcd.begin();
  M5.Lcd.setRotation(1);

  // Button initialization
  BtnA.begin();

  // Encoder initalization
  sensor.begin(&Wire, 0x40, 2, 1, 200000L);

  // WiFi connection
  connectToWiFi();

  // Time synchronization
  timeClient.begin();

  currentMenuLevel = 0; // Set to main menu initially

  // Initial home screen
  drawMenu();

  // Over-the-Air (OTA) updates
  setupOTA();
  

}

void loop() {
  ArduinoOTA.handle();
  M5Cardputer.update();

  // drawMenu();

  // Handle encoder
  handleEncoder();
  if (shouldRedrawMenu) {
    drawMenu();
    shouldRedrawMenu = false;
  }

  // Update time display every 5 seconds
  updateTimeDisplay();

  // Serial.println(currentMenuLevel);

  // Debugs
  Serial.println("Current variables begin");
  Serial.println("currentMenuLevel: " + String(currentMenuLevel));
  Serial.println("currentMenuItemIndex: " + String(currentMenuItemIndex));
  Serial.println("Encoder button pressed: " + String(sensor.getButtonStatus()));
  Serial.println("Selected menu item: " + String(menuItems[currentMenuItemIndex].name));
  Serial.println("Current variables end");
  delay(1000);
}

// ... Function implementations here ...
// Connect to WiFi
void connectToWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
}

int16_t calculateXPosition(String itemName) {
  // Measure the width of the item name using M5.Lcd.textWidth()
  int16_t itemWidth = M5.Lcd.textWidth(itemName);

  // Calculate the horizontal center position based on screen width
  int16_t x = (M5.Lcd.width() - itemWidth) / 2;

  // Adjust for indentation based on menu level if needed
  // (Consider adding a parameter for item level if applicable)

  return x;
}

int16_t calculateYPosition(int itemIndex, int itemLevel) {
    int16_t itemSpacing = 20; // Adjust as needed
    int16_t startingY = 20; // Adjust based on your layout
    int16_t levelIndentation = 10; // Adjust as needed

    int16_t y = startingY + itemSpacing * itemIndex + levelIndentation * itemLevel;

    // Check for screen boundary
    if (y + M5.Lcd.fontHeight() > M5.Lcd.height()) {
        // Handle overflow (e.g., scroll, paginate, or adjust layout)
        return -1; // Or indicate overflow in a different way
    }

    return y;
}

void drawMenu() {
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextSize(2);

  // Print current menu level and number of items for debugging
  Serial.print("Drawing menu level: ");
  Serial.println(currentMenuLevel);
  Serial.print("Number of items to draw: ");
  Serial.println(numMenuItems);


  for (int i = 0; i < numMenuItems; i++) {
    if (menuItems[i].level == currentMenuLevel) {
      int16_t x = calculateXPosition(menuItems[i].name);
      // Assuming you have a variable named `currentMenuLevel` representing the current menu level:
      int16_t y = calculateYPosition(i, currentMenuLevel); // Provide both itemIndex and itemLevel
      drawMenuItem(x, y, menuItems[i]);
      Serial.println(menuItems[i].name);
    }
  }
}

void drawMenuItem(int16_t x, int16_t y, MenuItem item) {
  // Set the cursor position
  M5.Lcd.setCursor(x, y);

  // Highlight the selected item (adjust colors as needed)
  if (item.selected) {
    M5.Lcd.setTextColor(TFT_GREEN); // Use white text on a black background for highlighting
  } else {
    M5.Lcd.setTextColor(TFT_WHITE); // Use regular black text on a white background
  }

  // Print the item name (without indentation)
  M5.Lcd.println(item.name);
}


void drawTimeAndDate() {
  // 1. Update the time from NTP server once:
  timeClient.update();

  // 2. Retrieve the current time:
  unsigned long epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime((time_t *)&epochTime);

  // 3. Format the time and date strings:
  char timeStr[9];
  strftime(timeStr, sizeof(timeStr), "%T", ptm);

  char dateStr[11];
  strftime(dateStr, sizeof(dateStr), "%m/%d/%Y", ptm);

  // 4. Clear the previous time and date display (optional):
  M5.Lcd.fillRect(timeX, timeY, 94, 14, TFT_BLACK); // Time adjust coordinates as needed
  M5.Lcd.fillRect(dateX, dateY, 116, 14, TFT_BLACK); // Date adjust coordinates as needed

  // 5. Draw the time string:
  M5.Lcd.setTextColor(TFT_BLUE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(timeX, timeY);
  M5.Lcd.println(timeStr);

  // 6. Position and draw the date string:
  int dateX = M5.Lcd.width() - M5.Lcd.textWidth(dateStr) - 2; // Adjust as needed
  int dateY = 2; // Adjust as needed
  M5.Lcd.setCursor(dateX, dateY);
  M5.Lcd.println(dateStr);
}

void handleEncoder() {
  // Get encoder value and button status
  signed short int encoder_value    = sensor.getEncoderValue();
  bool btn_status                   = sensor.getButtonStatus();
  // Serial.println(encoder_value);
  // Serial.println(sensor.getButtonStatus());

  // Debounce encoder (adjust threshold and delay as needed)
  if (abs(encoder_value - encoder_last_value) >= 2) {
      if (encoder_last_value > encoder_value) { // Rotated clockwise
          sensor.setLEDColor(1, 0x000080);
          navigateToNextMenuItem();
      } else {
          sensor.setLEDColor(2, 0x800000); // Rotated counterclockwise
          navigateToPreviousMenuItem();
      }
      encoder_last_value = encoder_value;
      shouldRedrawMenu = true;
  } else {
      sensor.setLEDColor(0, 0x000000);
  }
  
  // Debounce button (adjust delay as needed)
  if (!btn_status && millis() - previousButtonMillis > buttondebounceDelay) {
    if (!previousButtonState) { // Button press detected
      selectMenuItem(currentMenuItemIndex); // Call a function to handle selection
      previousButtonState = true;
      previousButtonMillis = millis();
      sensor.setLEDColor(0, 0x008000);
    }
  } else if (btn_status) {
    previousButtonState = false;
  }
}

void navigateToNextMenuItem() {
  Serial.println("Entering navigateToNextMenuItem()");
  currentMenuItemIndex++;
  while (menuItems[currentMenuItemIndex].level != currentMenuLevel) {
    currentMenuItemIndex = (currentMenuItemIndex + 1) % numMenuItems; // Wrap around
  // Clear previous highlights
  for (int i = 0; i < numMenuItems; i++) {
    menuItems[i].selected = false;
  }
  }
  menuItems[currentMenuItemIndex].selected = true; // Mark the new item as selected
  
  drawMenu(); // Update the displayed menu
}

void navigateToPreviousMenuItem() {
  Serial.println("Entering navigateToPreviousMenuItem()");
  currentMenuItemIndex--;  // Decrement to move to the previous item
  while (menuItems[currentMenuItemIndex].level != currentMenuLevel) {
    currentMenuItemIndex = (currentMenuItemIndex + numMenuItems - 1) % numMenuItems; // Wrap around
  // Clear previous highlights
  for (int i = 0; i < numMenuItems; i++) {
    menuItems[i].selected = false;
  }
  }
  menuItems[currentMenuItemIndex].selected = true; // Mark the new item as selected
  
  drawMenu(); // Update the displayed menu
}

void selectMenuItem(int index) {
    Serial.println("Entering selectMenuItem()");

    // If the selected item is a parent item (level 0)
    if (menuItems[index].level == 0) {
        // Enter the submenu
        currentMenuLevel = 1;  // Set the current level to the first submenu level
        drawMenu();           // Re-draw the menu to display only submenu items
    } else {
        // Handle submenu item selection logic
        currentMenuLevel = menuItems[index].level; // Update level based on selected submenu item
    }
        if (strcmp(menuItems[index].name, "Shutdown") == 0) {
            // Perform shutdown action
            ESP.deepSleep(0);  // Shutdown
        } else if (strcmp(menuItems[index].name, "Reboot") == 0) {
            // Perform restart action
            ESP.restart();     // Restart
        } else {
            // Handle other submenu item selections
            // ...
        }
}

// Update the time display only when needed
void updateTimeDisplay() {
  if (millis() - lastTimeUpdate >= REFRESH_INTERVAL) {
    drawTimeAndDate();
    lastTimeUpdate = millis();
  }
}

// Configure and start Over-the-Air (OTA) updates
void setupOTA() {
  // ... OTA configuration here ...
  ArduinoOTA.begin();
}
