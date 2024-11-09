// Test playing a succession of MIDI files from the SD card.
// Example program to demonstrate the use of the MIDFile library
// Just for fun light up a LED in time to the music.
//
// Hardware required:
//  SD card interface - change SD_SELECT for SPI comms
//  3 LEDs (optional) - to display current status and beat.
//  Change pin definitions for specific hardware setup - defined below.

#include <M5Stack.h>
#include <SdFat.h>
#include "MD_MIDIFile.h"

//#define CORE2
#define CORE1

HardwareSerial MIDI_SERIAL(2); // UART2 を使用

const uint16_t WAIT_DELAY = 2000; // ms

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

// The files in the tune list should be located on the SD card
// or an error will occur opening the file and the next in the
// list will be opened (skips errors).
const char *tuneList[] =
{
  "LOOPDEMO.MID",  // simplest and shortest file
  "BANDIT.MID",
  "ELISE.MID",
  "TWINKLE.MID",
  "GANGNAM.MID",
  "FUGUEGM.MID",
  "POPCORN.MID",
  "AIR.MID",
  "PRDANCER.MID",
  "MINUET.MID",
  "FIRERAIN.MID",
  "MOZART.MID",
  "FERNANDO.MID",
  "SONATAC.MID",
  "SKYFALL.MID",
  "XMAS.MID",
  "GBROWN.MID",
  "PROWLER.MID",
  "IPANEMA.MID",
  "JZBUMBLE.MID",
};

SDFAT	mySD;
MD_MIDIFile SMF;

void midiCallback(midi_event *pev)
// Called by the MIDIFile library when a file event needs to be processed
// thru the midi communications interface.
// This callback is set up in the setup() function.
{
  if ((pev->data[0] >= 0x80) && (pev->data[0] <= 0xe0))
  {
    MIDI_SERIAL.write(pev->data[0] | pev->channel);
    MIDI_SERIAL.write(&pev->data[1], pev->size - 1);
  }
  else
  {
    MIDI_SERIAL.write(pev->data, pev->size);
  }
}

void sysexCallback(sysex_event *pev)
// Called by the MIDIFile library when a system Exclusive (sysex) file event needs
// to be processed through the midi communications interface. Most sysex events cannot
// really be processed, so we just ignore it here.
// This callback is set up in the setup() function.
{
  // No action required
}

void midiSilence(void)
// Turn everything off on every channel.
// Some midi files are badly behaved and leave notes hanging, so between songs turn
// off all the notes and sound
{
  midi_event ev;

  // All sound off
  // When All Sound Off is received all oscillators will turn off, and their volume
  // envelopes are set to zero as soon as possible.
  ev.size = 0;
  ev.data[ev.size++] = 0xb0;
  ev.data[ev.size++] = 120;
  ev.data[ev.size++] = 0;

  for (ev.channel = 0; ev.channel < 16; ev.channel++)
    midiCallback(&ev);
}

// in the main file define the max SPI speed
#define SPI_SPEED SD_SCK_MHZ(25)                             // MHz: OK 4, 10, 20, 25  ->  too much: 29, 30, 40, 50 causes errors
#define TFCARD_CS_PIN 4
#define SD_CONFIG SdSpiConfig(TFCARD_CS_PIN, SHARED_SPI, SPI_SPEED) // TFCARD_CS_PIN is defined in M5Stack Config.h (Pin 4)

void setup(void)
{

#ifdef CORE2
  MIDI_SERIAL.begin(31250, SERIAL_8N1, -1, 32); // Core2 MIDI 出力をピン32で初期化
#elif CORE1
  MIDI_SERIAL.begin(31250, SERIAL_8N1, -1, 21); // Core1 MIDI 出力をピン32で初期化
#endif

  // Initialize SD
  if (!mySD.begin(SD_CONFIG))
  {
    while (true) ;
  }

  // Initialize MIDIFile
  SMF.begin(&mySD);
  SMF.setMidiHandler(midiCallback);
  SMF.setSysexHandler(sysexCallback);
}

void tickMetronome(void)
// flash a LED to the beat
{
  static uint32_t lastBeatTime = 0;
  static boolean  inBeat = false;
  uint16_t  beatTime;

  beatTime = 60000 / SMF.getTempo();    // msec/beat = ((60sec/min)*(1000 ms/sec))/(beats/min)
  if (!inBeat)
  {
    if ((millis() - lastBeatTime) >= beatTime)
    {
      lastBeatTime = millis();
      inBeat = true;
    }
  }
  else
  {
    if ((millis() - lastBeatTime) >= 100)	// keep the flash on for 100ms only
    {
      inBeat = false;
    }
  }
}

void loop(void)
{
  static enum { S_IDLE, S_PLAYING, S_END, S_WAIT_BETWEEN } state = S_IDLE;
  static uint16_t currTune = ARRAY_SIZE(tuneList);
  static uint32_t timeStart;

  switch (state)
  {
    case S_IDLE:    // now idle, set up the next tune
    {
      int err;

      currTune++;
      if (currTune >= ARRAY_SIZE(tuneList))
        currTune = 0;

      // use the next file name and play it
      err = SMF.load(tuneList[currTune]);
      if (err != MD_MIDIFile::E_OK)
      {
        timeStart = millis();
        state = S_WAIT_BETWEEN;
      }
      else
      {
        state = S_PLAYING;
      }
    }
    break;

    case S_PLAYING: // play the file
      if (!SMF.isEOF())
      {
        if (SMF.getNextEvent())
          tickMetronome();
      }
      else
        state = S_END;
      break;
    case S_END:   // done with this one
      SMF.close();
      midiSilence();
      timeStart = millis();
      state = S_WAIT_BETWEEN;
      break;

    case S_WAIT_BETWEEN:    // signal finished with a dignified pause
      if (millis() - timeStart >= WAIT_DELAY)
        state = S_IDLE;
      break;

    default:
      state = S_IDLE;
      break;
  }
}
