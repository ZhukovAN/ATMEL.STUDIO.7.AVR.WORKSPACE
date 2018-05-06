/*
 * Blink.cpp
 *
 * Created: 12.06.2016 19:00:37
 * Author : Alexey N. Zhukov
 */ 

#include <arduino.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <ModbusMaster.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte g_objLocalMAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE };
IPAddress g_objLocalIP(192, 168, 0, 71);
IPAddress g_objDNS(192, 168, 0, 2);
IPAddress g_objGateway(192, 168, 0, 65);
IPAddress g_objMask(255, 255, 255, 192);
EthernetClient g_objClient;
PubSubClient g_objMQTTClient(g_objClient);
unsigned long g_intLastReconnectAttempt = 0;

IPAddress g_objMQTTServerIP(192, 168, 0, 41);

// ������ ��� �����������, ��������������� ��������� ��������
#define PIN_BATH_COLD   35
#define PIN_BATH_HOT    37
#define PIN_TOILET_COLD 31
#define PIN_TOILET_HOT  33

// MyMQTT/2303-34/TOILET/WATER/COLD/TICKCOUNT
#define MAX_MSG_SIZE 64 
const char  FORMAT[] PROGMEM         = "MyMQTT/2303-34/%s/WATER/%s/%s";
// MyMQTT/2303-34/TOILET/WATER/COLD/INITIAL
const char  INITIAL_FORMAT[] PROGMEM = "MyMQTT/2303-34/%[^/]/WATER/%[^/]/INITIAL";
#define MAX_ITEM_SIZE 20
#define MAX_PAYLOAD_SIZE 10
const size_t g_intMaxIncomingTopicLength = strlen_P(INITIAL_FORMAT) - 4 + 2 * MAX_ITEM_SIZE;  
const char  SUBSCRIBE[]          = "MyMQTT/2303-34/+/WATER/+/INITIAL";
const char* DATA[]               = {"BATH", "TOILET", "COLD", "HOT"}; 
const char* CMD[]                = {"TICKCOUNT", "GET"};

// ����� ���������� �������� � ������� ����
#define SENSORS_COUNT        4
// uint8_t g_intIDs[SENSORS_COUNT] = {CHILD_ID_BATH_COLD, CHILD_ID_BATH_HOT, CHILD_ID_TOILET_COLD, CHILD_ID_TOILET_HOT};
uint8_t g_intPins[SENSORS_COUNT] = {PIN_BATH_COLD, PIN_BATH_HOT, PIN_TOILET_COLD, PIN_TOILET_HOT};
// ������� ���������
volatile unsigned long g_intPulseCounter[SENSORS_COUNT] = {0, 0, 0, 0};
unsigned long g_intLastQueryMS[SENSORS_COUNT] = {0, 0, 0, 0};
unsigned long g_intLastSensorQueryMS[SENSORS_COUNT] = {0, 0, 0, 0};
// ��������� ��������: ������ ����������, �� ��������� ������ �� ����������� �� ���������. ��� �� �����, 
// ������� ���������, �����������, ��������������. ����� ���������� ���������� � �� ����� ��������� ������.
// ���� ������ �� ������ - ���������� ������ �� ���������� ������ ������ �� ������ ��������, �������� ����� �����
//
// ������� ������ �����: ��� ��������� ������ �� ����������� � ������ g_boolMustRenewValue � ��������������� �������� 
// ������������ true ���� ������� ��� ���-�� �������� �� ���� �����. ��������������, � �������� ����� �����������,
// ���� �� ��������� ��������� � ����������� ������� "��� �������"
boolean g_boolMustRenewValue[SENSORS_COUNT] = {false, false, false, false};
	
// ������� ������ ����������� �� ������� ��������� ��������� �������� ��������� ���������	
#define QUERY_FREQUENCY 10000
// ������� ������ �������� ����. ������������ ��������, ��� � ������ ������������ ������� 
// ������������ ������������ �������, ������� ��� ������ ������������ ���������� ���� Bounce 
// � ����������� ��: ����� ���������� ������ � ��������� ��������, ����� ������������
// �������� ��������� �� ����� ���� ������ ���������� ������ (1 ������� ����� 10 ������ ����)
#define SENSOR_QUERY_FREQUENCY 500

// ��� ������ �������� g_intPulseCounter ����� ����, ������� ���� ����������� � ����������� 
// ��������� ���������� �� ������. ���� ��� ����� �� ��������, ���� ������� ��������, �� �� 
// �������� ���������� �� �� ����������
boolean g_boolInitialDataReceived[SENSORS_COUNT] = {false, false, false, false};
// ������� ���� ��������� �� +5�, ������� ������� � ������� �������� ������ �� ����, � ����� ������������ � +5�
// ???????|___________________|??????????????
//                            ^
// ����� ���������� ������ �� ������� ��������� ������ ��������, �.�. ��� �������� � 0 �� 1
//
// ����, �������� ������� "���� ������ ��� ����������"
boolean g_boolInitialState = true;
boolean g_boolPreviousState[SENSORS_COUNT] = {false, false, false, false};
	
#define PIN_485_ENABLE  27

void callback(char* theTopic, byte* thePayload, unsigned int theLength);
boolean reconnect();
void publishSensorData(const uint8_t theIdx);
void requestInitialData(const uint8_t theIdx);

void setup(void) {
	Serial.begin(9600);
	g_objMQTTClient.setServer(g_objMQTTServerIP, 1883);
	g_objMQTTClient.setCallback(callback);
	Ethernet.begin(g_objLocalMAC, g_objLocalIP, g_objDNS, g_objGateway, g_objMask);	
	delay(1500);
	Serial.print("Client is at ");
	Serial.println(Ethernet.localIP());
	g_intLastReconnectAttempt = 0;
	for (uint8_t i = 0 ; i < SENSORS_COUNT ; i++) {
		// Setup the button
		pinMode(g_intPins[i], INPUT);
		// Activate internal pull-up
		digitalWrite(g_intPins[i], HIGH);
		g_intLastSensorQueryMS[i] = millis();
	}
}

void loop(void) {
	if (!g_objMQTTClient.connected()) {
		unsigned long l_intNow = millis();
		if (l_intNow - g_intLastReconnectAttempt > 5000) {
			g_intLastReconnectAttempt = l_intNow;
			Serial.println("Reconnecting...");
			// Attempt to reconnect
			if (reconnect()) 
				g_intLastReconnectAttempt = 0;
			else 
				return;
		}
	} else 
		g_objMQTTClient.loop();
		
	unsigned long l_intDeltaMS = 0;
	for (int i = 0 ; i < SENSORS_COUNT ; i++) {
        // ���� ���������� ��� ��� � �� ������� (��������, ��-�� ����, ��� � ������ ������� �������,
        // �.�., ��� ������ �������, �� �� ��� �������), �� �������� �������
        if (!g_boolInitialDataReceived[i]) {
	        l_intDeltaMS = millis() - g_intLastQueryMS[i];
	        if (l_intDeltaMS > QUERY_FREQUENCY) {
		        #ifdef DEBUG
		        // Serial.print("No initial data received yet for sensor id = ");
		        // Serial.print(i);
		        // Serial.print(" since last attempt ");
		        // Serial.print(l_intDeltaMS);
		        // Serial.println(" ms ago. Let's try again");
		        #endif
		        requestInitialData(i);
	        }
        }
	    l_intDeltaMS = millis() - g_intLastSensorQueryMS[i];
	    if ((g_boolInitialState) || (l_intDeltaMS > SENSOR_QUERY_FREQUENCY)) {
		    #ifdef DEBUG
		    // Serial.print("No initial data received yet for sensor id = ");
		    // Serial.print(i);
		    // Serial.print(" since last attempt ");
		    // Serial.print(l_intDeltaMS);
		    // Serial.println(" ms ago. Let's try again");
		    #endif
		    // g_objSensorNode.request(g_intIDs[i], V_VAR1);
		    g_intLastSensorQueryMS[i] = millis();
			// ������� ������� ��������� ����
			boolean l_boolCurrentState = digitalRead(g_intPins[i]);
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
			if (l_boolNewTickArrived || g_boolMustRenewValue[i]) {
				g_boolMustRenewValue[i] = false;
				publishSensorData(i);
			}
		}
	}
	g_boolInitialState = false;
}

void callback(char* theTopic, byte* thePayload, unsigned int theLength) {
	Serial.print("Message arrived [");
	Serial.print(theTopic);
	Serial.print("] ");
	for (int i = 0 ; i < theLength ; i++) Serial.print((char)thePayload[i]);
	Serial.println();
	// �������� ����� ������
	size_t l_intTopicLength = strlen(theTopic);
	if ((0 == l_intTopicLength) || (l_intTopicLength > g_intMaxIncomingTopicLength)) return;
	// �������� ����� ������
	if (theLength > MAX_PAYLOAD_SIZE) return;
	// ��������� �����
	char l_strItem1[MAX_ITEM_SIZE + 1];
	char l_strItem2[MAX_ITEM_SIZE + 1];
	int l_intRes = sscanf_P(theTopic, INITIAL_FORMAT, l_strItem1, l_strItem2);
	if (2 != l_intRes) return;
	// �������� ���� �� �������� ���������� � "������������"
	boolean l_boolOK[2] = {false, false};
	uint8_t l_intIdx = 0;
	for (uint8_t i = 0 ; i < 2 ; i++) {
		for (uint8_t j = 0 ; j < 2 ; j++) {
			if (0 == i) {
				// ��������� ������ �������� (���������)
				l_boolOK[i] |= (0 == strcmp(l_strItem1, DATA[j]));
				if (l_boolOK[i] && (0 != j)) l_intIdx |= _BV(1);
			} else {
				// ��������� ������ �������� (�����������)
				l_boolOK[i] |= (0 == strcmp(l_strItem2, DATA[j + 2]));
				if (l_boolOK[i] && (0 != j)) l_intIdx |= _BV(0);
			}
			if (l_boolOK[i]) break;
		}
	}
	if (!(l_boolOK[0] && l_boolOK[1])) return;
	// ���������� ��������� ������ 
	if (g_boolInitialDataReceived[l_intIdx]) return;
	// Serial.println(l_strItem1);
	// Serial.println(l_strItem2);
	char l_strPayload[MAX_PAYLOAD_SIZE + 1];
	strncpy(l_strPayload, (const char*)thePayload, theLength);
	l_strPayload[theLength] = '\0';
	char* l_ptrRes;
	unsigned long l_intValue = strtoul(l_strPayload, &l_ptrRes, 10);
	if (('\0' == l_strPayload[0]) || ('\0' != *l_ptrRes)) return;
	// ���� ��� ���-�� �������� �� ���� �����: ���� ��������� �� ���� ����������
	g_boolMustRenewValue[l_intIdx] = (0 != g_intPulseCounter[l_intIdx]);
    g_intPulseCounter[l_intIdx] += l_intValue;
    Serial.print("Received last pulse count (");
    Serial.print(l_intValue);
    Serial.print("): ");
    Serial.print("g_intPulseCounter[");
    Serial.print(l_intIdx);
    Serial.print("]=");
    Serial.println(g_intPulseCounter[l_intIdx]);
    g_boolInitialDataReceived[l_intIdx] = true;
}

void publishSensorData(const uint8_t theIdx) {
	if (!g_objMQTTClient.connected()) return;
	// ���������� MQTT-���������
	char l_strMsg[MAX_MSG_SIZE];
	// PIN_BATH_COLD, PIN_BATH_HOT, PIN_TOILET_COLD, PIN_TOILET_HOT
	byte l_intIdx[2];
	// 0 : i = 0, 1
	// 1 : i = 2, 3
	l_intIdx[0] = theIdx >> 1;
	// 2 : i = 0, 2
	// 3 : i = 1, 3
	l_intIdx[1] = 2 + (theIdx & 1);
	sprintf_P(l_strMsg, FORMAT, DATA[l_intIdx[0]], DATA[l_intIdx[1]], CMD[0]);
	char l_strTickCount[12]; // "-2147483648\0"
	g_objMQTTClient.publish(l_strMsg, itoa(g_intPulseCounter[theIdx], l_strTickCount, 10));
}

void requestInitialData(const uint8_t theIdx) {
	if (!g_objMQTTClient.connected()) return;
	// ���������� MQTT-���������
	char l_strMsg[MAX_MSG_SIZE];
	// PIN_BATH_COLD, PIN_BATH_HOT, PIN_TOILET_COLD, PIN_TOILET_HOT
	byte l_intIdx[2];
	// 0 : i = 0, 1
	// 1 : i = 2, 3
	l_intIdx[0] = theIdx >> 1;
	// 2 : i = 0, 2
	// 3 : i = 1, 3
	l_intIdx[1] = 2 + (theIdx & 1);
	sprintf_P(l_strMsg, FORMAT, DATA[l_intIdx[0]], DATA[l_intIdx[1]], CMD[1]);
	g_objMQTTClient.publish(l_strMsg, "");
	g_intLastQueryMS[theIdx] = millis();
}

boolean reconnect() {
	if (g_objMQTTClient.connect("node01.net173.local")) {
		Serial.println("Reconnect attempt success");		
		// Once connected, publish an announcement...
		for (int i = 0 ; i < SENSORS_COUNT ; i++) 
			requestInitialData(i);
		// g_objMQTTClient.publish("MyMQTT/2303-34/Bath/Water/Hot/TICK","-1");
		Serial.println(F("MQTT announcements are sent"));
		// ... and resubscribe
		g_objMQTTClient.subscribe(SUBSCRIBE);
		Serial.println("Subscription sent");
	}
	return g_objMQTTClient.connected();
}