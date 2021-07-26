/*
  GPIO  Input  Output  Notes
  0  | pullup | OK   | outputs PWM signal at boot  "Low/GND  ROM serial bootloader" "High/VCC  Normal execution mode"
  1  | TX     | OK   | debug output at boot
  2  | OK     | OK   | GPIO2 must also be either left unconnected/floating, or driven Low, in order to enter the serial bootloader.In normal boot mode (GPIO0 high), GPIO2 is ignored.
  3  | OK     | RX   | HIGH at boot
  4  | OK     | OK
  5  | OK     | OK   | outputs PWM signal at boot
  6  | x      | x    | connected to the integrated SPI flash
  7  | x      | x    | connected to the integrated SPI flash
  8  | x      | x    | connected to the integrated SPI flash
  9  | x      | x    | connected to the integrated SPI flash
  10  | x      | x    | connected to the integrated SPI flash
  11  | x      | x    | connected to the integrated SPI flash
  12  | OK     | OK   | boot fail if pulled high  "Flash Voltage"
  13  | OK     | OK
  14  | OK     | OK   | outputs PWM signal at boot
  15  | OK     | OK   | outputs PWM signal at boot  If driven Low, silences boot messages printed by bootloader. Has an internal pull-up, so unconnected = High = normal output.
  16  | OK     | OK
  17  | OK     | OK
  18  | OK     | OK
  19  | OK     | OK
  21  | OK     | OK
  22  | OK     | OK
  23  | OK     | OK
  25  | OK     | OK
  26  | OK     | OK
  27  | OK     | OK
  32  | OK     | OK
  33  | OK     | OK
  34  | OK     | input only
  35  | OK     | input only
  36  | OK     | input only
  39  | OK     | input only
  -- only supports ADC input on GPIO32-GPIO39 as other GPIO's interfere with wifi.--
*/

#include <FS.h>
#include <SPIFFS.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include "UniversalTelegramBot.h"
#include <ArduinoJson.h>
#include <Ticker.h>
#include <EEPROM.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include "time.h"
#include "sunMoon.h"

//CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define PIN_PIR_IN 2      // SENSOR PIR  "active high"
#define FLASH_LED_PIN 4   // on board white led camera side
#define PIN_MANIOBRA 12   // relay 1  Door1
#define PIN_MANIOBRA2 13  // relay 2  Door2
#define LIGHT_PIN 14      // relay 3  LIGHT
#define SENSOR_PIN 15     // door sensor "connected to GND = closed"
#define CONFIG_PIN 3      // RX connect to GND for at least 5 seconds for WiFiConfig
#define RED_LED_PIN 33    // on board red led ESP32 side

#define FORMAT_SPIFFS_IF_FAILED true

Ticker ticker;
WiFiClientSecure client;
WebServer OtaServer(81);
UniversalTelegramBot *bot;
sunMoon  sm;

const char* host = "esp32";   //http://esp32.local:81/ota
const char* ntpServer = "pool.ntp.org";
const char* www_username = "admin";
const char* www_password = "pass";
const unsigned long Bot_mtbs = 3000; //mean time between scan messages
unsigned long Bot_lasttime;   //last time messages' scan has been done
unsigned long wifi_checkDelay = 30000;
unsigned long wifimilis;
camera_fb_t * fb = NULL;
bool flashState = false;
int currentByte;
uint8_t* fb_buffer;
size_t fb_length;
char caption_txt[51] = ("ESP32");
char BotToken[81];
char Chat_Id[31];
char Router_SSID [33];
char Router_Pass[65];
char Latitude[11] = ("52.2322");
char Longitude[11] = ("21.0083");
const char CONFIG_FILE[] = "/config.json";
int C_W_state = HIGH;
int last_C_W_state = HIGH;
unsigned long time_last_C_W_change = 0;
long C_W_delay = 5000;                      // WiFiConfig delay 5 seconds
int EstadoPuerta = LOW;
int ultimoEstadoPuerta = LOW;
unsigned long tiempoUltimoEstadoPuerta = 0;
long RetardoEstadoPuerta = 100;           
int PIR_state = LOW;
int last_PIR_state = LOW;
unsigned long time_last_PIR_change = 0;
long PIR_delay = 10;
bool shouldSaveConfig = false;
bool initialConfig = false;
bool tikOn = true;
bool pr_wifi = true;
bool WiFi_begin = true;
bool Pir_On = false;       
int lightState = 0;
String Lightmode = "Off";
float tempNtc = 25.90;
int unadecinc = 0;
unsigned long time_last_D_N = 10000;
long D_N_delay = 30000;
bool esdedia = false;
/* Style */                       //---------- OTA ----------------------
String style =
  "<style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
  "input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
  "#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
  "#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
  "form{background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
  ".btn{background:#3498db;color:#fff;cursor:pointer}</style>" "";

/* Server Index Page */          //---------- OTA ----------------------
String serverIndex =
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
  "<h1>ESP32CAM</h1>"
  "<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
  "<label id='file-input' for='file'>   Choose file...</label>"
  "<br><br>"
  "<input type='submit' class=btn value='Update'>"
  "<br><br>"
  "<div id='prg'></div>"
  "<br><div id='prgbar'><div id='bar'></div></div><br></form>"
  "<script>"
  "function sub(obj){"
  "var fileName = obj.value.split('\\\\');"
  "document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];"
  "};"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  "$.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "$('#bar').css('width',Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!') "
  "},"
  "error: function (a, b, c) {"
  "}"
  "});"
  "});"
  "</script>" + style;

bool isMoreDataAvailable() {                   //---------- Telegram BOT ----------------------
  return (fb_length - currentByte);
}
uint8_t getNextByte() {                        //---------- Telegram BOT ----------------------
  currentByte++;
  return (fb_buffer[currentByte - 1]);
}

void saveConfigCallback () {                   //---------- WiFi Config ----------------------
  Serial.println("Should save config");
  shouldSaveConfig = true;
}
void tick() {
  int state = digitalRead(RED_LED_PIN);
  digitalWrite(RED_LED_PIN, !state);
}

void ondemandwifiCallback() {                   //---------- WiFi Config ----------------------

  ticker.attach(0.5, tick);

  WiFiManager wm;

  WiFiManagerParameter custom_caption_txt("name", "caption Txt", caption_txt, 51, "required");
  WiFiManagerParameter custom_BotToken("BotToken", "Bot Token", BotToken, 81);
  WiFiManagerParameter custom_Chat_Id("Chat_Id", "Chat Id", Chat_Id, 31);
  WiFiManagerParameter custom_Latitude("Latitude", "Latitude", Latitude, 10);
  WiFiManagerParameter custom_Longitude("Longitude", "Longitude", Longitude, 10);

  wm.setBreakAfterConfig(true);
  wm.setSaveConfigCallback(saveConfigCallback);

  wm.addParameter(&custom_caption_txt);
  wm.addParameter(&custom_BotToken);
  wm.addParameter(&custom_Chat_Id);
  wm.addParameter(&custom_Latitude);
  wm.addParameter(&custom_Longitude);

  wm.setMinimumSignalQuality(8);
  wm.setConfigPortalTimeout(180);

  if (!wm.startConfigPortal("ESP32CAM")) {
    Serial.println("Not connected to WiFi but continuing anyway.");
  } else {
    Serial.println("connected...yeey :)");
  }
  strcpy(caption_txt, custom_caption_txt.getValue());
  strcpy(BotToken, custom_BotToken.getValue());
  strcpy(Chat_Id, custom_Chat_Id.getValue());
  strcpy(Latitude, custom_Latitude.getValue());
  strcpy(Longitude, custom_Longitude.getValue());
  wm.getWiFiSSID().toCharArray(Router_SSID, 33);
  wm.getWiFiPass().toCharArray(Router_Pass, 65);

  ticker.detach();
  WiFi.softAPdisconnect(true);
  initialConfig = false;
  digitalWrite(RED_LED_PIN, LOW);
}

void manIObra() {                                //---------- Maniobra  ----------------------
  digitalWrite(PIN_MANIOBRA, HIGH);//Activa salida
  delay (500);//Retardo
  digitalWrite(PIN_MANIOBRA, LOW);//Desactiva salida
}
void manIObra2() {                                //---------- Maniobra2  ----------------------
  digitalWrite(PIN_MANIOBRA2, HIGH);//Activa salida
  delay (500);//Retardo
  digitalWrite(PIN_MANIOBRA2, LOW);//Desactiva salida
}
void peaTonal() {                                //---------- PO ---------------------
  digitalWrite(PIN_MANIOBRA, HIGH);//Activa salida
  delay (500);//Retardo
  digitalWrite(PIN_MANIOBRA, LOW);//Desactiva salida
  delay (8000);//Retardo
  digitalWrite(PIN_MANIOBRA, HIGH);//Activa salida
  delay (500);//Retardo
  digitalWrite(PIN_MANIOBRA, LOW);//Desactiva salida
}
void ejecutaAccionC() {                          //----------Accion C ---------------------
  bot->sendMessage(Chat_Id, "Closed!");
}

void ejecutaAccionA() {                          //----------Accion A ---------------------
  bot->sendMessage(Chat_Id, "Open!");
}

void contolPIR() {                               //---------- Control Pir ---------------------
  if ( Pir_On == 1 ) {
    Pir_On = 0;
  } else {
    Pir_On = 1;
  }
  if (Pir_On != EEPROM.read(95)) {
    EEPROM.write(95, Pir_On);
    EEPROM.commit();
  }
}

void contolLUZ() {                               //---------- Control Light ---------------------
  if ( lightState >= 2 ) {
    lightState = 0;
  } else {
    lightState ++;
  }
  if ( lightState == 1 ) {
    Lightmode = "On";
    digitalWrite(LIGHT_PIN, HIGH);
  } else if ( lightState == 0 ) {
    Lightmode = "Off";
    digitalWrite(LIGHT_PIN, LOW);
  } else {
    Lightmode = "Auto";
    if (!digitalRead(PIN_PIR_IN)) digitalWrite(LIGHT_PIN, LOW);
  }
  if (lightState != EEPROM.read(96)) {
    EEPROM.write(96, lightState);
    EEPROM.commit();
  }
}

void ejecutaAccionPir(bool flash) {                             //----------Accion PIR ---------------------
  bool isFlashOn = digitalRead(FLASH_LED_PIN);
  if ((flash) && (!isFlashOn)) {
    digitalWrite(FLASH_LED_PIN, HIGH);
  }
  if ((!isFlashOn) && (!esdedia)) {
    digitalWrite(FLASH_LED_PIN, HIGH);
  }
  delay(100);
  camera_fb_t * fb = NULL;  // fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    bot->sendMessage(Chat_Id, "Camera capture failed", "");
    return;
  }

  currentByte = 0;
  fb_length = fb->len;
  fb_buffer = fb->buf;

  digitalWrite(FLASH_LED_PIN, isFlashOn);

  String sent = bot->sendMultipartFormDataToTelegramWithCaption("sendPhoto", "photo", "img.jpg",
                "image/jpeg", String(caption_txt), Chat_Id, fb->len,
                isMoreDataAvailable, getNextByte, nullptr, nullptr);

  Serial.println("done!");
  esp_camera_fb_return(fb);
  fb_buffer = NULL;
}

void printGmtTime(){
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}
/*void printDate(time_t date) {
  char buff[20];
  sprintf(buff, "%2d-%02d-%4d %02d:%02d:%02d",
          day(date), month(date), year(date), hour(date), minute(date), second(date));
  Serial.print(buff);
}*/
void handleNewMessages(int numNewMessages) {                   //---------- Telegram BOT handle New Messages----------------------
  Serial.print("handle New Messages: ");
  Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++) {

    String text = bot->messages[i].text;
    String from_name = bot->messages[i].from_name;
    String from_id = bot->messages[i].chat_id;
    String date = bot->messages[i].date;
    unsigned long date2 = date.toInt();
    time_t seconds; seconds = time(NULL);
    long desdeUltimoMen = (seconds + 60) - date2;
    if (from_name == "") from_name = "Guest";

    if (from_id != Chat_Id) {
      Serial.print("invalid access from: ");
      Serial.println(from_name);
      Serial.print("with content: ");
      Serial.println(text);
      //bot->sendMessage(Chat_Id, "Access attempt from: " + from_name + " with content: " + text + " ID:" + from_id, "");
      bot->sendMessage(from_id, "NOT authorized! .. ID:" + from_id , "");
      return;
    }

    if (text == "/gmt" || text == "/Gmt" ) {
      struct tm timeinfo;
      if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain GMT time");
        bot->sendMessage(Chat_Id, "no GMT time, updating time now", "");
        configTime( 0, 0, ntpServer);
        return;
      }
      tmElements_t  tm;
      tm.Second = timeinfo.tm_sec;
      tm.Minute = timeinfo.tm_min;
      tm.Hour   = timeinfo.tm_hour;
      tm.Day    = timeinfo.tm_mday;
      tm.Month  = timeinfo.tm_mon + 1;
      tm.Year   = timeinfo.tm_year - 70;
      time_t s_date = makeTime(tm);
      time_t sRise = sm.sunRise(s_date);
      time_t sSet  = sm.sunSet(s_date);
      bot->sendSilentMessage(Chat_Id, "Date " + String(timeinfo.tm_mday) + "/" + String(timeinfo.tm_mon + 1) + "/" + String(timeinfo.tm_year + 1900) + " Time " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec) + " GMT .\n" + "Sun Rise at: " + String(hour(sRise)) + " hours and " + String(minute(sRise)) + " minutes GMT .\n" + "Sun Set at: " + String(hour(sSet)) + " hours and " + String(minute(sSet)) + " minutes GMT" , "");
    }

    if (desdeUltimoMen > 120) {
      Serial.print("seconds since Telegram was sent: ");
      Serial.println(desdeUltimoMen - 60);
      bot->sendMessage(Chat_Id, "Message after more than 1 minute are not processed");
      configTime( 0, 0, ntpServer);
      return;
    }

    if (text == "/Door1") {
      manIObra();
      bot->sendSilentMessage(Chat_Id, "Ok Door1");
    }

    if (text == "/Door2") {
      manIObra2();
      bot->sendSilentMessage(Chat_Id, "Ok Door2");
    }

    if (text == "/ota") {
      bot->sendSilentMessage(Chat_Id, WiFi.localIP().toString() + ":81/ota\n" +  "you must be connected to the same local network");
    }

    if (text == "/PO") {
      if (EstadoPuerta) {
        bot->sendSilentMessage(Chat_Id, "Closing");
        manIObra();
      } else {
        bot->sendChatAction(Chat_Id, "typing");
        peaTonal();
        bot->sendSilentMessage(Chat_Id, "PO");
      }
    }

    if (text == "/flash" || text == "/Flash" ) {
      flashState = !flashState;
      digitalWrite(FLASH_LED_PIN, flashState);
      bot->sendSilentMessage(Chat_Id, "Ok /Flash is: " + String(flashState ? "On" : "Off"));
    }

    if (text == "/PhotoF" || text == "/photof") {
      bot->sendChatAction(Chat_Id, "upload_photo");
      ejecutaAccionPir(true);
    }

    if (text == "/Photo" ) {
      bot->sendChatAction(Chat_Id, "upload_photo");
      ejecutaAccionPir(false);
    }

    if (text == "/pir" || text == "/Pir" || text == "/PIR" ) {
      Serial.println("Recived Pir");
      contolPIR();
      bot->sendSilentMessage(Chat_Id, "Ok Sensor /Pir is: " + String(Pir_On ? "On" : "Off"));
    }

    if (text == "/Light" || text == "/light" || text == "/LIGHT" ) {
      contolLUZ();
      bot->sendSilentMessage(Chat_Id, "Ok /Light is: " + String(Lightmode));
    }

    if (text == "/State") {
      // bot->sendChatAction(Chat_Id, "typing");
      bot->sendSilentMessage(Chat_Id, "Door is: " + String(EstadoPuerta ? "Open!" : "Closed!") + ".\n" +  "Flash is: " + String(flashState ? "On" : "Off") + ".\n" + "Pir sensor is: " + String(Pir_On ? "On" : "Off") + ".\n" + "Light is: " + String(Lightmode) + ".\n" + "Day or night: " + String(esdedia ? "Day" : "Night"));
    }

    if (text == "/key" || text == "/Key") {
      // bot->sendChatAction(Chat_Id, "typing");
      String keyboardJson = "[[\"/Door1\", \"/Door2\", \"/PO\", \"/Pir\", \"/PhotoF\"],[\"/State\", \"/Flash\", \"/Light\", \"/Photo\"]]";
      bot->sendMessageWithReplyKeyboard(Chat_Id, "Keyboard", "", keyboardJson, true);
    }

    if (text == "/start" || text == "/Help"  || text == "/help" ) {
      String welcome = "Welcome to ESP32Cam Telegram bot.\n\n";
      welcome += "/Photo : will take a photo\n";
      welcome += "/PhotoF : will take a photo with flash\n";
      welcome += "/Flash : toggle flash LED\n";
      welcome += "/Light : toggle Light On-Off-Auto\n";
      welcome += "/Pir : toggle pir sensor On-Off\n";
      welcome += "/State : will report status\n";
      welcome += "/Key : will send keyboard\n";
      bot->sendSilentMessage(Chat_Id, welcome, "Markdown");
    }
  }
}

void setup() {  // ------------------------------------------------------------- SETUP -------------------------------------------------------------------

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  EEPROM.begin(512);
  digitalWrite(PIN_MANIOBRA, LOW);
  digitalWrite(PIN_MANIOBRA2, LOW);
  pinMode(PIN_MANIOBRA, OUTPUT);
  pinMode(PIN_MANIOBRA2, OUTPUT);
  pinMode(CONFIG_PIN, INPUT_PULLUP);
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, HIGH);
  pinMode(PIN_PIR_IN, INPUT_PULLDOWN);
  pinMode(SENSOR_PIN, INPUT_PULLUP);

  PIR_state = digitalRead(SENSOR_PIN);
  last_PIR_state = PIR_state;
  EstadoPuerta = digitalRead(SENSOR_PIN);
  ultimoEstadoPuerta = EstadoPuerta;
        float _latitude;
        float _longitude;

  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {                 //---------- WiFi Config ----------------------
    Serial.println("SPIFFS Mount Failed !Format!");
    initialConfig = true;
  } else {
    Serial.println("mounted file system");
    File configFile = SPIFFS.open (CONFIG_FILE, "r");
    if (!configFile || configFile.isDirectory()) {
      Serial.println("- failed to open file for reading");
    } else {
      Serial.println("opened config file");
      size_t size = configFile.size();
      std::unique_ptr<char[]> buf(new char[size]);
      configFile.readBytes(buf.get(), size);
      DynamicJsonDocument json(1024);
      DeserializationError deserializeError = deserializeJson(json, buf.get());
      serializeJsonPretty(json, Serial);
      if (!deserializeError) {
        Serial.println("\nparsed json");
        if (json.containsKey("caption_txt")) strcpy(caption_txt, json["caption_txt"]);
        if (json.containsKey("BotToken")) strcpy(BotToken, json["BotToken"]);
        if (json.containsKey("Chat_Id")) strcpy(Chat_Id, json["Chat_Id"]);
        if (json.containsKey("Latitude")) strcpy(Latitude, json["Latitude"]);
        if (json.containsKey("Longitude")) strcpy(Longitude, json["Longitude"]);
        if (json.containsKey("Router_SSID")) strcpy(Router_SSID, json["Router_SSID"]);
        if (json.containsKey("Router_Pass")) strcpy(Router_Pass, json["Router_Pass"]);        
        _latitude = atof(Latitude);
        _longitude = atof(Longitude);
      } else {
        Serial.println("failed to load json config");
        initialConfig = true;
      }
      configFile.close();
    }
  }

  WiFi.mode(WIFI_STA);
  WiFi.setHostname("ESP32Telegram");
  Serial.print("setup running on core ");
  Serial.println(xPortGetCoreID());

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_UXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
  config.jpeg_quality = 10;  // era 6
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
  }
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_SXGA);  //  FRAMESIZE_UXGA (1600 x 1200) --- FRAMESIZE_SXGA (1280 x 1024)
  s->set_brightness(s, 1);     // -2 to 2
  s->set_contrast(s, 0);       // -2 to 2
  s->set_saturation(s, 0);     // -2 to 2
  s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
  s->set_whitebal(s, 1);       // 0 = disable , 1 = enable  -- white balance
  s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable  -- auto white balance?
  s->set_wb_mode(s, 0);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
  s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable  -- auto exposure off
  s->set_aec2(s, 0);           // 0 = disable , 1 = enable  -- automatic exposure sensor?
  s->set_ae_level(s, 1);       // -2 to 2                   -- auto exposure levels
  s->set_aec_value(s, 600);    // 0 to 1200                 -- automatic exposure correction?
  s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable  -- auto gain off
  s->set_agc_gain(s, 0);       // 0 to 30                   -- set gain manually
  s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
  s->set_bpc(s, 0);            // 0 = disable , 1 = enable  -- black pixel correction
  s->set_wpc(s, 1);            // 0 = disable , 1 = enable  -- white pixel correction
  s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
  s->set_lenc(s, 1);           // 0 = disable , 1 = enable  -- lens correction?
  s->set_hmirror(s, 1);        // 0 = disable , 1 = enable  -- flip horizontally
  s->set_vflip(s, 1);          // 0 = disable , 1 = enable  -- flip vertically
  s->set_dcw(s, 0);            // 0 = disable , 1 = enable  -- downsize enable?
  s->set_colorbar(s, 0);       // 0 = disable , 1 = enable  -- testcard

  bot = new UniversalTelegramBot(BotToken, client);                   //---------- Telegram BOT ----------------------
  bot->longPoll = 0;

  Pir_On = EEPROM.read(95);                         //----------EEPROM Estado Pir  ----------------------
  lightState = EEPROM.read(96);                     //----------EEPROM Estado Light  ----------------------
  if ( lightState == 1 ) {
    Lightmode = "On";
    digitalWrite(LIGHT_PIN, HIGH);
  } else if ( lightState == 0 ) {
    Lightmode = "Off";
    digitalWrite(LIGHT_PIN, LOW);
  } else {
    Lightmode = "Auto";
    if (!digitalRead(PIN_PIR_IN)) digitalWrite(LIGHT_PIN, LOW);
  }
  pinMode(LIGHT_PIN, OUTPUT);

  OtaServer.on("/ota", HTTP_GET, []() {                   //---------- OTA ----------------------
    if (!OtaServer.authenticate(www_username, www_password)) {
      return OtaServer.requestAuthentication();
    }
    OtaServer.sendHeader("Connection", "close");
    OtaServer.send(200, "text/html", serverIndex);
  });
  OtaServer.on("/update", HTTP_POST, []() {               //---------- OTA ----------------------
    OtaServer.sendHeader("Connection", "close");
    OtaServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    delay(3000);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = OtaServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      SPIFFS.end();
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        //OtaServer.send(200, "text/plain", "¡Completado!");
      } else {
        Update.printError(Serial);
        //OtaServer.send(200, "text/plain", "¡Fallo!");
      }
    }
  });

  ticker.attach(1.0, tick);
  client.setInsecure();
  if (initialConfig == true) {                             //---------- WiFi Config ----------------------
    Serial.println("initial config...");
    ondemandwifiCallback();
  }
  WiFi.begin(Router_SSID, Router_Pass);

  sm.init( 0, _latitude, _longitude);

}

void loop() {

  int C_W_read = digitalRead(CONFIG_PIN);          //---------- WiFi Config ----------------------
  if (C_W_read != last_C_W_state) {
    time_last_C_W_change = millis();
  }
  if ((millis() - time_last_C_W_change) > C_W_delay) {
    if (C_W_read != C_W_state) {
      Serial.println("Triger sate changed");
      C_W_state = C_W_read;
      if (C_W_state == LOW) {
        ondemandwifiCallback();
      }
    }
  }
  last_C_W_state = C_W_read;

  if (shouldSaveConfig) {                           //---------- WiFi Config ----------------------
    Serial.println(" config...");
    DynamicJsonDocument json(1024);
    json["caption_txt"] = caption_txt;
    json["BotToken"] = BotToken;
    json["Chat_Id"] = Chat_Id;
    json["Latitude"] = Latitude;
    json["Longitude"] = Longitude;
    json["Router_SSID"] = Router_SSID;
    json["Router_Pass"] = Router_Pass;
    File configFile = SPIFFS.open (CONFIG_FILE, "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }
    serializeJsonPretty(json, Serial);
    serializeJson(json, configFile);
    configFile.close();
    Serial.println("Config written successfully");
    shouldSaveConfig = false;
    initialConfig = false;
    WiFi.mode(WIFI_STA);
    delay(5000);
    ESP.restart();
  }

  if (WiFi.status() == WL_CONNECTED) {

    if (pr_wifi == true) {
      Serial.println("");
      Serial.println("CONNECTED");
      Serial.print("local IP: ");
      Serial.println(WiFi.localIP());
      Serial.print("subnetMask: ");
      Serial.println(WiFi.subnetMask());
      Serial.print("gatewayIP: ");
      Serial.println(WiFi.gatewayIP());
      Serial.print("Signal Strength (RSSI): ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
      pr_wifi = false;

      configTime( 0, 0, ntpServer);   //---------- NTP ----------------------
      printGmtTime();

      if (tikOn) {
        ticker.detach();
        digitalWrite(RED_LED_PIN, LOW);
        tikOn = false;
      }

      if (!MDNS.begin(host)) {
        Serial.println("Error setting up MDNS responder!"); //---------- OTA ----------------------
      }
      OtaServer.begin();
    }

    if (millis() - Bot_lasttime > Bot_mtbs)  {           //---------- Telegram BOT ----------------------
      bool new_telegram_message = false;
      int numNewMessages = bot->getUpdates(bot->last_message_received + 1);
      while (numNewMessages) {
        // Serial.println("got response");
        handleNewMessages(numNewMessages);
        numNewMessages = bot->getUpdates(bot->last_message_received + 1);
        new_telegram_message = true;
      }
      if (!new_telegram_message) {
        unadecinc ++;
        if (unadecinc >= 5) {
          camera_fb_t * fb = NULL;  // fb = NULL;            
          fb = esp_camera_fb_get();
          esp_camera_fb_return(fb);
          fb_buffer = NULL;
          unadecinc = 0;
        }

        int lecturaSensorPuerta = digitalRead(SENSOR_PIN);    
        if (lecturaSensorPuerta != ultimoEstadoPuerta) {
          tiempoUltimoEstadoPuerta = millis();
        }
        if ((millis() - tiempoUltimoEstadoPuerta) > RetardoEstadoPuerta) {
          if (lecturaSensorPuerta != EstadoPuerta) {
            EstadoPuerta = lecturaSensorPuerta;
            if (EstadoPuerta == HIGH) {
              ejecutaAccionA();
            } else {
              ejecutaAccionC();
            }
          }
        }
        ultimoEstadoPuerta = lecturaSensorPuerta;

      }
      Bot_lasttime = millis();
    }

    OtaServer.handleClient();                             //---------- OTA ----------------------

    int PIR_read = digitalRead(PIN_PIR_IN);    //------------  Lectura estado sensor SENSOR_PIR   -----------------
    if (PIR_read != last_PIR_state) {
      time_last_PIR_change = millis();
    }
    if ((millis() - time_last_PIR_change) > PIR_delay) {
      if (PIR_read != PIR_state) {
        PIR_state = PIR_read;
        if (!tikOn) {
          if (digitalRead(RED_LED_PIN) != PIR_state) {
            digitalWrite(RED_LED_PIN, PIR_state);
          }
        }
        if (PIR_state == HIGH) {
          Serial.println("Triger PIR sate changed");
          if (Pir_On) {
            ejecutaAccionPir(true);
          }
        }
        if ( lightState == 2 ) {
          digitalWrite(LIGHT_PIN, PIR_read);
          Serial.println("PIR Triger Light sate change");
        }
      }
    }
    last_PIR_state = PIR_read;

  } else {
    WiFi_up();
    delay(100);
  }

  if (((millis() - time_last_D_N) > D_N_delay) && (!pr_wifi)) {
    DayNight();
    time_last_D_N = millis();
  }
}

void WiFi_up() { // conect to wifi

  if ((millis() - wifimilis) > wifi_checkDelay) {
    if (WiFi_begin) {
      WiFi.begin(Router_SSID, Router_Pass);
      Serial.println("CONNECT WIFI");
      WiFi_begin = false;
    } else {
      WiFi.disconnect();
      WiFi.reconnect();
      Serial.println("Reconnect WIFI");
    }
    pr_wifi = true;
    tikOn = true;
    wifimilis = millis() ;
    ticker.attach(1.0, tick);
  }
}

void DayNight() {

  time_t seconds; seconds = time(NULL);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain GMT time");
    configTime( 0, 0, ntpServer);
  }
  tmElements_t  tm;
  tm.Second = timeinfo.tm_sec;
  tm.Minute = timeinfo.tm_min;
  tm.Hour   = timeinfo.tm_hour;
  tm.Day    = timeinfo.tm_mday;
  tm.Month  = timeinfo.tm_mon + 1;
  tm.Year   = timeinfo.tm_year - 70;
  time_t s_date = makeTime(tm);
  time_t sRise = sm.sunRise(s_date);
  time_t sSet  = sm.sunSet(s_date);

  if ((sRise < seconds) && (sSet > seconds)) {
    if (!esdedia) {
     // bot->sendMessage(Chat_Id, "sunrise", "");
      Serial.println("sunrise");
    }
    esdedia = true;
  } else {
    if (esdedia) {
     // bot->sendMessage(Chat_Id, "sunset", "");
      Serial.println("sunset");
    }
    esdedia = false;
  }
}

