#include <enc28j60.h>
#include <net.h>
#include <EtherCard.h>

#include <dht.h>
#include <TimeLib.h>
#include <SPI.h>


#define DHT11_pin 7
#define RelayBoard4 6 //light
#define RelayBoard3 3 //fans
#define RelayBoard1 5 //deumidificator
#define RelayBoard2 2 //humidificator

// Impostazioni per l'orario
long t = millis() / 1000;
word h = t / 3600;
byte m = (t / 60) % 60;
byte s = t % 60;
//---------------------------


dht DHT110;
int pumpActive =0;
int tempAllert = 0; // if tempAllert = 1 tempmax exceeded, if tempAllert = 2 tempmin exceeded, if tempAllert = 0 normal temperature
int humAllert = 0; // if humAllert = 1 hummax exceeded, if humAllert = 2 hummin exceeded, if humAllert = 0 normal humidity
float customTempMax= 0;
float customTempMin= 0;
int customHumidityMax= 0;
int customHumidityMin= 0;
int moistureHumidity[5];
int customMoistureHumidity[5];
int attivazionePump[5];
int pumpPin[6]={10,11,12,13,14,15};
//+++++++++++++Setto ethernet shield++++++++++++++++++++
// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };
static byte myip[] = { 192,168,1,203 };

byte Ethernet::buffer[500];
BufferFiller bfill;



void setup() {

  // Change 'SS' to your Slave Select pin, if you arn't using the default pin
  if (ether.begin(sizeof Ethernet::buffer, mymac, 8) == 0)
    Serial.println(F("Failed to access Ethernet controller"));
  ether.staticSetup(myip);
  //-------------------

  Serial.begin(9600); // inizializzo il seriale
  pinMode(RelayBoard1, OUTPUT); //setto le relay board
  pinMode(RelayBoard2, OUTPUT);
  pinMode(RelayBoard3, OUTPUT);
  pinMode(RelayBoard4, OUTPUT);
  //Setto i pin delle pompette
  pinMode(13, OUTPUT);  //!! da impostare in base ai pin delle pompe
  setTime(14,47,10,2,11,2018); //setto l'orario, da cambiare


}

static word homePage() {

  char humidityChar[10];
  char temperatureChar[10];
  //Converto i duble delle funzioni in char per server HTTP
  dtostrf(humidityExternal(),2,2,humidityChar);
  dtostrf(temperatureExternal(),2,2,temperatureChar);
  //Converto boolean to string
  int state=0;
  int pompaAttiva= 0;
  if(lightManagment() == true) // stato delle luci se accese o spente
    state = 0;
    else
    state = 1;

  switch (pumpActive) { // !! impostare tutte le pompe con i pin giusti
    case 1 :
      pompaAttiva = 1;
      break;
    case 2 :
      pompaAttiva = 2;
      break;
  }

  bfill = ether.tcpOffset();
  bfill.emit_p(PSTR(
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
    "\r\n"
    "<meta http-equiv='refresh' content='1'/>"
    "<title>Really High Server</title>"
    "<h1>Temperature:$S Humidity:$S Time:$D:$D State:$D Moisture:$D PompaAttivaNr:$D Allert Hum:$D Allert Temp:$D</h1>"),
      temperatureChar, humidityChar, hour(),minute(), state, analogRead(0), pompaAttiva,humAllert,tempAllert);
  return bfill.position();
}
//--------------------------------FUNCTIONS-------------------------------------
int temperatureExternal(){

  //reading the input signal from pin 7 and trasform it in temp and humidity
  // Red led -> LOW
   
  if(DHT110.temperature > customTempMax){ //Setto un allert per futura web page
    //allert max temp reached
    tempAllert = 1;
  }
  if(DHT110.temperature < customTempMin){
    //allert min temp reached
    tempAllert = 2;
  }
return DHT110.temperature;

}
int humidityExternal(){


   if(DHT110.humidity < customHumidityMin){ // Setto allert per futura web page
    //allert min umidity reached
    //turn on humidificator
    humAllert =  2;
    digitalWrite(RelayBoard2,HIGH);
  }
  if(DHT110.humidity > customHumidityMax){
    //do something for a good humidity
    //turn on deumidificator
    humAllert = 1;
    digitalWrite(RelayBoard1,HIGH);
  }
  return DHT110.humidity;
}

boolean lightManagment(){
//Imposto l'orario
 // time_t t= now();
  int oraOffl= 14;
  int minutiOffl= 48;
  int oraOnl=14;
  int minutiOnl= 49;
  boolean On = false;
  Serial.print("Ora");
  Serial.print(hour());
  Serial.print(minute());

  if (hour()==oraOffl  && minute() == minutiOffl){ //sunlight
   
      digitalWrite(RelayBoard4,LOW);
      Serial.print("sei antrato il low");
  }
  if (hour()== oraOnl && minute() == minutiOnl){ //nightlight
   
      digitalWrite(RelayBoard4,HIGH);
      Serial.print("Sei entrto il high");    
  }
  if(digitalRead(RelayBoard4) == HIGH)
    On= true;
    else
    On= false;
    
   return On;
}

void pumpManagment(){
  int i;
   

  for(int analogPin = 0; analogPin<5;analogPin++){
    moistureHumidity[analogPin] = analogRead(analogPin);
    while( moistureHumidity[analogPin] < customMoistureHumidity[analogPin]){
      //Accendo la pompa desiderata finche' non raggiunge l'umidita' desiderata
      pumpActive= pumpPin[i];
      digitalWrite(pumpPin[i], HIGH);
      attivazionePump[pumpActive] = 1;
    }
    pumpActive= 0;
    attivazionePump[pumpActive] = 0;

  }
}

void loop() {
  // put your main code here, to run repeatedly:
  word len = ether.packetReceive();
  word pos = ether.packetLoop(len);

  if (pos)  // check if valid tcp data is received
     ether.httpServerReply(homePage()); // send web page data

  //____TOKENIZER per richieste dal server http (pc)
  char array[500] = {(char *)Ethernet::buffer + pos};
  char *strings[10];
  char *ptr = NULL;
  byte index = 0;
  int i=0;

    
    ptr = strtok(array, "=");  // takes a list of delimiters
    while(ptr != NULL)
    {
        strings[index] = ptr;
        index++;
        ptr = strtok(NULL, "=");  // takes a list of delimiters
    }
      
    
   
  //____Valori/settaggi dalla pagina web
   //GESTIONE DELLA TEMPERATURA/UMIDITA' ESTERNA
  for(i=0;i<40;i++){  
    if(strstr((char *)Ethernet::buffer + pos, "GET /?customTempMax="+i) != 0){
  
       customTempMax = i;
    }
    if(strstr((char *)Ethernet::buffer + pos, "GET /?customTempMin="+i) != 0){
  
       customTempMin = i;
    }
    if(strstr((char *)Ethernet::buffer + pos, "GET /?customHumidityMax="+i) != 0){
  
       customHumidityMax = i;
    }
    if(strstr((char *)Ethernet::buffer + pos, "GET /?customHumidityMin="+i) != 0){
  
       customHumidityMin = i;
    } 
  }
   //GESTIONE DELL' UMIDITA' DELLA TERRA
  for(i=0;i<2000;i++){
   if(strstr((char *)Ethernet::buffer + pos, "GET /?customMoistureHumidity1="+i) != 0){
  
       customMoistureHumidity[0] = i;
    }
    if(strstr((char *)Ethernet::buffer + pos, "GET /?customMoistureHumidity2="+i) != 0){
  
       customMoistureHumidity[1] = i;
    }
    if(strstr((char *)Ethernet::buffer + pos, "GET /?customMoistureHumidity3="+i) != 0){
  
       customMoistureHumidity[2] = i;
    }
    if(strstr((char *)Ethernet::buffer + pos, "GET /?customMoistureHumidity4="+i) != 0){
  
       customMoistureHumidity[3] = i;
    }
    if(strstr((char *)Ethernet::buffer + pos, "GET /?customMoistureHumidity5="+i) != 0){
  
       customMoistureHumidity[4] = i;
    }
    if(strstr((char *)Ethernet::buffer + pos, "GET /?customMoistureHumidity6="+i) != 0){
  
       customMoistureHumidity[5] = i;
    }    
    
  }
   //GESTIONE DELLE POMPE
  for(i=0;i<5;i++){
   
   if(strstr((char *)Ethernet::buffer + pos, "GET /?pumpActivation1=1") != 0 && attivazionePump[i] == 0 ){
      digitalWrite(pumpPin[i], HIGH);
     }
   if(strstr((char *)Ethernet::buffer + pos, "GET /?pumpActivation2=1") != 0 && attivazionePump[i] == 0 ){
      digitalWrite(pumpPin[i], HIGH);
     }
   if(strstr((char *)Ethernet::buffer + pos, "GET /?pumpActivation3=1") != 0 && attivazionePump[i] == 0 ){
      digitalWrite(pumpPin[i], HIGH);
     }
   if(strstr((char *)Ethernet::buffer + pos, "GET /?pumpActivation4=1") != 0 && attivazionePump[i] == 0 ){
      digitalWrite(pumpPin[i], HIGH);
     }
   if(strstr((char *)Ethernet::buffer + pos, "GET /?pumpActivation5=1") != 0 && attivazionePump[i] == 0 ){
      digitalWrite(pumpPin[i], HIGH);
     }
   if(strstr((char *)Ethernet::buffer + pos, "GET /?pumpActivation6=1") != 0 && attivazionePump[i] == 0 ){
      digitalWrite(pumpPin[i], HIGH);
     }
     

   if(strstr((char *)Ethernet::buffer + pos, "GET /?pumpActivation1=0") != 0 && attivazionePump[i] == 1 ){
      digitalWrite(pumpPin[i], HIGH);
     }
   if(strstr((char *)Ethernet::buffer + pos, "GET /?pumpActivation2=0") != 0 && attivazionePump[i] == 1 ){
      digitalWrite(pumpPin[i], HIGH);
     }
   if(strstr((char *)Ethernet::buffer + pos, "GET /?pumpActivation3=0") != 0 && attivazionePump[i] == 1 ){
      digitalWrite(pumpPin[i], HIGH);
     }
   if(strstr((char *)Ethernet::buffer + pos, "GET /?pumpActivation4=0") != 0 && attivazionePump[i] == 1 ){
      digitalWrite(pumpPin[i], HIGH);
     }
   if(strstr((char *)Ethernet::buffer + pos, "GET /?pumpActivation5=0") != 0 && attivazionePump[i] == 1 ){
      digitalWrite(pumpPin[i], HIGH);
     }
   if(strstr((char *)Ethernet::buffer + pos, "GET /?pumpActivation6=0") != 0 && attivazionePump[i] == 1 ){
      digitalWrite(pumpPin[i], HIGH);
     }
   }

   //GESTIONE DELLA ACCENSIONE/SPEGNIMENTO DELLE LUCI

    if(strstr((char *)Ethernet::buffer + pos, "GET /?lightActivation=1") != 0 && pumpActive == 0 ){
      digitalWrite(RelayBoard4,LOW);
     }
    if(strstr((char *)Ethernet::buffer + pos, "GET /?lightActivation=0") != 0 && pumpActive == 1 ){
     digitalWrite(RelayBoard4,HIGH);
     }
     
  

      
   

//------ Temperature and humidity funcions -----------------

  int chk = DHT110.read11(DHT11_pin); // leggo dal pin 11
  int temp= temperatureExternal();
  int hum = humidityExternal();
  boolean state = lightManagment();
  delay(100);
  pumpManagment();

}
