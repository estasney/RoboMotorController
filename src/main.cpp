#include <Streaming.h>
#include <RF24.h>

// Arduino pin numbers

#define XR_pin 6 // analog pin connected to X output (right thumb)
#define YR_pin 7 // analog pin connected to Y output (left thumb)
#define XL_pin 0
#define YL_pin 1
#define LSWITCH_pin 5

#define THROTTLE_LEFT
#define CALIB_READINGS 25
#define XY_THRESHOLD 10
#define SMOOTHNESS_X 20
#define SMOOTHNESS_Y 10
#define RADIO_LEVEL RF24_PA_MAX

//#define TESTING

#ifdef THROTTLE_LEFT
const uint8_t X_pin = YR_pin;
const uint8_t Y_pin = YL_pin;
#else
const uint8_t X_pin = XL_pin;
const uint8_t Y_pin = XR_pin;
#endif

const uint8_t calibReadings = CALIB_READINGS;
const uint8_t XY_Threshold = XY_THRESHOLD; // Ignore values inside XY_Threshold
const uint8_t smoothnessX = SMOOTHNESS_X; // Number of readings to average
const uint8_t smoothnessY = SMOOTHNESS_Y; // Number of readings to average

RF24 radio(13, 3);
uint8_t address[][6] = {"R", "C"};
bool radioNumber = true;
struct calibData {
    int16_t XMid;
    int16_t YMid;
    int16_t XTrim;
    int16_t YTrim;
};

int16_t readingsX[smoothnessX];
int16_t readingsY[smoothnessY];
uint8_t readingsIdxX = 0;
uint8_t readingsIdxY = 0;
int16_t readingsXTotal = 0;
int16_t readingsYTotal = 0;
int16_t readingsXMean = 0;
int16_t readingsYMean = 0;
bool LSW = false;

calibData jiggleData = {0, 0, 0, 0};

void calibrate() {
    uint8_t readIndex = 0;
    int16_t Xtotal = 0;
    int16_t Ytotal = 0;
    while (readIndex < calibReadings) {
        Xtotal += analogRead(X_pin);
        Ytotal += analogRead(Y_pin);
        readIndex ++;
        delay(1);
    }
    jiggleData.XMid = Xtotal / calibReadings;
    jiggleData.YMid = Ytotal / calibReadings;
    jiggleData.XTrim = abs(jiggleData.XMid - 512);
    jiggleData.YTrim = abs(jiggleData.YMid - 512);
}

void readJoystick(int16_t &p1, int16_t &p2) {
    int16_t p1Temp, p2Temp;
    p1Temp = (analogRead(X_pin) + jiggleData.XTrim);
    p2Temp = (analogRead(Y_pin) + jiggleData.YTrim);

    p1Temp = (int16_t)map(p1Temp, 0, 1024, -255 - XY_Threshold, 255 + XY_Threshold);
    p2Temp = (int16_t)map(p2Temp, 0, 1024, -255 - XY_Threshold, 255 + XY_Threshold);


    if (abs(p1Temp) > XY_Threshold) {
        p1Temp = constrain(p1Temp, -255, 255);
        p1 = p1Temp;
    } else {
        p1 = 0;
    }

    if (abs(p2Temp) > XY_Threshold) {
        p2Temp = constrain(p2Temp, -255, 255);
        p2 = p2Temp;
    } else {
        p2 = 0;
    }
}

void setup() {
    #ifdef TESTING
    Serial.begin(115200);
    #endif
    pinMode(LSWITCH_pin, INPUT_PULLUP);
    pinMode(X_pin, INPUT);
    pinMode(Y_pin, INPUT);
    for (int & i : readingsX) {
        i = 0;
    }
    for (int & i : readingsY) {
        i = 0;
    }
    calibrate();
    if (!radio.begin()) {
    #ifdef TESTING
        Serial.println(F("radio hardware is not responding!!"));
    #endif
        while (!radio.begin()) {
            delay(1);
        } // hold in infinite loop
    }
    radio.openWritingPipe(address[radioNumber]);
    radio.setPALevel(RADIO_LEVEL);
    radio.stopListening();
}

void readSwitch() {
    static bool toggled = false;
    bool swPressed = !digitalRead(LSWITCH_pin);
    if (swPressed) {
        toggled = true;
        return;
    }
    if (toggled) {
        LSW = !LSW;
        digitalWrite(LED_BUILTIN, LSW);
        toggled = false;
    }
}

void loop() {
    readSwitch();
    int16_t X, Y;
    int16_t XY[2];
    readingsXTotal = readingsXTotal - readingsX[readingsIdxX];
    readingsYTotal = readingsYTotal - readingsY[readingsIdxY];
    readJoystick(X, Y);

    readingsX[readingsIdxX] = X;
    readingsY[readingsIdxY] = Y;

    readingsXTotal += readingsX[readingsIdxX];
    readingsYTotal += readingsY[readingsIdxY];

    readingsIdxX += 1;
    readingsIdxY += 1;
    if (readingsIdxX >= smoothnessX) {
        readingsIdxX = 0;
    }

    if (readingsIdxY >= smoothnessY) {
        readingsIdxY = 0;
    }

    readingsXMean = readingsXTotal / smoothnessX;
    readingsYMean = readingsYTotal / smoothnessY;

    if (LSW) {
        readingsXMean /= 2;
        readingsYMean /= 2;
    }

    XY[0] = readingsXMean;
    XY[1] = readingsYMean;

    #ifdef TESTING
    Serial << XY[0] << F(", ") << XY[1] << endl;
    #endif
    radio.write(&XY, sizeof(XY));
    delay(1);
}
