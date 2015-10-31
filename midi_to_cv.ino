// =====================================================================
// USB MIDI to CV converter
// MCP4822 dac chip & Teensy 3
// Bruce Lott - August 2014
// ======================= ! THANKS YALL ! =============================
// - Olivier Gillet/Mutable Instruments, for making open source products.
//   CVPal's code and schematic were referenced. 
//
// - Ness Morris, who helped with the dac serial protocol,
//   and helped me understand a few bit operations.
//
// - Robin Price, whose code I referenced.
// ======================= ! THANKS YALL ! =============================
#include <SPI.h>


// CV outs
#define XOX_GATE_PIN 5 
#define ATL_GATE_PIN 6
#define ACCENT_PIN 7
#define SLIDE_PIN 8
// config options
#define MCP4822_CHANNEL_B 0x8000    // write to dac B, 0x0000 is dac A
#define MCP4822_LOW_GAIN 0x2000     // 2.048V, 0x0000 is 4.096V
#define MCP4822_SHUTDOWN 0x1000     // turns the chip on, 0x0000 is off
#define MCP4822_CONFIG_MASK 0xb000  // bit mask for config bits
// misc
#define TWELVE_BIT_MAX 0x0FFF      // for setting dac
#define CS_PIN 0                   // must be held low when setting dac 

// MCP4821/4822 message protocol
// which dac, dont care, gain 1x/2x, shutdown, then the data bits MSB to LSB
// |A/B| |X | |GA| |SD| |D11| |D10| |D9| |D8| |D7| |D6| |D5| |D4| |D3| |D2| |D1| |D0|
// bit 15                                                                         bit 0

uint16_t xox_config;      // configuration bits for dac messages
uint16_t atl_config;
float xox_pitches[49];         // xox_pitches in volts (0.0-4.0) table
float atl_pitches[49];
uint8_t xox_notes_held = 0;    // number of notes waiting for note up MIDI
uint8_t atl_notes_held = 0;

// tune C and F# up the octaves with these
float xox_voltages[9] = { 
  0.0, 0.51, 1, 1.4908, 1.9844, 2.477, 2.967, 3.458, 3.9528 
};

float atl_voltages[9] = { 
  0.0, 0.4845, 0.971, 1.4548, 1.9414, 2.4294, 2.916, 3.406 , 3.8942 
};

void setup(){
  SPI.begin();

  // set pin modes
  pinMode(CS_PIN, OUTPUT);
  pinMode(XOX_GATE_PIN, OUTPUT);
  pinMode(ACCENT_PIN, OUTPUT);
  pinMode(SLIDE_PIN, OUTPUT);
  pinMode(ATL_GATE_PIN, OUTPUT);

  // use | to join config options to create config bit
  // this config writes to dac a, with the chip on, at 4.096V (2x gain)
  uint16_t the_config = MCP4822_SHUTDOWN; 
  xox_config = the_config & MCP4822_CONFIG_MASK; // bit mask for safety
  the_config = MCP4822_SHUTDOWN | MCP4822_CHANNEL_B;
  atl_config = the_config & MCP4822_CONFIG_MASK;
  
  digitalWrite(CS_PIN, HIGH); // set low to send message

  // create pitch table
  for(int i=0; i<8; i++){
    for(int j=0; j<6; j++){
      xox_pitches[(i*6)+j] = (xox_voltages[i+1]-xox_voltages[i])*(j/6.0) + xox_voltages[i];
      xox_pitches[(i*6)+j] *= 0.25;
      atl_pitches[(i*6)+j] = (atl_voltages[i+1]-atl_voltages[i])*(j/6.0) + atl_voltages[i];
      atl_pitches[(i*6)+j] *= 0.25;
    }
  }
  xox_pitches[48] = xox_voltages[8]*0.25;
  atl_pitches[48] = atl_voltages[8]*0.25;

  // USB MIDI callbacks
  usbMIDI.setHandleNoteOn(note_on);
  usbMIDI.setHandleNoteOff(note_off);
}

void loop(){
  usbMIDI.read();
}

// set dac 0.0-1.0 for 0-4V
void set_dac_a(float level){ 
  uint16_t result = constrain(level, 0.0, 1.0)*TWELVE_BIT_MAX;
  result = result | xox_config; // Add bits together

  // send the message in 2 bytes
  digitalWrite(CS_PIN, LOW);  // CS pin must be low to transmit message
  SPI.transfer(uint8_t(result >> 8));
  SPI.transfer(uint8_t(result & 0xFF));
  digitalWrite(CS_PIN, HIGH);
}  

void set_dac_b(float level){ 
  Serial.println("tryna");
  uint16_t result = constrain(level, 0.0, 1.0)*TWELVE_BIT_MAX;
  result = result | atl_config; // Add bits together

  // send the message in 2 bytes
  digitalWrite(CS_PIN, LOW);  // CS pin must be low to transmit message
  SPI.transfer(uint8_t(result >> 8));
  SPI.transfer(uint8_t(result & 0xFF));
  digitalWrite(CS_PIN, HIGH);
} 

// ========================= MIDI CALLBACKS =========================
void note_on(byte chan, byte note, byte vel){
  // X0X
  if(chan == 2){
    // accent
    if(vel>100){
      digitalWrite(ACCENT_PIN, HIGH);
    }
    else{
      digitalWrite(ACCENT_PIN, LOW);
    }

    // gate or slide
    if(!xox_notes_held){
      digitalWrite(XOX_GATE_PIN, HIGH);
    }
    else{
      digitalWrite(SLIDE_PIN, HIGH);
    }

    // pitch
    set_dac_a(xox_pitches[constrain(note, 24, 72)-24]);

    xox_notes_held++;
  }
  // Atlantis
  else if(chan==1){
    // gate
    if(!atl_notes_held){
      digitalWrite(ATL_GATE_PIN, HIGH);
    }

    // pitch
    set_dac_b(atl_pitches[constrain(note, 24, 72)-24]);

    atl_notes_held++;
  }
}

void note_off(byte chan, byte note, byte vel){
  // X0X
  if(chan==2){
    if(xox_notes_held>0){ 
      xox_notes_held--;
    }
    if(!xox_notes_held){
      digitalWrite(XOX_GATE_PIN, LOW);
      digitalWrite(SLIDE_PIN, LOW);
    }
  }
  // Atlantis
  else if(chan==1){
    if(atl_notes_held>0) atl_notes_held--;
    if(!atl_notes_held){
      digitalWrite(ATL_GATE_PIN, LOW);
    }
  }
}


