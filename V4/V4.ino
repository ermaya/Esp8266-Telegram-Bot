
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all rs to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/kentaylor/WiFiManager
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <OneWire.h>                
#include <DallasTemperature.h>
WiFiUDP ntpUDP;
#define BOT_TOKEN_LENGTH 46
#define DEFAULT_CHAT_LENGTH 10
#define DEFAULT_EEPROM 100
#define PIN_LUZ D0         //LUZ
#define PIN_MANIOBRA D1    //MANIOBRA
#define PIN_MANIOBRA2 D2   //MANIOBRA2
const int PIN_CONFIGURACION = D3; //When high will reset the wifi manager config
#define PIN_AUX D4         //AUX
IRsend irsend(D5);         // Pin Led IR
#define PIN_PIR_IN D6      //SENSOR PIR         (pulled down to ground)
#define PIN_SENSOR_C D7    //SENSOR ABIERTO CERRADO     (pulled down to ground) low=Abierto high=Cerrado
#define LED_ESTADO D8     //LED ESTADO Y PIR ON-OFF  D8
#define pin_temp_ds 3      //  Se declara el pin donde se conectará la DATA RX

#include <Ticker.h>      //for LED status
Ticker ticker;
OneWire ourWire(pin_temp_ds); 
DallasTemperature sensors(&ourWire);
char botToken[46] = "";
char defaultChatId[12] = "";
char defaultChatId2[12] = "";
bool initialConfig = false;    // Indicates whether ESP has WiFi credentials saved from previous session
volatile bool sensorPirFlag = false;
boolean controlluz = 1;
boolean controlaux = 1;
boolean PirHighLow = 1;       //estado inician sensores
String esTadoPir = "Pir Encendido";
String esTadoLuz = "Apagada";
String esTadoAux2 = "Apagado";
String esTadoPuerta = "Cerrada";
char defaulteeprom[DEFAULT_EEPROM] = "";
long C_Termostato = 20;
WiFiClientSecure client;
UniversalTelegramBot *bot;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 0, 1800000);                 //-----1800 Segundos---------   0, 60000
int estado_boton_C_W = HIGH;                
int ultimoEstado_boton_C_W = HIGH;            
unsigned long tiempoUltimoCambio_boton_C_W = 0;    
long RetardoCambio_boton_C_W = 5000;               // Tiempo de pulsacion para entrar en configuracion
int EstadoPuerta = HIGH;                
int ultimoEstadoPuerta = HIGH;            
unsigned long tiempoUltimoEstadoPuerta = 0;    
long RetardoEstadoPuerta = 100;               // retarde de lectura sensor puerta
int Bot_mtbs = 2500;                   //tiempo entre comprobacion de nuevo mensajes
unsigned long Bot_lasttime;            
int Chek_net = 300000;                 //intervalo para comprobar el reloj
unsigned long Chek_lasttime;           
unsigned long Chek_time = 1508940474;
int Chek_net2 = 0;
int vez_comprobacion = 2;  
bool Start = false;
int ledStatus = 0;
int ajuste_bot= 0;

bool shouldSaveConfig = false;         //flag for saving data

//AireOn
uint16_t  rawData[115] = {3236,8922, 578,1518, 578,470, 578,468, 580,468, 578,470, 578,470, 578,468, 580,468, 580,468, 578,1516, 582,468, 578,470, 578,1516, 580,468, 578,1516, 580,1516, 580,468, 578,1518, 578,1518, 578,1516, 578,1516, 578,1518, 578,1516, 578,1516, 580,1516, 580,470, 578,470, 580,468, 578,468, 580,468, 580,468, 578,1518, 578,1518, 580,468, 580,468, 580,468, 578,470, 578,1516, 580,470, 578,1516, 580,1516, 580,468, 578,1518, 580,470, 580,468, 580,468, 578,1518, 580,470, 580,468, 580,468, 580,468, 580,468, 580,1516, 580,1516, 580,1516, 580,1516, 582};
//AireOff
uint16_t  rawData2[115] = {3200,8968, 518,1580, 542,506, 542,506, 542,508, 540,508, 542,506, 544,506, 518,532, 542,506, 542,1556, 544,506, 542,508, 542,506, 542,1554, 542,1554, 542,1554, 542,508, 540,1554, 542,1554, 542,1556, 542,1554, 542,1556, 544,1554, 542,1554, 518,1580, 542,508, 542,506, 542,506, 542,506, 542,508, 542,506, 518,1580, 542,1556, 542,508, 542,506, 542,506, 542,508, 516,1580, 542,506, 542,1556, 542,1556, 544,506, 540,1556, 544,508, 540,1556, 542,1556, 544,506, 542,506, 542,506, 542,506, 542,506, 542,508, 540,508, 542,506, 542,1554, 542,1556, 544};


//callback notifying us of the need to save config
void saveConfigCallback () {                   //---------- Save config ---------------------
  //Serial.println("Should save config");
  shouldSaveConfig = true;
}

void readBotTokenFromEeprom(int offset){        //---------- Token de mem ---------------------
  for(int i = offset; i<BOT_TOKEN_LENGTH; i++ ){
    botToken[i] = EEPROM.read(i);
  }
  EEPROM.commit();
}

void writeBotTokenToEeprom(int offset){         //---------- Token a mem ---------------------
  for(int i = offset; i<BOT_TOKEN_LENGTH; i++ ){
    EEPROM.write(i, botToken[i]);
  }
  EEPROM.commit();
}

void readDefaultChatIdFromEeprom(int offset){   //---------- ChadID de mem ---------------------
  for(int i = offset; i<DEFAULT_CHAT_LENGTH; i++ ){
    defaultChatId[i] = EEPROM.read(i+60);
  }
  EEPROM.commit();
}

void writeDefaultChatIdToEeprom(int offset){    //---------- ChadID a mem ---------------------
  for(int i = offset; i<DEFAULT_CHAT_LENGTH; i++ ){
    EEPROM.write(i+60, defaultChatId[i]);
  }
  EEPROM.commit();
}
void readDefaultChatId2FromEeprom(int offset){   //---------- ChadID2 de mem ---------------------
  for(int i = offset; i<DEFAULT_CHAT_LENGTH; i++ ){
    defaultChatId2[i] = EEPROM.read(i+80);
  }
  EEPROM.commit();
}

void writeDefaultChatId2ToEeprom(int offset){    //---------- ChadID2 a mem ---------------------
  for(int i = offset; i<DEFAULT_CHAT_LENGTH; i++ ){
    EEPROM.write(i+80, defaultChatId2[i]);
  }
  EEPROM.commit();
}

void tick()                                      //---------- Tiker ---------------------
{
  //toggle state
  int state = digitalRead(LED_ESTADO);  // get the current state of D4 pin
  digitalWrite(LED_ESTADO, !state);     // set pin to the opposite state
}
void interuptSensorPir() {                        //---------- Interupt Pir ---------------------
  if ( PirHighLow == 0 ) return;
  int sensorP = digitalRead(PIN_PIR_IN);
  if(sensorP == HIGH){
  //Serial.println("interuptSensorPir");
    sensorPirFlag = true;
  }
  return;
}
void manIObra() {                                //---------- Maniobra  ----------------------
  digitalWrite(PIN_MANIOBRA, LOW);//Activa salida
  delay (500);//Retardo
  digitalWrite(PIN_MANIOBRA, HIGH);//Desactiva salida
}
void manIObra2() {                                //---------- Maniobra2  ----------------------
  digitalWrite(PIN_MANIOBRA2, LOW);//Activa salida
  delay (500);//Retardo
  digitalWrite(PIN_MANIOBRA2, HIGH);//Desactiva salida
}
void peaTonal() {                                //---------- PP ---------------------
  digitalWrite(PIN_MANIOBRA, LOW);//Activa salida
  delay (500);//Retardo
  digitalWrite(PIN_MANIOBRA, HIGH);//Desactiva salida
  delay (8000);//Retardo
  digitalWrite(PIN_MANIOBRA, LOW);//Activa salida
  delay (500);//Retardo
  digitalWrite(PIN_MANIOBRA, HIGH);//Desactiva salida
}
void aire_on() {                                //---------- Aire ON --------------------- 
  irsend.sendRaw(rawData, 115, 38);  // Send a raw data capture at 38kHz.
}
void aire_off() {                                //---------- Aire OFF ---------------------
  irsend.sendRaw(rawData2, 115, 38);  // Send a raw data capture at 38kHz.
}
void contolPIR() {                               //---------- Control Pir ---------------------
  if ( PirHighLow == 1 ) {
    //Serial.println("Apagando");
    PirHighLow = 0;
    esTadoPir =  String("Pir Apagado");
    digitalWrite(LED_ESTADO, LOW); //LED is active low
    if (PirHighLow != EEPROM.read(95)) EEPROM.write(95,PirHighLow);
  } else {
    //Serial.println("Encendiendo");
    PirHighLow = 1;
    esTadoPir =  String("Pir Encendido");
    digitalWrite(LED_ESTADO, HIGH);
    if (PirHighLow != EEPROM.read(95)) EEPROM.write(95,PirHighLow);  
 }
 EEPROM.commit(); 
}

void contolLUZ() {                                //---------- Control Luz ---------------------
  if ( controlluz == 1 ) {
    controlluz = 0;
    esTadoLuz =  String("Encendida");
    digitalWrite(PIN_LUZ, LOW); 
    if (controlluz != EEPROM.read(96)) EEPROM.write(96,controlluz);

  } else {
    controlluz = 1;
    esTadoLuz =  String("Apagado");
    digitalWrite(PIN_LUZ, HIGH);
    if (controlluz != EEPROM.read(96)) EEPROM.write(96,controlluz);
 }
 EEPROM.commit();
}
void contolAUX() {                                 //---------- Control Aux ---------------------
  if ( controlaux == 1 ) {
    controlaux = 0;
    esTadoAux2 =  String("Encendido");
    digitalWrite(PIN_AUX, LOW); 
    if (controlaux != EEPROM.read(97)) EEPROM.write(97,controlaux);
  } else {
    controlaux = 1;
    esTadoAux2 =  String("Apagado");
    digitalWrite(PIN_AUX, HIGH);
    if (controlaux != EEPROM.read(97)) EEPROM.write(97,controlaux);
 }
 EEPROM.commit();
}
void ejecutaAccionPir() {                             //----------Accion PIR ---------------------
  // Changed to send a Telegram message
  //pin D6();  Pir in
   bot->sendMessage(defaultChatId2, "Movimiento Detectado!");
   sensorPirFlag = false;
}

void ejecutaAccionC() {                              //----------Accion C ---------------------
  // Changed to send a Telegram message
  bot->sendMessage(defaultChatId, "Cerrada!");
  esTadoPuerta =  String("Cerrada");
}

void ejecutaAccionA() {                               //----------Accion A ---------------------
  // Changed to send a Telegram message
  bot->sendMessage(defaultChatId, "Abierta!");
  esTadoPuerta =  String("Abierta");
}
void ejecutaTemp() {                               //----------Temperatura ---------------------
      //Serial.end();
      sensors.requestTemperatures();   //Se envía el comando para leer la temperatura
      //delay (150);//Retardo
      float temp= sensors.getTempCByIndex(0); //Se obtiene la temperatura en ºC
      //Serial.begin(115200);
}
void handleNewMessages(int numNewMessages) {         // ---------- Telgram functions ------------
  for (int i=0; i<numNewMessages; i++) { 
    String chat_id = String(bot->messages[i].chat_id);
    String text = bot->messages[i].text;
    String date = bot->messages[i].date;
    unsigned long date2 = date.toInt();       // convert 'String' to 'long
    if (text == "/Date") {
      bot->sendChatAction(chat_id, "typing");
    bot->sendMessage(chat_id,timeClient.getFormattedTime() + ".\n" + String(chat_id));
    }
    if(chat_id != defaultChatId) return;     // reject messages from other chats
    //timeClient.update();      //---------------------test 09.12.17-----------------------------------
    //Serial.print("Hora Actual: ");
    //Serial.println(timeClient.getEpochTime());
    //Serial.print("Hora mensaje ");
    //Serial.println(date);
    
    if (text == "/Resetesp8266") {        //reinicio remoto
      bot->sendChatAction(chat_id, "typing");
    accionReset();
    }
    if (timeClient.getEpochTime()< date2)  {
     bot->sendMessage(chat_id, "------"); //"Parece que la hora esta mal intento sincronizar"
     timeClient.forceUpdate();
     //return;
    }
    long desdeUltimoMen =  (timeClient.getEpochTime() + 100) - date2;
    //Serial.print("segundos del mensaje + 100 ");
    //Serial.println(desdeUltimoMen );
    if (desdeUltimoMen > 160) bot->sendMessage(chat_id, "Mensaje despues de mas de 1 minuto no se procesan");
    if (desdeUltimoMen > 160) return;
       
    String from_name = bot->messages[i].from_name;
    if (from_name == "") from_name = "Guest";

    if (text == "/Puerta") {
      manIObra();
      bot->sendMessage(chat_id, "Maniobra Puerta");
    }
    if (text == "/Puerta2") {
      manIObra2();
      bot->sendMessage(chat_id, "Maniobra Puerta2");
    }
    if (text == "/PP") {
      bot->sendChatAction(chat_id, "typing");
      peaTonal();
      bot->sendMessage(chat_id, "Maniobra PP");
    }
     if (text == "/AireOn") {
      aire_on();
      bot->sendMessage(chat_id, "Aire ON 26º Calentar");
    }
    if (text == "/AireOff") {
      aire_off();
      bot->sendMessage(chat_id, "Aire OFF");
    }
    if (text == "/Pir") {
      contolPIR();
      bot->sendMessage(chat_id, "Ok Sensor de Movimiento " + String(esTadoPir));
    }
    if (text == "/Luz") {
      contolLUZ();
      bot->sendMessage(chat_id, "Luz " + String(esTadoLuz));
    }
    if (text == "/Aux") {
      contolAUX();
      bot->sendMessage(chat_id, "Aux " + String(esTadoAux2));
    }
    if (text == "/Temp") {
      //Serial.end();
      bot->sendChatAction(chat_id, "typing");
      sensors.requestTemperatures();   //Se envía el comando para leer la temperatura
      float temp= sensors.getTempCByIndex(0); //Se obtiene la temperatura en ºC
      bot->sendMessage(chat_id, "temperatura "+ String(temp)+"º"); 
      //Serial.begin(115200); 
    }
    if (text == "/Estado") {
      //Serial.end();
      bot->sendChatAction(chat_id, "typing");
      sensors.requestTemperatures();   //Se envía el comando para leer la temperatura
      float temp= sensors.getTempCByIndex(0); //Se obtiene la temperatura en ºC
      bot->sendMessage(chat_id, "Puerta " + String(esTadoPuerta)+ ".\n" + "Sensor " + String(esTadoPir)+ ".\n"+ "Luz " + String(esTadoLuz)+ ".\n"+ "Aux " + String(esTadoAux2)+ ".\n"+"temperatura "+ String(temp)+"º" + ".");
      //Serial.begin(115200);
    }
    if (text == "/tecla" || text == "/Tecla") {
      bot->sendChatAction(chat_id, "typing");
      String keyboardJson = "[[\"/Puerta\", \"/PP\", \"/Puerta2\", \"/Luz\", \"/Aux\"],[\"/Estado\",\"/Pir\", \"/AireOn\", \"/AireOff\", \"/Temp\"]]";
      bot->sendMessageWithReplyKeyboard(chat_id, "Teclado", "", keyboardJson, true);
    }

    if (text == "/Ayuda" || text == "/ayuda") {
      bot->sendChatAction(chat_id, "typing");
      String welcome = "El Maya le da la Bienvenida, " + from_name + ".\n";
      welcome += "Funciones disponibles.\n";
      welcome += "/Puerta= Maniobra\n";
      welcome += "/PP= Paso Peatonal\n";
      welcome += "/Puerta2= Maniobra2\n";
      welcome += "/Pir : Conecta o Desconecta el Sensor de Movimiento\n";
      welcome += "/AireOn : Enciende el AA\n";
      welcome += "/AireOff : Apaga el AA\n";
      welcome += "/Aux : Aux 0-1\n";
      welcome += "/Luz : Luz 0-1\n";
      welcome += "/Temp : Temperatura Ambiente\n";
      welcome += "/Estado : Indica el estado de Puerta,Sensor de Movimiento,Luz,Etc.\n";
      welcome += "/tecla : Teclado\n";
      bot->sendMessage(chat_id, welcome, "Markdown");
    }
  }
}
void accionReset(){                        //----------------------ResetAction---------------------------
  int numNewMessages = bot->getUpdates(bot->last_message_received + 1);
    while(numNewMessages) {
      //Serial.println("tengo Mensaje");
      handleNewMessages(numNewMessages);
      numNewMessages = bot->getUpdates(bot->last_message_received + 1);
      }
      bot->sendMessage(defaultChatId, "Entendido!");
      delay (8000);//Retardo
  ESP.restart();
}
 // is configuration portal requested?     //------------------Configuration portal requested--------------
  void accionConfiguracion(){
     //Serial.println("Configuration portal requested."); //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManagerParameter p_botToken("botid", "Bot Token", botToken, 46);
  WiFiManagerParameter p_defaultChatId("chatid", "chat id", defaultChatId, 12);
  WiFiManagerParameter p_defaultChatId2("chatid2", "chat id2", defaultChatId2, 12);
  WiFiManager wifiManager;
  wifiManager.setMinimumSignalQuality(10);
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  //Adding an additional config on the WIFI manager webpage for the bot token
   wifiManager.addParameter(&p_botToken);
   WiFiManagerParameter custom_token("<p>TelegramBot Token</p>");
   wifiManager.addParameter(&custom_token);
   wifiManager.addParameter(&p_defaultChatId);
   WiFiManagerParameter custom_chad("<p>Chad ID Control</p>");
   wifiManager.addParameter(&custom_chad);
   wifiManager.addParameter(&p_defaultChatId2);
   WiFiManagerParameter custom_chad2("<p>Chad ID Aviso Pir</p>");
   wifiManager.addParameter(&custom_chad2);
  //If it fails to connect it will create a access point
  ticker.attach(0.2, tick);

    //it starts an access point 
    //if (!wifiManager.startConfigPortal()) {       // Ap nombre Esp+chipID ,Abierto
    if (!wifiManager.startConfigPortal("TelegramBot")) {      // Ap nombre TelegramBot no Pass 
      //Serial.println("Not connected to WiFi but continuing anyway.");
    } else {
      
      //Serial.println("connected...yeey :)");    //if you get here you have connected to the WiFi
    }
     strcpy(botToken, p_botToken.getValue());
     //Serial.println("read bot token");
     //Serial.println(botToken); 
     strcpy(defaultChatId, p_defaultChatId.getValue());
     //Serial.println("read chadID");
     //Serial.println(defaultChatId);
     strcpy(defaultChatId2, p_defaultChatId2.getValue());
     //Serial.println("read chadID2");
     //Serial.println(defaultChatId2);
     wifiManager.setSaveConfigCallback(saveConfigCallback);
     if (shouldSaveConfig) {
     writeBotTokenToEeprom(0);
     writeDefaultChatIdToEeprom(0);
     writeDefaultChatId2ToEeprom(0);
    } 
      ESP.reset(); // This is a bit crude. For some unknown reason webserver can only be started once per boot up 
    // so resetting the device allows to go back into config mode again when it reboots.
    delay(5000);
  }

// --- ----------------- ---


void setup() {                  //------------------------ SETUP-------------------------------- 
  //Serial.begin(115200);
  Serial.end();
  EEPROM.begin(DEFAULT_EEPROM);
  pinMode(PIN_CONFIGURACION, INPUT);
  pinMode(PIN_MANIOBRA, OUTPUT);
  pinMode(PIN_MANIOBRA2, OUTPUT);
  pinMode(PIN_LUZ, OUTPUT);
  pinMode(PIN_AUX, OUTPUT);
  pinMode(LED_ESTADO, OUTPUT); 
    // Initlaze the button
  pinMode(PIN_PIR_IN, INPUT);
  pinMode(PIN_SENSOR_C, INPUT);
  attachInterrupt(PIN_PIR_IN, interuptSensorPir, RISING);
  digitalWrite(PIN_MANIOBRA, HIGH); 
  digitalWrite(PIN_MANIOBRA2, HIGH);
  //digitalWrite(PIN_AUX, HIGH);
  ticker.attach(0.6, tick);   // start ticker with 0.5 because we start in AP mode and try to connect
  sensors.begin();   //Se inicia el sensor DS
    //Serial.println("read bot token");
    readBotTokenFromEeprom(0);
    //Serial.println(botToken);
    //Serial.println("read default chat id");
    readDefaultChatIdFromEeprom(0);
    //Serial.println(defaultChatId);
    readDefaultChatId2FromEeprom(0);
    //Serial.println(defaultChatId2);


  if (WiFi.SSID()==""){
    //Serial.println("We haven't got any access point credentials, so get them now");   
    initialConfig = true;
  }
  else{
    WiFi.mode(WIFI_STA); // Force to station mode because if device was switched off while in access point mode it will start up next time in access point mode.
    unsigned long startedAt = millis();
    //Serial.print("After waiting ");
    int connRes = WiFi.waitForConnectResult();
    float waited = (millis()- startedAt);
    //Serial.print(waited/1000);
    //Serial.print(" secs in setup() connection result is ");
    //Serial.println(connRes);
  }
    if (WiFi.status()!=WL_CONNECTED){
    //Serial.println("failed to connect, finishing setup anyway");
  } else{
    //Serial.print("local ip: ");
    //Serial.println(WiFi.localIP());
 }
  //ticker.attach(2.5, tick);
    ticker.detach();
    bot = new UniversalTelegramBot(botToken, client);
    timeClient.begin();
    timeClient.update();
    //Serial.print("Hora inicio: ");
    //Serial.println(timeClient.getEpochTime());

             
    PirHighLow=EEPROM.read(95);                      //----------EEPROM Estado Pir  ----------------------
    if (PirHighLow == 0){
    esTadoPir =  String("Pir Apagado");
    digitalWrite(LED_ESTADO, LOW); //LED is active low
    }
    if (PirHighLow == 1){
    esTadoPir =  String("Pir Encendido");
    digitalWrite(LED_ESTADO, HIGH); //LED is active low
    }
    controlluz=EEPROM.read(96);                      //----------EEPROM Estado Luz  ----------------------
    if (controlluz == 0){
    esTadoLuz =  String("Encendida");
    digitalWrite(PIN_LUZ, LOW); 
    }
    if (controlluz == 1){
    esTadoLuz =  String("Apagada");
    digitalWrite(PIN_LUZ, HIGH); 
    }
    controlaux=EEPROM.read(97);                      //----------EEPROM Estado Aux  ----------------------
    if (controlaux == 0){
    esTadoAux2 =  String("Encendido");
    digitalWrite(PIN_AUX, LOW);  
    }
    if (controlaux == 1){
    esTadoAux2 =  String("Apagado");
    digitalWrite(PIN_AUX, HIGH);  
    }
    
    irsend.begin();                    //inicio IR
    //bot->sendMessage(defaultChatId, "Hola!");   //mensaje inicio
}

void loop() {
 

    if ( sensorPirFlag ) {                 //----------------Interupt sensor Pir ---------------------
    ejecutaAccionPir();
  }

  
   int lecturaSensorPuerta = digitalRead(PIN_SENSOR_C);{    //------------Lectura estado sensor C -----------------
   if (lecturaSensorPuerta != ultimoEstadoPuerta) {       
     
     tiempoUltimoEstadoPuerta = millis();
   }
   if ((millis() - tiempoUltimoEstadoPuerta) > RetardoEstadoPuerta) {
     
     if (lecturaSensorPuerta != EstadoPuerta) {     // si ha cambiado de estado
       //Serial.println("Cambio de Estado Sensor");
       EstadoPuerta = lecturaSensorPuerta;
       //Serial.println(lecturaSensorPuerta);
       if (EstadoPuerta == HIGH) {
        ejecutaAccionC();
       }
       if (EstadoPuerta == LOW) {
        ejecutaAccionA();
       }
     }
    }
   ultimoEstadoPuerta = lecturaSensorPuerta;            
   }
   
  if (millis() > Bot_lasttime + Bot_mtbs)  {    //--------------Bot_lasttime--------------------
    int numNewMessages = bot->getUpdates(bot->last_message_received + 1);
    while(numNewMessages) {
      //Serial.println("tengo Mensaje");
      handleNewMessages(numNewMessages);
      numNewMessages = bot->getUpdates(bot->last_message_received + 1);
    }
    Bot_lasttime = millis();
  }
  
  if (millis() > Chek_lasttime + Chek_net) {    //-------------Chec_lasttime--------------------
    if(WiFi.status() == WL_CONNECTED){
    Chek_net2 = 0;
    //timeClient.update();
    //Serial.println("comprobando Hora Actual: ");
    //Serial.println(timeClient.getEpochTime());
    if (timeClient.getEpochTime() <=  (Chek_time+25) ){
    bot->sendMessage(defaultChatId, "Fallo de Reloj : Reinicio!" +(timeClient.getEpochTime()));   //mensaje Restart
    delay (10000);//Retardo
    ESP.restart();
    }
    }
  else{
    Chek_net2 = Chek_net2+1;
  }
    Chek_lasttime = millis();
    Chek_time = (timeClient.getEpochTime());
    ajuste_bot= 0;
  }
  if ((Chek_net2) >= vez_comprobacion){
   ESP.restart(); 
  }
   int Lectura_boton_C_W = digitalRead(PIN_CONFIGURACION);{    //------------Lectura estado PIN_CONFIGURACION -----------------
   if (Lectura_boton_C_W != ultimoEstado_boton_C_W) {       
     
     tiempoUltimoCambio_boton_C_W = millis();
   }
   if ((millis() - tiempoUltimoCambio_boton_C_W) > RetardoCambio_boton_C_W) {
     
     if (Lectura_boton_C_W != estado_boton_C_W) {     
       //Serial.println("Cambio de Estado Sensor");
       estado_boton_C_W = Lectura_boton_C_W;
       
       if (estado_boton_C_W == LOW) {
        accionConfiguracion();
       }
     }
    }
   ultimoEstado_boton_C_W = Lectura_boton_C_W;            
   }
}
