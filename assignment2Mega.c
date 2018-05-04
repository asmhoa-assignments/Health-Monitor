// Assignment 2
// Code for ATMega
// Eddy, Kyuri, Amol

#include <stdio.h>
#include <stdbool.h>
#include <Elegoo_GFX.h>    // Core graphics library
#include <Elegoo_TFTLCD.h> // Hardware-specific library
#include <TouchScreen.h>
#include <TimerThree.h> // SYSTEM TICK

/* SERIAL COMMUNICATIONS CONSTANT */
#define START_MESSAGE '>'
#define END_TERM ','
#define END_MESSAGE '<'

char requestingTaskID = 0;
char requestedFunction = 0;
int incomingData = 0;


// ================================================ TFT ================================================================
/* INITIALIZATION - SETUP TFT DISPLAY */
#define LCD_CS A3 // Chip Select goes to Analog 3
#define LCD_CD A2 // Command/Data goes to Analog 2
#define LCD_WR A1 // LCD Write goes to Analog 1
#define LCD_RD A0 // LCD Read goes to Analog 0
#define LCD_RESET A4 // Can alternately just connect to Arduino's reset pin

#define YP A3  // must be an analog pin, use "An" notation!
#define XM A2  // must be an analog pin, use "An" notation!
#define YM 9   // can be a digital pin
#define XP 8   // can be a digital pin

// Assign human-readable names to some common 16-bit color values:
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF
#define ORANGE  0xFD40

// Color definitions
#define ILI9341_BLACK       0x0000      /*   0,   0,   0 */
#define ILI9341_NAVY        0x000F      /*   0,   0, 128 */
#define ILI9341_DARKGREEN   0x03E0      /*   0, 128,   0 */
#define ILI9341_DARKCYAN    0x03EF      /*   0, 128, 128 */
#define ILI9341_MAROON      0x7800      /* 128,   0,   0 */
#define ILI9341_PURPLE      0x780F      /* 128,   0, 128 */
#define ILI9341_OLIVE       0x7BE0      /* 128, 128,   0 */
#define ILI9341_LIGHTGREY   0xC618      /* 192, 192, 192 */
#define ILI9341_DARKGREY    0x7BEF      /* 128, 128, 128 */
#define ILI9341_BLUE        0x001F      /*   0,   0, 255 */
#define ILI9341_GREEN       0x07E0      /*   0, 255,   0 */
#define ILI9341_CYAN        0x07FF      /*   0, 255, 255 */
#define ILI9341_RED         0xF800      /* 255,   0,   0 */
#define ILI9341_MAGENTA     0xF81F      /* 255,   0, 255 */
#define ILI9341_YELLOW      0xFFE0      /* 255, 255,   0 */
#define ILI9341_WHITE       0xFFFF      /* 255, 255, 255 */
#define ILI9341_ORANGE      0xFD20      /* 255, 165,   0 */
#define ILI9341_GREENYELLOW 0xAFE5      /* 173, 255,  47 */
#define ILI9341_PINK        0xF81F

#define MINPRESSURE 10
#define MAXPRESSURE 1000

//Touch For New ILI9341 TP
#define TS_MINX 120
#define TS_MAXX 900

#define TS_MINY 70
#define TS_MAXY 920
// We have a status line for like, is FONA working
#define STATUS_X 10
#define STATUS_Y 65

Elegoo_GFX_Button menuButtons[4];
Elegoo_GFX_Button measureSelectButtons[3];
Elegoo_GFX_Button AcknowledgeButton;

Elegoo_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);

TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// ================================================ TFT ================================================================


/* INITIALIZATION - GLOBAL VARIABLES */
// Measurements
unsigned int temperatureRawBuf[8] = {75, 75, 75, 75, 75, 75, 75, 75};
unsigned int bloodPressRawBuf[16] = {80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80};
unsigned int pulseRateRawBuf[8] = {8, 8, 8, 8, 8, 8, 8, 8};
int currTemp;
int currSys;
int currDia;
int currPr;

// index for flags (0 -> 4) : measure, compute, display, annunciate, status
unsigned short addFlags[6] = {0};
unsigned short removeFlags[6] = {0};

// Display
unsigned char tempCorrectedBuf[8];
unsigned char bloodPressCorrectedBuf[16];
unsigned char pulseRateCorrectedBuf[8];

// Status
unsigned short batteryState = 200;

// Alarms
unsigned char bpOutOfRange = 0,
    tempOutOfRange = 0,
    pulseOutOfRange = 0;

// Warning
bool bpHigh = false,
    tempHigh = false,
    pulseLow = false,
    battLow = false;

// TFT Keypad
unsigned short functionSelect = 0,
    measurementSelection = 0, alarmAcknowledge = 0;

/* INITIALIZATION - MAKE TASK BLOCK STRUCTURE */
struct TaskStruct {
    void (*taskFuncPtr)(void*);
    void* taskDataPtr;
    struct TaskStruct* next;
    struct TaskStruct* prev;
}; typedef struct TaskStruct TCB;


// HEAD OF TCB DOUBLE LINKED LIST 
TCB* linkedListHead = NULL;
// Traversing Pointer
TCB* currPointer = NULL;


/* INITIALIZATION - MAKE TASK DATA POINTER BLOCKS */
struct DataForMeasureStruct {
    unsigned int* temperatureRawPtr;
    unsigned int* bloodPressRawPtr;
    unsigned int* pulseRateRawPtr;
    unsigned short* measurementSelectionPtr;
}; typedef struct DataForMeasureStruct MeasureTaskData;

struct DataForComputeStruct {
    unsigned int* temperatureRawPtr;
    unsigned int* bloodPressRawPtr;
    unsigned int* pulseRateRawPtr;
    unsigned char* temperatureCorrectedPtr;
    unsigned char* bloodPressCorrectedPtr;
    unsigned char* prCorrectedPtr;
    unsigned short* measurementSelectionPtr;
}; typedef struct DataForComputeStruct ComputeTaskData;

struct DataForDisplayStruct {
    unsigned char* temperatureCorrectedPtr;
    unsigned char* bloodPressCorrectedPtr;
    unsigned char* prCorrectedPtr;
    unsigned short* batteryStatePtr;
}; typedef struct DataForDisplayStruct DisplayTaskData;

struct DataForWarningAlarmStruct {
    unsigned char* temperatureCorrectedPtr;
    unsigned char* bloodPressCorrectedPtr;
    unsigned char* prCorrectedPtr;
    unsigned short* batteryStatePtr;
}; typedef struct DataForWarningAlarmStruct WarningAlarmTaskData;

struct DataForStatusStruct {
    unsigned short* batteryStatePtr;
}; typedef struct DataForStatusStruct StatusTaskData;

struct DataForKeypadStruct {
    unsigned short* measurementSelectionPtr;
    unsigned short* alarmAcknowledgePtr;
}; typedef struct DataForKeypadStruct KeypadTaskData;

struct DataForCommsStruct {
    unsigned short* measurementSelectionPtr;
    unsigned char* temperatureCorrectedPtr;
    unsigned char* bloodPressCorrectedPtr;
    unsigned char* prCorrectedPtr;
}; typedef struct DataForCommsStruct CommsTaskData;



/* INITIALIZATION - MAKE INSTANCES OF TCB */
TCB MeasureTask;
TCB ComputeTask;
TCB DisplayTask;
TCB AnnunciateTask;
TCB StatusTask;
// Newly added Keypad task
TCB KeyPadTask;

TCB NullTask;

// Data
MeasureTaskData dataForMeasure;
ComputeTaskData dataForCompute;
DisplayTaskData dataForDisplay;
WarningAlarmTaskData dataForWarningAlarm;
StatusTaskData dataForStatus;
    // For Keypad
KeypadTaskData dataForKeypad;
CommsTaskData dataForComms;


/* INITIALIZATION - FUNCTION DEFINITIONS */
void requestMessage(String taskID, String funcToRun, String data) {
    Serial1.println(START_MESSAGE + taskID + END_TERM + funcToRun + END_TERM
        + data + END_MESSAGE);
}

void parseMessage() {
    while(0 == Serial1.available()) {

    }
    //  read incoming byte from the mega
    while(Serial1.available() > 0) {
        char currChar = Serial1.read();
        if ('>' == currChar) {
            delay(20);
            requestingTaskID = (char)Serial1.read();
            Serial1.read(); //Read over terminator

            delay(20);
            requestedFunction = Serial1.read();
            Serial1.read();

            delay(20);
            incomingData = Serial1.parseInt();
            Serial1.read();
        }
    }
}

void measureDataFunc(void* data) {
    MeasureTaskData* dataToMeasure = (MeasureTaskData*)data;
    MeasureTaskData dataStruct = *dataToMeasure;
    
    int i;
    const int sizeBuf = 8;
    unsigned int temperatureRawBufTemp[sizeBuf];
    unsigned int systolicPressRawBuf[sizeBuf];
    unsigned int diastolicPressRawBuf[sizeBuf];
    unsigned int pulseRateRawBufTemp[sizeBuf];
    
    for(i = 0; i < sizeBuf * 2; i++) {
        if(i < sizeBuf) {
            temperatureRawBufTemp[i] = *(dataStruct.temperatureRawPtr + i);
            systolicPressRawBuf[i] = *(dataStruct.bloodPressRawPtr + i);
            pulseRateRawBufTemp[i] = *(dataStruct.pulseRateRawPtr + i);
        } else {
            diastolicPressRawBuf[i- sizeBuf] = *(dataStruct.bloodPressRawPtr + i);
        }
        
    }
    
    // NEED TO SEND THESE FOUR NUMBERS OVER TO UNO ----------------------------------------------------------
    currTemp = temperatureRawBufTemp[0];
    currSys = systolicPressRawBuf[0];
    currDia = diastolicPressRawBuf[0];
    currPr = pulseRateRawBufTemp[0];

    // THIS IS WHERE COMMUNICATION SHOULD HAPPEN ------------------------------------------------------------
    // SEND REQUEST MESSAGE
    requestMessage("M", "T", String(currTemp));
    parseMessage();
    currTemp = incomingData;
    delay(100);
    requestMessage("M", "S", String(currSys));
    parseMessage();
    currSys = incomingData;
    delay(100);
    requestMessage("M", "D", String(currDia));
    parseMessage();
    currDia = 5;
    delay(100);
    requestMessage("M", "P", String(currPr));
    parseMessage();
    currPr = 5;

    char requestingTaskID = 0;
    char requestedFunction = 0;
    int incomingData = 0;

    // |/ UNO ---------------------------------------------------------------------------------------------------
      
    

        // if((prMeasured < low ) || (prMeasured > high)) {
        //     currPr = prMeasured;
        // }

        // ^ UNO ------------------------------------------------------------------------------------------

        // RECEIVE RESPONSE MESSAGE:
        // 4 NEW CURR VALUES SHOULD BE RETURNED TO MEGA AT THIS POINT,
        // UPDATE CURR VALUES
        
        
        // Update the buffer
        for(i = 0; i < sizeBuf - 1; i++) {
            temperatureRawBufTemp[i + 1] = temperatureRawBufTemp[i];
            systolicPressRawBuf[i + 1] = temperatureRawBufTemp[i];
            diastolicPressRawBuf[i + 1] = diastolicPressRawBuf[i];
            if(currPr != pulseRateRawBufTemp[0]) {
                pulseRateRawBufTemp[i + 1] = pulseRateRawBufTemp[i];
            }
        }

        temperatureRawBufTemp[0] = currTemp;
        systolicPressRawBuf[0] = currSys;
        diastolicPressRawBuf[0] = currDia;
        pulseRateRawBufTemp[0] = currPr;
        
        // Update the data pointers
        for(i = 0; i < sizeBuf * 2; i++) {
            if(i < sizeBuf) {
                *(dataStruct.temperatureRawPtr + i) = temperatureRawBufTemp[i];
                *(dataStruct.bloodPressRawPtr + i) = systolicPressRawBuf[i];
                *(dataStruct.pulseRateRawPtr + i) = pulseRateRawBufTemp[i];
            } else {
                *(dataStruct.bloodPressRawPtr + i) = diastolicPressRawBuf[i- sizeBuf];
            }
        }

    addFlags[1] = 1; // when the new data is being sent to compute
    removeFlags[0] = 1;
}

void computeDataFunc(void* x) {
    // Dereferencing void pointer to ComputeStruct
    ComputeTaskData* data = (ComputeTaskData*)x;
    ComputeTaskData dataStruct = *data;

    int rawTemp = *(dataStruct.temperatureRawPtr);
    int rawSys = *(dataStruct.bloodPressRawPtr);
    int rawDia = *(dataStruct.bloodPressRawPtr + 8);
    int rawPr = *(dataStruct.pulseRateRawPtr);

    // Computing and converting temperatureRaw to unsigned char* (Celcius)
    double correctedTemp = 5 + 0.75 * rawTemp;
    *dataStruct.temperatureCorrectedPtr = correctedTemp;
    
    // Computing and converting systolic pressure to unsigned char*
    double correctedSys = 9 + 2 * rawSys;
    *dataStruct.bloodPressCorrectedPtr = correctedSys;
    
    // Computing and converting diastolic pressure to unsigned char*
    double correctedDia =  6 + 1.5 * rawDia;
    *(dataStruct.bloodPressCorrectedPtr+8) = correctedDia;

    // Computing and converting pulse rate to unsigned char*
    double correctedPr =  8 + 3 * rawPr;
    *dataStruct.prCorrectedPtr = correctedPr;
    removeFlags[1] = 1;
}


// ENTER THIS ONLY WHEN ANNUNCIATION IS PRESSED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
void displayDataFunc(void* x) {
        DisplayTaskData* data = (DisplayTaskData*)x;
        DisplayTaskData dataStruct = *data;
        tft.setTextSize(1);
        tft.fillScreen(BLACK);
        tft.setCursor(0, 0);

    // Create buttons
    menuButtons[0].initButton(&tft, 120, 55, 240, 70, ILI9341_WHITE, ILI9341_BLUE, ILI9341_WHITE, "MENU", 2);
    menuButtons[0].drawButton();

    menuButtons[1].initButton(&tft, 120, 125, 240, 70, ILI9341_WHITE, ILI9341_BLUE, ILI9341_WHITE, "ANNUNCIATION", 2);
    menuButtons[1].drawButton();

    menuButtons[2].initButton(&tft, 120, 195, 240, 70, ILI9341_WHITE, ILI9341_BLUE, ILI9341_WHITE, "EXTRA MODE 1", 2);
    menuButtons[2].drawButton();

    menuButtons[3].initButton(&tft, 120, 265, 240, 70, ILI9341_WHITE, ILI9341_BLUE, ILI9341_WHITE, "EXTRA MODE 2", 2);
    menuButtons[3].drawButton();
}

void annunciateDataFunc(void* x) {
    // Dereferencing void pointer to WarningStruct
    WarningAlarmTaskData* data = (WarningAlarmTaskData*)x;
    WarningAlarmTaskData dataStruct = *data;
    
    // Temperature
        tempOutOfRange ? tft.setTextColor(ORANGE) : tft.setTextColor(GREEN);
        if (tempHigh) {
            tft.setTextColor(RED); // Add acknowledgement event??
        } 
        tft.println("Temperature: " + (String)*dataStruct.temperatureCorrectedPtr + " C");

        // Blood Pressure
        bpOutOfRange ? tft.setTextColor(ORANGE) : tft.setTextColor(GREEN);    
        if (bpHigh) { 
            tft.setTextColor(RED); // Add acknowledgement event??
        }
        tft.println("Systolic pressure: " + (String)*dataStruct.bloodPressCorrectedPtr + " mm Hg");

        // Pulse
        pulseOutOfRange ? tft.setTextColor(ORANGE) : tft.setTextColor(GREEN);        
        if (pulseLow) { 
            tft.setTextColor(RED); // Add acknowledgement event??
        }
        tft.println("Pulse rate: " + (String)*dataStruct.prCorrectedPtr + " BPM");

        // Battery
        battLow ? tft.setTextColor(RED) : tft.setTextColor(GREEN);
        tft.println("Battery: " + (String)*dataStruct.batteryStatePtr);

        // Acknowledge button
        AcknowledgeButton.initButton(&tft, 120, 265, 240, 70, ILI9341_WHITE, ILI9341_BLUE, ILI9341_WHITE, "DISMISS", 2);
        AcknowledgeButton.drawButton();

        bool staySensingPress = true; 
        while(staySensingPress) {
            TSPoint p = ts.getPoint();
            if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
                // scale from 0->1023 to tft.width
                p.x = map(p.x, TS_MINX, TS_MAXX, tft.width(), 0);
                p.y = (tft.height()-map(p.y, TS_MINY, TS_MAXY, tft.height(), 0));
            }
            if (AcknowledgeButton.contains(p.x, p.y)) {
                    AcknowledgeButton.press(true); // tell the button it is pressed

                    //Dismiss the alarm for five seconds

                    //If signal value remains out of range for five seconds, resume alarm annunciation

            } else {
                    AcknowledgeButton.press(false);  // tell the button it is NOT pressed
            }
        }

    // check temperature:
    double normalizedTemp = *dataStruct.temperatureCorrectedPtr;
    if (normalizedTemp < 36.1 || normalizedTemp > 37.8) {
        // TURN TEXT RED
        tempOutOfRange = 1;
        if (normalizedTemp > 40) {
            tempHigh = true;
            // Trigger alarm event

        } else {
            tempHigh = false;
        }
    } else {
        tempOutOfRange = 0;
        tempHigh = false;
    }

    // check systolic: Instructions unclear, googled for healthy and 
    // unhealthy systolic pressures. 
    // SysPressure > 130 == High blood pressure
    // SysPressure > 160 == CALL A DOCTOR
    int normalizedSystolic = *dataStruct.bloodPressCorrectedPtr;
    if (normalizedSystolic > 130) {
        // TURN TEXT RED
        bpOutOfRange = 1;
        if (normalizedSystolic > 160) {
            bpHigh = true;
            // Trigger alarm event

        } else {
            bpHigh = false;
        }
    } else {
        bpOutOfRange = 0;
        bpHigh = false;
    }
    
    /*  IF PRESSURE CRITICALLY HIGH, need interrupt flashing warnings -----------------------------------
        User can acknowledge it to DISABLE INTERRUPT
    */

    double normalizedDiastolic = *(dataStruct.bloodPressCorrectedPtr + 8);
    if (normalizedDiastolic > 90) {
        // TURN TEXT RED
        bpOutOfRange = 1;
        if (normalizedDiastolic > 120) {
            bpHigh = true;
            // Trigger alarm event

        } else {
            bpHigh = false;
        }
    } else {
        bpOutOfRange = 0;
        bpHigh = false;
    }
    
    // pulse rate 
    // HEALTHY PULSE => Between 60 and 100
    // IF PULSE < 30, WARNING

        /*  IF PULSE CRITICALLY LOW, need interrupt flashing warnings -----------------------------------
            User can acknowledge it to DISABLE INTERRUPT
        */

    int normalizedPulse = *dataStruct.prCorrectedPtr;
    if (normalizedPulse < 60 || normalizedPulse > 100) {
        pulseOutOfRange = 1;
        if (normalizedPulse < 30) {
            pulseLow = true;
            // Trigger alarm event

        } else {
            pulseLow = false;
        }
    } else {
        pulseOutOfRange = 0;
        pulseLow = false;
    }
    
    // battery
    // HEALTHY BATTERY LEVEL => Battery level above 20%
    // //printf("BATT LEVEL: %f\n", batteryLevel);

        /*  IF BATTERY CRITICALLY LOW, need interrupt flashing warnings -----------------------------------
            User can acknowledge it to DISABLE INTERRUPT
        */

    if (batteryState < 40) {
        battLow = true;
        // Trigger alarm event

    } else {
        battLow = false;
    }
}

void statusDataFunc(void* x) {
    if(unoCounter % 5 == 0) {
        // Dereferencing void pointer to WarningStruct
        StatusTaskData* data = (StatusTaskData*)x;
        StatusTaskData dataStruct = *data;
        
        *dataStruct.batteryStatePtr = 200 - (int)(unoCounter / 5);
    }
}

void KeypadDataFunc(void* x) {
    KeypadTaskData* data = (KeypadTaskData*)x;
    KeypadTaskData dataStruct = *data;

    bool staySensingPress = true;

    // add while loop
    while (staySensingPress) {
        TSPoint p = ts.getPoint();

        if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
            // scale from 0->1023 to tft.width
            p.x = map(p.x, TS_MINX, TS_MAXX, tft.width(), 0);
            p.y = (tft.height()-map(p.y, TS_MINY, TS_MAXY, tft.height(), 0));
        }
    
        // go thru all the buttons, checking if they were pressed
        for (uint8_t b = 0; b < 4; b++) {
            if (menuButtons[b].contains(p.x, p.y)) {
                Serial.print("Pressing: "); Serial.println(b);
                menuButtons[b].press(true);  // tell the button it is pressed
                if (b == 0) { // Menu
                    staySensingPress = false;
                    tft.fillScreen(BLACK);
                    menuView();
                } else if (b == 1) { // Annunciate
                    staySensingPress = false;
                    tft.fillScreen(BLACK);
                    // Setting flag in schedule 

                } else { // Nothing for now

                }
            } else {
                menuButtons[b].press(false);  // tell the button it is NOT pressed
            }
        }
    }
}

void menuView() {
    measureSelectButtons[0].initButton(&tft, 120, 60, 240, 100, ILI9341_WHITE, ILI9341_BLUE, ILI9341_WHITE, "Blood Pressure", 2);
    measureSelectButtons[0].drawButton();

    measureSelectButtons[1].initButton(&tft, 120, 160, 240, 100, ILI9341_WHITE, ILI9341_BLUE, ILI9341_WHITE, "Temperature", 2);
    measureSelectButtons[1].drawButton();

    measureSelectButtons[2].initButton(&tft, 120, 260, 240, 100, ILI9341_WHITE, ILI9341_BLUE, ILI9341_WHITE, "Pulse Rate", 2);
    measureSelectButtons[2].drawButton();
    
    bool staySensingPress = true;
    while (staySensingPress) {

        TSPoint p = ts.getPoint();

        if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
            // scale from 0->1023 to tft.width
            p.x = map(p.x, TS_MINX, TS_MAXX, tft.width(), 0);
            p.y = (tft.height()-map(p.y, TS_MINY, TS_MAXY, tft.height(), 0));
        }

        for (uint8_t b = 0; b < 3; b++) {
            if (measureSelectButtons[b].contains(p.x, p.y)) {
                Serial.print("Pressing: "); Serial.println(b);
                measureSelectButtons[b].press(true);  // tell the button it is pressed
                if (b == 0) { // Blood Pressure
                    staySensingPress = false;
                    tft.fillScreen(BLACK);
                    // Measure Blood Pressure
                    // AND THEN go into annunciation, blood pressure should now be updated

                    // SEND REQUEST, SET FLAG ADDTASK FOR MEASURE

                } else if (b == 1) { // Temperature
                    staySensingPress = false;
                    tft.fillScreen(BLACK);
                    // Measure Temperature
                    // AND THEN go into annunciation, temperature should now be updated

                    // SEND REQUEST, SET FLAG ADDTASK FOR MEASURE

                } else { // Measure pulse rate, and THEN go into annunciation, PR should now be updated 
                    staySensingPress = false;
                    tft.fillScreen(BLACK);

                    // SEND REQUEST, SET FLAG ADDTASK FOR MEASURE

                }
            } else {
                measureSelectButtons[b].press(false);  // tell the button it is NOT pressed
            }
        }
    }
}

// Delay for 100ms and update counter/time on peripheral system
void updateCounter(void) {
    Serial1.println('U');
}

/* INITIALIZATION */
void setup(void) {
    // Setup communication
    Serial1.begin(9600);

    // Initialise Timer3
    Timer3.initialize(1000000);
    Timer3.attachInterrupt(updateCounter);
    
    // Configure display
    Serial.begin(9600);
    Serial.println(F("TFT LCD test"));


    #ifdef USE_Elegoo_SHIELD_PINOUT
        Serial.println(F("Using Elegoo 2.4\" TFT Arduino Shield Pinout"));
    #else
        Serial.println(F("Using Elegoo 2.4\" TFT Breakout Board Pinout"));
    #endif

    Serial.print("TFT size is "); Serial.print(tft.width()); Serial.print("x"); Serial.println(tft.height());

    tft.reset();

    uint16_t identifier = tft.readID();
    if(identifier == 0x9325) {
        Serial.println(F("Found ILI9325 LCD driver"));
    } else if(identifier == 0x9328) {
        Serial.println(F("Found ILI9328 LCD driver"));
    } else if(identifier == 0x4535) {
        Serial.println(F("Found LGDP4535 LCD driver"));
    }else if(identifier == 0x7575) {
        Serial.println(F("Found HX8347G LCD driver"));
    } else if(identifier == 0x9341) {
        Serial.println(F("Found ILI9341 LCD driver"));
    } else if(identifier == 0x8357) {
        Serial.println(F("Found HX8357D LCD driver"));
    } else if(identifier==0x0101)
    {     
        identifier=0x9341;
        Serial.println(F("Found 0x9341 LCD driver"));
    }
    else if(identifier==0x1111)
    {     
        identifier=0x9328;
        Serial.println(F("Found 0x9328 LCD driver"));
    }
    else {
        Serial.print(F("Unknown LCD driver chip: "));
        Serial.println(identifier, HEX);
        Serial.println(F("If using the Elegoo 2.8\" TFT Arduino shield, the line:"));
        Serial.println(F("  #define USE_Elegoo_SHIELD_PINOUT"));
        Serial.println(F("should appear in the library header (Elegoo_TFT.h)."));
        Serial.println(F("If using the breakout board, it should NOT be #defined!"));
        Serial.println(F("Also if using the breakout, double-check that all wiring"));
        Serial.println(F("matches the tutorial."));
        identifier=0x9328;
    
    }
    tft.begin(identifier);

    // Set measurements to initial values
    // char* and char values are already set as global variables
    bpHigh = false;
    tempHigh = false;
    pulseLow = false;
    battLow = false;

    // Point data in data structs to correct information
    // Measure
    MeasureTaskData dataForMeasureTMP;
    dataForMeasureTMP.temperatureRawPtr = temperatureRawBuf; 
    dataForMeasureTMP.bloodPressRawPtr = bloodPressRawBuf;
    dataForMeasureTMP.pulseRateRawPtr = pulseRateRawBuf;
    dataForMeasureTMP.measurementSelectionPtr = &measurementSelection;
    dataForMeasure = dataForMeasureTMP;

    // Compute
    ComputeTaskData dataForComputeTMP;
    dataForComputeTMP.temperatureRawPtr = temperatureRawBuf;
    dataForComputeTMP.bloodPressRawPtr = bloodPressRawBuf;
    dataForComputeTMP.pulseRateRawPtr = pulseRateRawBuf;
    dataForComputeTMP.temperatureCorrectedPtr = tempCorrectedBuf; // Already a pointer
    dataForComputeTMP.bloodPressCorrectedPtr = bloodPressCorrectedBuf;
    dataForComputeTMP.prCorrectedPtr = pulseRateCorrectedBuf;
    dataForComputeTMP.measurementSelectionPtr = &measurementSelection;
    dataForCompute = dataForComputeTMP;

    // Display
    DisplayTaskData dataForDisplayTMP;
    dataForDisplayTMP.temperatureCorrectedPtr = tempCorrectedBuf; // Already a pointer
    dataForDisplayTMP.bloodPressCorrectedPtr = bloodPressCorrectedBuf;
    dataForDisplayTMP.prCorrectedPtr = pulseRateCorrectedBuf;
    dataForDisplayTMP.batteryStatePtr = &batteryState;
    dataForDisplay = dataForDisplayTMP;

    // WarningAlarm
    WarningAlarmTaskData dataForWarningAlarmTMP;
    dataForWarningAlarmTMP.temperatureCorrectedPtr = tempCorrectedBuf;
    dataForWarningAlarmTMP.bloodPressCorrectedPtr = bloodPressCorrectedBuf;
    dataForWarningAlarmTMP.prCorrectedPtr = pulseRateCorrectedBuf;
    dataForWarningAlarmTMP.batteryStatePtr = &batteryState;
    dataForWarningAlarm = dataForWarningAlarmTMP;

    // Status
    StatusTaskData dataForStatusTMP;
    dataForStatusTMP.batteryStatePtr = &batteryState;
    dataForStatus = dataForStatusTMP;

    // TFT Keypad
    KeypadTaskData dataForKeypadTMP;
    dataForKeypadTMP.measurementSelectionPtr = &measurementSelection;
    dataForKeypadTMP.alarmAcknowledgePtr = &alarmAcknowledge;
    dataForKeypad = dataForKeypadTMP;

    // Communications
    CommsTaskData dataForCommsTMP;
    dataForCommsTMP.measurementSelectionPtr = &measurementSelection;
    dataForCommsTMP.temperatureCorrectedPtr = tempCorrectedBuf;
    dataForCommsTMP.bloodPressCorrectedPtr = bloodPressCorrectedBuf;
    dataForCommsTMP.prCorrectedPtr = pulseRateCorrectedBuf;
    dataForComms = dataForCommsTMP;
    
    // Assign values in TCB's
    // Measure
    TCB MeasureTaskTMP; // Getting an error if I try to use MeasureTask directly
    MeasureTaskTMP.taskFuncPtr = &measureDataFunc;
    MeasureTaskTMP.taskDataPtr = &dataForMeasure;
    MeasureTaskTMP.next = NULL;
    MeasureTaskTMP.prev = NULL;
    MeasureTask = MeasureTaskTMP;

    // Compute
    TCB ComputeTaskTMP;
    ComputeTaskTMP.taskFuncPtr = &computeDataFunc;
    ComputeTaskTMP.taskDataPtr = &dataForCompute;
    ComputeTaskTMP.next = NULL;
    ComputeTaskTMP.prev = NULL;
    ComputeTask = ComputeTaskTMP;
    
    // Display
    TCB DisplayTaskTMP;
    DisplayTaskTMP.taskFuncPtr = &displayDataFunc;
    DisplayTaskTMP.taskDataPtr = &dataForDisplay;
    DisplayTaskTMP.next = NULL;
    DisplayTaskTMP.prev = NULL;
    DisplayTask = DisplayTaskTMP;

    // Warning/Alarm
    TCB AnnunciateTaskTMP;
    AnnunciateTaskTMP.taskFuncPtr = &annunciateDataFunc;
    AnnunciateTaskTMP.taskDataPtr = &dataForWarningAlarm;
    AnnunciateTaskTMP.next = NULL;
    AnnunciateTaskTMP.prev = NULL;
    AnnunciateTask = AnnunciateTaskTMP;

    // Status
    TCB StatusTaskTMP;
    StatusTaskTMP.taskFuncPtr = &statusDataFunc;
    StatusTaskTMP.taskDataPtr = &dataForStatus;
    StatusTaskTMP.next = NULL;
    StatusTaskTMP.prev = NULL;
    StatusTask = StatusTaskTMP;

    // Keypad
    TCB KeypadTaskTMP;
    KeypadTaskTMP.taskFuncPtr = &KeypadDataFunc;
    KeypadTaskTMP.taskDataPtr = &dataForKeypad;
    KeypadTaskTMP.next = NULL;
    KeypadTaskTMP.prev = NULL;
    KeypadTask = StatusTaskTMP;

    // NULL TCB
    TCB NullTaskTMP;
    NullTaskTMP.taskFuncPtr = NULL;
    NullTaskTMP.taskDataPtr = NULL;
    NullTaskTMP.next = NULL;
    NullTaskTMP.prev = NULL;
    NullTask = NullTaskTMP;

}

// Modify this to traverse linkedList instead
void loop(void) {
    
    scheduler();
    // LinkedList traversal
    if(currPointer != NULL) {
        TCB currentTask = *currPointer;
        currPointer->taskFuncPtr(currPointer->taskDataPtr);
    }

    if (currPointer->next == NULL) {
        // currPointer = linkedListHead;
    } else {
        currPointer = currPointer->next;
    }
}

void scheduler() {
    // PROPER LINKEDLIST SETUP --------- DO NOT DELETE
    appendAtEnd(&MeasureTask);
    appendAtEnd(&ComputeTask);
    appendAtEnd(&DisplayTask);
    appendAtEnd(&AnnunciateTask);
    appendAtEnd(&StatusTask);
    appendAtEnd(&KeyPadTask);

    // Serial.println("done making linkedlist");

    currPointer = linkedListHead;

    while(currPointer != NULL) {
        for(int i = 0; i < 5; i++) { // checks add task flags
            if(addFlags[i]) {
                runTask(i, true); // insert task
                addFlags[i] = 0;
            }
        }
        currPointer = currPointer->next; // moves current pointer to the next task
        for(int i = 0; i < 5; i++) { // checks remove task flags
            if(removeFlags[i]) {
                runTask(i, false); // remove task
                removeFlags[i] = 0;
            }
        }
    }
}

void runTask(int taskID, bool insertTask) {
    switch(taskID) {
        case 0: 
            if(insertTask) {
                appendAtEnd(&MeasureTask);
            } else {
                deleteNode(&MeasureTask);
            }
        break;
        case 1:
            if(insertTask) {
                appendAtEnd(&ComputeTask);
            } else {
                deleteNode(&ComputeTask);
            }
        break;
        case 2:
            if(insertTask) {
                appendAtEnd(&DisplayTask);
            } else {
                deleteNode(&DisplayTask);
            }
        break;
        case 3:
            if(insertTask) {
                appendAtEnd(&AnnunciateTask);
            } else {
                deleteNode(&AnnunciateTask);
            }
        break;
        case 4:
            if(insertTask) {
                appendAtEnd(&StatusTask);
            } else {
                deleteNode(&StatusTask);
            }
        break;
        case 5:
            if(insertTask) {
                appendAtEnd(&KeyPadTask);
            } else {
                deleteNode(&KeyPadTask);
            }
        break;
    }
}



// ------------------------------------Double Linked List Fns-------------------------------

// // Add to front
// void push(TCB newTCB) {
//     TCB* newNode = &newTCB;
//     /* 1. Make next of new node as head and previous as NULL */
//     newNode->next = linkedListHead;
//     newNode->prev = NULL;
 
//     /* 2. change prev of head node to new node */
//     if(linkedListHead !=  NULL) {
//       linkedListHead->prev = newNode ;
//     }
 
//     /* 3. move the head to point to the new node */
//     linkedListHead = newNode;
// }

// WORKS
// Add to back
void appendAtEnd(TCB* newNode) {
    // Set up second pointer to be moved to the end of the list, will be used in 3
    //TCB* newNode = newTCB;
    TCB* lastRef = linkedListHead;  
  
    /* 1. This new node is going to be the last node, so
          make next of it as NULL*/
    newNode->next = NULL;
 
    /* 2. If the Linked List is empty, then make the new
          node as head */
    if (linkedListHead == NULL) {
        newNode->prev = NULL;
        linkedListHead = newNode;
        return;
    }

    /* 3. Else traverse till the last node */
    while (lastRef->next != NULL) {
        lastRef = lastRef->next;
    }     
    
    /* 4. Change the next of last node */
    lastRef->next = newNode;

    /* 5. Make last node as previous of new node */
    newNode->prev = lastRef;

    return;
}

// Add within list
void insertAfterNode(TCB* prevNodeRef, TCB* newNode) {
    /* 1. check if the given prev_node is NULL */
    if (prevNodeRef == NULL) {
        Serial.println("the given previous node cannot be NULL");
        return;
    }
  
    /* 2. Make next of new node as next of prev_node */
    newNode->next = prevNodeRef->next;
 
    /* 3. Make the next of prev_node as new_node */
    prevNodeRef->next = newNode;
 
    /* 4. Make prev_node as previous of new_node */
    newNode->prev = prevNodeRef;
 
    /* 5. Change previous of new_node's next node */
    if (newNode->next != NULL) {
      newNode->next->prev = newNode;
    }
}

// Delete
/* Function to delete a node in a Doubly Linked List.
   head_ref --> pointer to head node pointer.
   del  -->  pointer to node to be deleted. */
void deleteNode(TCB* del) {
  /* base case */
  if (linkedListHead == NULL || del == NULL)
    return;
 
  /* If node to be deleted is head node */
  if (linkedListHead == del) {
    linkedListHead = del->next;
  }
 
  /* Change next only if node to be deleted is NOT the last node */
  if (del->next != NULL) {
    del->next->prev = del->prev;
  }
 
  /* Change prev only if node to be deleted is NOT the first node */
  if (del->prev != NULL) {
    del->prev->next = del->next;  
  }   
 
  /* Finally, free the memory occupied by del*/
  free(del);
  return;
}     