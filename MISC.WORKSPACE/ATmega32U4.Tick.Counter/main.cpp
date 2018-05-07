/*
 * ATmega32U4.Tick.Counter.cpp
 *
 * Created: 06.05.2018 16:40:26
 * Author : Alexey N. Zhukov
 */ 

#include <avr/io.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include "HAL.h"
#include "EERTOS.h"

#define SENSORS_COUNT   4
#define PIN_BATH_COLD   35
#define PIN_BATH_HOT    37
#define PIN_TOILET_COLD 31
#define PIN_TOILET_HOT  33
uint8_t g_intPins[SENSORS_COUNT] = {PIN_BATH_COLD, PIN_BATH_HOT, PIN_TOILET_COLD, PIN_TOILET_HOT};
    
#define SENSOR_QUERY_INTERVAL 500
void ReadSensorState();

byte g_objLocalMAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE };
IPAddress g_objLocalIP(192, 168, 0, 71);
IPAddress g_objDNS(192, 168, 0, 2);
IPAddress g_objGateway(192, 168, 0, 65);
IPAddress g_objMask(255, 255, 255, 192);
EthernetClient g_objClient;
PubSubClient g_objMQTTClient(g_objClient);
IPAddress g_objMQTTServerIP(192, 168, 0, 41);

int main(void) {
	InitAll();
	InitRTOS();
	RunRTOS();
    
    SetTask(ReadSensorState);
    	g_objMQTTClient.setServer(g_objMQTTServerIP, 1883);
    	// g_objMQTTClient.setCallback(callback);


	while(1) {
    	// Главный цикл диспетчера
    	wdt_reset();	// Сброс собачьего таймера
    	TaskManager();	// Вызов диспетчера
	}
}


void ReadSensorState() {
    SetTimerTask(ReadSensorState, SENSOR_QUERY_INTERVAL);
}
