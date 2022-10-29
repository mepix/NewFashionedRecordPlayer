#define LIN_OUT 1
#define FHT_N 128 // set to 256 point fht

#include <movingAvg.h>
#include <Adafruit_DotStar.h>
#include <SPI.h>
#include <FHT.h>
#include <TimerOne.h>

#define SERIAL_DEBUG true

#define NUM_FILTER_SAMPLES 100
movingAvg filter(NUM_FILTER_SAMPLES);

#define NUMPIXELS 101 // Number of LEDs in the strip
#define DATAPIN 4 // From ADAFRUIT standard
#define CLOCKPIN 5 // From ADAFRUIT standard

Adafruit_DotStar strip = Adafruit_DotStar(
  NUMPIXELS, DATAPIN, CLOCKPIN, DOTSTAR_BRG);

byte sample[64];
unsigned long startTime, endTime, oldTime;
String sampleSet;
int displaySize = 5;

#define DISPLAYS_ACROSS 1
#define DISPLAYS_DOWN 1

//Bits are GRB
uint32_t RED = 0x00FF00;
uint32_t GREEN = 0xCC0000;
//uint32_t ORANGE = 0xFF9900; NOT REALLY ORANGE?
uint32_t BLUE = 0x0000FF;
uint32_t YELLOW = 0xFFFF00;

///////////////////////////////////
//*********** GLOBALS ***********//
///////////////////////////////////
//Column Strips [Bottom, Top]
int Col1[2] = {16,0};
int Col2[2] = {66,50};
int Col3[2] = {67,83};
int Col4[2] = {100,84};
int Col5[2] = {25,41};

//Row Strips [Left, Right]
int RowB[2] = {17,24};
int RowT[2] = {49,42};

/////////////////////////???/////////
//*********** FUNCTIONS ***********//
////////////////////////////??///////

void setup() {

#if SERIAL_DEBUG
  Serial.begin(9600);
#endif

  //Setup Moving Average Filter to check if audio is playing
  filter.begin();
  
  //Setup DOTSTARs
  #if defined(__AVR_ATtiny85__) && (F_CPU == 16000000L)
  clock_prescale_set(clock_div_1); // Enable 16 MHz on Trinket
  #endif

  strip.begin(); // Initialize pins for output
  strip.show();  // Turn all LEDs off 

  //Setup FFT
  ADCSRA = 0xe7; // set the adc to free running mode
  ADMUX = 0x45; // use adc5
  DIDR0 = 0x20; // turn off the digital input for adc5
}

void loop() {
//  drawCol(Col1);
//  drawCol(Col2);
//  drawCol(Col3);
//  drawCol(Col4);
//  drawCol(Col5);
//  delay(20);

  startTime = millis();
  sampleInput();
  sampleFix();
  if (noAudioPlaying()){
    drawWaitPattern();
  } else {
    drawSpectrum();  
  }
 // delay(200);
  endTime = millis();
}

void drawSpectrum(){
  strip.clear();
  drawColLength(Col1, 0);
  drawColLength(Col2, 1);
  drawColLength(Col3, 2);
  drawColLength(Col4, 3);
  drawColLength(Col5, 4);  
}

void drawColLength(int col[], int num){
  
  if (col[0] > col[1]) //If bottom pixel ID > top pixel ID
  { //16,15,14,...,0 REVERSE ORDER
    int idBot = col[0];
    int idTop = col[1] + (16-sample[num]);
    if(idTop < col[1]){
        idTop = col[1];
    }
    for(; idBot > idTop; idBot--)
    {
      strip.setPixelColor(idBot, BLUE);
      //strip.show();
    }
    strip.setPixelColor(idBot, RED);
    if (abs(col[0]) - abs(idBot) > 1)
    {
      strip.setPixelColor(idBot+1, YELLOW);    
    }
    //strip.show();
  }
  else //bottom pixel ID < top pixel ID
  { //
    int idBot = col[0];
    int idTop = col[1] - (16-sample[num]);
    if(idTop > col[1])
        idTop = col[1];
    for(; idBot < idTop; idBot++)
    {
      strip.setPixelColor(idBot, BLUE);
      //strip.show();
    }
    strip.setPixelColor(idBot, RED);
    if (abs(idBot) - abs(col[0]) > 1)
    {
      strip.setPixelColor(idBot-1, YELLOW);      
    }
    //strip.show();
  }
  strip.show();
}

unsigned long timeSinceNoSample = 0;
unsigned long timeNoAudio = 0;

bool noAudioPlaying(){

  // scan the sample bins and sum the levels
  int lvlCount = 0;
  for (int n = 0; n < displaySize; n++){
    lvlCount += sample[n];
  }

  // add the bin capacity to the filter
  int val = filter.reading(lvlCount);

#if SERIAL_DEBUG
  Serial.println(val, DEC);
#endif
  
  return val < 1;
  
}

bool noAudioPlaying2(){
  static unsigned long lastTime = 0;
  static unsigned long noAudioTime = 0;
  unsigned long nowTime = millis();
  bool noAudio = true;

  // Check if ALL sample bins are low -> no audio playing
  for (int n = 0; n < displaySize; n++){
    if (sample[n] > 1){
      // there must be audio playing
//      noAudioTime = 0;
      return false;
    }
  }
  return true;

  // There is some noise that occurs within signal
  // We need to check that there has been a long
  // enough pause to indicate that that music has
  // realy stopped

  // Update the timer if it is the first time here
  if (noAudioTime == 0){
    noAudioTime = millis(); 
  }

  //TODO: there is something wrong with this timer part
  
  if (noAudioTime - millis() > 5000){
    return true;
  } else {
    return false;
  }

}

void drawWaitPattern(){
  for (int n = 0; n < NUMPIXELS; n++){
        strip.setPixelColor(n, GREEN);
  }
  strip.show();
}

void drawCol(int col[]){
  if (col[0] > col[1])
  {
    int n = col[0];
    for(; n > col[1]; n--)
    {
      strip.setPixelColor(n, GREEN);
      strip.show();
    }
    strip.setPixelColor(n, RED);
    strip.show();
  }
  else
  {
    int n = col[0];
    for(; n < col[1]; n++)
    {
      strip.setPixelColor(n, GREEN);
      strip.show();
    }
    strip.setPixelColor(n, RED);
    strip.show();
  }
  
}

void sampleInput() {
  cli();  // UDRE interrupt slows this way down on arduino1.0
  for (int x=0; x<FHT_N; x++) {
    while(!(ADCSRA & 0x10)); // wait for adc to be ready
    ADCSRA = 0xf5; // restart adc
    if (sampleSet == "L") {
      ADMUX = 0x45; // use adc5
    } else {
      ADMUX = 0x44; // use adc4
    }
    byte m = ADCL; // fetch adc data
    byte j = ADCH;
    int k = (j << 8) | m; // form into an int
    k -= 0x0200; // form into a signed int
    k <<= 6; // form into a 16b signed int
    fht_input[x] = k; // put real data into bins
  }
  sei();
  fht_window(); // window the data for better frequency response
  fht_reorder(); // reorder the data before doing the fht
  fht_run(); // process the data in the fht
  fht_mag_lin();

}

void sampleFix() {

  int newPos; 
  float fhtCount, tempY;
  for (int x=0; x < displaySize; x++) {
    fhtCount = FHT_N/2;
    newPos = x * (fhtCount / displaySize); // single channel half-display 15-LED wide
    tempY = fht_lin_out[newPos]; 
    sample[x] = ((tempY/256)*17); // single channel full 16 LED high
    }
} 
