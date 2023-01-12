#define LIN_OUT 1
#define FHT_N 128 // set to 256 point fht

#include <movingAvg.h>
#include <Adafruit_DotStar.h>
#include <SPI.h>
#include <FHT.h>
#include <TimerOne.h>

#define SERIAL_DEBUG true

#define FILTER_NUM_SAMPLES 10
#define FILTER_THRESH 1
movingAvg filter(FILTER_NUM_SAMPLES);

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
uint32_t ORANGE = 0xFF9900; //NOT REALLY ORANGE?
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

//////////////////////////////////////
//*********** FUNCTIONS ***********//
/////////////////////////////////////

void setup() {

#if SERIAL_DEBUG
  Serial.println("VOID_SETUP::START");
  Serial.begin(9600);
#endif

  // Setup Moving Average Filter to check if audio is playing
  filter.begin();

  // Setup digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  //Setup DOTSTARs
  #if defined(__AVR_ATtiny85__) && (F_CPU == 16000000L)
  clock_prescale_set(clock_div_1); // Enable 16 MHz on Trinket
  #endif

  strip.begin(); // Initialize pins for output
  strip.clear(); // Turn all LEDs off
  strip.show();  // Turn all LEDs off 

  //Setup FFT
  ADCSRA = 0xe7; // set the adc to free running mode
  ADMUX = 0x45; // use adc5
  DIDR0 = 0x20; // turn off the digital input for adc5

#if SERIAL_DEBUG
  Serial.println("VOID_SETUP::COMPLETE");
  Serial.begin(9600);
#endif
}

void loop() {

  startTime = millis();
  sampleInput();
  sampleFix();
  
  if (noAudioPlaying()){
    drawWaitPattern();
    digitalWrite(LED_BUILTIN, HIGH); //ON
  } else {
    drawSpectrum();
    digitalWrite(LED_BUILTIN, LOW); //OFF
  }

#if SERIAL_DEBUG
  Serial.print(sample[0]); Serial.print(",");
  Serial.print(sample[1]); Serial.print(",");
  Serial.print(sample[2]); Serial.print(",");
  Serial.print(sample[3]); Serial.print(",");
  Serial.println(sample[4]);
#endif

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


void drawWaitPattern(){
  for(int i = 0; i < NUMPIXELS; i++){
    if (i % 3 == 0){
      strip.setPixelColor(i, YELLOW);
    } else {
      strip.setPixelColor(i, BLUE);
    }
  }
  strip.show();
}

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
  
  return val < FILTER_THRESH;
  
}

//********************//
// SAMPLING FUNCTIONS //
//********************//

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
