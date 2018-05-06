/*
 * Blink.cpp
 *
 * Created: 13.09.2014 16:13:22
 *  Author: Alexey N. Zhukov
 */

// Pin 13 has an LED connected on most Arduino boards.
// give it a name:
#include <Arduino.h>
#include <pgmspace.h>
#include <SoftwareSerial.h>
#include <MySensor.h>
#include "GSM.h"

//**************************************************************************
char number[]="+79637163311";  //Destination number
char text[]="Hello world";  //SMS to send
// sms_state_enum type_sms = SMS_ALL;      //Type of SMS
// byte del_sms=0;                //0: No deleting sms - 1: Deleting SMS
//**************************************************************************

CGSM g_objGSM;
//char sms_rx[122]; //Received text SMS
//int inByte=0;    //Number of byte received on serial port
//char number_incoming[20];
//int call;
//int error;
/*
void Check_Protocol(String inStr)
{
       Serial.print("Command: ");
       Serial.println(inStr);

  Serial.println("Check_Protocol");

    switch (inStr[0])
      {
       case 'a' :  //Answer
           if (g_objGSM.CallStatus()==CALL_INCOM_VOICE){
             g_objGSM.PickUp();
             Serial.println("Answer");
           }
           else
           {
             Serial.println("No incoming call");
           }
         break;


       case 'c': // C  //Call
         if (inStr.length()<2)  //To call variable 'number'    comand   c
         {
           Serial.print("Calling ");
           Serial.println(number);
           g_objGSM.Call(number);
         }
         if (inStr.length()==2)  //To call number in phone book position   comand   cx where x is the SIM position
         {
             error=g_objGSM.GetPhoneNumber(inStr[1],number);
             if (error!=0)
             {
               Serial.print("Calling ");
               Serial.println(number);
               g_objGSM.Call(number);
             }
             else
             {
               Serial.print("No number in pos ");
               Serial.println(inStr[1]);
             }
         }
         break;

       case 'h': //H //HangUp if there is an incoming call
         if (g_objGSM.CallStatus()!=CALL_NONE)
         {
           Serial.println("Hang");
           g_objGSM.HangUp();
         }
         else
         {
           Serial.println("No incoming call");
         }
         break;


       case 's': //S //Send SMS
         Serial.print("Send SMS to ");
         Serial.println(number);
         error=g_objGSM.SendSMS(number,text);
         if (error==0)  //Check status
         {
             Serial.println("SMS ERROR \n");
         }
         else
         {
             Serial.println("SMS OK \n");
         }
         break;

       case 'p':  //Read-Write Phone Book
         if (inStr.length()==3)
         {

           switch (inStr[1])
           {
             case 'd':  //Delete number in specified position  pd2
               error=g_objGSM.DelPhoneNumber(inStr[2]);
               if (error!=0)
               {
                 Serial.print("Phone number position ");
                 Serial.print(inStr[2]);
                 Serial.println(" deleted");
               }
               break;



             case 'g':  //Read from Phone Book position      pg2
               error=g_objGSM.GetPhoneNumber(inStr[2],number);
               if (error!=0)  //Find number in specified position
               {
                 Serial.print("Phone Book position ");
                 Serial.print(inStr[2]);
                 Serial.print(": ");
                 Serial.println(number);
               }
               else  //Not find number in specified position
               {
                 Serial.print("No Phone number in position ");
                 Serial.println(inStr[2]);
               }
               break;
             case 'w':  //Write from Phone Book Position    pw2
               error=g_objGSM.WritePhoneNumber(inStr[2],number);
               if (error!=0)
               {
                 Serial.print("Number ");
                 Serial.print(number);
                 Serial.print(" writed in Phone Book position ");
                 Serial.println(inStr[2]);
               }
               else Serial.println("Writing error");
               break;



           }

         }
         break;

       }

    delay(1500);

    return;
 }
 */
 /*
 void Check_Call()  //Check status call if this is available
 {
     call=g_objGSM.CallStatus();
     switch (call)
     {
       case CALL_NONE:
         Serial.println("no call");
         break;
       case CALL_INCOM_VOICE:
         g_objGSM.CallStatusWithAuth(number_incoming,0,0);
         Serial.print("incoming voice call from ");
         Serial.println(number_incoming);
         break;
       case CALL_ACTIVE_VOICE:
         Serial.println("active voice call");
         break;
       case CALL_NO_RESPONSE:
         Serial.println("no response");
         break;
     }
     return;
 }

 */
 /*
 void Check_SMS()  //Check if there is an sms 'type_sms'
 {
     char pos_sms_rx;  //Received SMS position
     pos_sms_rx=g_objGSM.IsSMSPresent(type_sms);
     if (pos_sms_rx!=0)
     {
       //Read text/number/position of sms
       g_objGSM.GetSMS(pos_sms_rx,number_incoming,sms_rx,120);
       Serial.print("SMS from ");
       Serial.print(number_incoming);
       Serial.print("(pos: ");
       Serial.print(word(pos_sms_rx));
       Serial.println(")");
       Serial.println(sms_rx);
       if (del_sms==1)  //If 'del_sms' is 1, i delete sms
       {
         error=g_objGSM.DeleteSMS(pos_sms_rx);

         if (error==1)Serial.println("SMS deleted");
         else Serial.println("SMS not deleted");
         Serial.println(error);
         Serial.println((char *)g_objGSM.m_intCommBuffer);
       }
     }
     return;
}

*/
#define MAX_STRING 32
char g_strBuffer[MAX_STRING];

char* getString(const char* theString) {
	strcpy_P(g_strBuffer, (char*)theString);
	return g_strBuffer;
}

const char STARTUP[] PROGMEM = "System startup";

boolean g_boolGSMIsOn= false;

MySensor gw;

void setup()
{
    // Wire.begin();
    // Serial.begin(9600);
	// —хема подключени€ NRF24 к GBoard отличаетс€ от указанной на https://www.mysensors.org/build/connect_radio
	// CE соединен с D8 вместо D9
	// CS соединен с D9 вместо D10
    gw.begin(NULL, 100);

    Serial.println(getString(STARTUP));
    //
    // <CR><LF>+CMGR: "REC UNREAD","+XXXXXXXXXXXX",,"02/03/18,09:54:28+40"<CR><LF>
    // There is SMS text<CR><LF>OK<CR><LF>
    // const char* l_strSMS = "\r\n+CMGR: \"REC UNREAD\",\"+79637163311\",,\"02/03/18,09:54:28+40\"\r\nThere is SMS text\r\nOK\r\n";
    /*
    const char* l_strSMS = "\r\n+CMGR: \"REC UNREAD\",\"+79637163311\",,\"02/03/18,09:54:28+40\"\r\nThere is SMS text\r\nOK\r\n";
    strcpy((char*)(g_objGSM.m_intCommBuffer), l_strSMS);
    g_objGSM.m_intCommBufferLength = strlen(l_strSMS);
    SMS l_objSMS;
    SMS_TYPE l_intSMSRes = g_objGSM.GetSMS((uint8_t)1, l_objSMS);
    if ((SMS_READ_SMS == l_intSMSRes) || (SMS_UNREAD_SMS == l_intSMSRes)) {
        Serial.println(l_objSMS.m_strDate);
        Serial.println(l_objSMS.m_strSender);
        Serial.println(l_objSMS.m_strBody);
    }
*/

	g_boolGSMIsOn = g_objGSM.TurnOn(9600);          //module power on
    if (g_boolGSMIsOn) {
	    g_objGSM.InitParam(PARAM_GROUP_1);//configure the module
	    g_objGSM.Echo(false);               //enable AT echo
        uint8_t* l_strRes = NULL;
		// ѕервый SM - сообщени€ считываютс€ и удал€ютс€ из пам€ти SIM-карты
		// ¬торой SM - сообщени€ записываютс€ в пам€ть SIM-карты
		// “ретий SM - получаемые сообщени€ сохран€ютс€ в пам€ти SIM-карты если не сконфигурирован роутинг в PC(?)
        g_objGSM.SendArbitraryCommandWaitResponse("AT+CPMS=\"SM\",\"SM\",\"SM\"", 500, 10, l_strRes, 5);
        Serial.println((char*)l_strRes);
    }
}

//byte x = 0;

int readLine(int theChar, char *theBuffer, int theLength)
{
    static int l_intPos = 0;
    int l_intRes;

    if (theChar > 0) {
        switch (theChar) {
            case '\n': // Ignore new-lines
            break;
        case '\r': // Return on CR
            l_intRes = l_intPos;
            l_intPos = 0;  // Reset position index ready for next time
            return l_intRes;
        default:
            if (l_intPos < theLength - 1) {
                theBuffer[l_intPos++] = theChar;
                theBuffer[l_intPos] = 0;
            }
        }
    }
    // No end of line has been found, so return -1.
    return -1;
}

void loop() {
    static char l_strBuffer[80];
    if ((readLine(Serial.read(), l_strBuffer, 80) > 0) && (g_boolGSMIsOn)) {
        if (0 == strcmp("SMS", l_strBuffer)) {
            int8_t l_intRes = g_objGSM.IsSMSPresent();
            Serial.print("IsSMSPresent::");
            Serial.println(l_intRes);
        } else {
            Serial.println("AT command: ");
            Serial.println(l_strBuffer);
            uint8_t* l_strRes = NULL;
            if (AT_RESPONSE_OK == g_objGSM.SendArbitraryCommandWaitResponse(l_strBuffer, 5000, 1500, l_strRes, 5)) {
                Serial.println("Sim900 response:");
                Serial.println((char*)l_strRes);
            } else
                Serial.print("No Sim900 response");
        }
    }
}
