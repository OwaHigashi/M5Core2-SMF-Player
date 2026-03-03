/*
  M5Core2 Simple SMF Player

  A simple Standard MIDI File (SMF) player for M5Core2 using MD_MIDIFile library
  and UNIT-SYNTH for audio output.

  Features:
  - Plays SMF format 0 files from /smf folder on SD card
  - MIDI output to UNIT-SYNTH via Serial2 (pin 32)
  - Button controls: A=previous, B=play/stop, C=next
  - Display song info on M5Core2 screen
  - SD-Updater support

  Programmer: 尾和東@Pococha技術枠
*/

#include <M5Core2.h>
#include <SD.h>
#include <SPI.h>
#include "MD_MIDIFile.h"

// SD-Updater support
#include "M5StackUpdater.h"

// Constants
#define SMF_FOLDER "/smf"
#define MIDI_BAUD 31250
#define MIDI_SERIAL Serial2
#define MIDI_RX_PIN 32
#define MIDI_TX_PIN 33

// Globals
MD_MIDIFile SMF;
SDFAT sd;
char currentFile[256];
int currentTrackIndex = 0;
int totalTracks = 0;
bool isPlaying = false;
uint32_t lastDisplayUpdate = 0;
const uint32_t DISPLAY_UPDATE_INTERVAL = 100;  // Update display every 100ms

// Playlist storage
struct PlaylistEntry {
  char filename[256];
};
PlaylistEntry playlist[100];
int playlistCount = 0;

// ===== MIDI Callback Functions =====

/**
 * MIDI Event Callback
 * Called when a MIDI event is read from the SMF
 */
void midiEventHandler(midi_event *pev) {
  if (pev == nullptr) return;

  // Prepare MIDI message: status byte + data bytes
  uint8_t message[4];
  uint8_t messageSize = pev->size + 1;

  if (messageSize > 4) messageSize = 4;  // Safety limit

  // Status byte: 0x90 + channel for the command
  message[0] = pev->data[0];

  // Copy data bytes
  for (int i = 0; i < pev->size && (i + 1) < messageSize; i++) {
    message[i + 1] = pev->data[i + 1];
  }

  // Send MIDI message to UNIT-SYNTH
  for (int i = 0; i < messageSize; i++) {
    MIDI_SERIAL.write(message[i]);
  }
}

/**
 * SYSEX Event Callback
 * Called when a SYSEX event is read from the SMF
 */
void sysexEventHandler(sysex_event *pev) {
  if (pev == nullptr) return;

  // Send SYSEX message to UNIT-SYNTH
  // SysEx format: 0xF0 + data bytes + 0xF7
  MIDI_SERIAL.write(0xF0);

  for (int i = 0; i < pev->size; i++) {
    MIDI_SERIAL.write(pev->data[i]);
  }

  MIDI_SERIAL.write(0xF7);
}

/**
 * META Event Callback
 * Called when a META event is read from the SMF
 */
void metaEventHandler(const meta_event *pev) {
  // META events typically contain metadata like track names, tempo changes, etc.
  // For now we just ignore them
  if (pev == nullptr) return;
}

// ===== File System Functions =====

/**
 * Scan /smf folder and build playlist
 */
void scanForMIDIFiles() {
  playlistCount = 0;

  // Change to SMF folder
  if (!sd.chdir(SMF_FOLDER)) {
    M5.Lcd.println("Failed to open /smf folder");
    delay(2000);
    return;
  }

  // Open folder
  SdFile folder;
  if (!folder.open(SMF_FOLDER, O_RDONLY)) {
    M5.Lcd.println("Cannot open /smf folder");
    delay(2000);
    return;
  }

  SdFile entry;
  char filename[256];

  // Iterate through files in folder
  while (entry.openNext(&folder, O_RDONLY)) {
    // Get filename
    entry.getName(filename, sizeof(filename));

    // Check if it's a file (not a directory)
    if (!entry.isDir()) {
      // Check file extension (.mid or .smf)
      int len = strlen(filename);
      if ((len > 4 && (strcmp(&filename[len - 4], ".mid") == 0 ||
                       strcmp(&filename[len - 4], ".MID") == 0)) ||
          (len > 4 && (strcmp(&filename[len - 4], ".smf") == 0 ||
                       strcmp(&filename[len - 4], ".SMF") == 0))) {

        // Add to playlist
        if (playlistCount < 100) {
          snprintf(playlist[playlistCount].filename, sizeof(playlist[playlistCount].filename),
                   "%s/%s", SMF_FOLDER, filename);
          playlistCount++;
        }
      }
    }
    entry.close();
  }

  folder.close();
}

/**
 * Load and play a file from playlist
 */
void loadTrack(int index) {
  if (index < 0 || index >= playlistCount) return;

  // Stop current playback
  isPlaying = false;
  if (SMF.isOpen()) {
    SMF.close();
  }

  // Load new file
  currentTrackIndex = index;
  strncpy(currentFile, playlist[index].filename, sizeof(currentFile) - 1);

  int err = SMF.load(currentFile);
  if (err != MD_MIDIFile::E_OK) {
    M5.Lcd.printf("Error loading %s: %d\n", currentFile, err);
    return;
  }

  // Send "All Notes Off" to all channels at startup
  for (uint8_t ch = 0; ch < 16; ch++) {
    MIDI_SERIAL.write(0xB0 | ch);  // Control Change
    MIDI_SERIAL.write(0x7B);        // All Notes Off
    MIDI_SERIAL.write(0x00);
  }
}

/**
 * Play current track
 */
void playTrack() {
  if (!SMF.isOpen()) return;
  isPlaying = true;
  SMF.restart();
}

/**
 * Stop playback
 */
void stopTrack() {
  if (!isPlaying) return;

  isPlaying = false;
  SMF.pause(true);

  // Send "All Notes Off" to all channels
  for (uint8_t ch = 0; ch < 16; ch++) {
    MIDI_SERIAL.write(0xB0 | ch);  // Control Change
    MIDI_SERIAL.write(0x7B);        // All Notes Off
    MIDI_SERIAL.write(0x00);
  }
}

/**
 * Next track
 */
void nextTrack() {
  int nextIndex = currentTrackIndex + 1;
  if (nextIndex >= playlistCount) nextIndex = 0;

  loadTrack(nextIndex);
}

/**
 * Previous track
 */
void previousTrack() {
  int prevIndex = currentTrackIndex - 1;
  if (prevIndex < 0) prevIndex = playlistCount - 1;

  loadTrack(prevIndex);
}

// ===== Display Functions =====

/**
 * Update display with current track info
 */
void updateDisplay() {
  uint32_t now = millis();
  if (now - lastDisplayUpdate < DISPLAY_UPDATE_INTERVAL) {
    return;
  }
  lastDisplayUpdate = now;

  // Clear screen
  M5.Lcd.fillScreen(BLACK);

  // Draw header
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.println("SMF Player");

  // Draw current track info
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(10, 50);
  M5.Lcd.printf("Track: %d / %d\n", currentTrackIndex + 1, playlistCount);

  // Extract filename from full path for display
  const char *displayName = currentFile;
  const char *lastSlash = strrchr(currentFile, '/');
  if (lastSlash != nullptr) {
    displayName = lastSlash + 1;
  }

  M5.Lcd.setCursor(10, 70);
  M5.Lcd.setTextColor(YELLOW, BLACK);
  M5.Lcd.printf("File: %s\n", displayName);

  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setCursor(10, 90);
  if (isPlaying) {
    M5.Lcd.println("Status: PLAYING");
    M5.Lcd.setTextColor(GREEN, BLACK);
  } else {
    M5.Lcd.println("Status: STOPPED");
    M5.Lcd.setTextColor(RED, BLACK);
  }

  // Draw control info
  M5.Lcd.setTextColor(CYAN, BLACK);
  M5.Lcd.setCursor(10, 130);
  M5.Lcd.println("Controls:");
  M5.Lcd.setCursor(10, 150);
  M5.Lcd.println("A: Previous");
  M5.Lcd.setCursor(10, 170);
  M5.Lcd.println("B: Play/Stop");
  M5.Lcd.setCursor(10, 190);
  M5.Lcd.println("C: Next");
}

// ===== Button Handlers =====

/**
 * Button A pressed - Previous track
 */
void onButtonAPressed() {
  previousTrack();
}

/**
 * Button B pressed - Play/Stop toggle
 */
void onButtonBPressed() {
  if (!SMF.isOpen()) return;

  if (isPlaying) {
    stopTrack();
  } else {
    playTrack();
  }
}

/**
 * Button C pressed - Next track
 */
void onButtonCPressed() {
  nextTrack();
}

// ===== Main Setup and Loop =====

void setup() {
  // Initialize M5Core2
  M5.begin();

  // Clear display
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.println("SMF Player");
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(10, 50);
  M5.Lcd.println("Initializing...");

  // Initialize SD card
  if (!SD.begin(4, SPI, 40000000)) {
    M5.Lcd.setCursor(10, 70);
    M5.Lcd.println("SD init failed!");
    while (1) delay(100);
  }

  // Initialize SdFat for MD_MIDIFile
  if (!sd.begin(4, SPI_FULL_SPEED)) {
    M5.Lcd.setCursor(10, 70);
    M5.Lcd.println("SdFat init failed!");
    while (1) delay(100);
  }

  // Initialize MIDI Serial port
  MIDI_SERIAL.begin(MIDI_BAUD, SERIAL_8N1, MIDI_RX_PIN, MIDI_TX_PIN);

  // Initialize MD_MIDIFile library
  SMF.begin(&sd);

  // Set up callbacks
  SMF.setMidiHandler(midiEventHandler);
  SMF.setSysexHandler(sysexEventHandler);
  SMF.setMetaHandler(metaEventHandler);

  // Scan for MIDI files
  M5.Lcd.setCursor(10, 70);
  M5.Lcd.println("Scanning for files...");
  scanForMIDIFiles();

  M5.Lcd.setCursor(10, 90);
  M5.Lcd.printf("Found %d file(s)\n", playlistCount);
  delay(1000);

  // Load first track
  if (playlistCount > 0) {
    loadTrack(0);
  }

  // SD-Updater support
  // Check if update button is pressed on startup
  if (digitalRead(BUTTON_A_PIN) == LOW) {
    SD_Updater::updateFromFS(SD);
    ESP.restart();
  }
}

void loop() {
  // Handle button presses
  M5.update();

  if (M5.BtnA.wasPressed()) {
    onButtonAPressed();
  }
  if (M5.BtnB.wasPressed()) {
    onButtonBPressed();
  }
  if (M5.BtnC.wasPressed()) {
    onButtonCPressed();
  }

  // Update display
  updateDisplay();

  // Process MIDI events if playing
  if (isPlaying && SMF.isOpen()) {
    if (SMF.getNextEvent()) {
      // Event was processed
      if (SMF.isEOF()) {
        // End of file reached
        isPlaying = false;
      }
    }
  }

  delay(10);  // Small delay to prevent overwhelming the system
}
