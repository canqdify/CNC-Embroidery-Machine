#include <WiFi.h>
#include "style.h"    // CSS
#include "code.h"     // JS
#include "index.h"    // Starting page html
#include "dbasePage.h"// If we wanted a second page
#include <LittleFS.h> // Not needed, but seems to make allocating flash memory easy. tools -> memory
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "stepper.h"
#include "gc.h"
// 192.168.42.1 IP address
// Prototypes

void ParseGCodeStep();
void ScheduleMovement(float dx, float dy);
void LoadJS(void);            // Load the one javascript page
void LoadStyle(void);         // Load the one css page
void LoadMainPage(void);      // Call for webpage to display home page
void LoadDataBasePage(void);  // Blank page to test loading other html
void StartEmbroidery(void);   // --
void AddRecord(void);         // --
void ReadFile(void);          // Read the database
void ClearDatabase(void);     // Currently deletes all data
void SendDBRecordsToJS(void); // Send all needed local db data to js
// Variables
WiFiClient client; // The client that connects
String request;
File file;
const char* ssid = "PiNetwork";
const char* password = "password";
const int ledPin = LED_BUILTIN;
const int maxLength = 64; // Max size for 
const unsigned int maxRecords = 100;  
int t = 0; // Test variable for file data insertion
WiFiServer server(80); // Creating socket on port 80

float lastX = 0.0f;      // Previous x coordinate
float lastY = 0.0f;      // Previous y coordinate
int hasLastPoint = 0;    // Flag indicating previous xypoint exists

char line[128] = {0};    // Input buffer from gcfile
int gBufferLength = 0;   // Length of input buffer

float x = 0.0f;          // Current x coordinate
float y = 0.0f;          // Current y coordinate

char* xPointer = 0;      // Pointer to x portion of line
char* yPointer = 0;      // Pointer to y portion of line

float dx = 0.0f;         // Delta x
float dy = 0.0f;         // Delta y

static const char* gPointer = gCodeText;

static bool moveScheduled = false;
static int hasLast = 0;

static float lastZ = 1.0f;
static bool ledState = false;
// Database entry structure, not utilized
typedef struct DataBaseEntry
{
  unsigned int id;
  char name[maxLength];
  char filepath[maxLength];
  char img[maxLength];
} DataBaseEntry;
DataBaseEntry dbEntry;
// Store final data
typedef struct GridMovement{
  float x;
  float y;
  float deltaX;   // Movement in X
  float deltaY;   // Movement in Y
  int xDir;       // -1, 0, +1
  int yDir;       // -1, 0, +1
} GridMovement;
GridMovement gridLocation;
// Test fields
unsigned int id = 0;
char name[] = "NewDevice";
char filepath[] = "/files/newdevice.html";
char img[] = "/img/newdevice.png";
// Only really 2 states I suppose
// waiting, stitching or not turned on
// Might want something more for js?
enum CurrentState 
{
  initilize, // not really, but for now
  waiting,
  stitching
};
CurrentState machineState = waiting; // base state
void setup() {
  Serial.begin(115200);             // Allow serial coms
  WiFi.softAP(ssid, password);      // Put pico into SoftAP
  Serial.print("AP started. IP: "); 
  Serial.println(WiFi.softAPIP());
  server.begin(); // Start the server
  Serial.println("Server started on port 80");
  LittleFS.begin();
  gpioInit();
  pioInitXY();
  dmaInitXY();
  // Add 4 records on start for testing
  for(int i=0; i < 4;i++)
  {
    AddRecord();
  }
}
void loop() 
{
  client = server.accept(); // Creates new socket for connection, checks for new client
  // If a connection is formed
  if (client) {
    Serial.println("Client connected");

    request = client.readStringUntil('\r'); // Gets clients web req. No purpose, just used for checking
    Serial.println(request);
    // Clears leftover client data, only wanted for reading client data
    // Stops constant printing
    client.flush();
    //client.print(wepPage);
    //client.stop(); // Disconect client
    // Browser will see includes, and make request
    // So it will load the html, then it will see js included, and make a call for it
    if (request.indexOf("POST /startEmbroidery") >= 0) 
    {
      machineState = stitching;
      t = !t;
      if(t)
      {
        digitalWrite(ledPin, HIGH);
      }
      else
      {
        digitalWrite(ledPin, LOW);
      }
      // JSON response
      // Headers
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: application/json");
      client.println("Connection: close"); 
      client.println();
      // Body the js expects, kvps ifin the js needs anything
      client.print("{\"status\":\"ok\",\"led\":\"toggled\"}"); // use print, not println
    } 
    else if (request.indexOf("POST /getDBData") >= 0)
    {
      SendDBRecordsToJS();
    }
    // Handle the GET request for the new page
    // Need to use GET for new pages
    else if (request.indexOf("GET /viewDB") >= 0) 
    {
      LoadDataBasePage();
    }
    // js
    else if (request.indexOf("GET /code.js") >= 0)
    {
      LoadJS();
    }
    // css
    else if (request.indexOf("GET /style.css") >= 0)
    {
      LoadStyle();
    }
    // main HTML req
    else 
    {
      LoadMainPage();
    }
  }
  //ParseGCodeStep();
  if (machineState == stitching) 
  {
    ParseGCodeStep();
  }
}
// Functions
// Called last upon connection detection,
void LoadMainPage()
{
  // Load html
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println();
  client.println(indexHtml);
}
// Load other html
void LoadDataBasePage()
{
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println();
  client.println(dbasePageHtml);  // HTML page
}
void LoadStyle()
{
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/css");
  client.println();
  client.println(styleCss);
}
void LoadJS()
{
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/javascript");
  client.println();
  client.println(codeJs);
}
// Test of pin control from web, and starts simple pattern stitching in main for testing
// Not actually called
void StartEmbroidery()
{
  t = !t;
  machineState = stitching;
  if(t)
  {
    digitalWrite(ledPin, HIGH);
  }
  else
  {
    digitalWrite(ledPin, LOW);
  }
  // JSON response
  // Headers
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close"); 
  client.println();
  // Body the js expects, kvps, ifin we need to talk back to js
  client.print("{\"status\":\"ok\",\"led\":\"toggled\"}"); // Use print, not println
}
// Adds a record to pico. This is a test function that skips the upload step, and assumes data is in place
void AddRecord()
{
  file = LittleFS.open("/data.json", "r");
  unsigned int hasEntries = 0;
  if (file)
  {
    hasEntries = file.size() > 2; // more than []
    file.close();
  }
  // Likely don't need to worry about r+, but could be wanted if considering update function
  file = LittleFS.open("/data.json", hasEntries ? "r+" : "w"); // r+, adds to if existing, w, overwrite
  if (!file)
  {
    Serial.println("Failed to open data.json");
    return;
  }
  if (hasEntries)
  {
    file.seek(file.size() - 1); // Move point to end of line, the closing ]
    file.println(",");
  }
  else
  {
    file.println("[");
  }
  // Adds new entry
  file.print("  {\"id\":");
  file.print(id++);
  file.print(",\"name\":\"");
  file.print(name);
  file.print("\",\"filepath\":\"");
  file.print(filepath);
  file.print("\",\"img\":\"");
  file.print(img);
  file.println("\"}");
  // Replaces closing ]
  file.println("]");
  file.close();
  Serial.println("New JSON record added");
}
void ReadFile() 
{
  // r for read
  file = LittleFS.open("/data.json", "r");
  if (!file) 
  {
    Serial.println("Failed to open file for reading");
    return;
  }
  Serial.println("----- /data.json contents -----");
  while (file.available()) 
  {
    Serial.write(file.read());  // Prints the file
  }
  Serial.println("\n--------------------------------");
  file.close();
}
// Just clears entire file for testing. Can be changed for indiviual line
void ClearDatabase()
{
  file = LittleFS.open("/data.json", "w");
  if (!file) 
  {
    Serial.println("Failed to open file for clearing.");
    return;
  }
  file.println("[]");
  file.close();
  Serial.println("Database cleared");
}
// Send stored db records to js for display
void SendDBRecordsToJS()
{
  // Not implemented yet
}

// Move through gcode file, line by line. Extracting x y and z. Z serves diagnostic purpose only
void ParseGCodeStep()
{
  static char line[128]; // Stores the line of gcode
  int len;               // Current length of line
  const char* start;     // Pointer to start of gcode line
  // Parsed x y and z coords
  float x;               
  float y;
  float z;
  // Calculated delta from last movement
  float dx;
  float dy;
  // Pointer to x y and z location in line
  char* xPos;
  char* yPos;
  char* zPos;

  int stitchThisLine; // Flag to see if a 'stitch' would be on this line. Likely not correct
  // If previous move not finished, wait
  if (moveScheduled && !(*xReady && *yReady))
    return;

  // Previous move just finished
  if (moveScheduled && (*xReady && *yReady))
  {
    moveScheduled = false;
  }
  // End of G-code
  if (*gPointer == '\0')
  {
    digitalWrite(ledPin, HIGH);
    return;
  }
  // Read line
  start = gPointer;
  while (*gPointer != '\n' && *gPointer != '\0')
    gPointer++;

  len = gPointer - start;
  if (*gPointer == '\n')
    gPointer++;

  if (len > 127)
    len = 127;

  memcpy(line, start, len);
  line[len] = '\0';

  // Parse coords
  x = NAN;
  y = NAN;
  z = NAN;

  xPos = strstr(line, "X");
  yPos = strstr(line, "Y");
  zPos = strstr(line, "Z");

  if (xPos) x = atof(xPos + 1);
  if (yPos) y = atof(yPos + 1);
  if (zPos) z = atof(zPos + 1);

  // First point initializes position
  if (!hasLast)
  {
    if (x == x) lastX = x;
    if (y == y) lastY = y;
    if (z == z) lastZ = z;

    hasLast = 1;
    return;
  }

  // Missing XY stays same
  if (x != x) x = lastX;
  if (y != y) y = lastY;

  dx = x - lastX;
  dy = y - lastY;

  stitchThisLine = 0;

  if (zPos)
  {
    if (lastZ == 1.0f && z == 0.0f)
    {
        ledState = !ledState;
        stitchThisLine = 1;
    }
    lastZ = z;
  }

  // If ready, schedule new move
  if (*xReady && *yReady)
  {
    ScheduleMovement(dx, dy);
    moveScheduled = true;

    if (stitchThisLine)
    {
        // delayMicroseconds(100000);
    }
  }
  lastX = x;
  lastY = y;
}
void ScheduleMovement(float dx, float dy) 
{
    uint32_t sx = STEP(fabsf(dx));
    uint32_t sy = STEP(fabsf(dy));

    if (sx == 0 && sy == 0) return;

    // Compute directions
    // 0 = 0, fine becuase no movement would be occuring
    // Direction: 1 = positive, 0 = negative
    int dirX = dx >= 0;            
    int dirY = dy >= 0;             

    // Store directions into the motor structs so we use them
    xMotor->dir = dirX;
    yMotor->dir = dirY;

    gpio_put(MOTOR_DIR_PIN_X, dirX);
    gpio_put(MOTOR_DIR_PIN_Y, dirY);

    xMotor->steps = sx;
    yMotor->steps = sy;

    moveMotor(xMotor);
    moveMotor(yMotor);

    // print
    // Serial.print("dx=");
    // Serial.print(dx, 6);
    // Serial.print(" dy=");
    // Serial.print(dy, 6);
    // Serial.print(" dirX=");
    // Serial.print(dirX);
    // Serial.print(" dirY=");
    // Serial.println(dirY);
}

// R marks it as a string literal, rawliteral is for stating the start and end
// Talking to the browser HTTP/1.1 200 OK
// HTTP version, error code, OK