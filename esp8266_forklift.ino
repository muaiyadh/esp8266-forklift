/**
 *  Sources:
 *    POST request example: https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WebServer/examples/PostServer/PostServer.ino
 *    L298N motor driver example: https://create.arduino.cc/projecthub/ryanchan/how-to-use-the-l298n-motor-driver-b124c5
 *    28BYJ-48 and ULN2003 example: https://www.makerguides.com/28byj-48-stepper-motor-arduino-tutorial/
 */

#include <ESP8266WiFi.h> // Import required ESP8266 libraries to make the website and function properly
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include <Stepper.h> // Import the stepper motor library

// Define the stepper motor IN pins from D5-D8
#define SMIN1 D5
#define SMIN2 D6
#define SMIN3 D7
#define SMIN4 D8

// Define DC motors controller pins
#define DCIN1 D1
#define DCIN2 D2
#define DCIN3 D3
#define DCIN4 D4

//Define the limit switch pin as A0 (Cannot use other GPIOs as they're connected by SPI to flash the ESP8266)
#define LS A0

const int stepsPerRevolution = 200; // Change this to fit the number of steps per revolution (for the 28BYJ-48: 360 / 1.8 degrees)
bool stopStepper = true; // Stop the motor initially
int stepperSteps = 0;

// Initialize the stepper library on the stepper motor IN pins
Stepper stepperMotor(stepsPerRevolution, SMIN1, SMIN3, SMIN2, SMIN4);

const char* ssid     = ""; // Network's name
const char* password = ""; // Network's password

ESP8266WebServer server(80); // Start a server on port 80 (default HTTP port)

// HTML content of page to be displayed to the user.
const String postForms = "<!DOCTYPE HTML>\
<html>\
  <head>\
    <title>ESP8266 Forklift</title>\
    <!-- Required meta tags -->\
    <meta charset=\"utf-8\">\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1, shrink-to-fit=no\">\
    <!-- Bootstrap CSS -->\
    <link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/4.0.0-beta.2/css/bootstrap.min.css\">\
    <style>\
    button {\
      margin: auto;\
    }\
    </style>\
  </head>\
  <body>\
    <div class=\"container\">\
      <h2 style=\"text-align: center;\">ESP8266 Controlled Mini Forklift</h2>\
      <div class=\"row\">\
        <div class=\"col\">\
          <div class=\"row\">\
            <button id=\"up\" type=\"button\" class=\"btn btn-danger\">↑</button>\
          </div>\
          <div class=\"row\">\
            <div style=\"margin-left:auto; margin-right:40px\">\
              <button id=\"left\" type=\"button\" class=\"btn btn-danger\">←</button>\
            </div>\
            <div style=\"margin-right:auto;\">\
              <button id=\"right\" type=\"button\" class=\"btn btn-danger\">→</button>\
            </div>\
          </div>\
          <div class=\"row\">\
            <button id=\"down\" type=\"button\" class=\"btn btn-danger\">↓</button>\
          </div>\
        </div>\
        <div class=\"col\">\
          <div class=\"row\">\
            <button id=\"liftup\" type=\"button\" class=\"btn btn-warning\">↑</button>\
          </div>\
          <br>\
          <div class=\"row\">\
            <button id=\"liftdown\" type=\"button\" class=\"btn btn-warning\">↓</button>\
          </div>\
        </div>\
      </div>\
    </div>\
    <script>\
    // Function to send POST requests (Used instead of GET requests to not fill the browser history)\n\
    function sendRequest(msg) {\n\
      let xhr = new XMLHttpRequest();\n\
      xhr.open(\"POST\", window.location.href, true);\n\
      xhr.setRequestHeader(\"Content-Type\", \"text/plain\");\n\
      xhr.send(msg);\n\
    }\n\
    // Common release button function\n\
    function release(){\n\
      sendRequest(\"-1\");\n\
    }\n\
    \
    // Get each button using id and add to buttons list\n\
    let buttons = [];\
    buttons.push(document.querySelector(\"#up\"));\n\
    buttons.push(document.querySelector(\"#left\"));\n\
    buttons.push(document.querySelector(\"#right\"));\n\
    buttons.push(document.querySelector(\"#down\"));\n\
    buttons.push(document.querySelector(\"#liftup\"));\n\
    buttons.push(document.querySelector(\"#liftdown\"));\n\
    \
    // Assign the proper functions for each button\n\
    for(let i = 0; i < buttons.length; i++){\n\
      \
      // Use onpointerup and onpointerdown to work on both touch and non-touch devices\n\
      buttons[i].onpointerup = release;\n\
      buttons[i].onpointerdown = function(){ sendRequest(i.toString()) };\n\
    }\n\
    // Use global pointer listeners to release as a safety measure\n\
    window.addEventListener('pointerup', release)\n\
    window.addEventListener('pointerleave', release)\n\
    window.addEventListener('pointerout', release)\n\
    </script>\
  </body>\
</html>";

// Handle all requests sent to the root directory '/'
void handleRoot() {
  if (server.method() == HTTP_GET) { // If the user needs to see the website, send the HTML code above
    server.send(200, "text/html", postForms);
  } else if (server.method() == HTTP_POST){ // If the user is sending a POST request, process it as a request to start/stop motors
    server.send(200, "text/plain", "Processing request...");
    String req = server.arg("plain"); // Get the plain text part of the request
    Serial.println(req); // Print it to the serial monitor for debugging purposes
    startMove(req.toInt()); // Convert the text to an integer, and send it as a parameter of the startMove function
  }
}

void setup() {
  Serial.begin(115200); // Start a serial connection with a baudrate of 115200
  pinMode(DCIN1, OUTPUT); // Set all DC motors controller pins as output
  pinMode(DCIN2, OUTPUT);
  pinMode(DCIN3, OUTPUT);
  pinMode(DCIN4, OUTPUT);
  
  pinMode(SMIN1, OUTPUT); // Set all stepper motor pins as output
  pinMode(SMIN2, OUTPUT);
  pinMode(SMIN3, OUTPUT);
  pinMode(SMIN4, OUTPUT);

  pinMode(LS, INPUT_PULLUP); // Set the limit switches pin as input and attach a pull-up to 3.3v
  
  WiFi.begin(ssid, password); // Connect to the specified network
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print some information
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP()); // ***** USED FOR CONNECTING TO THE WEBSERVER *****

  server.on("/", handleRoot); // Attach the handleRoot function for all requests going to the root directory '/'

  server.begin(); // Begin the server
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient(); // Handle client requests

  // Check if the stepper motor is allowed to move, and if non of the limit switches are triggered ( > 512 means the voltage is larger than 3.3v / 2)
  if (!stopStepper && analogRead(LS) > 512) {
    stepperMotor.step(stepperSteps); // Move the stepper motor the specified number of steps
    delay(5); // Required delay by the stepper motor to finish the step
  }
}

void startMove(int i){
  switch (i){
    case 0: // Move forwards
      digitalWrite(DCIN1, HIGH); //Set both motors to the same direction
      digitalWrite(DCIN2, LOW);
      digitalWrite(DCIN3, HIGH);
      digitalWrite(DCIN4, LOW);
      break;
    case 1: // Turn left
      digitalWrite(DCIN1, LOW); //Set both motors to opposite directions
      digitalWrite(DCIN2, HIGH);
      digitalWrite(DCIN3, HIGH);
      digitalWrite(DCIN4, LOW);
      break;
    case 2: // Turn right
      digitalWrite(DCIN1, HIGH); //Set both motors to opposite directions (invert of turn left)
      digitalWrite(DCIN2, LOW);
      digitalWrite(DCIN3, LOW);
      digitalWrite(DCIN4, HIGH);
      break;
    case 3: // Move backwards
      digitalWrite(DCIN1, LOW); //Set both motors to the same direction (invert of move forwards)
      digitalWrite(DCIN2, HIGH);
      digitalWrite(DCIN3, LOW);
      digitalWrite(DCIN4, HIGH);
      break;
    case 4: // Lift up
      startMove(-1); // Stop the DC motors as a safety measure (Has to be before the next lines as this stops the stepper temporarily)
      stopStepper = false; // The stepper needs to move
      stepperSteps = 1; // Set the steps as positive to move up
      break;
    case 5: // Lift down
      startMove(-1); // Stop the DC motors as a safety measure (Has to be before the next lines as this stops the stepper temporarily)
      stopStepper = false; // The stepper needs to move
      stepperSteps = -1; // Set the steps as negative to move down
      break;
    default: // Any number other than the other cases, STOP ALL MOTORS
      digitalWrite(DCIN1, LOW); //Set all motors' pins to low to stop them
      digitalWrite(DCIN2, LOW);
      digitalWrite(DCIN3, LOW);
      digitalWrite(DCIN4, LOW);

      stopStepper = true; // Make the stepper motor stop
      stepperSteps = 0; // Set the steps as 0 to stop the motor (the line above should be enough but this is put as a safety measure) 
      break;
  }
}
