/*
* Module01_ESP8266_Sensors.ino     
* --> Circuit using an ESP8266, a BMP280 module and a LDR
* --> Reads the sensors and publish in a MQTT server.
*/


// --> Required Libraries
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>




/*****************************************************
** Constants and global variables                   **
*****************************************************/
#define LDR_PIN A0
#define NETWORK_SSID "YOUR WI-FI NETWORK SSID"
#define NETWORK_PASS "YOUR WI-FI NETWORK PASSWORD"
#define MQTT_SERVER "192.168.0.7" //IP from RPi in the Wi-Fi network
#define MQTT_PORT 1883

#define BMP280_I2C_ADDRESS 0x76 //I2C address of the BMP280 module

//--> Global-Scope variables:
float temp, pressure;
unsigned int adc_ldr;
unsigned long lastMillis = 0;


// --> Objetos para controlar aos dispositivos usados aqui      
WiFiClient espClient; //--> Wi-Fi client
PubSubClient mqtt_client(espClient); //--> Configurar o servidor MQTT
Adafruit_BMP280 bmp; //Objeto para acessar os recursos do sensor BMP280:


// --> Function prototypes:
void connect_esp8266_wifi_network(char* network_ssid, char* network_pass);
void connect_mqtt_server( void );
void read_ldr_sensor( void );
void read_bmp280_sensors( void );




/******************
** setup()       ** 
******************/
void setup() {
      //Begin the Monitor Serial
      Serial.begin(115200); 

      // Initialize sensors:
      pinMode(LDR_PIN, INPUT); //LDR sensor
      bmp.begin(BMP280_I2C_ADDRESS); //BMP280 sensor module

      // --> Connect to the Wi-Fi Network (Using constants defined in global scope)
      connect_esp8266_wifi_network(NETWORK_SSID, NETWORK_PASS);

      // --> Set the MQTT server
      mqtt_client.setServer(MQTT_SERVER, MQTT_PORT);
      
      //--> Se conectar ao servidor MQTT
      connect_mqtt_server();
}




/****************
** loop()      **  
****************/
void loop() {
      mqtt_client.loop();
      delay(10); // <- fixes some issues with WiFi stability
      
      if(!mqtt_client.connected()) {
            connect_mqtt_server();
      }

      // Publicar mensagens a cada 10s.
      if( (millis() - lastMillis) > 10000 ){
             //--> Read the sensors
             read_ldr_sensor();
             read_bmp280_sensors();

             //--> Publish the sensor readings:
             mqtt_client.publish("sensors/temp", String(temp).c_str());
             delay(500);

             mqtt_client.publish("sensors/pressure", String(pressure).c_str());
             delay(500);

             mqtt_client.publish("sensors/adc_ldr", String(adc_ldr).c_str());
             delay(500);

             //--> Refresh 'lastMillis'
             lastMillis = millis();
      }
}






/****************************************
** connect_esp8266_wifi_network()      **
****************************************/
void connect_esp8266_wifi_network(char* network_ssid, char* network_pass){
      Serial.println("");
      // Connect to the Wi-Fi network
      WiFi.begin(network_ssid, network_pass);
      Serial.print("Connecting to ");
      Serial.println(network_ssid);
      while(WiFi.status() != WL_CONNECTED){ 
            Serial.print(".");
            delay(500);
      }
      Serial.println("\nConected!");
      // --> Show IP and MAC Address:
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      Serial.print("MAC Address: ");
      Serial.println(WiFi.macAddress());
      Serial.println("\n");
}



/****************************
** connect_mqtt_server()   **
****************************/
void connect_mqtt_server( void ){
      Serial.print("Checking Wi-Fi");
      while( WiFi.status() != WL_CONNECTED ) {
            Serial.print(".");
            delay(500);
      }

      while (!mqtt_client.connected()) {
            Serial.println("\nConnecting to the MQTT server...");
            if (mqtt_client.connect("sensors")){
                  Serial.print(F("Connected to the MQTT server in "));
                  Serial.print(MQTT_SERVER);
                  Serial.print(":");
                  Serial.println(MQTT_PORT);
            }
            else{
                  Serial.print("Fail in connecting to the MQTT server [rc = ");
                  Serial.print(mqtt_client.state());
                  Serial.println("]. Trying again in 5s.");
                  // Wait 5 seconds before retrying
                  delay(5000);
            }
      }
}




/****************************************
** read_ldr_sensor()                   **
****************************************/
void read_ldr_sensor( void ){
      adc_ldr = analogRead(LDR_PIN);
}


/****************************************
** read_bmp280_sensors()               **
****************************************/
void read_bmp280_sensors( void ){
      temp = bmp.readTemperature(); //Temperature in Celsius
      pressure = bmp.readPressure()/100.0; //pressure en hPa (in hundreds of Pascals)
}
