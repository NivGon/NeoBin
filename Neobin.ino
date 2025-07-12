//libraries
#include <Wire.h>
#include <Adafruit_GFX.h>       //OLED screen
#include <Adafruit_SH110X.h>    //OLED screen
#include <ESP32Servo.h>         //Servo
#include <Adafruit_NeoPixel.h>  //NeoLed
#include <BluetoothSerial.h>    //Bluetooth
#include <Adafruit_AHTX0.h>     //aht temp sensor


//total defines
#define i2c_Address 0x3c


//defines for OLED screen
#define SCREEN_WIDTH 128  //OLED display width, in pixels
#define SCREEN_HEIGHT 64  //OLED display height, in pixels
#define OLED_RESET -1
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


//defines for NeoLed
#define PIN 14
#define NUMPIXELS 8
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);


//defines for hcsr (ultrasonic raddar)
#define ECHO_PIN 33
#define TRIG_PIN 32


//defines for servo
Servo myServo;
#define SERVO_PIN 18


//define for AHT
Adafruit_AHTX0 aht;


//define LDR
#define LDR 34


//defines for IR
#define LIR 35    //left from front side
int irLeft = 0;   //left IR value read
#define RIR 36    //right from front side
int irRight = 0;  //right IR value read


//define for bluetooth connection
#define BT_NAME "NeoBin"
BluetoothSerial ESP_BT;


//define for button
#define BUTTONR 19  //right button below OLED


//counters and checkers
bool check = false;  //for bin full or problem
bool clean = false;  //for bin clean or checked
int message = 0;     //for sent message on bluetooth app once
String command = ""; //for enter commands from bluetooth app


void setup() {
  Serial.begin(9600);


  //neo led
  pixels.begin();
  pixels.clear();


  //RGB
  pinMode(2, OUTPUT);   //blue
  pinMode(26, OUTPUT);  //red
  pinMode(27, OUTPUT);  //green


  //IR
  pinMode(LIR, INPUT);
  pinMode(RIR, INPUT);


  //hcsr (ultrasonic raddar)
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);


  //servo
  myServo.attach(SERVO_PIN);
  myServo.write(0);
  delay(500);
  myServo.write(180);
  delay(500);
  myServo.write(90);


  //OLED
  display.begin(i2c_Address, true);
  display.clearDisplay();
  display.display();
  display.setTextColor(SH110X_WHITE, SH110X_BLACK);  //(0,1)


  //Buttons
  pinMode(BUTTONR, INPUT_PULLUP);


  //AHT sensor initialization check
  if (!aht.begin()) {
    Serial.println("Could not find AHT sensor!");
    displayMessage("Sensor", "Error");
    while (1) delay(10);
  }


  //Start Bluetooth
  ESP_BT.begin(BT_NAME);
  Serial.println("Bluetooth Started. Waiting for connections...");
}




void loop() {
  //AHT
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);


  //IR
  irLeft = digitalRead(LIR);
  irRight = digitalRead(RIR);


  //calc distance hcsr04
  long duration;
  float distance;
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  duration = pulseIn(ECHO_PIN, HIGH);
  distance = duration * 0.034 / 2;
  Serial.print("distance -> ");
  Serial.println(distance);


  //LDR Light
  int light = analogRead(LDR);
  Serial.print("light -> ");
  Serial.println(light);


  if (light <= 2000) {
    digitalWrite(26, HIGH);
    digitalWrite(27, HIGH);
    digitalWrite(2, HIGH);
  } else {
    digitalWrite(26, LOW);
    digitalWrite(27, LOW);
    digitalWrite(2, LOW);
  }


  if (irLeft == HIGH && irRight == HIGH && check != true) {
    if (distance >= 20) {
      displayMessage("Bin", "Closed");
      tone(15, 250, 75);
      delay(1000);
      servoAngle(90);
    } else {
      servoAngle(179);
      ESP_BT.println("bin open");
      displayMessage("Bin", "Open");
      for (int i = 0; i < NUMPIXELS; i++) {
        pixels.setPixelColor(i, 0, 100, 0);
      }
      pixels.show();
      for (int i = 0; i < 38; i++) {
        //sound
        tone(15, 250, 20);
        delay(250);


        //check for insert item into the bin
        irLeft = digitalRead(LIR);
        irRight = digitalRead(RIR);
        if (irLeft == LOW || irRight == LOW) {     //if one of IR get contact with item
          ESP_BT.println("Item detected in bin");  //message for user if bin was insert
        }
      }
      servoAngle(90);
    }  //else end
    pixels.clear();
    pixels.show();
  }


  if (irLeft == LOW && irRight == LOW) {
    displayMessage("Bin", "Full");  //message on screen
    tone(15, 250, 100);             //tone for bin full
    delay(250);
    for (int i = 0; i < NUMPIXELS; i++) {  //set neo led pixels color to red
      pixels.setPixelColor(i, 100, 0, 0);
    }
    pixels.show();                                      //turn on neo led
    ESP_BT.println("Bin full, go ahead and clean it");  //message for user to clean bin
    check = true;                                       //change checker so the code part to open/close the bin will not work
  }


  if (temp.temperature > 40) {         //if the temp is too high
    displayMessage("Bin", "Problem");  //message on screen
    tone(15, 250, 100);
    delay(250);
    for (int i = 0; i < NUMPIXELS; i++) {  //set neo led pixels color to red
      pixels.setPixelColor(i, 100, 0, 0);
    }
    pixels.show();
    check = true;                                       //change checker like previous one
    ESP_BT.println("Problem with temperature of bin");  //message for user to check bin
  }


  if (ESP_BT.available()) {
    command = ESP_BT.readStringUntil('\n'); //to enter commands from bluetooth app
    command.trim();
  }


  if (check == true) {
    if (clean == false) {
      while (message == 0) { //to sent message in bluetooth once
        ESP_BT.println("to open the bin, prees the right button or enter 'open' in app");
        message++;
      }


      if (digitalRead(BUTTONR) == LOW || command.equalsIgnoreCase("open")) { //if button was touched or user enter command from app
        servoAngle(179);
        ESP_BT.println("bin open");
        displayMessage("Bin", "Open");
        clean = true;
        message = 0;
        delay(2500);
      }
    } else if (clean == true) {
      while (message == 0) {
        ESP_BT.println("to close the bin, prees the right button again or enter 'close' in app");
        message++;
      }


      if (digitalRead(BUTTONR) == LOW || command.equalsIgnoreCase("close")) { //if button was touched or user enter command from app
        servoAngle(90);
        ESP_BT.println("Bin close and return to work");
        clean = false;
        check = false;
        message = 0;
      }
    }
  }
}


void displayMessage(String line1, String line2) {  //method to print on OLED
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(50, 10);
  display.print(line1);
  display.setCursor(35, 35);
  display.print(line2);
  display.display();
}

void servoAngle(int angle) {
  myServo.attach(SERVO_PIN);
  myServo.write(angle);


  //Servo check
  int angleNow = myServo.read();
  Serial.print("angle -> ");
  Serial.println(angleNow);


  delay(50);
  myServo.detach();
}
