// Assignment 2
// Code for ATMega
// Eddy, Kyuri, Amol

#include <stdio.h>
#include <stdbool.h>
#include <Elegoo_GFX.h>    // Core graphics library
#include <Elegoo_TFTLCD.h> // Hardware-specific library
#include "TimerThree.h"

int unoCounter = 0;

// Define number of tasks
#define NUM_TASKS 6 // Need to ensure this is accurate else program breaks

/* INITIALIZATION - SETUP TFT DISPLAY */
#define LCD_CS A3 // Chip Select goes to Analog 3
#define LCD_CD A2 // Command/Data goes to Analog 2
#define LCD_WR A1 // LCD Write goes to Analog 1
#define LCD_RD A0 // LCD Read goes to Analog 0
#define LCD_RESET A4 // Can alternately just connect to Arduino's reset pin

// Assign human-readable names to some common 16-bit color values:
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

Elegoo_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);


/* INITIALIZATION - GLOBAL VARIABLES */
// Measurements
unsigned int temperatureRawBuf[8] = {75};
unsigned int bloodPressRawBuf[16] = {80};
unsigned int pulseRateRawBuf[8] = {0};

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
    unsigned int* temperatureRawPtr;
    unsigned int* bloodPressRawPtr;
    unsigned int* pulseRateRawPtr;
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

// Arrays containing changes in temp and pulse, will be switched
// when upper/lower limits are hit. 0th elements are called at Even
// function calls.
int tempChange[2] = {-2, 1}; // initially falling towards 15
int pulseChange[2] = {1, -3}; // initially falling towards 15

// Initially both temperature and pulse are very high and must keep 
// falling until they cross 50 and 40 respectively before their 
// standard boundaries can work.
bool tempCrossedFifty = false;
bool pulseCrossedForty = false;

// Systolic completion
bool systolicComplete = false;
// Diastolic completion
bool diastolicComplete = false;


/* INITIALIZATION - MAKE INSTANCES OF TCB */
TCB MeasureTask;
TCB ComputeTask;
TCB DisplayTask;
TCB AnnunciateTask;
TCB StatusTask;
TCB NullTask;

// Data
MeasureTaskData dataForMeasure;
ComputeTaskData dataForCompute;
DisplayTaskData dataForDisplay;
WarningAlarmTaskData dataForWarningAlarm;
StatusTaskData dataForStatus;
KeypadTaskData dataForKeypad;
CommsTaskData dataForComms;


/* INITIALIZATION - FUNCTION DEFINITIONS */
void measureDataFunc(void* data) {
    MeasureTaskData* dataToMeasure = (MeasureTaskData*)data;
    MeasureTaskData dataStruct = *dataToMeasure;

    unsigned int temperatureRaw = *dataStruct.temperatureRawPtr;
    unsigned int systolicPressRaw = *dataStruct.systolicPressRawPtr;
    unsigned int diastolicPressRaw = *dataStruct.diastolicPressRawPtr;
    unsigned int pulseRateRaw = *dataStruct.pulseRateRawPtr;

    if (temperatureRaw < 50) {
        tempCrossedFifty = true;   
    }
    
    if (pulseRateRaw < 40) {
        pulseCrossedForty = true;
    }
    
    /*
    if (systolicComplete && diastolicComplete) {
        systolicPressRaw = 80;
        diastolicPressRaw = 80;
        systolicComplete = false;
        diastolicComplete = false;
    }*/
    
    // Perform measurement every 5 counts
    if (unoCounter % 5 == 0) {
        
        // Check if its an Even number of function call
        bool even = (unoCounter % 2 == 0);
        
        // Temperature
        if ((temperatureRaw > 50 || temperatureRaw < 15) && tempCrossedFifty) {
            int temp = tempChange[0];
            tempChange[0] = -1 * tempChange[1];
            tempChange[1] = -1 * temp;
        }
        if (even) {
            temperatureRaw += tempChange[0];
        } else {
            temperatureRaw += tempChange[1];
        }
        
        // Systolic: Resets to 80 at the end of sys-dias cycle
        if (systolicPressRaw <= 100) {
            if (even) {
                systolicPressRaw += 3;
            } else {
                systolicPressRaw--;
            }
        } else {
            systolicComplete = true;
            if(diastolicComplete) {
                systolicPressRaw = 20;
                diastolicPressRaw = 80;
                diastolicComplete = false;
                systolicComplete = false;
            }
        }
        
        // Diastolic: Resets to 80 at the end of sys-dias cycle
        if (diastolicPressRaw >= 40) {
            if (even) {
                diastolicPressRaw -= 2;
            } else {
                diastolicPressRaw++;
            }
        } else {
            diastolicComplete = true;
            
        }        

        // Pulse
        if ((pulseRateRaw > 40 || pulseRateRaw < 15) && pulseCrossedForty) {
            int temp = pulseChange[0];
            pulseChange[0] = -1 * pulseChange[1];
            pulseChange[1] = -1 * temp; 
        }
        if (even) {
            pulseRateRaw += pulseChange[0];
        } else {
            pulseRateRaw += pulseChange[1];
        }

        *dataStruct.temperatureRawPtr = temperatureRaw;
        *dataStruct.systolicPressRawPtr = systolicPressRaw;
        *dataStruct.diastolicPressRawPtr = diastolicPressRaw;
        *dataStruct.pulseRateRawPtr = pulseRateRaw;
    }
}

void computeDataFunc(void* x) {
    if (unoCounter % 5 == 0) {
        // Dereferencing void pointer to ComputeStruct
        ComputeTaskData* data = (ComputeTaskData*)x;
        ComputeTaskData dataStruct = *data;
        
        // Computing and converting temperatureRaw to unsigned char* (Celcius)
        double correctedTemp = 5 + 0.75 * *dataStruct.temperatureRawPtr;
        *dataStruct.temperatureCorrectedPtr = correctedTemp;
        
        // Computing and converting systolic pressure to unsigned char*
        double correctedSys = 9 + 2 * *dataStruct.systolicPressRawPtr;
        *dataStruct.sysCorrectedPtr = correctedSys;
        
        // Computing and converting diastolic pressure to unsigned char*
        double correctedDia =  6 + 1.5 * *dataStruct.diastolicPressRawPtr;
        *dataStruct.diasCorrectedPtr = correctedDia;

        // Computing and converting pulse rate to unsigned char*
        double correctedPr =  8 + 3 * *dataStruct.pulseRateRawPtr;
        *dataStruct.prCorrectedPtr = correctedPr;
    }
}

void displayDataFunc(void* x) {
    if (unoCounter % 5 == 0) { // Every 5 seconds
        DisplayTaskData* data = (DisplayTaskData*)x;
        DisplayTaskData dataStruct = *data;
        tft.setTextSize(1);
        tft.fillScreen(BLACK);
        tft.setCursor(0, 0);
        tempOutOfRange ? tft.setTextColor(RED) : tft.setTextColor(GREEN);
        tft.println("Temperature: " + (String)*dataStruct.temperatureCorrectedPtr + " C");
        bpOutOfRange ? tft.setTextColor(RED) : tft.setTextColor(GREEN);        
        tft.println("Systolic pressure: " + (String)*dataStruct.sysCorrectedPtr + " mm Hg");
        tft.println("Diastolic pressure: " + (String)*dataStruct.diasCorrectedPtr + " mm Hg");
        pulseOutOfRange ? tft.setTextColor(RED) : tft.setTextColor(GREEN);        
        tft.println("Pulse rate: " + (String)*dataStruct.prCorrectedPtr + " BPM");
        battLow ? tft.setTextColor(RED) : tft.setTextColor(GREEN);
        tft.println("Battery: " + (String)*dataStruct.batteryStatePtr);
    }
}

void annunciateDataFunc(void* x) {
    // Dereferencing void pointer to WarningStruct
    WarningAlarmTaskData* data = (WarningAlarmTaskData*)x;
    WarningAlarmTaskData dataStruct = *data;
    
    // check temperature:
    double normalizedTemp = 5 + 0.75 * *dataStruct.temperatureRawPtr;
    if (normalizedTemp < 36.1 || normalizedTemp > 37.8) {
        // TURN TEXT RED
        tempOutOfRange = 1;
        if (normalizedTemp > 40) {
            tempHigh = true;
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
    int normalizedSystolic = 9 + 2 * *dataStruct.systolicPressRawPtr;
    if (normalizedSystolic > 130) {
        // TURN TEXT RED
        bpOutOfRange = 1;
        if (normalizedSystolic > 160) {
            bpHigh = true;
        } else {
            bpHigh = false;
        }
    } else {
        bpOutOfRange = 0;
        bpHigh = false;
    }
    
    // check diastolic
    // DiasPressure > 90 == High blood pressure
    // DiasPressure > 120 == CALL A DOCTOR

            /*  IF PRESSURE CRITICALLY HIGH, need interrupt flashing warnings -----------------------------------
                User can acknowledge it to DISABLE INTERRUPT
            */

    double normalizedDiastolic = 6 + 1.5 * *dataStruct.diastolicPressRawPtr;
    if (normalizedDiastolic > 90) {
        // TURN TEXT RED
        bpOutOfRange = 1;
        if (normalizedDiastolic > 120) {
            bpHigh = true;
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

    int normalizedPulse = 8 + 3 * *dataStruct.pulseRateRawPtr;
    if (normalizedPulse < 60 || normalizedPulse > 100) {
        pulseOutOfRange = 1;
        if (normalizedPulse < 30) {
            pulseLow = true;
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
        // printf("LOW BATT: %f\n", batteryLevel);
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

// Delay for 100ms and update counter/time on peripheral system
void updateCounter(void) {
    unoCounter++;
}

/* INITIALIZATION */
void setup(void) {
    // Setup communication
    Serial1.begin(9600);

    // Initialise Timer3
    Timer3.initialize(1000000);
    Timer3.attachInterrupt(updateCounter());
    
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
        dataForWarningAlarmTMP.temperatureRawPtr = temperatureRawBuf;
        dataForWarningAlarmTMP.bloodPressRawPtr = bloodPressRawBuf;
        dataForWarningAlarmTMP.pulseRateRawPtr = pulseRateRawBuf;
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

    // NULL TCB
    TCB NullTaskTMP;
    NullTaskTMP.taskFuncPtr = NULL;
    NullTaskTMP.taskDataPtr = NULL;
    NullTaskTMP.next = NULL;
    NullTaskTMP.prev = NULL;
    NullTask = NullTaskTMP;


    // Head is the start of the empty doubly linkedlist
    // Adding each task to the Double Linked List
    append(&linkedListHead, &MeasureTask);
    append(&linkedListHead, &ComputeTask);
    append(&linkedListHead, &DisplayTask);
    append(&linkedListHead, &AnnunciateTask);
    append(&linkedListHead, &StatusTask);

    currPointer = linkedListHead;
}

// Modify this to traverse linkedList instead
void loop(void) {
    unsigned long start = micros(); 
    
    /* SCHEDULE 
    TCB tasksArray[NUM_TASKS];
    tasksArray[0] = MeasureTask;
    tasksArray[1] = ComputeTask;
    tasksArray[2] = DisplayTask;
    tasksArray[3] = AnnunciateTask;
    tasksArray[4] = StatusTask;
    tasksArray[5] = NullTask;

    for(int i = 0; i < NUM_TASKS; i++) { // QUEUE
        tasksArray[i].taskFuncPtr(tasksArray[i].taskDataPtr);
    }*/

    // LinkedList traversal
    *currPointer.taskFuncPtr(*currPointer.taskDataPtr);

    if (currPointer->next == NULL) {
        currPointer = linkedListHead;
    } else {
        currPointer = currPointer->next;
    }
    //updateCounter();
}



// ------------------------------------Double Linked List Fns-------------------------------

// Add to front
void push(TCB** headRef, TCB* newNode) {
  
    /* 1. Make next of new node as head and previous as NULL */
    newNode->next = (*headRef);
    newNode->prev = NULL;
 
    /* 2. change prev of head node to new node */
    if((*headRef) !=  NULL) {
      (*headRef)->prev = newNode ;
    }
 
    /* 3. move the head to point to the new node */
    (*headRef) = newNode;
}

// Add to back
void append(TCB** headRef, TCB* newNode) {
    // Set up second pointer to be moved to the end of the list, will be used in 3
    TCB* lastRef = *headRef;  
  
    /* 1. This new node is going to be the last node, so
          make next of it as NULL*/
    newNode->next = NULL;
 
    /* 2. If the Linked List is empty, then make the new
          node as head */
    if (*headRef == NULL) {
        newNode->prev = NULL;
        *headRef = newNode;
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
void insertAfter(TCB* prevNodeRef, TCB* newNode) {
    /*1. check if the given prev_node is NULL */
    if (prevNodeRef == NULL) {
        printf("the given previous node cannot be NULL");
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
void deleteNode(TCB** headRef, TCB* del) {
  /* base case */
  if (*headRef == NULL || del == NULL)
    return;
 
  /* If node to be deleted is head node */
  if (*headRef == del) {
    *headRef = del->next;
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