#include <LiquidCrystal.h> // write to display

#include <WiFi.h> // connect to wifi

#include <JSON_Decoder.h> // parse weather json

#include <OpenWeather.h> // weather info

#include <Time.h> // time conversion

#include <PDM.h> // microphone

#define TIME_OFFSET (-6 * 3600) // UTC -6 hours

// ##### neccesary weather info ##### //

String api_key = "dcf7e97fc58e21a9de091ad61593a246";
String units = "imperial";  // or "metric"
String language = "en";   // See notes tab

// Set both your longitude and latitude to at least 4 decimal places
String latitude =  "33.4504"; // 90.0000 to -90.0000 negative for Southern hemisphere
String longitude = "-88.8184"; // 180.000 to -180.000 negative for West

#define MAX_DAYS 1 // keep in mind, 8 forecasts per day. 
OW_Weather ow; // Weather forecast library instance


// ##### Forecast Data Storage ##### //
struct ForecastData {
  String dt[MAX_DAYS*8];
  String temp[MAX_DAYS*8];
  String temp_min[MAX_DAYS*8];
  String temp_max[MAX_DAYS*8];
  String humidity[MAX_DAYS*8];
  String clouds_all[MAX_DAYS*8];
  String wind_speed[MAX_DAYS*8];
  String visibility[MAX_DAYS*8];
  String main[MAX_DAYS*8];
  String description[MAX_DAYS*8];
};

ForecastData forecastArray;


// ##### LCD Info #####//

// Initialize Constants
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2; // LED interface pins
int Contrast = 65; // Display contrast amount
LiquidCrystal lcd(rs, en, d4, d5, d6, d7); // initialize LCD

// ##### Wifi Information ##### //
#define WIFI_SSID     "Redpoint Starkville"
#define WIFI_PASSWORD "w48txg2n"


// ##### Microphone Information ##### //
bool LED_SWITCH = false;

// default number of output channels
static const char channels = 1;

// default PCM output frequency
static const int frequency = 16000;

// Buffer to read samples into, each sample is 16-bits
short sampleBuffer[512];

// Number of audio samples read
volatile int samplesRead;



// set text to screen
void set_text(int row, String message, bool clear_screen = false){
  if (clear_screen){
    lcd.clear();
  }else{
    lcd.setCursor(0, row);
    lcd.print("                          "); // clear message
  }
  
  lcd.setCursor(0, row);
  lcd.print(message);
}

int time_since_update = 0;
void populate_weather_info(){
  //check time
  int current_time = (millis()/1000);
  if (time_since_update == 0){
    time_since_update = current_time;
  }else{
    int time_difference = time_since_update - current_time;

    if (time_difference <= (45*60)){ // 45 minutes (time is in seconds)
      // return here because weather is up to date
      // and we have a limited number of calls per day
      return ;
    } else{
      time_since_update = current_time;
    }
  }// continue after checking the timer

  int roundedTemp, roundedTempMin, roundedTempMax; // store the forecast values
  int loop_counter = 0;

  set_text(0,"Getting Forecast ...", true); // set text to LCD

  do{
    Serial.println("GETTING NEW FORECAST");
    setForecast(); // get the forecast
    get_curr_min_max(roundedTemp, roundedTempMin, roundedTempMax); // get the current, min, and max values

    // keep track of number of loops to ensure it is properly looping ( error checking on screen)
    if (loop_counter >= 1){
      set_text(1, "Retrying: " + String(loop_counter));
    }
    loop_counter += 1;

  }while(!check_valid_forecast(roundedTemp, roundedTempMin, roundedTempMax)); // if all 0's, invalid forecast. Redo. 

  set_text(0,"Curr:" + String(roundedTemp) + ":[" + String(roundedTempMin) + "," + String(roundedTempMax) + "]     " );
  set_text(1,forecastArray.description[1] + "            " );
  

}


void screen_power(bool option){
  if (option){ // true
    //turn backlight on
    digitalWrite(7, HIGH);
    // turn on display
    lcd.display();
    // turn on led
    digitalWrite(LEDB, HIGH); 
  }else{
    // turn backlight off
    digitalWrite(7, LOW);
    // turn off display
    lcd.noDisplay();
    // turn led off
    digitalWrite(LEDB, LOW);
  }

}

bool check_valid_forecast(int roundedTemp, int roundedTempMin, int roundedTempMax){
  // if all are 0, invalid forecast
  if (roundedTemp==0 && roundedTempMin==0 && roundedTempMax==0){
    Serial.println("INVALID FORECAST: " + String(roundedTemp) + ", "+ String(roundedTempMin) + ", "+ String(roundedTempMax) );
    return false;
  }
  return true; // real forecast. return true
}

void get_curr_min_max(int &roundedTemp, int &roundedTempMin, int &roundedTempMax) {
  int lowestTemp = forecastArray.temp[0].toFloat(); // Assuming the first entry is the lowest initially
  int highestTemp = forecastArray.temp[0].toFloat(); // Assuming the first entry is the highest initially

  for (int i = 0; i < MAX_DAYS * 8; i++) { // 8 hour forecast for each day. 
    // compare and then output each to ensure it is working properly. 
    int currentTemp = forecastArray.temp[i].toFloat();
    Serial.println("Lowest of : " + String(lowestTemp) + " " + String(currentTemp));
    lowestTemp = min(lowestTemp, currentTemp);
    Serial.println("Highest of : " + String(highestTemp) + " " + String(currentTemp));
    highestTemp = max(highestTemp, currentTemp);
  }

  roundedTemp = static_cast<int>(round(forecastArray.temp[1].toFloat())); // current temp
  roundedTempMin = static_cast<int>(round(lowestTemp));
  roundedTempMax = static_cast<int>(round(highestTemp));
}

// get forecast
void setForecast()
{
  // Create the structures that hold the retrieved weather
  OW_forecast  *forecast = new OW_forecast;

  Serial.print("\nRequesting weather information from OpenWeather... ");

  ow.getForecast(forecast, api_key, latitude, longitude, units, language); //  get whole forecast

  Serial.println("Weather from OpenWeather\n");

  Serial.print("city_name           : "); Serial.println(forecast->city_name);
  Serial.print("sunrise             : "); Serial.println(strTime(forecast->sunrise));
  Serial.print("sunset              : "); Serial.println(strTime(forecast->sunset));
  Serial.print("Latitude            : "); Serial.println(ow.lat);
  Serial.print("Longitude           : "); Serial.println(ow.lon);
  Serial.print("Timezone            : "); Serial.println(forecast->timezone);
  Serial.println();

  if (forecast)
  {
    Serial.println("###############  Forecast weather  ###############\n");
    for (int i = 0; i < (MAX_DAYS * 8 ); i++)
    {
      Serial.print("3 hourly forecast   "); if (i < 10) Serial.print(" "); Serial.print(i);
      Serial.println();
      Serial.print("dt (time)        : "); Serial.print(strTime(forecast->dt[i]));

      Serial.print("temp             : "); Serial.println(forecast->temp[i]);
      Serial.print("temp.min         : "); Serial.println(forecast->temp_min[i]);
      Serial.print("temp.max         : "); Serial.println(forecast->temp_max[i]);

      Serial.print("humidity         : "); Serial.println(forecast->humidity[i]);

      Serial.print("clouds           : "); Serial.println(forecast->clouds_all[i]);
      Serial.print("wind_speed       : "); Serial.println(forecast->wind_speed[i]);

      Serial.print("visibility       : "); Serial.println(forecast->visibility[i]);
      Serial.println();

      Serial.print("main             : "); Serial.println(forecast->main[i]);
      Serial.print("description      : "); Serial.println(forecast->description[i]);

      Serial.println();

      Serial.print("###############  Storing Forecast Weather  ###############\n");

      // Store forecast information in the global array of ForecastData
      forecastArray.dt[i] = strTime(forecast->dt[i]);
      forecastArray.temp[i] = String(forecast->temp[i]);
      forecastArray.temp_min[i] = String(forecast->temp_min[i]);
      forecastArray.temp_max[i] = String(forecast->temp_max[i]);
      forecastArray.humidity[i] = String(forecast->humidity[i]);
      forecastArray.clouds_all[i] = String(forecast->clouds_all[i]);
      forecastArray.wind_speed[i] = String(forecast->wind_speed[i]);
      forecastArray.visibility[i] = String(forecast->visibility[i]);
      forecastArray.main[i] = String(forecast->main[i]);
      forecastArray.description[i] = String(forecast->description[i]);
      Serial.print("###############  Stored Successfully  ###############\n");

    }

    
  }
  // Delete to free up space and prevent fragmentation as strings change in length
  delete forecast;
}

//Convert unix time to a time string
String strTime(time_t unixTime)
{
  unixTime += TIME_OFFSET; // time offset to central time.
  return ctime(&unixTime);
}

//Callback function to process the data from the PDM microphone.
void onPDMdata() {
  // Query the number of available bytes
  int bytesAvailable = PDM.available();

  // Read into the sample buffer
  PDM.read(sampleBuffer, bytesAvailable);

  // 16-bit, 2 bytes per sample
  samplesRead = bytesAvailable / 2;
}


void setup() {
  Serial.begin(9600); // start serial

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  analogWrite(6,Contrast);
  pinMode(7, OUTPUT); // set power pin
  screen_power(true);


  // Connect to Wi-Fi
  set_text(0, "Connecting WIFI");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected\n");

  // mic setup
  pinMode(LEDB, OUTPUT); // led for mic visual
  //while (!Serial);

  set_text(0, "Init Mic");
  // Configure the data receive callback
  PDM.onReceive(onPDMdata);
  if (!PDM.begin(channels, frequency)) {
    Serial.println("Failed to start PDM!");
    set_text(1, "FAILED");
  }
}

void loop() {

  // Wait for samples to be read
  if (samplesRead) {
    for (int i = 0; i < samplesRead; i++) {
      
      if (sampleBuffer[i] > 2000 || sampleBuffer[i] <= -2000) {

        // turn lcd on
        screen_power(true);
        // set weather 
        populate_weather_info();

        // wait 30s before turning off
        delay(1 * 30 * 1000); // min, sec, msec
        screen_power(false);
      
      }
    }
    // Clear the read count
    samplesRead = 0;
  }
}