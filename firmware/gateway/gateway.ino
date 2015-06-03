#include <SoftwareSerial.h>

#include <EtherCard.h>
#include <SoftwareSerial.h>

#define requestPin  6
#define rxPin       4
#define txPin       5
#define BUFSIZE     90

// change these settings to match your own setup
#define APIKEY "adc984f0efa3f9d6114b6677c6f08cd3" // Robert
//#define APIKEY "121ac49b2af30c3c1bd82110dd877c52" // Marten

// ethernet interface mac address, must be unique on the LAN
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };

const char website[] PROGMEM = "emoncms.org";

byte Ethernet::buffer[350];
uint32_t timer;
Stash stash;
byte session;

//timing variable
int res = 0;

int incomingByte = 0;
String inputString = "0";
String P181, P182, P170, P270, G;
int pos181, pos182, pos281, pos282, G_pos, pos170, pos270;
bool stringComplete = false;

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
  while (mySerial.available()) {
    incomingByte = mySerial.read();
    incomingByte &= ~(1 << 7);    // forces 0th bit of x to be 0.  all other bits left alone.
    // add it to the inputString:
    inputString += (char)incomingByte;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if ((char)incomingByte == '!') {
      stringComplete = true;
    }
  }

   if (stringComplete) {
      Serial.println(inputString);

      pos181 = inputString.indexOf("1-0:1.8.1", 0);
      P181 = inputString.substring(pos181 + 10, pos181 + 17);
      pos182 = inputString.indexOf("1-0:1.8.2", 0);
      P182 = inputString.substring(pos182 + 10, pos182 + 17);

//      strMOD += "&field3=";
//      pos281 = inputString.indexOf("1-0:2.8.1", pos182 + 1);
//      strMOD += inputString.substring(pos281 + 10, pos281 + 17);
//      
//      strMOD += "&field4=";
//      pos282 = inputString.indexOf("1-0:2.8.2", pos281 + 1);
//      strMOD += inputString.substring(pos282 + 10, pos282 + 17);
      
      pos170 = inputString.indexOf("1-0:1.7.0", 0);
      //strMOD += inputString.substring(P1_pos + 10, P1_pos + 17);
      P170 = inputString.substring(pos170 + 10, pos170 + 17);
      
      pos270 = inputString.indexOf("1-0:2.7.0", 0);
      P270 = inputString.substring(pos270 + 10, pos270 + 17);
      
//      G_pos = inputString.indexOf("(m3)", P2_pos + 1);
//      G = inputString.substring(G_pos + 7, G_pos + 16);
      
      Serial.println("\nData:");
      Serial.println(P181.c_str());
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
  if (stringComplete) {
    
    // Build the json structure
    byte sd = stash.create();
    stash.print("{power:}");
    stash.print(P181);
    stash.print(",gas:");
    stash.print(P182);
    stash.print("}");
    stash.save();
    int stash_size = stash.size();

    // Build the header with json structure in the URL
    Stash::prepare(PSTR("GET http://$F/input/post.json?json=$H&apikey=$F HTTP/1.0" "\r\n"
      "Host: $F" "\r\n"
      "Content-Length: 0" "\r\n"
      "\r\n"
      ""), website, sd, PSTR(APIKEY), website);

    // send the packet - this also releases all stash buffers once done
    session = ether.tcpSend();
      
    Serial.print("RAM: ");
    Serial.println(freeRam()); 

    stringComplete = false;
  }
  
   const char* reply = ether.tcpReply(session);
   
   if (reply != 0) {
     res = 0;
     Serial.println("Respone");
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

    if (!ether.dnsLookup(website)) {
      Serial.println("DNS failed");
    } else {
      ether.printIp("SRV: ", ether.hisip);
  
      //reset init value
      res = 0;
      break;
    }
  }
}

int freeRam() {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
