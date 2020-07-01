#include <ESP8266WiFi.h>
#include <espnow.h>
#include "Arduino.h"

extern "C"{
  #include "pwm.h"
  #include "user_interface.h"
}

const char* ssid = "ESP32-Access-Point";
const char* password = "123456789";

#define PWM_CHANNELS 3
#define PWM_PERIOD 255; // * 200ns ~= 19 kHz

// PWM pins init 
uint32 io_info[PWM_CHANNELS][3] = {
  // MUX, FUNC, PIN
  {PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0,   0},
  {PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2,   2},
  {PERIPHS_IO_MUX_U0RXD_U, FUNC_GPIO3,   3},
};

uint32 pwm_duty_init[PWM_CHANNELS];

// Create a color struct
typedef struct colors {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} colors;

colors nowColor;

void setup() {
  Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY); //RX used for one of the outputs
  
  //set pins to LOW
  pinMode(0,OUTPUT);
  pinMode(2,OUTPUT);
  pinMode(3,OUTPUT);

  digitalWrite(0,HIGH); //Strip starts as red until connected to Wifi
  digitalWrite(2,LOW);
  digitalWrite(3,LOW);

  //Initialize pwm with 0 duty
  for (uint8_t channel = 0; channel < PWM_CHANNELS; channel++) {
    pwm_duty_init[channel] = 0;
  }
  uint32_t period = PWM_PERIOD;
  pwm_init(period, pwm_duty_init, PWM_CHANNELS, io_info);
  pwm_start();  //start pwm

  
  //Connect to master AP to improve esp-now packet reception
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to Access Point...");
  }
  
  //Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Set as esp now slave
  // Set recieve callback function
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);
}

void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
  memcpy(&nowColor, incomingData, sizeof(nowColor)); //copy color info
  pwm_set_duty(nowColor.red,0);
  pwm_set_duty(nowColor.green,1); //set new duty cycle
  pwm_set_duty(nowColor.blue,2);
  pwm_start(); //update pwm
}

void loop(){
  // do nothing
}
