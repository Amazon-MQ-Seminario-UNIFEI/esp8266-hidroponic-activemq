#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <string.h>
#include <EEPROM.h>
#include <LiquidCrystal_I2C.h> // display LCD lib
#include <BH1750.h> // brightness sensor lib
#include <Wire.h>

LiquidCrystal_I2C lcd(0x27, 16, 2); 
BH1750 lightMeter(0x23); 

const char* ssid = ""; //your wifi id
const char* password = ""; //your wifi password
const char* brokerUrl = ""; 
const int brokerPort = 8883; //mqtt port
const char* mqttUsername = "";
const char* mqttPassword = "";
const char* mqttClientId = "";

//topicos
const char* topicStream = "esp8266_01/var_stream";                 // publisher
const char* topicPh = "esp8266_01/ph_set";                         // subscriber
const char* topicTemperature = "esp8266_01/temp_set";              // subscriber

//definition of global variables
float ph, ph_target;
int temperature, temperatureMin, temperatureMax, brightness;



WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

void setup() {

  //Initialize the I2C bus (BH1750 library doesn't do this automatically)
  Wire.begin(D4, D3);
  
  // begin of BH1750 returns a boolean that can be used to detect setup problems.
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 Advanced begin"));
  } else {
    Serial.println(F("Error initialising BH1750"));
  }
  
  lcd.init();                 
  lcd.backlight();
  //posiciona o cursor para escrita
  //.setCursor(coluna, linha)
  lcd.setCursor(0,0);
  lcd.print("---Hidroponia---");
  delay(10000);
  
  EEPROM.begin(512); // inicializate EEPROM with the size of 512 bytes
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    lcd.clear();
    lcd.print("Connecting to");
    lcd.setCursor(0,1);
    lcd.print(ssid);
  }

  Serial.println("Connected to WiFi");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("WiFi Connected");
  delay(5000);
  lcd.clear();
  Serial.print("device local IP: ");
  lcd.print("device local IP:");
  Serial.println(WiFi.localIP());
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP());
  lcd.setCursor(0,0);
  delay(5000);
  lcd.clear();
  
  espClient.setInsecure(); // SSL is off. Amazon MQ shoud be automatically assigned by AWS.
  mqttClient.setServer(brokerUrl, brokerPort);
  mqttClient.setCallback(callback);

  while (!mqttClient.connected()) {
    if (mqttClient.connect(mqttClientId, mqttUsername, mqttPassword)) {
      Serial.println("Connected to MQTT broker");
      lcd.print("Connected to");
      lcd.setCursor(0,1);
      lcd.print("MQTT broker");
      // subscribing to the topics
      mqttClient.subscribe(topicPh); 
      mqttClient.subscribe(topicTemperature);
    } else {
      lcd.clear();
      lcd.print("Failed to");
      lcd.setCursor(0,1);
      lcd.print("connect broker");
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqttClient.state());
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Retrying in 3s.");
      delay(3000);
    }
  }
  lcd.clear();
  float aux0 = 5;
  int aux1 = 5;
  EEPROM.put(0, aux0);
  EEPROM.put(20, aux1);
  EEPROM.put(40, aux1);
  EEPROM.commit();
  //recovering values from the last session
 
  EEPROM.get(0, ph_target);
  EEPROM.get(20, temperatureMin);
  EEPROM.get(40, temperatureMax);
  
  Serial.println("[EEPROM VALUES] ph_target = " + String(ph_target));
  Serial.println("[EEPROM VALUES] temperatureMin = " + String(temperatureMin));
  Serial.println("[EEPROM VALUES] temperatureMax = " + String(temperatureMax));
}

void loop() {
   mqttClient.loop();
   // publish messages every 30 seconds.
   static unsigned long lastMillis = 0;
   unsigned long currentMillis = millis();
   if (currentMillis - lastMillis >= 30000) {
      int lux = lightMeter.readLightLevel();
      Serial.print("Light: ");
      Serial.print(lux);
      int potenciometer = (analogRead(A0));
      Serial.print("\nPotenciometer: ");
      Serial.print(potenciometer);
      Serial.println("\n\nSending values to the server...");
      lastMillis = currentMillis;
      float ph = random(3, 8);
      float ph_target = 6.8;
      int temperature = random(20, 30);
      int temperatureMin = 19;
      int temperatureMax = 26;
      int brightness = random(50, 588);
      lcd.clear();
      lcd.print("pH:"+String(ph)+" tg:"+String(ph_target));
      lcd.setCursor(0, 1);
      lcd.print("T:"+String(temperature)+"C tg:"+String(temperatureMin)+"~"+String(temperatureMax));
      String aux;
      aux.concat("{\"amb_id\": \"100\", \"temp_atual\": \"");
      aux.concat(temperature);
      aux.concat("\", \"temp_min\": \"");
      aux.concat(temperatureMin);
      aux.concat("\", \"temp_max\": \"");
      aux.concat(temperatureMax);
      aux.concat("\", \"ph_atual\": \"");
      aux.concat(ph);
      aux.concat("\", \"ph_target\": \"");
      aux.concat(ph_target);
      aux.concat("\", \"lum_atual\": \"");
      aux.concat(brightness);
      aux.concat("\"}");
      Serial.println("JSON GERADO: " + aux);

      //sending it in a JSON format content
      //mqttClient.publish(topicStream, aux.c_str());
   }
}

// receiving the answers and printing them.
void callback(char* topic, byte* payload, unsigned int length) {
  String response = "";
  String strTopic = String(topic);

  String strTemp = String(topicTemperature);
  String strPh = String(topicPh);
  
  for (int i = 0; i < length; i++) {
    response += (char)payload[i];
  }
  
  if(strPh.equals(strTopic)){
    float ph = atof(response.c_str());
    Serial.println("[ pH ] Definindo novo valor para: " + String(ph));
    EEPROM.put(0, ph);
    EEPROM.commit();
    float eepromPh;
    Serial.println("[ pH ] Gravado valor na EEPROM: " + String(EEPROM.get(0, eepromPh)));
  }
  if(strTemp.equals(strTopic)){
    int temp1, temp2;

    int posicaoEspaco = response.indexOf(' ');
    if (posicaoEspaco != -1) {
      String parte1 = response.substring(0, posicaoEspaco);
      String parte2 = response.substring(posicaoEspaco + 1);
    
      temp1 = parte1.toInt();
      temp2 = parte2.toInt();
      Serial.println("[ temperature ] temperatura 1: " + String(temp1) + "\n[ temperature ] temperatura 2: " + String(temp2)); 
    }
    int aux1, aux2;
    Serial.println("[ temperature ] Definindo novo valor para: " + String(temp1) + " " + String(temp2));
    EEPROM.put(20, temp1);
    EEPROM.commit();
    Serial.println("[ temperature ] Gravado valor na EEPROM: " + String(EEPROM.get(1, aux1)));
    
    EEPROM.put(40, temp2);
    EEPROM.commit();
    Serial.println("[ temperature ] Gravado valor na EEPROM: " + String(EEPROM.get(2, aux2)));
  }
 
  //printing the topic and the content
  Serial.println('[' + topic + ']');
  Serial.println(response);
}
