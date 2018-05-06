#include <MySensor.h>
#include <SPI.h>
#include <Bounce2.h>

// ������������� ����
#define NODE_ID 10
// ����� ���������� �������� � ������� ����
#define SENSORS_COUNT        4
// ��������� ������� �� �������� � ������� ���� � ������� � ������
#define CHILD_ID_BATH_COLD   0
#define CHILD_ID_BATH_HOT    1
#define CHILD_ID_TOILET_COLD 2
#define CHILD_ID_TOILET_HOT  3
// ������ ��� �����������, ��������������� ��������� ��������
#define PIN_BATH_COLD   3
#define PIN_BATH_HOT    4
#define PIN_TOILET_COLD 5
#define PIN_TOILET_HOT  6

uint8_t g_intIDs[SENSORS_COUNT] = {CHILD_ID_BATH_COLD, CHILD_ID_BATH_HOT, CHILD_ID_TOILET_COLD, CHILD_ID_TOILET_HOT};
uint8_t g_intPins[SENSORS_COUNT] = {PIN_BATH_COLD, PIN_BATH_HOT, PIN_TOILET_COLD, PIN_TOILET_HOT};
volatile unsigned long g_intPulseCounter[SENSORS_COUNT] = {0, 0, 0, 0};
unsigned long g_intLastQueryMS[SENSORS_COUNT] = {0, 0, 0, 0};
#define QUERY_FREQUENCY 10000    

MySensor g_objSensorNode;
Bounce g_objDebouncer[SENSORS_COUNT] = {Bounce(), Bounce(), Bounce(), Bounce()}; 
// MyMessage  msg(PIN, V_VOLUME);	
int oldValue=-1;

// Change to V_LIGHT if you use S_LIGHT in presentation below
MyMessage g_objMessage[SENSORS_COUNT] = {
    MyMessage(CHILD_ID_BATH_COLD, V_VOLUME),
    MyMessage(CHILD_ID_BATH_HOT, V_VOLUME),
    MyMessage(CHILD_ID_TOILET_COLD, V_VOLUME),
    MyMessage(CHILD_ID_TOILET_HOT, V_VOLUME)
};
//MyMessage lastCounterMsg(CHILD_ID_BATH_COLD, V_VAR1);

boolean g_boolInitialDataReceived[SENSORS_COUNT] = {false, false, false, false};
boolean g_boolControllerJustConnected[SENSORS_COUNT] = {false, false, false, false};

// ������� ���� ��������� �� +5�, ������� ������� � ������� �������� ������ �� ����, � ����� ������������ � +5�
// ???????|___________________|??????????????
//                            ^
// ����� ���������� ������ �� ������� ��������� ������ ��������, �.�. ��� �������� � 0 �� 1
// 
boolean g_boolInitialState = true;

void incomingMessage(const MyMessage &message);

void setup() {  
    g_objSensorNode.begin(incomingMessage, NODE_ID);
  
    for (uint8_t i = 0 ; i < SENSORS_COUNT ; i++) {
        // Setup the button
        pinMode(g_intPins[i], INPUT);
        // Activate internal pull-up
        digitalWrite(g_intPins[i], HIGH);
        // After setting up the button, setup debouncer
        g_objDebouncer[i].attach(g_intPins[i]);
        g_objDebouncer[i].interval(5);	  
        g_objSensorNode.present(g_intIDs[i], S_WATER);  
        // ����������� � ����������� ��������� ��������� ��������
        // (�� �� ����, ��� �� ��� �������)
        g_objSensorNode.request(g_intIDs[i], V_VAR1);
        g_intLastQueryMS[i] = millis();
    }
}

boolean g_boolPreviousState[SENSORS_COUNT] = {false, false, false, false};
/*
������ ���������:
g_boolInitialState      0 1 0 1 0 1 0 1
g_boolPreviousState     0 0 1 1 0 0 1 1
l_boolCurrentState      0 0 0 0 1 1 1 1
���������� ������?              1 
*/
void loop() {
    g_objSensorNode.process();

    for (uint8_t i = 0 ; i < SENSORS_COUNT ; i++) {
        // ���� ���������� ��� ��� � �� ������� (��������, ��-�� ����, ��� � ������ ������� �������,
        // �.�., ��� ������ �������, �� �� ��� �������), �� �������� �������
        if (!g_boolInitialDataReceived[i]) {
            unsigned long l_intDeltaMS = millis() - g_intLastQueryMS[i];
            if (l_intDeltaMS > QUERY_FREQUENCY) {
#ifdef DEBUG
                Serial.print("No initial data received yet for sensor id = ");
                Serial.print(i);
                Serial.print(" since last attempt ");
                Serial.print(l_intDeltaMS);
                Serial.println(" ms ago. Let's try again");
#endif
                g_objSensorNode.request(g_intIDs[i], V_VAR1);
                g_intLastQueryMS[i] = millis();
            }
        }
        g_objDebouncer[i].update();
	    // ������� ������� ��������� ����
	    boolean l_boolCurrentState = g_objDebouncer[i].read();
        // ���� ������ ��� ���������� - ���������� ��������� ����������, ��� �������������� 
        // ������� ��� ����� ��, ��� � ������� ��������� ����
	    if (g_boolInitialState) 
            g_boolPreviousState[i] = l_boolCurrentState;

		boolean l_boolNewTickArrived = false;
		do {
			// ���� ��������� ���� �� ���������� - ������ �� ������
			if (l_boolCurrentState == g_boolPreviousState[i]) break;
			g_boolPreviousState[i] = l_boolCurrentState;
			// ���� ��������� ���� ����������, �� ������� = LOW, �� ��� ������� � HIGH �� LOW,
			// ������� ��� �� ����������
			if (!l_boolCurrentState) break;
			l_boolNewTickArrived = true;
		} while (false);
		
		if (l_boolNewTickArrived) {
		    g_intPulseCounter[i]++;
#ifdef DEBUG
			Serial.print("g_intPulseCounter[");
			Serial.print(i);
			Serial.print("]=");
			Serial.println(g_intPulseCounter[i]);
#endif
		}
	    if (!g_boolInitialDataReceived[i]) continue;
		if (l_boolNewTickArrived || g_boolControllerJustConnected[i])
			g_objSensorNode.send(g_objMessage[i].set(g_intPulseCounter[i]));
		g_boolControllerJustConnected[i] = false;
    }
    g_boolInitialState = false;
} 

void incomingMessage(const MyMessage &theMessage) {
    Serial.println("incomingMessage");
	if (V_VAR1 == theMessage.type) {
        if (theMessage.sensor >= SENSORS_COUNT) return;
        // ���� ���� �������� ��������� �� OpenHAB � ����� MQTTSUBSCRIBE
        // � ���� ������ payload �������������� ����� ������
        if (0 == strlen(theMessage.getString())) {
			g_boolControllerJustConnected[theMessage.sensor] = true;
			Serial.print("Zero-length message arrived (subscription) for sensor ");
			Serial.println(theMessage.sensor);
			return;
		}
        if (g_boolInitialDataReceived[theMessage.sensor]) return;
		g_intPulseCounter[theMessage.sensor] += theMessage.getULong();
		Serial.print("Received last pulse count (");
	    Serial.print(theMessage.getULong());
		Serial.print(") from gw:");
	    Serial.print("g_intPulseCounter[");
	    Serial.print(theMessage.sensor);
	    Serial.print("]=");
	    Serial.println(g_intPulseCounter[theMessage.sensor]);
		g_boolInitialDataReceived[theMessage.sensor] = true;
	}
}
