#include <Servo.h>

Servo servo1;
Servo servo2;

const int pinServo1 = 9;
const int pinServo2 = 10;

int positionServo1 = 90;
int positionServo2 = 90;

void setup() {
  servo1.attach(pinServo1);
  servo2.attach(pinServo2);

  servo1.write(90);
  servo2.write(90);

  Serial.begin(9600);
}

void loop() {
  if (Serial.available() > 1) {
    char command = Serial.read();
    int value = Serial.read();

    if (command == 'l') {
      positionServo1 = max(0, positionServo1 - 15);
      servo1.write(positionServo1);
    } else if (command == 'r') {
      positionServo1 = min(180, positionServo1 + 15);
      servo1.write(positionServo1);
    } else if (command == 'u') {
      positionServo2 = max(0, positionServo2 - 15);
      servo2.write(positionServo2);
    } else if (command == 'd') {
      positionServo2 = min(180, positionServo2 + 15);
      servo2.write(positionServo2);
    } else if (command == 'h') {
      positionServo2 = constrain(value, 0, 180);
      servo2.write(positionServo2);
    } else if (command == 'g') {
      positionServo1 = constrain(value, 0, 180);
      servo1.write(positionServo1);
    }
  }
}