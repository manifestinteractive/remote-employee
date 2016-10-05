// include SPI, MP3 and SD libraries
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>

// These are the pins used for the music maker shield
#define SHIELD_RESET  -1 // VS1053 reset pin (unused!)
#define SHIELD_CS     7  // VS1053 chip select pin (output)
#define SHIELD_DCS    6  // VS1053 Data/command select pin (output)

// These are common pins between breakout and shield
#define CARDCS 4 // Card chip select pin
#define DREQ 3   // VS1053 Data request, ideally an Interrupt pin

Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);
File root;

// Default Settings

const int sampleWindow = 50;        // sample window width in mS (50 mS = 20Hz)
const int numReadings = 10;         // how many samples to take for creating the average
long randNumber;                    // randomly Generated Number

unsigned int volume = 0;            // weird Volume stuff where the lower the number, the higher the volume ( yep, seriously )
unsigned int sample;                // last recorded sample
unsigned int readings[numReadings]; // the readings from the analog input
unsigned int readIndex = 0;         // the index of the current reading
unsigned int total = 0;             // the running total
unsigned int average = 0;           // the average
unsigned int talkTime = 0;          // counter to track how long person was talking
unsigned int talkTimeMin = 10;      // how long a person needs to talk before we play a clip
unsigned int talkThreshold = 40;    // the volume level expected for talking
unsigned int minWaitTime = 60;      // minimum time between playback
unsigned int curWaitTime = 0;       // current time between playback
unsigned int numMP3files = 0;

boolean initialized = false;        // disable actual audio playback for development
boolean mute = false;               // disable actual audio playback for development
boolean debug = false;              // use this to debug output in serial monitor
boolean debugLevels = false;        // whether to debug microphone levels
boolean startedTalking = false;     // person started talking above talkThreshold
boolean finishedTalking = false;    // person finished talking above talkThreshold
boolean waitCheck = false;          // should wait for a bit before next response played
boolean playClip = false;           // whether talking went on above talkThreshold longer than talkTimeMin
boolean playedClip = false;         // whether a clip was actually played

// Store Previously Played MP3 files

String prevFile1;
String prevFile2;
String prevFile3;
String prevFile4;
String prevFile5;

void setup() {
  Serial.begin(9600);

  if (! musicPlayer.begin()) {
    if (debug) Serial.println(F("Unable to detect MP3 Player.  Check wiring."));
    while (1);
  }

  SD.begin(CARDCS);

  musicPlayer.setVolume(volume, volume);
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);

  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }

  // Make "random" a little more random
  randomSeed(analogRead(0));

  // Get Count of Audio Files
  root = SD.open("/");
  countFiles(root);
}

void loop() {
  if (initialized) {
    unsigned long startMillis= millis();
    unsigned int peakToPeak = 0;
  
    unsigned int signalMax = 0;
    unsigned int signalMin = 1024;
  
    // collect data for 50 mS
    while (millis() - startMillis < sampleWindow) {    
      sample = analogRead(0);
      if (sample < 1024) {
        if (sample > signalMax) {
          signalMax = sample;
        } else if (sample < signalMin) {
          signalMin = sample;
        }
      }
    }
  
    peakToPeak = signalMax - signalMin;
  
    // generate false reading of low volume when playing audio clip
    if (!musicPlayer.stopped()) {
      peakToPeak = 0;
    }
  
    total = total - readings[readIndex];
    readings[readIndex] = peakToPeak;
    total = total + readings[readIndex];
    readIndex = readIndex + 1;
  
    if (readIndex >= numReadings) {
      readIndex = 0;
    }
  
    average = total / numReadings;
  
    // Check for starting a new conversation
    if (average >= talkThreshold && !startedTalking) {
      startedTalking = true;
      if (debug) Serial.println("Started Talking");
    }
  
    // Check that talking went on long enough to play a clip
    if (average >= talkThreshold && startedTalking && !finishedTalking && !playClip) {
      talkTime++;
  
      if (talkTime >= talkTimeMin) {
        playClip = true;
        if (debug) Serial.println("Talked Long Enough to Play Response");
      }
    }
  
    // Check that talking has finished
    if (startedTalking && !finishedTalking && average < talkThreshold && !waitCheck) {
      finishedTalking = true;
      if (debug) Serial.println("Finished Talking");
  
      if (playClip) {
        String playFile = randomFile(0);
        if (playFile.length() == 12) {
          char fileName[13];
          playFile.toCharArray(fileName, 13);
          playMP3(fileName);
        } else {
          if (debug) Serial.println("Invalid File Name");
          resetTalking();
        }      
      } else {
        resetTalking();
      }
    } else if (waitCheck) {
      if (curWaitTime < minWaitTime) {
        curWaitTime++;
        resetTalking();
      } else {
        curWaitTime = 0;
        waitCheck = false;
      }
    }
  
    // Check if we were playing an audio file
    if (finishedTalking && playClip && musicPlayer.stopped() && playedClip) {
      if (debug) Serial.println("Done Playing Response");
      waitCheck = true;
      resetTalking();
    } else if (finishedTalking && playClip && !musicPlayer.stopped() && !playedClip) {
      if (debug) Serial.println("Playing Response");
      playedClip = true;
    }
  
    if (debug && debugLevels) {
      Serial.print("talkTime: ");
      Serial.print(talkTime);
  
      Serial.print(", average: ");
      Serial.print(average);
  
      Serial.print(", playClip: ");
      Serial.print(playClip);
  
      Serial.print(", playedClip: ");
      Serial.print(playedClip);
  
      Serial.print(", startedTalking: ");
      Serial.print(startedTalking);
  
      Serial.print(", finishedTalking: ");
      Serial.print(finishedTalking);
  
      Serial.println("");
    }
  } else {
    if (debug) Serial.println("Initializing ...");
  }
}

/**
 * Reset Talking State
 */
void resetTalking() {
  talkTime = 0;
  startedTalking = false;
  finishedTalking = false;
  playClip = false;
  playedClip = false;
}

/**
 * Play MP3 File with Specified `fileName`
 */
void playMP3(char* fileName){
  musicPlayer.stopPlaying();
  if (debug) {
    Serial.print("playMP3: ");
    Serial.println(fileName);
  }
  
  if (SD.exists(fileName) && !mute) {
    musicPlayer.startPlayingFile(fileName);
  } else if(mute) {
    resetTalking();
    if (debug) Serial.println("Audio Muted");
  } else if(!SD.exists(fileName)) {
    resetTalking();
    if (debug) Serial.println("Missing Audio File");
  }
}

/**
 * Create a Random File Name based on `numMP3files` count 
 */
String randomFile (int attempt) {
  randNumber = random(1, numMP3files+1);

  String prefix = leftPad(randNumber);
  String suffix = ".mp3";
  String playFile = prefix + suffix;

  // add some checks to make sure we are not playing the same files over and over
  if (playFile != prevFile1 && playFile != prevFile2 && playFile != prevFile3 && playFile != prevFile4 && playFile != prevFile5) {
    prevFile5 = prevFile4;
    prevFile4 = prevFile3;
    prevFile3 = prevFile2;
    prevFile2 = prevFile1;
    prevFile1 = playFile;
    
    return playFile;
  } else if (attempt <= 5) {
    if (debug) Serial.println("Random Audio Attempt #" + attempt);
    return randomFile(attempt+1);
  } else if (attempt >= 5) {
    resetTalking();
    if (debug) Serial.println("Giving Up");
  }
}

/**
 * Pass over Random Number and pad with leading zeros
 */
char * leftPad (int num) {
  static char strOut[8];

  if (num >= 0 && num < numMP3files) {
    sprintf(strOut, "%08d", num);
  }

  return strOut;
}

/**
 * Count MP3 files in the root directory
 */
void countFiles(File dir) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      break;
    }

    if (!entry.isDirectory()) {      
      String fileName = entry.name();
      fileName.toLowerCase();

      if (fileName.length() == 12 && fileName.endsWith(".mp3")) {
        numMP3files++;
      }
    }

    entry.close();
  }

  initialized = true;
}
