#include <SPI.h>
#include <HttpClient.h>
#include <Ethernet.h>
#include <EthernetClient.h>
#include <SoftwareSerial.h>

// Define all digital pins used
#define requestPin  6  // P1 request line
#define rxPin       7  // P1 UART RX
#define txPin       5  // Unused TX (is needed for SoftwareSerial)
#define ledPin      13 // LED

// Ethernet configuration constants, change these settings to match your own setup
static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 }; // ethernet interface mac address, must be unique on the LAN
#define HOST "emoncms.org"
//#define HOST "192.168.2.3"
#define APIKEY "adc984f0efa3f9d6114b6677c6f08cd3" // Robert
//#define APIKEY "121ac49b2af30c3c1bd82110dd877c52" // Marten

// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 30*1000;
// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;

// P1 hardware configuration
SoftwareSerial mySerial(rxPin, txPin, false); // RX, TX, inverted
// P1 parsing variables
String inputString = ""; // A string object that will contain one P1 message line
String P181, P182, P281, P282, P170, P270, G; // The energy value strings cut from the P1 message
// Flags representing states TODO: Implement real state machine
bool lineComplete = false; // Indicates that a line of a P1 message is received, need to check the line for value to parse
bool msgComplete = false; // Indicates that a line of a P1 message is received, parse values can be send
bool nextLineIsGas = false; // Indicates that the gas tag is found, its value is on the next line


void setup () {
  // Configure debug serial output
  Serial.begin(57600);
  Serial.println("\n[EmonCMS example]");

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

    if (!buildAndSendRequest()) {
      // Failed to send, re-initialize ethernet
      initialize_ethernet();
    }
    
    // Message handled
    msgComplete = false;
      
    Serial.print("RAM: ");
    Serial.println(freeRam()); 
  }
  Ethernet.maintain();
}


void initialize_ethernet(void){  
  while (!Ethernet.begin(mymac)) {
    // failed to get a DHCP response, try to recover later
    Serial.print("failed to get a DHCP response, try to recover after one second");
    delay(1000);
  }
  
  // DHCP received, get DNS
  Serial.print("Received IP from DHCP server:");
  Serial.println(Ethernet.localIP());
}

bool buildAndSendRequest(void) {
  int err =0;
  String url = "/input/post.json?json=";
  url += "{181:";
  url += P181;
  url += ",182:";
  url += P182;
  url += ",281:";
  url += P281;
  url += ",282:";
  url += P282;
  url += ",170:";
  url += P170;
  url += ",270:";
  url += P270;
  url += ",gas:";
  url += G;
  url += "}";
  url += "}&apikey=" APIKEY;

  EthernetClient c;
  HttpClient http(c);
  err = http.get(HOST, url.c_str());
  if (err == 0)
  {
    Serial.println("startedRequest ok");
    err = http.responseStatusCode();
    if (err >= 0)
    {
      Serial.print("Got status code: ");
      Serial.println(err);
      // Usually you'd check that the response code is 200 or a
      // similar "success" code (200-299) before carrying on,
      // but we'll print out whatever response we get
      err = http.skipResponseHeaders();
      if (err >= 0)
      {
        int bodyLen = http.contentLength();
        Serial.print("Content length is: ");
        Serial.println(bodyLen);
        Serial.println();
        Serial.println("Body returned follows:");
        // Now we've got to the body, so we can print it out
        unsigned long timeoutStart = millis();
        char c;
        // Whilst we haven't timed out & haven't reached the end of the body
        while ( (http.connected() || http.available()) &&
        ((millis() - timeoutStart) < kNetworkTimeout) )
        {
          if (http.available())
          {
            c = http.read();
            // Print out this character
            Serial.print(c);
            bodyLen--;
            // We read something, reset the timeout counter
            timeoutStart = millis();
          }
          else
          {
            // We haven't got any data, so let's pause to allow some to
            // arrive
            delay(kNetworkDelay);
          }
        }
        Serial.println();
      }
      else
      {
        Serial.print("Failed to skip response headers: ");
        Serial.println(err);
      }
    }
    else
    {
      Serial.print("Getting response failed: ");
      Serial.println(err);
    }
  }
  else
  {
    Serial.print("Connect failed: ");
    Serial.println(err);
  }

  return err == 0;
}

int freeRam() {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
