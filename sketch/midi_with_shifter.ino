#include <MIDI.h>
#include <midi_Defs.h>
#include <midi_Message.h>
#include <midi_Namespace.h>
#include <midi_Settings.h>

#define NUM_SHIFTERS 3
#define NUM_BUTTONS 18
#define CLOCK 2
#define PLATCH 3
#define DATAIN 4
#define OCTAVE 12
#define STARTNOTE 21
#define OCT_MAX 8

// Won't be handling SYSEX
struct MySettings : public MIDI_NAMESPACE::DefaultSettings
{
   static const unsigned SysExMaxSize = 0;
};
MIDI_CREATE_CUSTOM_INSTANCE(TinySoftwareSerial, Serial, MIDI, MySettings)

byte scanin[NUM_SHIFTERS];  //scanin data from shift reg
byte scanin_p[NUM_SHIFTERS];//previous scanin data
byte oct_transp = 2;        //transpose/oct offset
bool switch_oct = false;
byte num_latched_keys = 0;
byte latch_list[NUM_BUTTONS];

void setup()
{
  bitWrite(DDRB, CLOCK, 1);   //clock
  bitWrite(DDRB, PLATCH, 1);  //parallel_serialn latch
  bitWrite(DDRB, DATAIN, 0);  //data_in
  MIDI.begin(1);
  for (int i = 0; i < NUM_BUTTONS; i++) {
    latch_list[i] = 0xFF;
  }
}

void loop()
{
  latchdata(PLATCH);
  switch_oct = false;
  for(int shiftreg = NUM_SHIFTERS-1; shiftreg >= 0; shiftreg--) {
    scanin[shiftreg] = 0;
    for(int i = 7; i >= 0; i--) {
      bitWrite(PORTB, CLOCK, 0);
      delayMicroseconds(0.2);
      scanin[shiftreg] = scanin[shiftreg] | (bitRead(PINB,DATAIN) << i);
      if (shiftreg == NUM_SHIFTERS-1 && i >=6) {
        //if one of the transpose keys is pressed, we need to remember the octave offset of any pressed keys
        switch_oct = bitRead(PINB,DATAIN) ? true : switch_oct;
      } else {
        if (bitRead(PINB,DATAIN) && switch_oct) {
          if (latch_list[shiftreg*8+i] == 0xFF) {
            latch_list[shiftreg*8+i] = oct_transp;
            num_latched_keys++;
          }
        }
      }
      bitWrite(PORTB, CLOCK, 1);
    }
  }

  for(int shiftreg = 0; shiftreg < NUM_SHIFTERS; shiftreg++) {
    for(int i = 0; i < 8; i++) {
      if (bitRead(scanin[shiftreg],i) != bitRead(scanin_p[shiftreg],i)) {
        bitWrite(scanin_p[shiftreg],i, bitRead(scanin[shiftreg],i));
        if ((shiftreg == NUM_SHIFTERS-1) && (i >= 6)) {
          if (i == 6 && bitRead(scanin_p[shiftreg],i)){
            if (oct_transp > 0) {
              oct_transp -= bitRead(scanin_p[shiftreg],i);
            }
          }
          if (i == 7 && bitRead(scanin_p[shiftreg],i)) {
            if (oct_transp < OCT_MAX) {
              oct_transp += bitRead(scanin_p[shiftreg],i);
            }
          }
        } else {
          if (bitRead(scanin_p[shiftreg],i) && (latch_list[shiftreg*8+i] != oct_transp || latch_list[shiftreg*8+i] == 0xFF)) {
            MIDI.sendNoteOn(STARTNOTE+ OCTAVE*oct_transp + shiftreg*8+i, 127, 1);
          } else {
            if (num_latched_keys > 0 && latch_list[shiftreg*8+i] != 0xFF) {
              MIDI.sendNoteOff(STARTNOTE+ OCTAVE*latch_list[shiftreg*8+i] + shiftreg*8+i, 127, 1);
              num_latched_keys--;
              latch_list[shiftreg*8+i] = 0xFF;
            } else {
              MIDI.sendNoteOff(STARTNOTE+ OCTAVE*oct_transp + shiftreg*8+i, 127, 1);
            }
          }
        }
      }
    }
  }
}

void latchdata(int latchpin) {
  bitWrite(PORTB, PLATCH, 1);
  delayMicroseconds(20);
  bitWrite(PORTB, PLATCH, 0);
}
