// Load Wi-Fi library
#include <ESP8266WiFi.h>
#include <espnow.h>
#include "Arduino.h"

extern "C"{
  #include "pwm.h"
  #include "user_interface.h"
}

#define DEBUG

//Set no. of channels to controll
#define PWM_CHANNELS 3
#define PWM_PERIOD 255; //Set pwm period = 255* 200ns ~= 19 kHz

const char* ssid     = "#319";        //Fill with your wifi ssid
const char* password = "pdk7-0ao4";   //Fill with your wifi password

const char* ssidAP = "ESP32-Access-Point";
const char* passwordAP = "123456789";

#define CHAN_AP 1 //Channel for softAP

// Set web server port number to 80
WiFiServer server(80);

String redString = "0";
String greenString = "0";
String blueString = "0";
String freqString = "0";
//index values  variable
int pos1 = 0;
int pos2 = 0;
int pos3 = 0;
int pos4 = 0;

// Variable to store the HTTP req  uest
String header;

#define PWM_CHANNELS 3

// PWM pins init 
uint32 io_info[PWM_CHANNELS][3] = {
  // MUX, FUNC, PIN
  {PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0,   0},
  {PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2,   2},
  {PERIPHS_IO_MUX_U0RXD_U, FUNC_GPIO3,   3},
};

uint32 pwm_duty_init[PWM_CHANNELS];

//MAC adress for slave devices
uint8_t broadcastAddress1[] = {0xCC, 0x50, 0xE3, 0x1D, 0xF1, 0x6D};//cc-50-e3-1d-f1-6d
uint8_t broadcastAddress2[] = {0xCC, 0x50, 0xE3, 0x1D, 0xC2, 0xE2};//cc:50:e3:1d:c2:e2

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

// Create a color struct
typedef struct colors {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} colors;

colors nowColor;

//preset colors for random jumps
colors randColors[8] = { {255,255,255}, {255,0,0}, {0,255,248}, {255,93,0}, {0,255,0}, {255,183,0}, {255,0,255}, {0,0,255} };
unsigned long timer = 0;

//jump delay in ms
int timerCd = 1000;

//current random color index
int randIndex = 0;

//flag for random mode
bool isRand = false;

//Callback function for esp-now
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  #ifdef DEBUG
  if (sendStatus == 0){
    Serial.println("Packet sent");
  }
  else{
    Serial.println("Packet lost");
  }
  #endif
}

void setup() {
  Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY);//RX used for one of the outputs
  
  //set pins to LOW
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

  //Start wifi AP to improve esp-now packet reception
  WiFi.softAP(ssidAP, passwordAP, CHAN_AP, true);  

  //Initialize pwm with 0 duty
  for (uint8_t channel = 0; channel < PWM_CHANNELS; channel++) {
    pwm_duty_init[channel] = 0;
  }
  uint32_t period = PWM_PERIOD;
  pwm_init(period, pwm_duty_init, PWM_CHANNELS, io_info);
  pwm_start();  //start pwm

  //Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  //Set as esp-now master
  //Register callback function
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  esp_now_register_send_cb(OnDataSent);
  
  //Register peer
  esp_now_add_peer(broadcastAddress1, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
  esp_now_add_peer(broadcastAddress2, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
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

            // Request sample: /?r201g32b255&
            // Red = 201 | Green = 32 | Blue = 255
            if(header.indexOf("GET /?") >= 0) {
              //Get data from header
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
void extractData(){
  //check if random mode
  if(header.indexOf("rand")>=0)
  {
    if(header.indexOf("&t")>=0)
      timerCd = header.substring(header.indexOf("&t")+2, header.indexOf("#")).toInt();
    randIndex = (RANDOM_REG32%7 + 1 + randIndex)%8;
    updateColors(randColors[randIndex]);
    esp_now_send(NULL, (uint8_t *) &randColors[randIndex], sizeof(randColors[randIndex]));
    timer = millis();
    isRand = true;
  }
  else{
    isRand = false;
    pos1 = header.indexOf('r');
    pos2 = header.indexOf('g');
    pos3 = header.indexOf('b');
    pos4 = header.indexOf('&');
    nowColor.red = header.substring(pos1+1, pos2).toInt();
    nowColor.green = header.substring(pos2+1, pos3).toInt();
    nowColor.blue = header.substring(pos3+1, pos4).toInt();
    updateColors(nowColor);
    esp_now_send(NULL, (uint8_t *) &nowColor, sizeof(nowColor));
  }
}

void updateColors(colors setColor){
  pwm_set_duty(setColor.red,0);
  pwm_set_duty(setColor.green,1);
  pwm_set_duty(setColor.blue,2);
  pwm_start();
}

void loop(){
  webPage();//print webpage to client (if any)
  if(isRand){ //if random switch colors every timerCd miliseconds
    if(millis()-timer >= timerCd){
      randIndex = (RANDOM_REG32%7 + 1 + randIndex)%8;
      updateColors(randColors[randIndex]);
      esp_now_send(NULL, (uint8_t *) &randColors[randIndex], sizeof(randColors[randIndex]));
      timer = millis();
    }
  }
}
