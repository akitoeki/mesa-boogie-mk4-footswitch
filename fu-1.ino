#include <Bounce2.h>
#include <EEPROM.h>
#include <MIDI.h>
#include <LiquidCrystal_I2C.h>
#include <BigFont01_I2C.h>

#define LED 13

MIDI_CREATE_DEFAULT_INSTANCE();
LiquidCrystal_I2C lcd(0x27, 16, 2);
BigFont01_I2C     big(&lcd);

const int numSwitches = 6;  // Total number of footswitches
const int numChannels = 3;  // Number of channels switches

const int mode = 0; // 0: relay, 1: xlr
const int menu = 0; // 0: main screen, 1: setup

int footSwitchPins[numSwitches] = { 8, 9, 10, 11, 12 };       // Pins for footswitches
int relayPins[numSwitches] = { 2, 3, 4, 5, 6 };              // Pins for relays
bool lastStates[numChannels][numSwitches - numChannels];  // Adjusted to not initialize here, as it will be loaded from EEPROM

int activeChannel = 0;  // Will be loaded from EEPROM

Bounce debouncers[numSwitches];

void setup() {


  // Start the Serial port with the MIDI baud rate
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);


  // Initialize the LCD connected to the I2C address
  lcd.init();
  // Turn on the backlight
  lcd.backlight();
  big.begin();

  // Initialize MIDI communications, with the default settings.
  // MIDI input will be read from Arduino's RX pin.
  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.setHandleNoteOn(MyHandlerNoteOn);
  MIDI.setHandleNoteOff(MyHandlerNoteOff);

  // Initialize from EEPROM
  for (int i = 0; i < numChannels; i++) {
    for (int j = 0; j < numSwitches - numChannels; j++) {
      int address = i * numChannels + j;  // Calculate EEPROM address
      lastStates[i][j] = EEPROM.read(address);
    }
  }
  activeChannel = EEPROM.read(numChannels * (numSwitches - numChannels));  // Last address for the active channel

  for (int i = 0; i < numSwitches; i++) {
    pinMode(footSwitchPins[i], INPUT_PULLUP);
    debouncers[i].attach(footSwitchPins[i]);
    debouncers[i].interval(50);  // Debounce interval of 50ms

    // Update relays if mode is relay
    if (mode == 0) {
      pinMode(relayPins[i], OUTPUT);
      digitalWrite(relayPins[i], LOW);  // Ensure all relays are off at start
    }
    
  }

  // Apply the loaded state to relays
  if (mode == 0) {    
    for (int i = 0; i < numSwitches - numChannels; i++) {
      digitalWrite(relayPins[i + numChannels], lastStates[activeChannel][i] ? HIGH : LOW);
    }
    digitalWrite(relayPins[activeChannel], HIGH);  // Ensure the active channel is on
  }
  
  writeText(activeChannel);
}

void MyHandlerNoteOn(byte channel, byte pitch, byte velocity) {
  digitalWrite(LED, HIGH);
  lcd.setCursor(7, 1);
  lcd.print(channel);
  lcd.print(pitch);
}

void MyHandlerNoteOff(byte channel, byte pitch, byte velocity) {
  digitalWrite(LED, LOW);
}
int signal = 0;
void loop() {

  MIDI.read();

  for (int i = 0; i < numSwitches; i++) {

    debouncers[i].update();
    if (debouncers[i].fell()) {  // Check for a state change to LOW (footswitch pressed)


      if (i < numChannels) {  // Unique behavior for channel switches
        Serial.println(i);
        handleUniqueGroup(i);
      } else {
        // Toggle the state for the other relays
        bool currentState = digitalRead(relayPins[i]);
        digitalWrite(relayPins[i], !currentState);

        // Save the changes to the current active channel
        saveState();
      }

      writeText(activeChannel);
    }
  }
}

void writeText(int channel) {
  lcd.clear();

  big.writechar(0,0,'C');
  big.writeint(0,4,channel + 1,1,true);
  
  if (lastStates[channel][0]) {
      lcd.setCursor(8, 0);
      lcd.print("EQ");
    }

  if (lastStates[channel][1]) {
    lcd.setCursor(12, 0);
    lcd.print("LOOP");
  }

  if (mode == 0) {
    lcd.setCursor(8, 1);
    lcd.print("--->");
    lcd.print("rlay");
  } else {
    lcd.setCursor(8, 1);
    lcd.print("--->");
    lcd.print(" xlr");
  }
  
}

void saveState() {
  for (int i = 0; i < numSwitches - numChannels; i++) {
    lastStates[activeChannel][i] = digitalRead(relayPins[i + numChannels]);
    int address = activeChannel * numChannels + i;  // Calculate EEPROM address
    EEPROM.update(address, lastStates[activeChannel][i]);
  }
  EEPROM.update(numChannels * (numSwitches - numChannels), activeChannel);  // Save the active channel
}

void handleUniqueGroup(int index) {
  bool currentState = digitalRead(relayPins[index]);

  // If it's already on, do nothing
  if (currentState) {
    return;
  }


  // turn off everything different than yourself
  for (int i = 0; i < numChannels; i++) {
    if (i != index) {
      digitalWrite(relayPins[i], LOW);
    }
  }

  // turn yourself on
  digitalWrite(relayPins[index], HIGH);


  // writing the current stored value for the active channel
  for (int i = 0; i < numChannels; i++) {
    digitalWrite(relayPins[i + numChannels], lastStates[index][i] ? HIGH : LOW);
  }

  // change the active channel
  activeChannel = index;
  saveState();  // Save this change to EEPROM
}