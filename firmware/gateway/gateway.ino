#include <SoftwareSerial.h>

#include <EtherCard.h>
#include <SoftwareSerial.h>

// Define all digital pins used
#define requestPin  6  // P1 request line
#define rxPin       4  // P1 UART RX
#define txPin       5  // Unused TX (is needed for SoftwareSerial)
#define ledPin      15 // LED

// Ethernet configuration constants, change these settings to match your own setup
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 }; // ethernet interface mac address, must be unique on the LAN
const char website[] PROGMEM = "emoncms.org";
#define APIKEY "adc984f0efa3f9d6114b6677c6f08cd3" // Robert
//#define APIKEY "121ac49b2af30c3c1bd82110dd877c52" // Marten

// Ethernet variables
byte Ethernet::buffer[350]; // The buffer used by the Ethernet stack
Stash stash; // The buffer that contains the POST data
byte session; // Session identification
int res = 0; //timing variable for resetting the Ethernet stack

// P1 hardware configuration
SoftwareSerial mySerial(rxPin, txPin, true); // RX, TX, inverted

// P1 parsing variables
String inputString = ""; // A string object that will contain one P1 message line
String P181, P182, P281, P282, P170, P270, G; // The energy value strings cut from the P1 message
// Flags representing states TODO: Implement real state machine
bool lineComplete = false; // Indicates that a line of a P1 message is received, need to check the line for value to parse
bool msgComplete = false; // Indicates that a line of a P1 message is received, parse values can be send
bool nextLineIsGas = false; // Indicates that the gas tag is found, its value is on the next line
bool isSending = false; // Busy with sending the last received P1 data


void setup () {
  // Configure debug serial output
  Serial.begin(57600);
  Serial.println("\n[Xively example]");

  // Initialize ethernet, it is blocking until it receives a DHCP lease
  initialize_ethernet();

  // Configure P1 port pinning
  mySerial.begin(9600);
  pinMode(requestPin, OUTPUT);
  digitalWrite(requestPin, HIGH);
  
  // Configure LED
  pinMode(ledPin, OUTPUT);
  // Let LED pulse 10 times on 10Hz at boot
  for (int i = 0; i < 10; i++) {
    digitalWrite(ledPin, LOW);
    delay(100);
    digitalWrite(ledPin, HIGH);
    delay(100);
  }
}


void loop () { 
  if (!isSending) {
    while (mySerial.available() && !(lineComplete || msgComplete)) {
      int incomingByte = mySerial.read();
      incomingByte &= ~(1 << 7);    // forces 0th bit of x to be 0.  all other bits left alone.
      // add it to the inputString:
      inputString += (char)incomingByte;
      // if the incoming character is a newline, set a flag
      // so the main loop can do something about it:
      if ((char)incomingByte == '\n') {
        lineComplete = true;
      }
      if ((char)incomingByte == '!') {
        msgComplete = true;
        digitalWrite(ledPin, HIGH); // Clear LED to indicate that P1 message receiption completed
      }
    }
     if (lineComplete) {
       if (nextLineIsGas) {
         Serial.println("Found gas value");
         Serial.print("inputString: ");
         Serial.println(inputString);
         G = inputString.substring(1, 1+5+1+3);
         nextLineIsGas = false;
       } else if (inputString.length() >= 9) { // Only handle lines larger than 9 chars
         String tag = inputString.substring(0, 9);
         Serial.print("inputString: ");
         Serial.println(inputString);
         if (tag == "1-0:1.8.1") {
            digitalWrite(ledPin, LOW); // Set LED to indicate receiving P1 message started (first tag of P1 message is received)
            P181 = inputString.substring(10, 10+5+1+3);
         } else if (tag == "1-0:1.8.2") {
            P182 = inputString.substring(10, 10+5+1+3);
         } else if (tag == "1-0:2.8.1") {
            P281 = inputString.substring(10, 10+5+1+3);
         } else if (tag == "1-0:2.8.2") {
            P282 = inputString.substring(10, 10+5+1+3);
         } else if (tag == "1-0:1.7.0") {
            P170 = inputString.substring(10, 10+4+1+2);
         } else if (tag == "1-0:2.7.0") {
            P270 = inputString.substring(10, 10+4+1+2);
         } else {
          if (inputString.indexOf("(m3)") > 0) {
            nextLineIsGas = true;
            Serial.println("Found gas tag");
          }
         }
       }
       
       // Line handled, reset for next line
       inputString = "";
       lineComplete = false;     
     } else if (!mySerial.available()) {
       // When no line and no char available, wait a little to chill the processor
       delay(50);
     }
  } 
 
  if (msgComplete) {
      Serial.print("181:");    
      Serial.println(P181);    
      Serial.print("182:");    
      Serial.println(P182);    
      Serial.print("281:");    
      Serial.println(P281);    
      Serial.print("282:");    
      Serial.println(P282);    
      Serial.print("170:");    
      Serial.println(P170);    
      Serial.print("270:");    
      Serial.println(P270);    
      Serial.print("gas:");    
      Serial.println(G);    
     
      Serial.print("RAM: ");
      Serial.println(freeRam()); 
     
      // Build the json structure
      byte sd = stash.create();
      stash.print("{181:");
      stash.print(P181);
      stash.print(",182:");
      stash.print(P182);
      stash.print(",281:");
      stash.print(P281);
      stash.print(",282:");
      stash.print(P282);
      stash.print(",170:");
      stash.print(P170);
      stash.print(",270:");
      stash.print(P270);
      stash.print(",gas:");
      stash.print(G);
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
      isSending = true;        
      
      // Message handled
      msgComplete = false;
      
      Serial.print("RAM: ");
      Serial.println(freeRam()); 
   }
 
   if (isSending) {
     const char* reply = ether.tcpReply(session);
     
     if (reply != 0) {
       res = 0;
       Serial.println("Respone");
       Serial.println(reply);
       isSending = false;
     } else {
        //if correct answer is not received then re-initialize ethernet module
        if (res > 220){
          initialize_ethernet(); 
        }
        res = res + 1;
        delay(50);
     }
   }

   ether.packetLoop(ether.packetReceive()); // Need to be touched continuesly
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
      continue;
    } else {
      ether.printIp("SRV: ", ether.hisip);
  
      //reset init value
      res = 0;
      break;
    }    
  }
    Serial.print("RAM: ");
    Serial.println(freeRam()); 
}

int freeRam() {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
