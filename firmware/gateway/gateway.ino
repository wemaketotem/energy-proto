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

bool ledOn = false;

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
    while (mySerial.available()) {
      int incomingByte = mySerial.read();
      ledOn = !ledOn;
      digitalWrite(ledPin, ledOn); // Toggle LED on each received byte
    }
    digitalWrite(ledPin, HIGH); // Make sure the led is off when no incomming P1 bytes to handle
 
    ether.packetLoop(ether.packetReceive()); // Need to be touched continuesly

    //if correct answer is not received then re-initialize ethernet module
    if (res > 220){
      initialize_ethernet(); 
    }
    res = res + 1;
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
