#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <vector>
#include <algorithm>
#include <numeric> 

// WiFi and Blynk credentials
const char* ssid  = "Tuna";
const char* pass = "0956344472";
const char* auth = "FkfQVS4--9AHFmY7CBuOkfenV641PSbq";


// RS485 module
#define RE 32
#define DE 33
byte n_val,p_val,k_val;
const byte nitro[] = {0x01, 0x03, 0x00, 0x1e, 0x00, 0x01, 0xe4, 0x0c};
const byte phos[] = {0x01, 0x03, 0x00, 0x1f, 0x00, 0x01, 0xb5, 0xcc};
const byte pota[] = {0x01, 0x03, 0x00, 0x20, 0x00, 0x01, 0x85, 0xc0};
byte NPK_response[11];

SoftwareSerial mod(18, 19); //RO,DI

// Photo Sensor
const int ledPin2 = 2;
const int photo_pin = 34;
int photo_val = 0;
const int photo_cutoff_val = 500;

// pH Sensor
const int ph_pin = 35;
float phTot, temTot;
float phAvg, temAvg;
float ph_val;
int x;
float C = 25.85;
float m = -6.80;


// Relay Pin
const int relay_NPK = 16;
const int relay_pH = 17;
bool relay_NPK_blynk = false;
bool relay_pH_blynk = false;

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Set the LCD I2C address to 0x27 for a 16x2 display
bool showNPK = true;


//Blynk Listening

BLYNK_WRITE(V5) {
    int blynkMaualRelayButton = param.asInt();  
    if (blynkMaualRelayButton == 0 ){
        relay_NPK_blynk = false;
    }
    else{
        relay_NPK_blynk = true;
    }
}

BLYNK_WRITE(V6) {
    int blynkMaualRelayButton = param.asInt();  
    if (blynkMaualRelayButton == 0 ){
        relay_pH_blynk = false;
    }
    else{
        relay_pH_blynk = true;
    }
}

void update_blynk(void* pvParameters){

    while(true){

    Blynk.run();
    String npk_val_string = String(n_val) + "_" + String(p_val) + "_" + String(k_val) ;
    Blynk.virtualWrite(V0,npk_val_string);
    Blynk.virtualWrite(V1,photo_val);
    Blynk.virtualWrite(V2, ph_val);
    vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


byte readSensor_NPK(const byte request[], size_t requestSize, const char* nutrient) {
  digitalWrite(DE, HIGH);
  digitalWrite(RE, HIGH);
  delay(10);
  if (mod.write(request, requestSize) == requestSize) {
    digitalWrite(DE, LOW);
    digitalWrite(RE, LOW);
    delay(10); // Give some time for the response
    for (byte i = 0; i < 7; i++) {
      NPK_response[i] = mod.read();
      Serial.print(NPK_response[i], HEX);
    }
    Serial.println();
    Serial.print(nutrient);
    Serial.print(": ");
    Serial.print(NPK_response[4]);
    Serial.println(" mg/kg");
    return NPK_response[4];
  }
  return 0;
}

#define NUM_READINGS 10

// Overload calculateMedian to handle both int and float vectors
float calculateMedian(std::vector<int>& data) {
  size_t size = data.size();
  if (size == 0) {
    return 0;  // Undefined, really.
  } else {
    sort(data.begin(), data.end());
    if (size % 2 == 0) {
      return (data[size / 2 - 1] + data[size / 2]) / 2;
    } else {
      return data[size / 2];
    }
  }
}

float calculateMedian(std::vector<float>& data) {
  size_t size = data.size();
  if (size == 0) {
    return 0;  // Undefined, really.
  } else {
    sort(data.begin(), data.end());
    if (size % 2 == 0) {
      return (data[size / 2 - 1] + data[size / 2]) / 2;
    } else {
      return data[size / 2];
    }
  }
}

float calculateMAD(std::vector<int>& data, float median) {
  std::vector<float> absDeviations;
  for (int value : data) {
    absDeviations.push_back(abs(value - median));
  }
  return calculateMedian(absDeviations);
}

void readSensor_NPK_avg() {
  std::vector<int> n_readings, p_readings, k_readings;

  for (int i = 0; i < NUM_READINGS; i++) {
    n_readings.push_back(readSensor_NPK(nitro, sizeof(nitro), "Nitrogen"));
    delay(50);
    p_readings.push_back(readSensor_NPK(phos, sizeof(phos), "Phosphorous"));
    delay(50);
    k_readings.push_back(readSensor_NPK(pota, sizeof(pota), "Potassium"));
    delay(50);
  }

  float n_median = calculateMedian(n_readings);
  float p_median = calculateMedian(p_readings);
  float k_median = calculateMedian(k_readings);

  float n_mad = calculateMAD(n_readings, n_median);
  float p_mad = calculateMAD(p_readings, p_median);
  float k_mad = calculateMAD(k_readings, k_median);

  std::vector<int> n_filtered, p_filtered, k_filtered;
  for (int value : n_readings) {
    if (abs(value - n_median) <= 2 * n_mad) {
      n_filtered.push_back(value);
    }
  }
  for (int value : p_readings) {
    if (abs(value - p_median) <= 2 * p_mad) {
      p_filtered.push_back(value);
    }
  }
  for (int value : k_readings) {
    if (abs(value - k_median) <= 2 * k_mad) {
      k_filtered.push_back(value);
    }
  }

  n_val = std::accumulate(n_filtered.begin(), n_filtered.end(), 0) / n_filtered.size();
  p_val = std::accumulate(p_filtered.begin(), p_filtered.end(), 0) / p_filtered.size();
  k_val = std::accumulate(k_filtered.begin(), k_filtered.end(), 0) / k_filtered.size();
}


void readSensor_photo(){

  photo_val = analogRead(photo_pin);
  Serial.print("Photo sensor Value = ");
  Serial.println(photo_val);
  if (photo_val < photo_cutoff_val) {
    digitalWrite(ledPin2, HIGH);
  } else {
    digitalWrite(ledPin2, LOW);
  }
  delay(100);

}

void readSensor_pH(){

  phTot = 0;
  for(x = 0; x < 10; x++) {
    phTot += analogRead(ph_pin);
    delay(10);
  }
  phAvg = phTot / 10;
  float phVoltage = phAvg * (3.3 / 4095);
  ph_val = phVoltage * m + C;
  Serial.print("phVoltage = ");
  Serial.print(phVoltage);
  Serial.print(" ");
  Serial.print("pH=");
  Serial.println(ph_val);


  delay(1000);

}

void relay(){

    //relay NPK

  if (relay_NPK_blynk){
    if (n_val < 50 || p_val < 50 || k_val < 50) {
      // digitalWrite(relay_NPK, HIGH);
      digitalWrite(relay_NPK, LOW);
    }
  }
  else{
      digitalWrite(relay_NPK, HIGH);

  }

    //relay pH
  if (relay_pH_blynk){
    digitalWrite(relay_pH, (ph_val >= 5 && ph_val <= 7));
  }
  else{
    digitalWrite(relay_pH, HIGH);
    
  }


}


void taskDisplay(void* pvParameters){

    while(true){


        lcd.clear(); // Clear the LCD before displaying new values

        if (showNPK) {
            // Display NPK values
            lcd.setCursor(0, 0);
            lcd.print("N:");
            lcd.print(n_val);
            lcd.print("    P:");
            lcd.print(p_val);
            lcd.setCursor(0, 1);
            lcd.print("K:");
            lcd.print(k_val);
        } else {
            // Display pH and Photo values
            lcd.setCursor(0, 0);
            lcd.print("pH: ");
            lcd.print(ph_val, 2);  // Display with 2 decimal points
            lcd.setCursor(0, 1);
            lcd.print("Photo: ");
            lcd.print(photo_val);
        }
        
        // Toggle the display flag for next call
        showNPK = !showNPK;
        vTaskDelay(pdMS_TO_TICKS(5000));

    }
}



void taskMain(void* pvParameters){

    while(true){
    // Read and average sensor values
      readSensor_NPK_avg();

      // Photo Sensor
      readSensor_photo();

      // pH Sensor
      readSensor_pH();

      //Relay checking
      relay();
      
      vTaskDelay(pdMS_TO_TICKS(1000));

    }
}


void setup() {
  Serial.begin(115200);
  mod.begin(4800);
  pinMode(RE, OUTPUT);
  pinMode(DE, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  pinMode(relay_NPK,OUTPUT);
  pinMode(relay_pH,OUTPUT);
  digitalWrite(relay_NPK, HIGH);
  digitalWrite(relay_pH, HIGH);
  

  // Setup LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // Setup Wifi
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
  }
  
  Blynk.begin(auth, ssid, pass);


    //using concurrent program
  xTaskCreatePinnedToCore(taskDisplay, "taskDisplay", 1024*8, NULL, 1, NULL,0);
  xTaskCreatePinnedToCore(taskMain, "taskMain", 1024*8, NULL, 1, NULL,1);
  xTaskCreatePinnedToCore(update_blynk, "update Blynk", 1024*8, NULL, 1, NULL,0);
  delay(500);



  
}

void loop() {

  delay(1000);
}