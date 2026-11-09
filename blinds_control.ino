#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Stepper.h>
#include <IRremote.h>

// --- OLED DISPLAY SETTINGS ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- STEPPER MOTOR SETTINGS ---
const int stepsPerRevolution = 2048; 
Stepper myStepper(stepsPerRevolution, 19, 5, 18, 17);

// --- IR RECEIVER SETTING ---
const int IR_RECEIVE_PIN = 15; 

// --- LED SETTING ---
const int LED1_PIN = 26;
const int LED2_PIN = 27;

// --- LIGHT SENSOR (LDR) SETTING ---
const int LDR_PIN = 34;

unsigned long lastIrTime = 0;
const unsigned long irTimeout = 250; // Stop if no signal received for 250ms

enum MotorState { IDLE, MOVING_UP, MOVING_DOWN, PAUSED };
MotorState currentState = IDLE;

enum ControlMode { MANUAL, AUTOMATIC };
ControlMode currentControlMode = MANUAL;

// Remote Button Codes
const unsigned long UP_BUTTON_CODE     = 0x18;   
const unsigned long DOWN_BUTTON_CODE  = 0x52; 
const unsigned long PAUSE_BUTTON_CODE  = 0x44; // '*' button
const unsigned long SWITCH_BUTTON_CODE = 0x1C; // 'OK' button

void setup() {
  Serial.begin(115200);
  delay(1000);

  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);

  Wire.begin(21, 22);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed."));
    for(;;); 
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("System Initialized"));
  display.display();
  delay(1000);

  myStepper.setSpeed(16); 
}

void loop() {
  // 1. Read current ambient light level from LDR circuit (0 to 4095)
  int lightLevel = analogRead(LDR_PIN);

  // 2. Check for incoming IR signals
  if (IrReceiver.decode()) {
    if (!(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)) {
      unsigned long commandCode = IrReceiver.decodedIRData.command;
      
      // Check if the Pause button (*) was pressed
      if (commandCode == PAUSE_BUTTON_CODE) {
        if (currentState == PAUSED) {
          currentState = IDLE;
          Serial.println("System Resumed");
        } else {
          currentState = PAUSED;
          Serial.println("System Paused");
        }
      } 
      // Check if the OK button was pressed to switch modes
      else if (commandCode == SWITCH_BUTTON_CODE && currentState != PAUSED) {
        if (currentControlMode == MANUAL) {
          currentControlMode = AUTOMATIC;
          Serial.println("Switched to AUTOMATIC mode");
        } else {
          currentControlMode = MANUAL;
          Serial.println("Switched to MANUAL mode");
        }
      }
      // If in MANUAL mode and not paused, allow remote UP/DOWN movement
      else if (currentControlMode == MANUAL && currentState != PAUSED) {
        if (commandCode == UP_BUTTON_CODE) {
          currentState = MOVING_UP;
        } else if (commandCode == DOWN_BUTTON_CODE) {
          currentState = MOVING_DOWN;
        }
      }
    }
    lastIrTime = millis();
    IrReceiver.resume(); 
  }

  // 3. Safety Timeout for manual button hold
  if (currentControlMode == MANUAL && currentState != PAUSED && (millis() - lastIrTime > irTimeout)) {
    currentState = IDLE;
  }

  // 4. Determine display strings and motor actions
  String modeString = (currentControlMode == MANUAL) ? "Mode: Manual" : "Mode: Auto";
  String statusString = "";

  if (currentState == PAUSED) {
    statusString = "State: PAUSED";
  }
  else if (currentControlMode == AUTOMATIC) {
    if (lightLevel < 500) {
      statusString = "Auto: Closing";
      myStepper.step(-32); 
    } else {
      statusString = "Auto: Opening";
      myStepper.step(32);  
    }
  }
  else {
    if (currentState == MOVING_UP) {
      statusString = "State: Opening";
      myStepper.step(64); 
    } 
    else if (currentState == MOVING_DOWN) {
      statusString = "State: Closing";
      myStepper.step(-64); 
    } 
    else {
      statusString = "State: Idle";
    }
  }

  // 5. Update the OLED display with 4 clean lines
  updateDisplay(modeString, statusString, lightLevel);

  // 6. LED control based on light level
  if (lightLevel < 500) {
    digitalWrite(LED1_PIN, HIGH);
    digitalWrite(LED2_PIN, HIGH);
  } else {
    digitalWrite(LED1_PIN, LOW);
    digitalWrite(LED2_PIN, LOW);
  }
}

void updateDisplay(String modeText, String statusText, int lightVal) {
  display.clearDisplay();
  
  // Line 1: Title (Stays in the top yellow section)
  display.setCursor(0, 0);
  display.println(F("Blinds Controller"));
  
  // Line 2: Mode (Moved safely down into the blue section)
  display.setCursor(0, 18);
  display.println(modeText);
  
  // Line 3: State (Blue section)
  display.setCursor(0, 32);
  display.println(statusText);
  
  // Line 4: Light Level (Blue section)
  display.setCursor(0, 46);
  display.print(F("Light: "));
  display.print(lightVal);

  display.display();
}