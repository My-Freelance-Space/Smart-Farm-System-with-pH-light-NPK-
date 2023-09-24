#include <SoftwareSerial.h>
#include <Wire.h>

// RS485 module
#define RE 32
#define DE 33
byte n_val,p_val,k_val;
const byte nitro[] = {0x01, 0x03, 0x00, 0x1e, 0x00, 0x01, 0xe4, 0x0c};
const byte phos[] = {0x01, 0x03, 0x00, 0x1f, 0x00, 0x01, 0xb5, 0xcc};
const byte pota[] = {0x01, 0x03, 0x00, 0x20, 0x00, 0x01, 0x85, 0xc0};
byte NPK_response[11];

SoftwareSerial mod(18, 19);

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


byte readSensor_NPK(const byte request[], size_t requestSize, const char* nutrient) {
  digitalWrite(DE, HIGH);
  digitalWrite(RE, HIGH);
  delay(10);
  if (mod.write(request, requestSize) == requestSize) {
    digitalWrite(DE, LOW);
    digitalWrite(RE, LOW);
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
  float phVoltage = phAvg * (5.0 / 4095.0);
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
  if (n_val < 50 || p_val < 50 || k_val < 50) {
    digitalWrite(relay_NPK, LOW);
  } else {
    digitalWrite(relay_NPK, HIGH);
  }

    //relay pH
  digitalWrite(relay_pH, (ph_val >= 5 && ph_val <= 7));

}


void setup() {
  Serial.begin(115200);
  mod.begin(4800);
  pinMode(RE, OUTPUT);
  pinMode(DE, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  pinMode(relay_NPK,OUTPUT);
  pinMode(relay_pH,OUTPUT);
  delay(500);
}

void loop() {
  // Soil Sensor
  n_val = readSensor_NPK(nitro, sizeof(nitro), "Nitrogen");
  delay(500);
  p_val = readSensor_NPK(phos, sizeof(phos), "Phosphorous");
  delay(500);
  k_val = readSensor_NPK(pota, sizeof(pota), "Potassium");
  delay(500);

  // Photo Sensor
  readSensor_photo();

  // pH Sensor
  readSensor_pH();


  //Relay checking
  relay();


  delay(1000);
}