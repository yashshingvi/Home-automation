#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <Servo.h>
Servo servo;
#define rel 4                   //relay on pin 4
#define ser 12                  //servo on pin 12
ESP8266WiFiMulti wifiMulti;     // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

ESP8266WebServer server(80);    // Create a webserver object that listens for HTTP request on port 80
int pos=0;
boolean adm=LOW;
//String L1Status, L2Status, L3Status, L4Status, Temperature;
void handleRoot();              // function prototypes for HTTP handlers
void handleLogin();
void handleNotFound();
void handleSERVO();
void handleRelay();
void handleUpdate();
void handleTest();

const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";
void setup(void) {
  //  WiFi.softAPdisconnect (true);
  Serial.begin(115200);                     // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println('\n');

  wifiMulti.addAP("Username0", "Pass0");   // add Wi-Fi networks you want to connect to
  wifiMulti.addAP("Username1", "Pass1");
  
  IPAddress ip(192, 168, 1, 50);          // This is used to set desired IP Address (static ip)
  IPAddress gateway(192, 168, 1, 1);      // set gateway to match your network
  Serial.print(F("Setting static ip to : "));
  Serial.println(ip);
  IPAddress subnet(255, 255, 255, 0);     // set subnet mask to match your network

  WiFi.config(ip, gateway, subnet);       //connection configuration
  Serial.println("Connecting ...");
  int i = 0;
  while (wifiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    delay(250);
    Serial.print('.');
  }
  Serial.println('\n');
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());               // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());            // Send the IP address of the ESP8266 to the computer

  if (MDNS.begin("Home")) {                 // Start the mDNS responder for esp8266.local
                                            //You can access web page on Home.local using ios/windows
                                            //android does not support this yet
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }
  server.on("/", HTTP_GET, handleRoot);        // Call the 'handleRoot' function when client requests '/' or opens webpage
  server.on("/login", HTTP_POST, handleLogin); // Call the 'handleLogin' function when a POST request is made to URI "/login"
  server.onNotFound(handleNotFound);           // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"
  server.on("/SERVO", HTTP_POST, handleSERVO); // Call the 'handleSERVO' function when a POST request is made to URI "/SERVO"
  server.on("/rel", HTTP_POST, handleRelay);   // Call the 'handleRelay' function when a POST request is made to URI "/rel"
  server.on("/test", HTTP_POST, handleTest);   // Call the 'handleTest' function when a POST request is made to URI "/test"
  server.on("/update", HTTP_POST, []() {       // function when a POST request is made to URI "/Update"
                                               //This function is used for OTA update this function can be found in Examples
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.setDebugOutput(true);
      WiFiUDP::stopAll();
      Serial.printf("Update: %s\n", upload.filename.c_str());
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      if (!Update.begin(maxSketchSpace)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
      Serial.setDebugOutput(false);
    }
    yield();
  });
  
  server.begin();                            // Actually start the server
  Serial.println("HTTP server started");
  servo.attach(ser);
 
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(rel, OUTPUT);
  digitalWrite(rel, LOW);
}

void loop(void) {
  server.handleClient();                     // Listen HTTP requests from clients
  
  MDNS.update();
}

void handleRoot() {                          // When URI / is requested, send a web page with form for entering credentials
  server.send(200, "text/html", "<body bgcolor=\"#add8e6\"> <form action=\"/login\" method=\"POST\"><input type=\"text\" name=\"username\" placeholder=\"Username\"></br><input type=\"password\" name=\"password\" placeholder=\"Password\"></br><input type=\"submit\" value=\"Login\"></form><p>Please enter credentials ...</p>");
}

void handleLogin() {                         // If a POST request is made to URI /login
  if ( ! server.hasArg("username") || ! server.hasArg("password")
       || server.arg("username") == NULL || server.arg("password") == NULL) { // If the POST request doesn't have username and password data
    server.send(400, "text/plain", "400: Invalid Request");         // The request is invalid, so send HTTP status 400
    return;
  }
  if ((server.arg("username") == "yash" && server.arg("password") == "111") || (server.arg("username") == "lok" && server.arg("password") == "123") || (server.arg("username") == "guest" && server.arg("password") == "123")) { // If both the username and the password are correct
    //If credentials are correct send a page with buttons for servo and relay
    server.send(200, "text/html",  "<h1>Welcome, " + server.arg("username") + "!</h1><p>Login successful</p><body bgcolor=\"#add8e6\"><form action=\"/SERVO\" method=\"POST\"><input type=\"submit\" value=\"UNLOCK DOOR\"></form> <form action=\"/rel\" method=\"POST\"><input type=\"submit\" value=\"relay\"></form>");
  }
  else if (server.arg("username") == "admin" && server.arg("password") == "update"){ //Update login to update the code over the air
    server.send(200, "text/html", serverIndex);
  }
  else if (server.arg("username") == "admin" && server.arg("password") == "test"){ //Admin login to test motor
    adm=HIGH;
    server.send(200, "text/html", "<h1>Welcome, " + server.arg("username") + "!</h1><p>Login successful</p><form action=\"/SERVO\" method=\"POST\"><input type=\"submit\" value=\"UNLOCK DOOR\"></form> <form action=\"/rel\" method=\"POST\"><input type=\"submit\" value=\"relay\"></form><form action=\"/test\" method=\"POST\"><input type=\"number\" name=\"val1\" placeholder=\"Val1\"></br><input type=\"number\" name=\"degree\" placeholder=\"Degree pref.0\"></br><input type=\"text\" name=\"val2\" placeholder=\"Val2\"><input type=\"number\" name=\"degree2\" placeholder=\"dergree pref.100\"></br></br><input type=\"submit\" value=\"GO\"></form><p>Please enter values ...</p>");
   }
  else {                                                                              // Username and password don't match
    server.send(401, "text/plain", "401: Unauthorized");
  }
}

void handleRelay() {
  digitalWrite(rel, !digitalRead(rel));//change the state of relay
  server.send(200, "text/html", "<form action=\"/SERVO\" method=\"POST\"><input type=\"submit\" value=\"UNLOCK DOOR\"></form> <form action=\"/rel\" method=\"POST\"><input type=\"submit\" value=\"relay\"></form>");
}

void handleSERVO() {                              // If a POST request is made to URI /SERVO
  //  digitalWrite(led,!digitalRead(led));      // Change the state of the LED if required for indication
  digitalWrite(LED_BUILTIN, LOW);
  for (pos = 0; pos <= 120; pos += 1){
    servo.write(pos);                             //Make servo rotate by angle required
    delay (15);
  }
  delay (1000 * 3);
   for (pos = 120; pos >= 0; pos -= 1){
    servo.write(pos);                             //Bring back servo to home position
    delay(15);
   }
  Serial.println("hi");
  if (adm==HIGH){
    adm=LOW;
    server.send(200, "text/html", "<form action=\"/SERVO\" method=\"POST\"><input type=\"submit\" value=\"UNLOCK DOOR\"></form> <form action=\"/rel\" method=\"POST\"><input type=\"submit\" value=\"relay\"></form><form action=\"/test\" method=\"POST\"><input type=\"number\" name=\"val1\" placeholder=\"Val1\"></br><input type=\"text\" name=\"val2\" placeholder=\"Val2\"></br><input type=\"submit\" value=\"GO\"></form><p>Please enter values ...</p>");
  }
  else{
    server.sendHeader("Location", "/");       // Add a header to respond with a new location for the browser to go to the home page again
    server.send(303);                         // Send it back to the browser with an HTTP status 303 (See Other) to redirect
  }
  digitalWrite(LED_BUILTIN, HIGH);            // Change the state of the LED if required for indication
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there is no handler for URI in the request
}

void handleTest(){    //for testing take servo values from webpage and then execute them
  digitalWrite(LED_BUILTIN, LOW);           // Change the state of the LED if required for indication
  servo.write((server.arg("degree")).toInt());
//  delay(385);
  delay((server.arg("val1")).toInt());
  servo.write(90);
  delay(1000 * 5);
  servo.write((server.arg("degree2")).toInt());
//  delay(400);
  delay((server.arg("val2")).toInt());
  servo.write(90);
  server.send(200, "text/html", "<form action=\"/SERVO\" method=\"POST\"><input type=\"submit\" value=\"UNLOCK DOOR\"></form> <form action=\"/rel\" method=\"POST\"><input type=\"submit\" value=\"relay\"></form><form action=\"/test\" method=\"POST\"><input type=\"text\" name=\"val1\" placeholder=\"Val1\"></br><input type=\"text\" name=\"val2\" placeholder=\"Val2\"></br><input type=\"submit\" value=\"GO\"></form><p>Please enter values ...</p>");
  digitalWrite(LED_BUILTIN, HIGH);
}
