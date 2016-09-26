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

// Default Settings
int volume = 0;
const int sampleWindow = 50;      // Sample window width in mS (50 mS = 20Hz)
unsigned int sample;

const int numReadings = 10;       // how many samples to take for creating the average
long randNumber;
int readings[numReadings];        // the readings from the analog input
int readIndex = 0;                // the index of the current reading
int total = 0;                    // the running total
int average = 0;                  // the average

int talkTime = 0;                 // counter to track how long person was talking
int talkTimeMin = 5;             // how long a person needs to talk before we play a clip
int talkThreshold = 40;           // the volume level expected for talking

boolean debug = false;            // use this to debug output in serial monitor
boolean startedTalking = false;   // person started talking above talkThreshold
boolean finishedTalking = false;  // person finished talking above talkThreshold
boolean playClip = false;         // whether talking went on above talkThreshold longer than talkTimeMin
boolean playedClip = false;       // whether a clip was actually played

void setup() {
  Serial.begin(9600);

  if (! musicPlayer.begin()) {
    Serial.println(F("Unable to detect MP3 Player.  Check wiring."));
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
}

void loop() {
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
    Serial.println("Started Talking");
  }

  // Check that talking went on long enough to play a clip
  if (average >= talkThreshold && startedTalking && !finishedTalking && !playClip) {
    talkTime++;

    if (talkTime >= talkTimeMin) {
      playClip = true;
      Serial.println("Play Clip");
    }
  }

  // Check that talking has finished
  if (startedTalking && !finishedTalking && average < talkThreshold) {
    finishedTalking = true;
    Serial.println("Finished Talking");

    if (playClip) {
      randNumber = random(10, 25);
      String prefix = "voice_";
      String suffix = ".mp3";
      String stringOne = prefix + randNumber;
      String stringTwo = stringOne + suffix;
      
      char fileName[13];
      stringTwo.toCharArray(fileName, 13);
      playMP3(fileName);
    } else {
      resetTalking();
    }
  }

  // Check if we were playing an audio file
  if (finishedTalking && playClip && musicPlayer.stopped() && playedClip) {
    Serial.println("Done playing clip");
    resetTalking();
  } else if (finishedTalking && playClip && !musicPlayer.stopped() && !playedClip) {
    Serial.println("Playing Clip");
    playedClip = true;
  }

  if (debug) {
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
}

void resetTalking() {
  talkTime = 0;
  startedTalking = false;
  finishedTalking = false;
  playClip = false;
  playedClip = false;
  Serial.println("Reset Talking");
}

void changeVolume(int change){
  musicPlayer.stopPlaying();
  int newVolume = volume + change;
  volume = constrain(newVolume, 0, 100);
  musicPlayer.setVolume(volume, volume);
  playMP3("volclick.mp3");
}

void playMP3(char* fileName){
  musicPlayer.stopPlaying();
  musicPlayer.startPlayingFile(fileName);
}

String read() {
  while ( !Serial.available());

  String str = "";
  while (Serial.available()) {
      str += (char) Serial.read();
      delay(1);
  }
  return str;
}
