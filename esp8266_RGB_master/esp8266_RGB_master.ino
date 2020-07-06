// Load Wi-Fi library
#include <ESP8266WiFi.h>
#include <espnow.h>
#include "Arduino.h"

extern "C"{
  #include "pwm.h"
  //#include "user_interface.h"
}

//#define DEBUG
#define PWM_CHANNELS 3
#define PWM_PERIOD 255; // * 200ns ^= 1 kHz

// Replace with your network credentials
const char* ssid     = "weMeshUP";
const char* password = "atelierwmu";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP req  uest
String header;

#define PWM_CHANNELS 3
#define LED_STRIPS 2

// PWM setup (choice all pins that you use PWM)

uint32 io_info[PWM_CHANNELS][3] = {
  // MUX, FUNC, PIN
  {PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0,   0},
  {PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2,   2},
  {PERIPHS_IO_MUX_U0RXD_U, FUNC_GPIO3,   3},
};

// PWM initial duty: all off

uint32 pwm_duty_init[PWM_CHANNELS];

uint8_t broadcastAddress[LED_STRIPS][6] = {{0xCC, 0x50, 0xE3, 0x1D, 0xF1, 0x6D},{0xCC, 0x50, 0xE3, 0x00, 0x00, 0x00}};//cc-50-e3-1d-f1-6d //cc:50:e3:1d:c2:e2

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

typedef struct colors {
  float nowHSL;
  float HSL1;
  float HSL2;
  uint8_t animateType;
  uint8_t intensity;
  uint8_t animSpeed;
} colors;

// Create a struct_message called myData
colors rgbStrip;

uint8_t dir = 1;
bool mirror;
unsigned long updateTimer = 0;
int timerCd = 0;
bool mirror = true;

#ifdef DEBUG
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Last Packet Send Status: ");
  if (sendStatus == 0){
    Serial.println("Delivery success");
  }
  else{
    Serial.println("Delivery fail");
  }
}
#endif

void setup() {
  Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY);
  
  // configure LED PWM resolution/range and set pins to LOW
  pinMode(0,OUTPUT);
  pinMode(2,OUTPUT);
  pinMode(3,OUTPUT);

  digitalWrite(0,HIGH);
  digitalWrite(2,LOW);
  digitalWrite(3,LOW);
  
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();

  for (uint8_t channel = 0; channel < PWM_CHANNELS; channel++) {
    pwm_duty_init[channel] = 0;
  }
  uint32_t period = PWM_PERIOD;
  pwm_init(period, pwm_duty_init, PWM_CHANNELS, io_info);
  pwm_start();

   // Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  #ifdef DEBUG
    esp_now_register_send_cb(OnDataSent);
  #endif
  // Register peer
  for(int i=0;i<LED_STRIPS;i++){
    esp_now_add_peer(broadcastAddress[i] , ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
  }
}

void webPage(){
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {            // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
                   
            // Display the HTML web page
            client.println(html);
            client.println();
            if(header.indexOf("GET /?r") >= 0) {
              extractData();
            }
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

uint8_t getRed(float val){
  uint8_t red = ((val>=0 && val<=60) || (val>=300 && val<=360)) ? 255 : ((val>=120 && val<=240) ? 0 : (val>=60 && val<=120) ? (255 - 255.0/60*(val-60)) : ((val>=240 && val<=300) ? (0 + 255.0/60*(val-240)) : 0));
  return red;
}
uint8_t getGreen(float val){
  uint8_t green = (val>=60 && val<=180) ? 255 : ((val>=240 && val<=360) ? 0 : ((val>=0 && val<=60) ? (0 + 255.0/60*val) : ((val>=180 && val<=240) ? (255 - 255.0/60*(val-180)) : 0)));
  return green;
}
uint8_t getBlue(float val){
  uint8_t blue = (val>=0 && val<=120) ? 0 : ((val>=180 && val<=300) ? 255 : ((val>=120 && val<=180) ? (0 + 255.0/60*(val-120)) : ((val>=300 && val<=360) ? (255 - 255.0/60*(val-300)) : 0)));
  return blue;
}

int getGETvalue(int index){
  if(header.indexOf('&',index) >= 0)
    return header.substring(header.indexOf('=',index),header.indexOf('&',index)).toInt();
  else
    return header.substring(header.indexOf('=',index)).toInt();
}

void extractData(){
  colors tmpRgbStrip;
  int val;
  int index;
  
  //todo on off. range's range. check if sumbit value exists
  
  index = header.indexOf("intens");
  val = getGETvalue(index);
  tmpRgbStrip.intensity = val;

  index = header.indexOf("anim");
  val = getGETvalue(index);
  tmpRgbStrip.animateType = val;
  
  index = header.indexOf("rgb1");
  tmpRgbStip.nowHSL = getGETvalue(index);
  tmpRgbStrip.red = getRed(valF);
  tmpRgbStrip.green = getGreen(valF);
  tmpRgbStrip.blue = getBlue(valF);

  index = header.indexOf("rgb2");
  float valF = getGETvalue(index);
  tmpRgbStrip.red2 = getRed(valF);
  tmpRgbStrip.green2 = getGreen(valF);
  tmpRgbStrip.blue2 = getBlue(valF);

  index = header.indexOf("same");
  mirror = getGETvalue(index) == 0 ? false : true;
//---------------------------------------------------
  if(mirror){
    esp_now_send(NULL, (uint8_t *) &tmpRgbStrip, sizeof(tmpRgbStrip));
    updateLED(false);
  }
  else{
    index = header.indexOf("strip");
    while(index > 0){
      val = getGETvalue(index);
      if(val == 0){
        rgbStrip = tmpRgbStrip;
        updateLED(false);
      }
      else
        esp_now_send(broadcastAddress[val-1], (uint8_t *) &tmpRgbStrip, sizeof(tmpRgbStrip));
      index = header.indexOf("strip",index+1);
    }
  }
}

updateLED(bool initial){
  if(ititial){
    switch(rgbStrip.animateType){
      case 1:
        rgbStrip.nowHSL += dir*rgbStrip.step;
        if(rgbStrip.nowHSL == 0)
          rgbStrip.nowHSL = 360;
        else if(rgbStrip.nowHSL == 360)
          rgbStrip.nowHSL = 0;
        if(rgbStrip.nowHSL == rgbStrip.HSL1 || rgbStrip.nowHSL == rgbStrip.HSL2)
          dir *=-1;
      break;

      case 2:
        rgbStrip.nowHSL = RANDOM_REG32%12*30;
      break;

      case 3:

      break;
      
      case default:
      break; 
    }
    updateTimer = millis();
  }
  if(mirror)
    esp_now_send(NULL, (uint8_t *) &rgbStrip, sizeof(RgbStrip));  
  pwm_set_duty(getRed(rgbStrip.nowHSL),0);
  pwm_set_duty(getGreen(rgbStrip.nowHSL),1);
  pwm_set_duty(getBlue(rgbStrip.nowHSL),2);
  pwm_start();
}

void loop(){
  webPage();
  if(anim&& (millis() - updateTimer >= timerCd)){
    updateAnimation(true);
  }
}
