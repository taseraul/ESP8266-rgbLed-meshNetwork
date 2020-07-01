
// Load Wi-Fi library
#include <ESP8266WiFi.h>
#include <espnow.h>
#include "Arduino.h"

extern "C"{
  #include "pwm.h"
  //#include "user_interface.h"
}


#define PWM_CHANNELS 3
#define PWM_PERIOD 255; // * 200ns ^= 1 kHz

// Replace with your network credentials
const char* ssid     = "weMeshUP";
const char* password = "atelierwmu";

// Set web server port number to 80
WiFiServer server(80);

// Decode HTTP GET value
String redString = "0";
String greenString = "0";
String blueString = "0";
String freqString = "0";
int pos1 = 0;
int pos2 = 0;
int pos3 = 0;
int pos4 = 0;

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
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t red2;
  uint8_t green2;
  uint8_t blue2;
  uint8_t red3;
  uint8_t green3;
  uint8_t blue3;
  uint8_t animateType;
  uint8_t intensity;
} colors;

// Create a struct_message called myData
colors rgbStrip;

bool mirror = true;

void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("Last Packet Send Status: ");
  if (sendStatus == 0){
    Serial.println("Delivery success");
  }
  else{
    Serial.println("Delivery fail");
  }
}

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
  esp_now_register_send_cb(OnDataSent);
  
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
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<link rel=\"stylesheet\" href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/css/bootstrap.min.css\">");
            client.println("<script src=\"https://cdnjs.cloudflare.com/ajax/libs/jscolor/2.0.4/jscolor.min.js\"></script>");
            client.println("</head><body><div class=\"container\"><div class=\"row\"><h1>ESP Color Picker</h1></div>");
            client.println("<a class=\"btn btn-primary btn-lg\" href=\"#\" id=\"change_color\" role=\"button\">Change Color</a> ");
            client.println("<input class=\"jscolor {onFineChange:'update(this)'}\" id=\"rgb\"></div>");
            client.println("<script>function update(picker) {document.getElementById('rgb').innerHTML = Math.round(picker.rgb[0]) + ', ' +  Math.round(picker.rgb[1]) + ', ' + Math.round(picker.rgb[2]);");
            client.println("document.getElementById(\"change_color\").href=\"?r\" + Math.round(picker.rgb[0]) + \"g\" +  Math.round(picker.rgb[1]) + \"b\" + Math.round(picker.rgb[2]) + \"&\";}</script></body></html>");
            // The HTTP response ends with another blank line
            client.println();
            // Request sample: /?r201g32b255
            // Red = 201 | Green = 32 | Blue = 255
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

uint8_t getRed(int val){
  uint8_t red = ((val>=0 && val<=60) || (val>=300 && val<=360)) ? 255 : ((val>=120 && val<=240) ? 0 : (val>=60 && val<=120) ? (255 - 255.0/60*(val-60)) : ((val>=240 && val<=300) ? (0 + 255.0/60*(val-240)) : 0));
  return red;
}
uint8_t getGreen(int val){
  uint8_t green = (val>=60 && val<=180) ? 255 : ((val>=240 && val<=360) ? 0 : ((val>=0 && val<=60) ? (0 + 255.0/60*val) : ((val>=180 && val<=240) ? (255 - 255.0/60*(val-180)) : 0)));
  return green;
}
uint8_t getBlue(int val){
  uint8_t blue = (val>=0 && val<=120) ? 0 : ((val>=180 && val<=300) ? 255 : ((val>=120 && val<=180) ? (0 + 255.0/60*(val-120)) : ((val>=300 && val<=360) ? (255 - 255.0/60*(val-300)) : 0)));
  return blue;
}

int getGETvalue(int index){
  return header.substring(header.indexOf('=',index),header.indexOf('&',index)).toInt();
}

void extractData(){
  bool thisUpdate = false;
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
  val = getGETvalue(index);
  tmpRgbStrip.red = getRed(val);
  tmpRgbStrip.green = getGreen(val);
  tmpRgbStrip.blue = getBlue(val);

  index = header.indexOf("anim");
  val = getGETvalue(index);
  tmpRgbStrip.animateType = val;

  index = header.indexOf("range");
  int range = getGETvalue(index);

  if(tmpRgbStrip.animateType == 1){
    tmpRgbStrip.red2 = getRed(val+range);
    tmpRgbStrip.green2 = getGreen(val+range);
    tmpRgbStrip.blue2 = getBlue(val+range);  

    tmpRgbStrip.red3 = getRed(val-range);
    tmpRgbStrip.green3 = getGreen(val-range);
    tmpRgbStrip.blue3 = getBlue(val-range);
  }
  else if(tmpRgbStrip.animateType>1){
    index = header.indexOf("rgb2");
    val = getGETvalue(index);
    tmpRgbStrip.red2 = getRed(val);
    tmpRgbStrip.green2 = getGreen(val);
    tmpRgbStrip.blue2 = getBlue(val);
  
    index = header.indexOf("rgb3");
    val = getGETvalue(index);
    tmpRgbStrip.red3 = getRed(val);
    tmpRgbStrip.green3 = getGreen(val);
    tmpRgbStrip.blue3 = getBlue(val);
  }

  index = header.indexOf("same");
  int mirror = getGETvalue(index);

  if(mirror){
    esp_now_send(NULL, (uint8_t *) &tmpRgbStrip, sizeof(tmpRgbStrip));
  }
  else{
    index = header.indexOf("strip");
    while(index > 0){
      val = getGETvalue(index);
      if(val == 0){
        rgbStrip = tmpRgbStrip;
        thisUpdate = true;
      }
      else
        esp_now_send(broadcastAddress[val-1], (uint8_t *) &tmpRgbStrip, sizeof(tmpRgbStrip));
      index = header.indexOf("strip",index+1);
    }
  }
  if(thisUpdate){
    pwm_set_duty(rgbStrip.red,0);
    pwm_set_duty(rgbStrip.green,1);
    pwm_set_duty(rgbStrip.blue,2);
    pwm_start();
  }
}

void loop(){
  webPage();
}
