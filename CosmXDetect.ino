
#include <SPI.h>
#include <HttpClient.h>
#include <Ethernet.h>
#include <EthernetClient.h>
//#include <Cosm.h>
#include <Adafruit_Thermal.h>
#include <SoftwareSerial.h>

const int
led_pin         = 3,           // To status LED (hardware PWM pin)
// Pin 4 is skipped -- this is the Card Select line for Arduino Ethernet!
printer_RX_Pin  = 5,           // Printer connection: green wire
printer_TX_Pin  = 6,           // Printer connection: yellow wire
printer_Ground  = 7;           // Printer connection: black wire
const unsigned long              // Time limits, expressed in milliseconds:
pollingInterval = 10L * 1000L, // Note: Twitter server will allow 150/hr max
connectTimeout  = 15L * 1000L, // Max time to retry server link
responseTimeout = 15L * 1000L; // Max time to wait for data from server
Adafruit_Thermal
printer(printer_RX_Pin, printer_TX_Pin);
byte
sleepPos = 0, // Current "sleep throb" table position

// assign a MAC address for the ethernet controller.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
// fill in your address here:
mac[] = { 
  0x90, 0xA2, 0xDA, 0x0D, 0x68, 0xA1 };

// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 30*1000;
// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;

// Define the string for our datastream ID
//char datastreamId[] = "test";

const char kHostname[] = "cosmx-installs.herokuapp.com";
const char kPath[] = "/";
const char kUserAgent[] = "Levent's Little Printer";

// fill in an available IP address on your network here,
// for manual configuration:
IPAddress ip(192,168,60,203);

PROGMEM byte
sleepTab[] = { // "Sleep throb" brightness table (reverse for second half)
  0,   0,   0,   0,   0,   0,   0,   0,   0,   1,
  1,   1,   2,   3,   4,   5,   6,   8,  10,  13,
  15,  19,  22,  26,  31,  36,  41,  47,  54,  61,
  68,  76,  84,  92, 101, 110, 120, 129, 139, 148,
  158, 167, 177, 186, 194, 203, 211, 218, 225, 232,
  237, 242, 246, 250, 252, 254, 255 };

// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
//IPAddress server(216,52,233,121);      // numeric IP for api.cosm.com
char server[] = "api.cosm.com";   // name address for cosm API

unsigned long lastConnectionTime = 0;          // last time you connected to the server, in milliseconds
boolean lastConnected = false;                 // state of the connection last time through the main loop
const unsigned long postingInterval = 10*1000; //delay between updates to cosm.com

// Function prototypes -------------------------------------------------------

int
unidecode(byte),
timedRead(void);

// ---------------------------------------------------------------------------

String previousCount = "";
String currentCount = "";

void setup() {
  // start serial port:
  Serial.begin(57600);

  pinMode(printer_Ground, OUTPUT);
  digitalWrite(printer_Ground, LOW);  // Just a reference ground, not power
  printer.begin();
  printer.sleep();

  while (Ethernet.begin(mac) != 1)
  {
    Serial.println("Error getting IP address via DHCP, trying again...");
    delay(15000);
  }
}

void loop() {
  int err =0;
  
  EthernetClient c;
  HttpClient http(c);
  
  unsigned long startTime, t;
  int           i;
//  char          c;

  startTime = millis();

  // Disable Timer1 interrupt during network access, else there's trouble.
  // Just show LED at steady 100% while working.  :T
  TIMSK1 &= ~_BV(TOIE1);
  analogWrite(led_pin, 255);




  err = http.get(kHostname, kPath, kUserAgent);
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
        currentCount = "";
        // Whilst we haven't timed out & haven't reached the end of the body
        while ( (http.connected() || http.available()) &&
               ((millis() - timeoutStart) < kNetworkTimeout) )
        {

            if (http.available())
            {
                c = http.read();
                // Print out this character
                currentCount += c;
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
  http.stop();

  if (currentCount != previousCount) {

    printer.wake();
    printer.underlineOff();
    printer.println("CosmX clients installed:");
    printer.print(currentCount);
    printer.feed(3);
    printer.sleep();
  } else {
    Serial.println("same");
  }
  previousCount = currentCount;

//  printer.wake();
//  printer.underlineOff();
//  printer.print("Datastream is... ");
//  printer.println(feed[0].getString());
//  printer.feed(1);
//  printer.sleep();
  
  // Sometimes network access & printing occurrs so quickly, the steady-on
  // LED wouldn't even be apparent, instead resembling a discontinuity in
  // the otherwise smooth sleep throb.  Keep it on at least 4 seconds.
  t = millis() - startTime;
  if(t < 4000L) delay(4000L - t);

  // Pause between queries, factoring in time already spent on network
  // access, parsing, printing and LED pause above.
  t = millis() - startTime;
  if(t < pollingInterval) {
    Serial.print("Pausing...");
    sleepPos = sizeof(sleepTab); // Resume following brightest position
    TIMSK1 |= _BV(TOIE1); // Re-enable Timer1 interrupt for sleep throb
    delay(pollingInterval - t);
    Serial.println("done");
  }
}

// Timer1 interrupt handler for sleep throb
ISR(TIMER1_OVF_vect, ISR_NOBLOCK) {
  // Sine table contains only first half...reflect for second half...
  analogWrite(led_pin, pgm_read_byte(&sleepTab[
    (sleepPos >= sizeof(sleepTab)) ?
  ((sizeof(sleepTab) - 1) * 2 - sleepPos) : sleepPos]));
  if(++sleepPos >= ((sizeof(sleepTab) - 1) * 2)) sleepPos = 0; // Roll over
  TIFR1 |= TOV1; // Clear Timer1 interrupt flag
}



