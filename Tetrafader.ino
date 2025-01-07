#include <Encoder.h>
#include <LiquidCrystal_I2C.h>

const int NUM_PAIRS = 4;
const int NUM_CHANNELS = 8;
const int MIDI_MAX_VALUE = 127;
const int PWM_MAX_VALUE = 255;
const int DEBOUNCE_DELAY = 20;
const int MIDI_BAUD_RATE = 31250;
const int THRESHOLD = 2;

byte barLevels[8][8] = {
    {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b11111},
    {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b11111, 0b00000},
    {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000},
    {0b00000, 0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000},
    {0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000, 0b00000},
    {0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000},
    {0b00000, 0b11111, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000},
    {0b11111, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000}};

LiquidCrystal_I2C lcd(0x27, 16, 2);
const int A = 2;
const int B = 3;
const int C = 4;
Encoder myEnc(5, 6);
const int buttonPin = 7;
const int pwmPin = 10;
const int xfaderLedPin = 11;
bool lastButtonState = false;
unsigned long lastDebounceTime = 0;
int XFaderValue = 0;
int oldXfaderValue = 0;
int lastPrintedPairValues[NUM_PAIRS] = {-1, -1, -1, -1};
int lastPrintedValues[NUM_CHANNELS] = {-1, -1, -1, -1, -1, -1, -1, -1};
int midiSettings[NUM_CHANNELS][2] = {
    {1, 2},
    {1, 4},
    {2, 6},
    {2, 8},
    {1, 10},
    {1, 22},
    {2, 33},
    {2, 44}};
int oldEncPosition = 0;
int debouncedEncPosition = 0;
unsigned long lastEncoderDebounceTime = 0;
int initialEncoderPosition = 0;

int selectedChannel = 0;

void setup()
{
  pinMode(A, OUTPUT);
  pinMode(B, OUTPUT);
  pinMode(C, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(pwmPin, OUTPUT);
  pinMode(xfaderLedPin, OUTPUT);

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
      selectedChannel = (selectedChannel + 1) % NUM_PAIRS;
      lcd.clear();
    }
  }
  lastButtonState = currentButtonState;
}

void handleEncoder()
{
  int newPosition = myEnc.read() / 4;
  if (newPosition != oldEncPosition)
  {
    lastEncoderDebounceTime = millis();
    oldEncPosition = newPosition;
  }

  if ((millis() - lastEncoderDebounceTime) > DEBOUNCE_DELAY)
  {
    if (debouncedEncPosition != oldEncPosition)
    {
      debouncedEncPosition = oldEncPosition;
      int relativeChange = debouncedEncPosition - initialEncoderPosition;
      midiSettings[selectedChannel][1] = constrain(midiSettings[selectedChannel][1] + relativeChange, 0, MIDI_MAX_VALUE);
      initialEncoderPosition = debouncedEncPosition;
    }
  }
}

void updateDisplay()
{
  static int lastDisplayedValues[NUM_CHANNELS] = {-1, -1, -1, -1, -1, -1, -1, -1};
  static int lastSelectedChannel = -1;
  int mappedXFaderValue = map(XFaderValue, 0, 1023, 7, 0);
  analogWrite(pwmPin, mappedXFaderValue);
  analogWrite(xfaderLedPin, max(0, 4 - mappedXFaderValue)); // Inverted

  lcd.setCursor(0, 0);
  lcd.print("CC");
  lcd.print(midiSettings[selectedChannel][1]);
  lcd.print("X");
  lcd.write((byte)mappedXFaderValue);
  lcd.setCursor(0, 1);
  lcd.print("MIDI:");
  lcd.print(midiSettings[selectedChannel][0]);

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
  Serial.flush();
}

void readAndSendMidiValues()
{
  int analogValues[NUM_CHANNELS];
  int XfaderReading = analogRead(A2);

  if (XfaderReading != oldXfaderValue)
  {
    XFaderValue = XfaderReading;
  }

  for (int channel = 0; channel < NUM_CHANNELS; channel++)
  {
    selectChannel(channel);
    analogValues[channel] = analogRead(A0);
    int midiValue = map(analogValues[channel], 0, 1023, 0, MIDI_MAX_VALUE);
    lastPrintedValues[channel] = midiValue;
  }

  for (int pair = 0; pair < NUM_CHANNELS; pair += 2)
  {
    float morphFactor = 1.0 - XFaderValue / 1023.0; // Inverted
    int morphedValue = analogValues[pair] * (1 - morphFactor) + analogValues[pair + 1] * morphFactor;
    int midiValue = map(morphedValue, 0, 1023, 0, MIDI_MAX_VALUE);

    if (abs(midiValue - lastPrintedPairValues[pair]) >= THRESHOLD)
    {
      lastPrintedPairValues[pair] = midiValue;

      sendMIDIControlChange(midiSettings[pair][0], midiSettings[pair][1], midiValue);
    }
  }
  oldXfaderValue = XfaderReading;
}