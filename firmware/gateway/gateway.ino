#include <EtherCard.h>
#include <SoftwareSerial.h>

#define requestPin  6
#define rxPin       4
#define txPin       5
#define BUFSIZE     90

// change these settings to match your own setup
#define FEED "831930262"
#define APIKEY "Io7klQa8OBF8etrXTlqYGyvOrHHSZtyaaa4KT2USeopZxQJc"

// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };

const char website[] PROGMEM = "api.xively.com";

byte Ethernet::buffer[350];
uint32_t timer;
Stash stash;
byte session;

//timing variable
int res = 0;

int incomingByte = 0;
String inputString = "0";
String P181, P182, P170, P270, G;
int pos181, pos182, pos281, pos282, P1_pos, P2_pos, G_pos;

SoftwareSerial mySerial(rxPin, txPin, true); // RX, TX, inverted

void setup () {
  Serial.begin(57600);
  Serial.println("\n[Xively example]");

  initialize_ethernet();

  mySerial.begin(9600);
  pinMode(requestPin, OUTPUT);
  digitalWrite(requestPin, HIGH);
}


void loop () { 
  while (mySerial.available() > 0) {
    incomingByte = mySerial.read();
    incomingByte &= ~(1 << 7);    // forces 0th bit of x to be 0.  all other bits left alone.
    inputString += (char)incomingByte;
  }  
  
   if (inputString.length() > 100) {
      Serial.println(inputString);

      pos181 = inputString.indexOf("1-0:1.8.1", 0);
      P181 = inputString.substring(pos181 + 10, pos181 + 17);

      pos182 = inputString.indexOf("1-0:1.8.2", pos181 + 1);
      P182 = inputString.substring(pos182 + 10, pos182 + 17);

//      strMOD += "&field3=";
//      pos281 = inputString.indexOf("1-0:2.8.1", pos182 + 1);
//      strMOD += inputString.substring(pos281 + 10, pos281 + 17);
//      
//      strMOD += "&field4=";
//      pos282 = inputString.indexOf("1-0:2.8.2", pos281 + 1);
//      strMOD += inputString.substring(pos282 + 10, pos282 + 17);
      
      P1_pos = inputString.indexOf("1-0:1.7.0", pos282 + 1);
      //strMOD += inputString.substring(P1_pos + 10, P1_pos + 17);
      P170 = inputString.substring(P1_pos + 10, P1_pos + 17);
      
      P2_pos = inputString.indexOf("1-0:2.7.0", P1_pos + 1);
      P270 = inputString.substring(P2_pos + 10, P2_pos + 17);
      
//      G_pos = inputString.indexOf("(m3)", P2_pos + 1);
//      G = inputString.substring(G_pos + 7, G_pos + 16);
      
      Serial.println("\nData:");
      Serial.println(P181);
      Serial.println(P182);
      Serial.println(P170);
      Serial.println(P270);
      Serial.println(G + "\n");
   }

  
  //if correct answer is not received then re-initialize ethernet module
  if (res > 220){
    initialize_ethernet(); 
  }
  
  res = res + 1;
  
  ether.packetLoop(ether.packetReceive());
  
  //200 res = 10 seconds (50ms each res)
  //if (res == 200) {
  if (inputString != "0") {
    
    byte sd = stash.create();
    stash.print("P181,");
    stash.println(P181);
    stash.print("P182,");
    stash.println(P182);
    stash.print("P170,");
    stash.println(P170);
    stash.print("P270,");
    stash.println(P270);
//    stash.print("Gas,");
//    stash.println(G);
    stash.save();

    Stash::prepare(PSTR("PUT http://$F/v2/feeds/$F.csv HTTP/1.0" "\r\n"
      "Host: $F" "\r\n"
      "X-PachubeApiKey: $F" "\r\n"
      "Content-Length: $D" "\r\n"
      "\r\n"
      "$H"),
    website, PSTR(FEED), website, PSTR(APIKEY), stash.size(), sd);

    // send the packet - this also releases all stash buffers once done
    session = ether.tcpSend();
      
    Serial.print("RAM: ");
    Serial.println(freeRam()); 

    inputString = "0";
  }
  
   const char* reply = ether.tcpReply(session);
   
   if (reply != 0) {
     res = 0;
     Serial.println(reply);
   }
   
   delay(50);
}



void initialize_ethernet(void){  
  for(;;){ // keep trying until you succeed 
    //Reinitialize ethernet module
    pinMode(5, OUTPUT);
    Serial.println("Reseting Ethernet...");
    digitalWrite(5, LOW);
    delay(1000);
    digitalWrite(5, HIGH);
    delay(500);

    if (ether.begin(sizeof Ethernet::buffer, mymac) == 0){ 
      Serial.println( "Failed to access Ethernet controller");
      continue;
    }
    
    if (!ether.dhcpSetup()){
      Serial.println("DHCP failed");
      continue;
    }

    ether.printIp("IP:  ", ether.myip);
    ether.printIp("GW:  ", ether.gwip);  
    ether.printIp("DNS: ", ether.dnsip);  

    if (!ether.dnsLookup(website))
      Serial.println("DNS failed");

    ether.printIp("SRV: ", ether.hisip);

    //reset init value
    res = 0;
    break;
  }
}

int freeRam() {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
