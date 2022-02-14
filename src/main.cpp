#include <Streaming.h>
#include <RF24.h>
#include <SPI.h>

// Arduino pin numbers

#define LY A2
#define LX A1

#define RX A3
#define RY A0

#define THROTTLE_LEFT
#define CALIB_READINGS 25
#define XY_THRESHOLD 10
#define SMOOTHNESS_X 20
#define SMOOTHNESS_Y 10
#define RADIO_LEVEL RF24_PA_MAX
#define RADIO_CE_PIN 7
#define RADIO_CS_PIN 8

//#define TESTING
#define STDOUT

#ifdef THROTTLE_LEFT
const uint8_t X_pin = LY;
const uint8_t Y_pin = RX;
#else
const uint8_t X_pin = RY;
const uint8_t Y_pin = LX;
#endif

const uint8_t calibReadings = CALIB_READINGS;
const uint8_t XY_Threshold = XY_THRESHOLD; // Ignore values inside XY_Threshold
const uint8_t smoothnessX = SMOOTHNESS_X; // Number of readings to average
const uint8_t smoothnessY = SMOOTHNESS_Y; // Number of readings to average



RF24 radio(RADIO_CE_PIN, RADIO_CS_PIN);
uint8_t address[][6] = {"R", "C"};
uint8_t radioNumber = 1;
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
    #ifdef STDOUT
    Serial.begin(9600);
    #endif
    pinMode(X_pin, INPUT);
    pinMode(Y_pin, INPUT);
    pinMode(RADIO_CS_PIN, OUTPUT);
    pinMode(RADIO_CE_PIN, OUTPUT);
    memset(readingsX, 0, smoothnessX);
    memset(readingsY, 0, smoothnessY);
    calibrate();
    if (!radio.begin()) {
    #ifdef STDOUT
        Serial.println(F("Radio hardware is not responding!!"));
    #endif
        while (!radio.begin()) {
            delay(1);
        } // hold in infinite loop
    }
    radio.openWritingPipe(address[radioNumber]);
    radio.setPALevel(RADIO_LEVEL);
    radio.stopListening();
}

void loop() {
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

    #ifdef STDOUT
    Serial << XY[0] << F(", ") << XY[1] << endl;
    #endif
    radio.write(&XY, sizeof(XY));
    delay(1);
}
