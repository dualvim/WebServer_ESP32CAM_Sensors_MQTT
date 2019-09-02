/*
 * Module02_ESP32CAM_WebServer.ino
 * --> Runs the web server showing the last picture taken and the sensor readings published in the MQTT server
 * 
*/


/*****************************
** Required libraries       **
*****************************/
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "fd_forward.h"
#include "fr_forward.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "dl_lib.h"
#include "driver/rtc_io.h"
#include "WiFi.h"
#include <NTPClient.h>
#include <ESPAsyncWebServer.h>
#include <StringArray.h>
#include <SPIFFS.h>
#include <FS.h>
#include "PAG_WEB.h" //Header Filewith the HTML code of the web server page
#include <PubSubClient.h>




/*******************************************************************************
** Constantes referentes aos pinos da camera conectados internamente no ESP32 **
*******************************************************************************/
// --> Pins from the OV2640 camera module (CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1 // RESET pin is not available
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26 //SDA pin - 'GPIO 26'
#define SIOC_GPIO_NUM     27 //SCL pin - 'GPIO 26'

#define Y9_GPIO_NUM       35 //D7 pin - 'GPIO 35'
#define Y8_GPIO_NUM       34 //D6 pin - 'GPIO 34'
#define Y7_GPIO_NUM       39 //D5 pin - 'GPIO 39'
#define Y6_GPIO_NUM       36 //D4 pin - 'GPIO 36'
#define Y5_GPIO_NUM       21 //D3 pin - 'GPIO 21'
#define Y4_GPIO_NUM       19 //D2 pin - 'GPIO 19'
#define Y3_GPIO_NUM       18 //D1 pin - 'GPIO 18'
#define Y2_GPIO_NUM        5 //D0 pin - 'GPIO 5'
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22




/*****************************************************
** Constants and global variables                   **
*****************************************************/
#define INTERVAL 5 * 60 * 1000 // Interval between the pictures: 5 min
#define NETWORK_SSID "YOUR WI-FI NETWORK SSID"
#define NETWORK_PASS "YOUR WI-FI NETWORK PASSWORD"
#define MQTT_SERVER "192.168.0.7" //IP from RPi in the Wi-Fi network
#define MQTT_PORT 1883


/* --> Time-Zone adjustment (in seconds):
   --> Difference, in seconds, between the hour in this time-zone and the time in Greenwich
   --> Brasilia TZ (GMT -3) = -3600 * 3 = -10800 */
#define TZ_ADJUST -10800
// --> Nome do arquivo onde serao inseridos os dados
#define FILE_PHOTO "/Photo.jpg"


//--> Global-Scope variables:
camera_config_t CONFIG_CAM; // OV2640 camera configuration
unsigned long lastMillis = 0;
String formattedDate;
String dayStamp;
String timeStamp;
String strTemp = "XX", strPressure = "XX", strADCldr = "XX", strLastMessage = "";


// --> Objects from the classes available in the imported header files
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP); //NTP client:
AsyncWebServer server(8888); // Server on port 8888. The Ngrok service doesn't work in port '80'
WiFiClient espClient; 
PubSubClient mqtt_client(espClient); //--> MQTT Server


// --> Function prototypes:
void connect_esp32_wifi_network( void );
void ov2640_camera_module_configurations( void );
void initialize_spiffs( void );
bool verify_picture_taken( fs::FS &fs );
void take_picture_save_spiffs( void );
void getTimeStamp( void );
String processor(const String& var);
void connect_mqtt_server( void );
void messageReceived(char* topic, byte* payload, unsigned int length);
void write_received_message(String topicName, byte* payload, unsigned int length);
void call_take_picture_functions( void );





/******************
** setup()       **
******************/
void setup() {
      delay(5000);
      // Turn-off the 'browout detector'
      WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

      // --> Flash pin
      pinMode(4, OUTPUT);
      digitalWrite(4, LOW);

      // --> Monitor Serial
      Serial.begin(115200);
      Serial.println("Initializing...");
      
      // --> SPIFFS:
      initialize_spiffs();

      // --> OV2640 camera module
      ov2640_camera_module_configurations();

      // --> Connect to the Wi-Fi Network
      connect_esp32_wifi_network();

      //--> Iniiialize the NTP client
      Serial.print("Initializing the NTP (Network Time Protocol) client...");
      timeClient.begin();
      timeClient.setTimeOffset(TZ_ADJUST);
      Serial.println("Ok!");

      // --> Set-up the MQTT server
      mqtt_client.setServer(MQTT_SERVER, MQTT_PORT);

      // --> Define the 'messageReceived()' funtion as the callback function
      mqtt_client.setCallback(messageReceived);

      //--> Connect to the MQTT server
      connect_mqtt_server();

      // --> Define the routes for the asynchronous web server:
      server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
            request->send_P(200, "text/html", index_html, processor);
      });

      // Last picture taken:
      server.on("/photo_cam", HTTP_GET, [](AsyncWebServerRequest * request) {
            request->send(SPIFFS, FILE_PHOTO, "image/jpg");
      });

      // Date:
      server.on("/date_photo", HTTP_GET, [](AsyncWebServerRequest * request) {
            request->send_P(200, "text/plain", formattedDate.c_str());
      });

      // Hour:
      server.on("/hour_photo", HTTP_GET, [](AsyncWebServerRequest * request) {
            request->send_P(200, "text/plain", timeStamp.c_str());
      });

      // Temperature:
      server.on("/temp", HTTP_GET, [](AsyncWebServerRequest * request) {
            request->send_P(200, "text/plain", strTemp.c_str());
      });

      // Pressure:
      server.on("/pressure", HTTP_GET, [](AsyncWebServerRequest * request) {
            request->send_P(200, "text/plain", strPressure.c_str());
      });

      // ADC from the LDR:
      server.on("/adc_ldr", HTTP_GET, [](AsyncWebServerRequest * request) {
            request->send_P(200, "text/plain", strADCldr.c_str());
      });

      // --> Initialize the asyncronous web server
      server.begin();

      Serial.println("Ok!\n");

      //--> First call to 'call_take_picture_functions()'
      call_take_picture_functions();
}




/****************
** loop()      **
****************/
void loop() {
      mqtt_client.loop();
      delay(10); // <- fixes some issues with WiFi stability


      if ( !mqtt_client.connected() ) {
            connect_mqtt_server();
      }

      //--> Take a picture every 'INTERVAL' seconds
      if ( (millis() - lastMillis) > INTERVAL ) {
            call_take_picture_functions();
            lastMillis = millis(); // Refresh 'lastMillis'
      }
}



/****************************************
** connect_esp32_wifi_network()        **
****************************************/
void connect_esp32_wifi_network( void ) {
      // Connect to the Wi-Fi network
      WiFi.begin(NETWORK_SSID, NETWORK_PASS);
      Serial.print("Connecting to ");
      Serial.println(NETWORK_SSID);
      while (WiFi.status() != WL_CONNECTED) {
            Serial.print(".");
            delay(500);
      }
      Serial.println("\nConected!");
      // Show IP and MAC Address:
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      Serial.print("MAC Address: ");
      Serial.println(WiFi.macAddress());
      Serial.println("\n");
}





/****************************************************
** ov2640_camera_module_configurations()           **
****************************************************/
void ov2640_camera_module_configurations( void ) {
      Serial.print("Inicializing the OV2640 camera module...");

      // --> OV2640 camera configuration
      CONFIG_CAM.ledc_channel = LEDC_CHANNEL_0;
      CONFIG_CAM.ledc_timer = LEDC_TIMER_0;
      CONFIG_CAM.pin_d0 = Y2_GPIO_NUM;
      CONFIG_CAM.pin_d1 = Y3_GPIO_NUM;
      CONFIG_CAM.pin_d2 = Y4_GPIO_NUM;
      CONFIG_CAM.pin_d3 = Y5_GPIO_NUM;
      CONFIG_CAM.pin_d4 = Y6_GPIO_NUM;
      CONFIG_CAM.pin_d5 = Y7_GPIO_NUM;
      CONFIG_CAM.pin_d6 = Y8_GPIO_NUM;
      CONFIG_CAM.pin_d7 = Y9_GPIO_NUM;
      CONFIG_CAM.pin_xclk = XCLK_GPIO_NUM;
      CONFIG_CAM.pin_pclk = PCLK_GPIO_NUM;
      CONFIG_CAM.pin_vsync = VSYNC_GPIO_NUM;
      CONFIG_CAM.pin_href = HREF_GPIO_NUM;
      CONFIG_CAM.pin_sscb_sda = SIOD_GPIO_NUM;
      CONFIG_CAM.pin_sscb_scl = SIOC_GPIO_NUM;
      CONFIG_CAM.pin_pwdn = PWDN_GPIO_NUM;
      CONFIG_CAM.pin_reset = RESET_GPIO_NUM;
      CONFIG_CAM.xclk_freq_hz = 20000000;
      CONFIG_CAM.pixel_format = PIXFORMAT_JPEG;


      // --> Frame size:
      if (psramFound()) {
            CONFIG_CAM.frame_size = FRAMESIZE_UXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
            CONFIG_CAM.jpeg_quality = 10;
            CONFIG_CAM.fb_count = 2;
      } 
      else {
            CONFIG_CAM.frame_size = FRAMESIZE_SVGA;
            CONFIG_CAM.jpeg_quality = 12;
            CONFIG_CAM.fb_count = 1;
      }

      // --> Initialize the camera module
      esp_err_t err = esp_camera_init(&CONFIG_CAM);
      if (err != ESP_OK) {
            Serial.printf("Camera init failed with error 0x%x", err);
            return;
      }

      Serial.println("Ok!");
}




/*************************************
** Funcao initialize_spiffs()       **
*************************************/
void initialize_spiffs( void ) {
      // Initialize the SPIFFS file system
      Serial.print("Initializing the SPIFFS file system...");
      if (!SPIFFS.begin( true )) {
            Serial.println("ERROR");
            return;
      } 
      else {
            delay(500);
            Serial.println("Ok!");
      }
}



/****************************************
** verify_picture_taken()              **
****************************************/
bool verify_picture_taken( fs::FS &fs ) {
        File f_pic = fs.open( FILE_PHOTO );
        unsigned int pic_sz = f_pic.size();
        return ( pic_sz > 100 );
}



/****************************************
** take_picture_save_spiffs()         **
****************************************/
void take_picture_save_spiffs( void ) {
      //--> Turn on the flash
      digitalWrite(4, HIGH); 
      camera_fb_t * fb = NULL; // pointer 
      bool ok = 0; // Boolean indicating if the picture has been taken correctly

      do {
            // Take a picture with the camera
            Serial.println("Taking a picture...");
            
            fb = esp_camera_fb_get();
            if (!fb) {
                  Serial.println("Camera capture failed");
                  return;
            }  

            // Picture file name
            Serial.printf("Picture file name: %s\n", FILE_PHOTO);
            File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);

            // Insert the data in the picture file
            if (!file) {
                  Serial.println("Failed to open file in writing mode");
            }
            else {
                  file.write(fb->buf, fb->len); // payload (image), payload length
                  Serial.print("The picture has been saved in ");
                  Serial.print(FILE_PHOTO);
                  Serial.print(" - Size: ");
                  Serial.print(file.size());
                  Serial.println(" bytes");
            }
            // Close the file
            file.close();
            esp_camera_fb_return(fb);

            // --> Verify wether the file has been correctly saved in SPIFFS:
            ok = verify_picture_taken( SPIFFS );
      } while ( !ok );

      // --> Turns off the ESP32-CAM white on-board LED (flash) connected to GPIO 4
      digitalWrite(4, LOW);
  
}



/****************************************
** getTimeStamp()                      **
****************************************/
void getTimeStamp( void ) {
      while (!timeClient.update()) {
            timeClient.forceUpdate();
      }
      
      // --> Date format in 'formattedDate': 2018-05-28T16:00:13Z
      // --> Its needed to extract the date and the time
      formattedDate = timeClient.getFormattedDate();
      Serial.println(formattedDate);

      // --> Extract the date
      int splitT = formattedDate.indexOf("T"); //Position of the character 'T' in the string
      dayStamp = formattedDate.substring(0, splitT);
      Serial.println(dayStamp);

      // --> Extract the hour
      timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1);
      Serial.println(timeStamp);
}



/****************************************
** processor()                         **
****************************************/
String processor(const String& var) {
      //--> If 'var' is DATE_PHOTO:
      if (var == "DATE_PHOTO") {
            return dayStamp;
      }
      //-->  If 'var' is HOUR_PHOTO:
      else if (var == "HOUR_PHOTO") {
            return timeStamp;
      }
      //--> If 'var' is TEMPERATURE:
      else if (var == "TEMPERATURE") {
            return strTemp;
      }
      //--> If 'var' is PRESSURE:
      else if (var == "PRESSURE") {
            return strPressure;
      }
      //--> If 'var' is ADC_LDR:
      else if (var == "ADC_LDR") {
            return strADCldr;
      }

      return String();
}



/***********************************
** connect_mqtt_server()          **
***********************************/
void connect_mqtt_server( void ) {
      while (!mqtt_client.connected()) {
            Serial.println("\nConnecting to the MQTT server...");
            if (mqtt_client.connect("sensorReadings")) {
                  Serial.print(F("Connected to the MQTT server in "));
                  Serial.print(MQTT_SERVER);
                  Serial.print(":");
                  Serial.println(MQTT_PORT);

                  // Subscribe to the topics
                  mqtt_client.subscribe("sensors/temp");
                  mqtt_client.subscribe("sensors/pressure");
                  mqtt_client.subscribe("sensors/adc_ldr");
            }
            else {
                  Serial.print("Fail in connecting to the MQTT server [rc = ");
                  Serial.print(mqtt_client.state());
                  Serial.println("]. Trying again in 5s.");
                  // Wait 5 seconds before retrying
                  delay(5000);
            }
      }
}




/*******************************
** messageReceived()          **
*******************************/
void messageReceived(char* topic, byte* payload, unsigned int length) {
      String topicName = String(topic);

      // --> Inserir o conteudo de 'payload' em um array de caracteres
      char content[30];
      for ( unsigned int i = 0; i < length; i++ ) {
            content[i] = static_cast<char>( payload[i] );
      }
      content[length] = '\0';

      // Write the received message
      write_received_message(topicName, payload, length);

      //--> Topic 'sensors/temp'
      if (topicName == "sensors/temp") {
            strTemp = String(content);
      }
      //--> Topic 'sensors/pressure'
      else if (topicName == "sensors/pressure") {
            strPressure = String(content);
      }
      //--> Topic 'sensors/adc_ldr'
      else if (topicName == "sensors/adc_ldr") {
            strADCldr = String(content);
      }
}




/********************************
** write_received_message()    **
********************************/
void write_received_message(String topicName, byte* payload, unsigned int length) {
      // Write the topic name:
      Serial.print("Received message [");
      Serial.print(topicName);
      Serial.print("] - ");
      String msg = "";
      // Write the message in the 'payload[]' array
      for ( unsigned int i = 0; i < length; i++ ) {
            Serial.print(static_cast<char>(payload[i]));
            msg = msg + String(payload[i]);
      }
      Serial.println("");

      //Ultima mensagem
      strLastMessage = "Received message [" + topicName + "] - " + msg;
}





/*******************************************
** call_take_picture_functions()          **
*******************************************/
void call_take_picture_functions( void ) {
      take_picture_save_spiffs(); //Take a picture and save it in the SPIFFS
      getTimeStamp(); // Get the date and time
      Serial.println("");
}
