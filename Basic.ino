#include <Encoder.h>
#include <LiquidCrystal_I2C.h>

// Constants
const int NUM_CHANNELS = 8;
const int MIDI_MAX_VALUE = 127;
const int PWM_MAX_VALUE = 255;
const int DEBOUNCE_DELAY = 20;
const int MIDI_BAUD_RATE = 31250;
const int THRESHOLD = 2;

// Bar levels for LCD
byte barLevels[8][8] = {
    {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b11111},
    {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b11111, 0b11111},
    {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b11111, 0b11111, 0b11111},
    {0b00000, 0b00000, 0b00000, 0b00000, 0b11111, 0b11111, 0b11111, 0b11111},
    {0b00000, 0b00000, 0b00000, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111},
    {0b00000, 0b00000, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111},
    {0b00000, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111},
    {0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111}};

// Pin definitions
const int A = 2;
const int B = 3;
const int C = 4;
Encoder myEnc(5, 6);
const int buttonPin = 7;
const int pwmPin = 10;
LiquidCrystal_I2C lcd(0x27, 16, 2);
bool lastButtonState = false;
unsigned long lastDebounceTime = 0;

int lastPrintedValues[NUM_CHANNELS] = {-1, -1, -1, -1, -1, -1, -1, -1};
int settingsMidiCC[NUM_CHANNELS][2] = {
    {1, 2},
    {1, 4},
    {2, 6},
    {2, 8},
    {1, 10},
    {1, 22},
    {2, 33},
    {2, 44}};
int oldPosition = 0;
int debouncedPosition = 0;
unsigned long lastEncoderDebounceTime = 0;
int selectedChannel = 0;
int initialEncoderPosition = 0;

void setup()
{
  pinMode(A, OUTPUT);
  pinMode(B, OUTPUT);
  pinMode(C, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(pwmPin, OUTPUT);

  // Increase PWM frequency for Timer 2 (pins 3 and 11 on Arduino Uno)
  TCCR2B = TCCR2B & B11111000 | B00000001; // Set Timer 2 prescaler to 1 (31.25 kHz PWM frequency)

  lcd.init();
  lcd.backlight();
  for (int i = 0; i < NUM_CHANNELS; i++)
  {
    lcd.createChar(i, barLevels[i]);
  }
  Serial.begin(MIDI_BAUD_RATE);
}

void loop()
{
  handleButtonPress();
  handleEncoder();
  updateDisplay();
  readAndSendMidiValues();
}

void handleButtonPress()
{
  bool currentButtonState = digitalRead(buttonPin);
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY)
  {
    if (currentButtonState == LOW && lastButtonState == HIGH)
    {
      selectedChannel = (selectedChannel + 1) % NUM_CHANNELS;
      lcd.clear();
    }
  }
  lastButtonState = currentButtonState;
}

void handleEncoder()
{
  int newPosition = myEnc.read() / 3;
  if (newPosition != oldPosition)
  {
    lastEncoderDebounceTime = millis();
    oldPosition = newPosition;
  }

  if ((millis() - lastEncoderDebounceTime) > DEBOUNCE_DELAY)
  {
    if (debouncedPosition != oldPosition)
    {
      debouncedPosition = oldPosition;
      int relativeChange = debouncedPosition - initialEncoderPosition;
      settingsMidiCC[selectedChannel][1] = constrain(settingsMidiCC[selectedChannel][1] + relativeChange, 0, MIDI_MAX_VALUE);
      initialEncoderPosition = debouncedPosition;
    }
  }
}

void updateDisplay()
{
  static int lastDisplayedValues[NUM_CHANNELS] = {-1, -1, -1, -1, -1, -1, -1, -1};
  static int lastSelectedChannel = -1;

  lcd.setCursor(0, 0);
  lcd.print("CC:");
  lcd.print(settingsMidiCC[selectedChannel][1]);
  lcd.print(" ");
  lcd.setCursor(0, 1);
  lcd.print("MIDI:");
  lcd.print(settingsMidiCC[selectedChannel][0]);

  for (int channel = 0; channel < NUM_CHANNELS; channel++)
  {
    int barLevel = map(lastPrintedValues[channel], 0, MIDI_MAX_VALUE, 0, 7);

    if (lastDisplayedValues[channel] != barLevel || lastSelectedChannel != selectedChannel)
    {
      int col = 8 + channel;
      lcd.setCursor(col, 0);
      lcd.write((byte)barLevel);

      lcd.setCursor(col, 1);
      lcd.print(channel == selectedChannel ? "*" : " ");

      lastDisplayedValues[channel] = barLevel;
    }
  }

  lastSelectedChannel = selectedChannel;
}

void selectChannel(int channel)
{
  digitalWrite(A, channel & 0x01);
  digitalWrite(B, (channel >> 1) & 0x01);
  digitalWrite(C, (channel >> 2) & 0x01);
}

void sendMIDIControlChange(int midiChannel, int ccNumber, int value)
{
  Serial.write(0xB0 | (midiChannel - 1));
  Serial.write(ccNumber);
  Serial.write(value);
}

void readAndSendMidiValues()
{
  int analogValues[NUM_CHANNELS];

  for (int channel = 0; channel < NUM_CHANNELS; channel++)
  {
    selectChannel(channel);
    analogValues[channel] = analogRead(A0);

    int midiValue = map(analogValues[channel], 0, 1023, 0, MIDI_MAX_VALUE);

    if (abs(midiValue - lastPrintedValues[channel]) >= THRESHOLD)
    {
      lastPrintedValues[channel] = midiValue;
      sendMIDIControlChange(settingsMidiCC[channel][0], settingsMidiCC[channel][1], midiValue);
    }
    int pwmValue = 180; // map(analogValues[channel], 0, 1023, 0, PWM_MAX_VALUE);
    analogWrite(pwmPin, pwmValue);
  }
}