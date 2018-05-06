#include <MySensor.h>
#include <SPI.h>
#include <Bounce2.h>

// Идентификатор узла
#define NODE_ID 10
// Общее количество сенсоров у данного узла
#define SENSORS_COUNT        4
// Отдельные сенсоры на холодную и горячую воду в туалете и ванной
#define CHILD_ID_BATH_COLD   0
#define CHILD_ID_BATH_HOT    1
#define CHILD_ID_TOILET_COLD 2
#define CHILD_ID_TOILET_HOT  3
// Номера ног контроллера, соответствующие отдельным сенсорам
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

// Входная нога подтянута на +5в, поэтому импульс с датчика опускает сигнал до нуля, а затем возвращается к +5в
// ???????|___________________|??????????????
//                            ^
// Будем отправлять данные по приходу переднего фронта импульса, т.е. при переходе с 0 на 1
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
        // Запрашиваем у контроллера последние показания счетчика
        // (но не факт, что он нам ответит)
        g_objSensorNode.request(g_intIDs[i], V_VAR1);
        g_intLastQueryMS[i] = millis();
    }
}

boolean g_boolPreviousState[SENSORS_COUNT] = {false, false, false, false};
/*
Машина состояний:
g_boolInitialState      0 1 0 1 0 1 0 1
g_boolPreviousState     0 0 1 1 0 0 1 1
l_boolCurrentState      0 0 0 0 1 1 1 1
Отправлять сигнал?              1 
*/
void loop() {
    g_objSensorNode.process();

    for (uint8_t i = 0 ; i < SENSORS_COUNT ; i++) {
        // Если контроллер нам так и не ответил (например, из-за того, что в момент первого запроса,
        // т.е., при старте сенсора, он не был запущен), то повторим попытку
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
	    // Считаем текущее состояние ноги
	    boolean l_boolCurrentState = g_objDebouncer[i].read();
        // Если только что стартовали - предыдущее состояние неизвестно, для определенности 
        // считаем его таким же, как и текущее состояние ноги
	    if (g_boolInitialState) 
            g_boolPreviousState[i] = l_boolCurrentState;

		boolean l_boolNewTickArrived = false;
		do {
			// Если состояние ноги не изменилось - ничего не делаем
			if (l_boolCurrentState == g_boolPreviousState[i]) break;
			g_boolPreviousState[i] = l_boolCurrentState;
			// Если состояние ноги изменилось, но текущее = LOW, то это переход с HIGH на LOW,
			// который нас не интересует
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
        // Шлюз тупо перешлет сообщение от OpenHAB с типом MQTTSUBSCRIBE
        // В этом случае payload радиосообщения будет пустым
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
