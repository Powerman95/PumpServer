/*********
  22 My 2025.  Changed pump time from 3 minutes to 4 minutes.  Also,
  changed cycle time from twice a day to 3 times.  The comment below says
  pump time was 2 minutes.  I don't know when I changed it to 3.
  22 Sep 2024: Changed Pump time to 120 seconds from 150.
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
*********/

// Load Wi-Fi library
#include <WiFi.h>
#include <time.h>
//#include <esp_timer.h> 
#define PUMP 18
// Stop button is attached to PIN 0 (IO0)

#define SECONDS_PER_CYCLE (60*60*24/3)
#define  PUMP_TIME (4*60)

#define SECONDS_PER_CYCLE (30)
#define  PUMP_TIME (11)

/*********
  22 Sep 2024: Changed Pump time to 120 seconds from 150.
 *********/

// Load Wi-Fi library
#include <WiFi.h>
#include <time.h>
//#include <esp_timer.h> 


// Replace with your network credentials
const char* ssid = "Ulrichhome";
const char* password = "powerman";

//3 lines needed for internet time functions.
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -21600;     //6 hours offset for standard time.
const int   daylightOffset_sec = 3600;

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String output26State = "off";

// Assign output variables to GPIO pins
const int output26 = 26;
//const int output27 = 27;

// Current time since startup of the micro.
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const int32_t timeoutTime = 2000;

// Timer setup stuff.
hw_timer_t *timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t lastIsrAt = 0;

time_t last_cycle; //Record the time of the previous pump cycle.
time_t next_cycle;

void ARDUINO_ISR_ATTR onTimer() {
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  lastIsrAt = millis();
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
  // It is safe to use digitalRead/Write here if you want to toggle an output
}

void setup() {
  Serial.begin(115200);
  // Initialize the output variables as outputs
  pinMode(output26, OUTPUT);
//  pinMode(output27, OUTPUT);
  pinMode(PUMP, OUTPUT);

  // Set outputs to LOW
  digitalWrite(output26, HIGH);
//  digitalWrite(output27, HIGH);
///////////////Timer Setup
  // Create semaphore to inform us when the timer has fired
  timerSemaphore = xSemaphoreCreateBinary();

  // Set timer frequency to 1Mhz
  timer = timerBegin(1000000);

  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer, &onTimer);

  // Set alarm to call onTimer function every second (value in microseconds).
  // Repeat the alarm (third parameter) with unlimited count = 0 (fourth parameter).
  timerAlarm(timer, 1000000, true, 0);

/////////////////////////////////////////////////////////////////////////

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
    last_cycle = time(NULL);  //Initialize
}

void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.println();//Blank line at end of html.
}

void loop(){
  struct tm timeinfo;
  struct tm nextcycle;
  static enum {STARTUP,PUMP_ON,PUMP_OFF,WAIT}PumpState = STARTUP;
  bool StateChange = false;

  WiFiClient client = server.available();   // Listen for incoming clients


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  if (client) {                             // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected() && currentTime - previousTime <= timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");

            client.println();
            
            // turns the GPIOs on and off
            if (header.indexOf("GET /26/on") >= 0) {
              Serial.println("GPIO 26 on");
              output26State = "on";
              digitalWrite(output26, HIGH);
            } else if (header.indexOf("GET /26/off") >= 0) {
              Serial.println("GPIO 26 off");
              output26State = "off";
              digitalWrite(output26, LOW);
            }
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" http-equiv=\"refresh\" content=\"10\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>ESP32 Web Server</h1>");
            
            // Display current state, and ON/OFF buttons for GPIO 26  
            client.println("<p>GPIO 26 - State " + output26State + "</p>");
            // If the output26State is off, it displays the ON button       
            if (output26State=="off") {
              client.println("<p><a href=\"/26/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/26/off\"><button class=\"button button2\">OFF</button></a></p>");
            } 
            client.println("<title>Jim's Water Garden </title>");
           
            client.println("<h2>Elapsed Time.</h2>");
            client.printf("%6d \n",time(NULL)-last_cycle);
            client.println("</body></html>");

            if(getLocalTime(&timeinfo)){
              client.println(&timeinfo, "<b>%A, %B %d %Y %H:%M:%S</b>"); //Print the time.
              return;
            }
            
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      } //if client available
    }   //end of while connected
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  } //if client
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    time_t elapsed;   //Time elapsed since last pump run.
  // put your main code here, to run repeatedly:
  // If Timer has fired
  if (xSemaphoreTake(timerSemaphore, 0) == pdTRUE) {
    uint32_t isrTime = 0;
    char buff[21];  //Used to display time in ascii.
    // Read the interrupt count and time
    portENTER_CRITICAL(&timerMux);
    isrTime = lastIsrAt;    //milliseconds since startup.
    portEXIT_CRITICAL(&timerMux);
    elapsed = time(NULL) - last_cycle;
   
    /////////////////////////////////////////////////////////////////////////////////////////
    switch(PumpState){
      case  STARTUP:
        digitalWrite(PUMP,LOW);
         Serial.printf("Startup .");
        PumpState = PUMP_ON;
        StateChange = true;
        //Initialize timer to internet if available, or zero.
        //
        break;
      case  PUMP_ON:
        digitalWrite(PUMP,HIGH);
               
          if (elapsed >= PUMP_TIME){
          PumpState = PUMP_OFF;
          StateChange = true;
          next_cycle = last_cycle + SECONDS_PER_CYCLE;
          Serial.printf("Pump Off");
        }
          else{
            PumpState = PUMP_ON;
          }
      
        //Initialize timer to internet if available, or zero.
        //
        break;
      case  PUMP_OFF:
      
        digitalWrite(PUMP,LOW);
      //  Serial.printf("Pump Off ");
        if (elapsed >= SECONDS_PER_CYCLE){
            last_cycle += SECONDS_PER_CYCLE;
            PumpState = PUMP_ON;
            StateChange = true; 
            Serial.printf("Pump On ");
            next_cycle = last_cycle + PUMP_TIME; 
        }
        else{
          PumpState = PUMP_OFF;
        }
        //Check internet time.
        break;

      default:
        digitalWrite(PUMP,LOW);        
        PumpState = STARTUP;
    }
      /////////////////////////////////////////////////////////////////////////////////////////
      if(StateChange){
        strftime(buff, 21, "%Y-%m-%d %H:%M:%S", localtime(&last_cycle));
        // Print information
        Serial.print(" Last cycle began: ");
        Serial.printf(buff);
        Serial.print(" Elapsed: ");
        Serial.printf("%5d",elapsed);
        strftime(buff, 21, "%Y-%m-%d %H:%M:%S", localtime(&next_cycle));
        Serial.printf(" Cycle End: %s", buff);
        Serial.println();

      }



  //  printLocalTime();
  } //If semaphore
}   //End of loop()