#include <M5Core2.h>
// for SD-Updater
#define SDU_ENABLE_GZ
#include <M5StackUpdater.h>

// LovyanGFX関連削除

#include "MD_MIDIFile.h"

// #define CORE1
#define CORE2

HardwareSerial MIDI_SERIAL(2);

const uint16_t WAIT_DELAY = 2000; // ms

#define MAX_SONGS 200
#define MAX_FILENAME_LENGTH 32

SdFat mySD;
MD_MIDIFile SMF;

char songFilenames[MAX_SONGS][MAX_FILENAME_LENGTH]; 
int songCount = 0;
char *currentFilename = NULL;
int playdataCnt = 0;

#define SPI_SPEED SD_SCK_MHZ(25)
#define TFCARD_CS_PIN 4
#define SD_CONFIG SdSpiConfig(TFCARD_CS_PIN, SHARED_SPI, SPI_SPEED)

char *makeFilename(int seq)
{
  if (songCount == 0) {
    return NULL;
  }
  playdataCnt += seq;
  if (playdataCnt >= songCount) {
    playdataCnt = 0;
  } else if (playdataCnt < 0) {
    playdataCnt = songCount - 1;
  }
  return songFilenames[playdataCnt];
}

void scanSongs()
{
  songCount = 0;
  FsFile root = mySD.open("/smf");
  if (!root || !root.isDirectory())
  {
    M5.Lcd.println("Failed to open /smf folder or /smf is not a directory");
    return;
  }

  FsFile entry;
  while (entry.openNext(&root, O_RDONLY))
  {
    if (!entry.isDir())
    {
      char filename[MAX_FILENAME_LENGTH];
      entry.getName(filename, sizeof(filename));
      String filenameStr(filename);
      if (filenameStr.endsWith(".mid") || filenameStr.endsWith(".MID") ||
          filenameStr.endsWith(".smf") || filenameStr.endsWith(".SMF"))
      {
        strncpy(songFilenames[songCount], filename, MAX_FILENAME_LENGTH);
        songFilenames[songCount][MAX_FILENAME_LENGTH - 1] = '\0';
        Serial.println(filename);
        songCount++;
        if (songCount >= MAX_SONGS) break;
      }
    }
    entry.close();
  }
  root.close();
}

void midiCallback(midi_event *pev)
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
{
  // No action required
}

void midiSilence(void)
{
  midi_event ev;
  ev.size = 0;
  ev.data[ev.size++] = 0xb0;
  ev.data[ev.size++] = 120;
  ev.data[ev.size++] = 0;

  for (ev.channel = 0; ev.channel < 16; ev.channel++)
    midiCallback(&ev);
}

void tickMetronome(void)
{
  // Optional
}

enum PlayState {
  S_STOPPED,
  S_PLAYING,
  S_PAUSE,
  S_WAITING,
  S_ERROR
};
PlayState state = S_STOPPED;
bool isEOFReached = false;

void backscreen() {
  M5.Lcd.setTextFont(0);
  for (int chd = 1; chd <= 16; chd++)
  {
    int y = 49 + chd * 10;
    M5.Lcd.setTextColor(WHITE, BLACK);
    M5.Lcd.drawNumber(chd, 4, y + 1, 1);
    M5.Lcd.drawFastHLine(2, y - 1, 316, 0xF660);
    M5.Lcd.fillRect(18, y, 300, 9, TFT_DARKGREY);
    M5.Lcd.setTextColor(TFT_BLACK, TFT_DARKGREY);
    for (int oct = 0; oct < 11; ++oct)
    {
      int x = 18 + oct * 28;
      for (int n = 0; n < 7; ++n)
      {
        M5.Lcd.drawFastVLine(x + n * 4 + 3, y, 9, TFT_BLACK);
      }
      M5.Lcd.fillRect(x + 2, y, 3, 5, TFT_BLACK);
      M5.Lcd.fillRect(x + 6, y, 3, 5, TFT_BLACK);
      M5.Lcd.fillRect(x + 14, y, 3, 5, TFT_BLACK);
      M5.Lcd.fillRect(x + 18, y, 3, 5, TFT_BLACK);
      M5.Lcd.fillRect(x + 22, y, 3, 5, TFT_BLACK);
    }
  }
  M5.Lcd.drawFastVLine(16, 58, 161, 0xF660);
  M5.Lcd.drawRect(1, 58, 318, 161, 0xF660);
}

void updateScreen()
{
  static String last_filename = "";
  static int last_status = -1;

  if ((last_filename != String(currentFilename)) || (last_status != state))
  {
    M5.Lcd.fillScreen(TFT_BLACK);
    backscreen();
    M5.Lcd.setTextFont(4);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

    M5.Lcd.setCursor(5, 0);
    M5.Lcd.println(currentFilename);
    M5.Lcd.setCursor(5, 27);
    M5.Lcd.print(F("Status:"));
    switch (state)
    {
    case S_ERROR:
      Serial.println("ERROR");
      M5.Lcd.println(F("File load failed."));
      break;
    case S_STOPPED:
      Serial.println("STOP");
      M5.Lcd.println(F("stop."));
      M5.Lcd.fillRect(260, 5, 40, 40, TFT_WHITE);
      break;
    case S_PLAYING:
      Serial.println("PLAY");
      M5.Lcd.println(F("playing."));
      M5.Lcd.fillRect(280, 5, 30, 40, TFT_BLACK);
      M5.Lcd.fillTriangle(280, 5, 280, 45, 310, 25, TFT_YELLOW);
      backscreen();
      break;
    case S_PAUSE:
      Serial.println("PAUSE");
      M5.Lcd.println(F("pause."));
      break;
    case S_WAITING:
      Serial.println("WAIT");
      M5.Lcd.println(F("wait."));
      break;
    default:
      break;
    }

    last_filename = String(currentFilename);
    last_status = state;
  }
}

void startPlaying()
{
  if (currentFilename == NULL || songCount == 0) {
    M5.Lcd.println("No songs to play.");
    state = S_ERROR;
    updateScreen();
    return;
  }

  char filepath[256];
  snprintf(filepath, sizeof(filepath), "/smf/%s", currentFilename);
  
  int err = SMF.load(filepath);
  if (err != MD_MIDIFile::E_OK) {
    M5.Lcd.printf("Failed to load %s\n", currentFilename);
    state = S_ERROR;
    updateScreen();
    return;
  }
  SMF.setMidiHandler(midiCallback);
  SMF.setSysexHandler(sysexCallback);
  isEOFReached = false;
  state = S_PLAYING;
  M5.Lcd.printf("Playing: %s\n", currentFilename);
  updateScreen();
}

void stopPlaying()
{
  if (state == S_PLAYING) {
    SMF.close();
    midiSilence();
    state = S_STOPPED;
    M5.Lcd.println("Stopped.");
  }
  updateScreen();
}

void setup(void)
{
  M5.begin();
  Serial.begin(115200);

  checkSDUpdater( SD, MENU_BIN, 5000 );

  Serial.println("SMF Player");

  MIDI_SERIAL.begin(31250, SERIAL_8N1, -1, 32);

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.println(F("Initializing SD card..."));
  if (!mySD.begin(SD_CONFIG)) {
    M5.Lcd.println(F("Card failed, or not present"));
    delay(2000);
    return;
  }
  M5.Lcd.println(F("Card initialized."));
  delay(2000);

  M5.Lcd.println(F("Scanning SMF files..."));
  scanSongs();
  if (songCount == 0) {
    M5.Lcd.println("No SMF files found.");
    return;
  }

  currentFilename = makeFilename(0);
  if (currentFilename == NULL) {
    M5.Lcd.println("No SMF files to play.");
    return;
  }

  M5.Lcd.printf("Ready. Current: %s\n", currentFilename);

  backscreen();
}

void loop(void)
{
  M5.update();

  // Aボタン: 前の曲へ
  if (M5.BtnA.wasPressed()) {
    if (state == S_PLAYING) {
      stopPlaying();
      currentFilename = makeFilename(-1);
      startPlaying();  // 再生中なら次曲も再生
    } else {
      currentFilename = makeFilename(-1);
      // 停止中なら再生開始しない
      updateScreen();
    }
  }

  // Bボタン: 再生・停止トグル
  if (M5.BtnB.wasPressed()) {
    if (state == S_PLAYING) {
      stopPlaying();
    } else {
      startPlaying();
    }
  }

  // Cボタン: 次の曲へ
  if (M5.BtnC.wasPressed()) {
    if (state == S_PLAYING) {
      stopPlaying();
      currentFilename = makeFilename(1);
      startPlaying();  // 再生中なら次曲も再生
    } else {
      currentFilename = makeFilename(1);
      // 停止中なら再生開始しない
      updateScreen();
    }
  }

  if (state == S_PLAYING) {
    if (!SMF.isEOF()) {
      if (SMF.getNextEvent()) {
        tickMetronome();
      }
    } else {
      // EOFに達したら自動的に次の曲再生へ
      stopPlaying();
      currentFilename = makeFilename(1);
      startPlaying();
    }
  }

  updateScreen();
}
