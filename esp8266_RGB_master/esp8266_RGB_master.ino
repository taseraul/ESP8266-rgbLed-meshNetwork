// Load Wi-Fi library
#include <ESP8266WiFi.h>
#include <espnow.h>
#include "Arduino.h"

extern "C"
{
#include "pwm.h"
  //#include "user_interface.h"
}

#define DEBUG
#define PWM_CHANNELS 3
#define PWM_PERIOD 255; // * 200ns ^= 1 kHz

// Replace with your network credentials
const char *ssid = "#319";
const char *password = "pdk7-0ao4";

const char *ssidAP = "ESP32-Access-Point";
const char *passwordAP = "123456789";

#define CHAN_AP 1 //Channel for softAP

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP req  uest
String header;

#define PWM_CHANNELS 3
#define LED_STRIPS 2

// PWM setup (choice all pins that you use PWM)

uint32 io_info[PWM_CHANNELS][3] = {
    // MUX, FUNC, PIN
    {PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0, 0},
    {PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2, 2},
    {PERIPHS_IO_MUX_U0RXD_U, FUNC_GPIO3, 3},
};

// PWM initial duty: all off

uint32 pwm_duty_init[PWM_CHANNELS];

uint8_t broadcastAddress[LED_STRIPS][6] = {{0xCC, 0x50, 0xE3, 0x1D, 0xF1, 0x6D}, {0xCC, 0x50, 0xE3, 0x00, 0x00, 0x00}}; //cc-50-e3-1d-f1-6d //cc:50:e3:1d:c2:e2

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0;
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

typedef struct colors
{
  float HSLnow;
  float HSL1;
  float HSL2;
  uint8_t animateType;
  float intensity;
  uint8_t animSpeed;
  float step = 0.1f;
} colors;

// Create a struct_message called myData
colors rgbStrip;

int8_t dir = 1;
unsigned long updateTimer = 0;
int timerCd = 0;
bool mirror = true;

const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html><html>
<head><meta name="viewport" content="width=device-width, initial-scale=0.5">
<style>
body {
	background: white;
}

input[type="range"]	 {
	 -webkit-appearance: none;
    width: 100%;
    height: 150px;
    margin: auto;
	background: transparent;
}
input::-webkit-slider-runnable-track {
    width: 450px;
    height: 30px;
    background: linear-gradient(to right, red,yellow,green,turquoise,blue,violet,red);
    border: solid 2px;
	border-color : #000000;
    border-radius: 15px;

}


#slider4::-webkit-slider-runnable-track {
background: linear-gradient(to right,black,white);
}
#slider3::-webkit-slider-runnable-track {
background: white;
}
#animRange::-webkit-slider-runnable-track {
background: #787878;
}

input::-webkit-slider-thumb {
    -webkit-appearance: none;
    border: none;
    height: 35px;
    width: 35px;
    border-radius: 50%;
    background: rgba(0,0,0,1);
    margin-top: -4px;
}
input:focus {
    outline: none;
}
.center{
	margin:auto;
	width:70%;
}

group + group {
  margin-top: 20px;
}

.inline-radio {
	display: flex;
	border-radius: 3px;
	overflow: hidden;
	border: 1px solid #b6b6b6;
}

.inline-radio div {
	position: relative;
	flex: 1;
}

.inline-radio input {
	width: 100%;
	height: 60px;
	opacity: 0;
}

.inline-radio label {
	position: absolute;
	top: 0; left: 0;
	color: #b6b6b6;
	width: 100%;
	height: 100%;
	background: #505050;
	display: flex;
	align-items: center;
	justify-content: center;
	pointer-events: none;
	border-right: 1px solid #b6b6b6;
}

.inline-radio div:last-child label {
	border-right: 0;
}

.inline-radio input:checked + label {
	background: #707070;
	font-weight: 500;
	color: #000;
}

group + group {
  margin-top: 20px;
}
*, *::before, *::after {
  box-sizing: border-box;
}


</style>
<head>
<body>
<form action="" method="get">
<div id="container1" style="border-radius:30px;margin-bottom:20px;margin-top:20px;">
	<div class="center">
		<input id="slider1" name="rgb1" type="range" step="0.1" max="360" min="0" value="0"/>
	</div>
</div>
<div id="container2" style="display:none;border-radius:30px;margin-bottom:20px;margin-top:20px;">
	<div class="center">
		<input id="slider2" name="rgb2" type="range" step="0.1" max="360" min="0" value="0"/>
	</div>
</div>
<div id="container3" style="border-radius:30px;margin-bottom:20px;margin-top:20px;border:solid 2px;border-color : #000000;">
	<div class="center">
		<input id="slider3" name="timer" type="range" step="10" max="10000" min="200" value="1000"/>
		<h1 id="timerText" style="text-align:center;">10</h1>
	</div>
</div>
<div id="container4" style="border-radius:30px;margin-bottom:20px;margin-top:20px;border:solid 2px;border-color : #000000;">
	<div class="center">
		<input id="slider4" name="intens" type="range" step="1" max="10" min="0" value="10"/>
	</div>
</div>
<div id="container6" style="margin-bottom:20px;margin-top:20px;border:solid 2px;border-color:#000000;overflow:none;">

	<group class="inline-radio">
		  <div><input type="radio" id="static" name="anim" value="0" checked>
		  <label for="st">Static color</label></div>
		  <div><input type="radio" id="cfl" name="anim" value="1">
		  <label for="cfl">Color fade loop</label></div>
		  <div><input type="radio" id="rand" name="anim" value="2">
		  <label for="rand">Random colors</label></div>
		  <div><input type="radio" id="mic" name="anim" value="3">
		  <label for="mic">Microphone input</label></div>
	  </group>

</div>
<div id="container7" style="margin-bottom:20px;margin-top:20px;border:solid 2px;border-color : #000000;">
		<group class="inline-radio">
		  <div><input type="radio" id="mirror" name="same" value="1" checked>
		  <label for="mir">Mirrored colors</label></div>
		  <div><input type="radio" id="independent" name="same" value="0">
		  <label for="ind">Independent colors</label></div>
		</group>
</div>
<div id="container8" style="margin-bottom:20px;margin-top:20px;border:solid 2px;border-color : #000000;">
		<group class="inline-radio">
		  <div><input type="checkbox" id="ls0" name="strips" value="0">
		  <label for="ls0">Zone 0</label></div>
		  <div><input type="checkbox" id="ls1" name="strips" value="1">
		  <label for="ls1">Zone 1</label></div>
		  <div><input type="checkbox" id="ls2" name="strips" value="2">
		  <label for="ls2">Zone 2</label></div>
		  <div><input type="checkbox" id="ls3" name="strips" value="3">
		  <label for="ls3">Zone 3</label></div>
		</group>
</div>
<div id="container9" style="margin-bottom:20px;margin-top:20px;border:solid 2px;border-color : #000000;">
		<group class="inline-radio">
		  <div><input type="submit" id="upd">
		  <label for="upd">Update</label></div>

		</group>
</div>
</form>
</body>
<script>

var slider1 = document.getElementById("slider1");
var slider2 = document.getElementById("slider2");
var slider4 = document.getElementById("slider4");
setInterval(function(){
	var val1 = slider1.value;
	document.getElementById("container1").style.background = "hsl("+val1+", 100%, 50%)";

	var val2 = slider2.value;
	document.getElementById("container2").style.background = "hsl("+val2+", 100%, 50%)";

	var val4 = slider4.value;
	document.getElementById("container4").style.background = "hsl(0, 0%, "+(100*val4/10)+"%)";
	var anim1 = document.getElementById("cfl").checked;
	if(!anim1){
		document.getElementById("container2").style.display = "none";
	}
	else{
		document.getElementById("container2").style.display = "block";
	}
	var timer = document.getElementById("cfl").checked || document.getElementById("rand").checked;
	if(!timer){
		document.getElementById("container3").style.display = "none";
	}
	else{
		document.getElementById("container3").style.display = "block";
		document.getElementById("timerText").textContent = document.getElementById("slider3").value;
	}
	var mir = document.getElementById("mirror").checked;
	var ind = document.getElementById("independent").checked;
	if(mir){
		document.getElementById("container8").style.display = "none";
	}
	if(ind){
		document.getElementById("container8").style.display = "block";
	}
})
</script>

</html>)rawliteral";

__FlashStringHelper *html = (__FlashStringHelper *)index_html;

#ifdef DEBUG
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus)
{
  Serial.print("Last Packet Send Status: ");
  if (sendStatus == 0)
  {
    Serial.println("Delivery success");
  }
  else
  {
    Serial.println("Delivery fail");
  }
}
#endif

void setup()
{
#ifdef DEBUG
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
#endif
#ifndef DEBUG
  pinMode(1, INPUT);
#endif
  // configure LED PWM resolution/range and set pins to LOW
  pinMode(0, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);

  digitalWrite(0, HIGH);
  digitalWrite(2, LOW);
  digitalWrite(3, LOW);

  // Connect to Wi-Fi network with SSID and password
  #ifdef DEBUG
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  #endif
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    #ifdef DEBUG
    Serial.print(".");
    #endif
  }
  // Print local IP address and start web server
  #ifdef DEBUG
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  #endif
  server.begin();

  //Start wifi AP to improve esp-now packet reception
  WiFi.softAP(ssidAP, passwordAP, CHAN_AP, true);

  for (uint8_t channel = 0; channel < PWM_CHANNELS; channel++)
  {
    pwm_duty_init[channel] = 0;
  }
  uint32_t period = PWM_PERIOD;
  pwm_init(period, pwm_duty_init, PWM_CHANNELS, io_info);
  pwm_start();

  // Init ESP-NOW
  if (esp_now_init() != 0)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
#ifdef DEBUG
  esp_now_register_send_cb(OnDataSent);
#endif
  // Register peer
  for (int i = 0; i < LED_STRIPS; i++)
  {
    esp_now_add_peer(broadcastAddress[i], ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
  }
}

void webPage()
{
  WiFiClient client = server.available(); // Listen for incoming clients

  if (client)
  { // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client."); // print a message out in the serial port
    String currentLine = "";       // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime)
    { // loop while the client's connected
      currentTime = millis();
      if (client.available())
      {                         // if there's bytes to read from the client,
        char c = client.read(); // read a byte, then
        Serial.write(c);        // print it out the serial monitor
        header += c;
        if (c == '\n')
        { // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0)
          {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // Display the HTML web page
            client.println(html);
            client.println();
            if (header.indexOf("GET /") >= 0)
            {
              extractData();
            }
            // Break out of the while loop
            break;
          }
          else
          { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        }
        else if (c != '\r')
        {                   // if you got anything else but a carriage return character,
          currentLine += c; // add it to the end of the currentLine
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

uint8_t getRed(float val)
{
  if (val <= 0)
    val = 0;
  if (val >= 360)
    val = 360;
  uint8_t red = ((val >= 0 && val <= 60) || (val >= 300 && val <= 360)) ? 255 : ((val >= 120 && val <= 240) ? 0 : (val >= 60 && val <= 120) ? (255 - 255.0 / 60 * (val - 60)) : ((val >= 240 && val <= 300) ? (0 + 255.0 / 60 * (val - 240)) : 0));
  return red;
}
uint8_t getGreen(float val)
{
  if (val <= 0)
    val = 0;
  if (val >= 360)
    val = 360;
  uint8_t green = (val >= 60 && val <= 180) ? 255 : ((val >= 240 && val <= 360) ? 0 : ((val >= 0 && val <= 60) ? (0 + 255.0 / 60 * val) : ((val >= 180 && val <= 240) ? (255 - 255.0 / 60 * (val - 180)) : 0)));
  return green;
}
uint8_t getBlue(float val)
{
  if (val <= 0)
    val = 0;
  if (val >= 360)
    val = 360;
  uint8_t blue = (val >= 0 && val <= 120) ? 0 : ((val >= 180 && val <= 300) ? 255 : ((val >= 120 && val <= 180) ? (0 + 255.0 / 60 * (val - 120)) : ((val >= 300 && val <= 360) ? (255 - 255.0 / 60 * (val - 300)) : 0)));
  return blue;
}

int getGETvalueInt(int index)
{
  String tmp = "";
  if (header.indexOf('&', index) >= 0)
    tmp = header.substring(header.indexOf('=', index) + 1, header.indexOf('&', index));
  else
    tmp = header.substring(header.indexOf('=', index) + 1);
#ifdef DEBUG
  Serial.println(tmp);
#endif
  return tmp.toInt();
}

float getGETvalueFloat(int index)
{
  String tmp = "";
  if (header.indexOf('&', index) >= 0)
    tmp = header.substring(header.indexOf('=', index) + 1, header.indexOf('&', index));
  else
    tmp = header.substring(header.indexOf('=', index) + 1);
#ifdef DEBUG
  Serial.println(tmp);
#endif
  return tmp.toFloat();
}

void extractData()
{
  dir = 1;
  colors tmpRgbStrip;
  int val;
  int index;

  //todo on off. range's range. check if sumbit value exists

  index = header.indexOf("intens");
  if (index >= 0)
  {
    val = getGETvalueInt(index);
    tmpRgbStrip.intensity = val / 10.0f;
#ifdef DEBUG
    Serial.println(tmpRgbStrip.intensity);
#endif
  }

  index = header.indexOf("anim");
  if (index >= 0)
  {
    val = getGETvalueInt(index);
    tmpRgbStrip.animateType = val;
#ifdef DEBUG
    Serial.println(tmpRgbStrip.animateType);
#endif
  }

  index = header.indexOf("rgb1");
  if (index >= 0)
  {
    tmpRgbStrip.HSLnow = getGETvalueFloat(index);
    tmpRgbStrip.HSL1 = tmpRgbStrip.HSLnow;
#ifdef DEBUG
    Serial.println(tmpRgbStrip.HSLnow);
    Serial.println(tmpRgbStrip.HSL1);
#endif
  }

  index = header.indexOf("rgb2");
  if (index >= 0)
  {
    tmpRgbStrip.HSL2 = getGETvalueFloat(index);
#ifdef DEBUG
    Serial.println(tmpRgbStrip.HSL2);
#endif
  }

  index = header.indexOf("same");
  if (index >= 0)
  {
    mirror = getGETvalueInt(index) == 0 ? false : true;
#ifdef DEBUG
    Serial.println(mirror);
#endif
  }

  index = header.indexOf("timer");
  if (index >= 0)
  {
    val = getGETvalueInt(index);
    if (rgbStrip.animateType == 1)
    {
      uint8_t diff = rgbStrip.HSL1 > rgbStrip.HSL2 ? (360 - rgbStrip.HSL1 + rgbStrip.HSL2) : (rgbStrip.HSL2 - rgbStrip.HSL1);
      timerCd = val / (diff / rgbStrip.step);
    }
    if (rgbStrip.animateType == 2)
    {
      timerCd = val;
    }
    if (rgbStrip.animateType == 3)
    {
      timerCd = val / (360 / rgbStrip.step);
    }
#ifdef DEBUG
    Serial.println(timerCd);
#endif
  }
  //---------------------------------------------------
  if (mirror)
  {
#ifdef DEBUG
    Serial.println("SENDING TO ALL");
#endif
    esp_now_send(NULL, (uint8_t *)&tmpRgbStrip, sizeof(tmpRgbStrip));
    rgbStrip = tmpRgbStrip;
    updateLED();
  }
  else
  {
    index = header.indexOf("strip");
    while (index > 0)
    {
      val = getGETvalueInt(index);
      if (val == 0)
      {
        rgbStrip = tmpRgbStrip;
        updateLED();
      }
      else
        esp_now_send(broadcastAddress[val - 1], (uint8_t *)&tmpRgbStrip, sizeof(tmpRgbStrip));
      index = header.indexOf("strip", index + 1);
    }
  }
}

void updateLED()
{
  switch (rgbStrip.animateType) {
  case 1:
    rgbStrip.HSLnow += dir * rgbStrip.step;
    if (rgbStrip.HSLnow <= 0)
    {
      rgbStrip.HSLnow = 360;
    }
    else if (rgbStrip.HSLnow >= 360)
    {
      rgbStrip.HSLnow = 0;
    }
    if (rgbStrip.HSL1 < rgbStrip.HSL2)
      if (rgbStrip.HSLnow <= rgbStrip.HSL1 || rgbStrip.HSLnow >= rgbStrip.HSL2)
        dir *= -1;
    if (rgbStrip.HSL1 >= rgbStrip.HSL2)
      if (rgbStrip.HSLnow <= rgbStrip.HSL1 && rgbStrip.HSLnow >= rgbStrip.HSL2)
        dir *= -1;
    if (rgbStrip.HSL1 == 0 && rgbStrip.HSL2 == 360)
      dir = 1;
#ifdef DEBUG
    Serial.println(rgbStrip.HSLnow);
    Serial.println(getRed(rgbStrip.HSLnow));
#endif
    break;

  case 2:
    rgbStrip.HSLnow = RANDOM_REG32 % 12 * 30;
    break;

  case 3:
#ifndef DEBUG
    rgbStrip.HSLnow += rgbStrip.step;
    if (rgbStrip.HSLnow == 360)
      rgbStrip.HSLnow = 0;
    if (digitalRead(1) == 1)
      rgbStrip.intensity *= 1.1;
    else
      rgbStrip.intensity /= 1.1;
    if (rgbStrip.intensity >= 1)
      rgbStrip.intensity = 1;
    if (rgbStrip.intensity <= 0.2)
      rgbStrip.intensity = 0.2;
#endif
    break;

  default:
    break;
  }
  updateTimer = millis();
  //if (mirror)
  //  esp_now_send(NULL, (uint8_t *)&rgbStrip, sizeof(rgbStrip)); //must check if it can send pakets as fast as the led refresh time
  pwm_set_duty(getRed(rgbStrip.HSLnow) * rgbStrip.intensity, 0);
  pwm_set_duty(getGreen(rgbStrip.HSLnow) * rgbStrip.intensity, 1);
  pwm_set_duty(getBlue(rgbStrip.HSLnow) * rgbStrip.intensity, 2);
  pwm_start();
}

void loop()
{
  webPage();
  if (rgbStrip.animateType > 0 && (millis() - updateTimer >= timerCd))
  {
    updateLED();
  }
}
