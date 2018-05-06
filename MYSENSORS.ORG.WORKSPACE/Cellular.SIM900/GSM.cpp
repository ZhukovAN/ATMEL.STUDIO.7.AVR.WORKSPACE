/*
 * GSM.cpp
 *
 * Created: 08.07.2015 22:08:44
 *  Author: bug
 */ 

#include <Arduino.h>
#include <SoftwareSerial.h>
#include <pgmspace.h>
#include "GSM.h"

extern "C" {
    #include <string.h>
}

// Если у GBoard джамперы установлены в режим управления Sim900 посредством программного UART, 
// то RX- и TX-сигналы берутся с PD2 и PD3 соответственно
#define rxPin 2 
#define txPin 3

const char _command_AT[] PROGMEM = "AT";
// Reset to factory settings
const char _command_AT_F[] PROGMEM = "AT&F";
// Request calling line identification
const char _command_AT_CLIP1[] PROGMEM = "AT+CLIP=1";
// Mobile Equipment Error Code
const char _command_AT_CMEE0[] PROGMEM = "AT+CMEE=0";   // Disable
const char _command_AT_CMEE1[] PROGMEM = "AT+CMEE=1";   // Enable
const char _command_AT_CMEE2[] PROGMEM = "AT+CMEE=2";   // Enable verbose
// Set the SMS mode to PDU
const char _command_AT_CMGF0[] PROGMEM = "AT+CMGF=0";
// Set the SMS mode to text
const char _command_AT_CMGF1[] PROGMEM = "AT+CMGF=1";   
// Select phone book memory storage
const char _command_AT_CPBS_SM[] PROGMEM = "AT+CPBS=\"SM\"";
// Disable messages about new SMS from the GSM module
const char _command_AT_CNMI_20[] PROGMEM = "AT+CNMI=2,0";
// Initialize memory for SMS in the SIM card
const char _command_AT_CPMS_SM_SM_SM[] PROGMEM = "AT+CPMS=\"SM\",\"SM\",\"SM\"";

const char _command_AT_CMGL_ALL[] PROGMEM = "AT+CMGL=\"ALL\"";
const char _command_AT_CMGR[] PROGMEM = "AT+CMGR=";
const char _sms_state_REC_READ[] PROGMEM = "\"REC READ\"";
const char _sms_state_REC_UNREAD[] PROGMEM = "\"REC UNREAD\"";

const char _command_AT_CMGD[] PROGMEM = "AT+CMGD=";

// Echo mode
const char _command_ATE0[] PROGMEM = "ATE0";   // Disable
const char _command_ATE1[] PROGMEM = "ATE1";   // Enable


const char _response_OK[] PROGMEM = "OK";
const char _response_ERROR[] PROGMEM = "ERROR";
const char _response_CPMS[] PROGMEM = "+CPMS:";
const char _response_CMGL[] PROGMEM = "+CMGL:";
const char _response_CMGR[] PROGMEM = "+CMGR:";

#ifdef DEBUG_PRINT
#define DBG(args...) this->DebugPrint(args)
#else
#define DBG(args...) 
#endif

AT_RESPONSE CGSM::SendCommandWaitResponse(PGM_P theCommand, uint16_t theStartTimeout, uint16_t theInterCharTimeout, PGM_P theExpectedResponse, uint8_t theRetries) {
    return this->_SendCommandWaitResponse(theCommand, theStartTimeout, theInterCharTimeout, theExpectedResponse, theRetries);
}

AT_RESPONSE CGSM::SendCommandWaitResponse(PGM_P theCommand, uint16_t theStartTimeout, uint16_t theInterCharTimeout, uint8_t theRetries) {
	return this->_SendCommandWaitResponse(theCommand, theStartTimeout, theInterCharTimeout, NULL, theRetries);
}

CGSM::CGSM(void) {
    m_objSerial = new SoftwareSerial(rxPin, txPin);	

    pinMode(GSM_PWR, OUTPUT);               // sets pin 5 as output
    pinMode(GSM_RESET, OUTPUT);            // sets pin 4 as output

    // not registered yet
    // this->m_intModuleStatus = STATUS_NONE;
}

void CGSM::sendPGM(PGM_P theString, bool theDoCR)
{
    int i = 0;
    char l_chrChar;
    do {
        l_chrChar = pgm_read_byte_near(theString + i);
        if (l_chrChar != 0) this->m_objSerial->write(l_chrChar);
        i++;
    } while (l_chrChar != 0);
    if (theDoCR) this->m_objSerial->write("\r");
}

RX_STATE CGSM::WaitResponse(uint16_t theStartTimeout, uint16_t theInterCharTimeout) {
    RX_STATE l_intRes;

    this->RxInit(theStartTimeout, theInterCharTimeout);
    // wait until response is not finished
    do {
        l_intRes = this->IsRxFinished();
    } while (RX_NOT_FINISHED == l_intRes);
    return l_intRes;
}

RX_STATE CGSM::WaitResponse(uint16_t theStartTimeout, uint16_t theInterCharTimeout, PGM_P theExpectedResponse) {
    RX_STATE l_intRes = this->WaitResponse(theStartTimeout, theInterCharTimeout);

    if (RX_FINISHED == l_intRes) {
        // Gолучены какие-то данные из UART Sim900
        // Соответствуют ли они ожидаемым?
        if (this->IsStringReceived(theExpectedResponse)) 
            l_intRes = RX_FINISHED_STR_RECV;
        else 
            l_intRes = RX_FINISHED_STR_NOT_RECV;
    }
    return l_intRes;
}

void CGSM::RxInit(uint16_t theStartTimeout, uint16_t theInterCharTimeout) {
    this->m_intRxState = RX_NOT_STARTED;
    this->m_intStartTimeout = theStartTimeout;
    this->m_intMaxIntercharTimeout = theInterCharTimeout;
    this->m_intLastRxTime = millis();
    this->m_intCommBuffer[0] = 0x00; // end of string
    this->m_ptrCommBuffer = &m_intCommBuffer[0];
    this->m_intCommBufferLength = 0;
    this->m_objSerial->flush(); // Erase Rx circular buffer
}

// Конечный автомат, принимающий данные по UART из Sim900
RX_STATE CGSM::IsRxFinished(void) {
    byte l_intNumberOfBytes;
    RX_STATE l_intRes = RX_NOT_FINISHED;  // default not finished

    if (RX_NOT_STARTED == this->m_intRxState) {
        // Автомат в начальном состоянии. Данные есть?
        if (!this->m_objSerial->available()) {
            // В начальном состоянии и данных нет - проверяем на стартовый таймаут
            if ((unsigned long)(millis() - this->m_intLastRxTime) >= this->m_intStartTimeout) {
                // timeout elapsed => GSM module didn't start with response
                // so communication is takes as finished
                this->m_intCommBuffer[this->m_intCommBufferLength] = 0x00;
                l_intRes = RX_TIMEOUT_ERR;
            }
        } else {
            // at least one character received => so init inter-character
            // counting process again and go to the next state
            this->m_intLastRxTime = millis(); // init timeout for inter-character space
            this->m_intRxState = RX_ALREADY_STARTED;
        }
    }

    if (RX_ALREADY_STARTED == this->m_intRxState) {
        // Reception already started
        // check new received bytes
        // only in case we have place in the buffer
        l_intNumberOfBytes = this->m_objSerial->available();
        // if there are some received bytes postpone the timeout
        if (l_intNumberOfBytes) this->m_intLastRxTime = millis();
        
        // read all received bytes
        while (l_intNumberOfBytes) {
            l_intNumberOfBytes--;
            if (this->m_intCommBufferLength < COMM_BUF_LEN) {
                // we have still place in the GSM internal comm. buffer =>
                // move available bytes from circular buffer
                // to the Rx buffer
                *(this->m_ptrCommBuffer) = this->m_objSerial->read();
                this->m_ptrCommBuffer++;
                this->m_intCommBufferLength++;
                this->m_intCommBuffer[this->m_intCommBufferLength] = 0x00;  // and finish currently received characters
                // so after each character we have
                // valid string finished by the 0x00
            } else {
                // comm buffer is full, other incoming characters
                // will be discarded
                // but despite of we have no place for other characters
                // we still must to wait until
                // inter-character timeout is reached
                
                // so just readout character from circular RS232 buffer
                // to find out when communication id finished(no more characters
                // are received in inter-char timeout)
                this->m_objSerial->read();
            }
        }

        // finally check the inter-character timeout
        if ((unsigned long)(millis() - this->m_intLastRxTime) >= this->m_intMaxIntercharTimeout) {
            // timeout between received character was reached
            // reception is finished
            // ---------------------------------------------
            this->m_intCommBuffer[this->m_intCommBufferLength] = 0x00;  // for sure finish string again
            // but it is not necessary
            l_intRes = RX_FINISHED;
        }
    }
    return l_intRes;
}

boolean CGSM::IsStringReceived(PGM_P theExpectedResponse) {
    boolean l_boolRes = false;

    if (this->m_intCommBufferLength) {
        uint16_t l_intLen = strlen_P(theExpectedResponse);    // string length
        char *l_strTemp = (char*)malloc(l_intLen + 1);
        if (NULL == l_strTemp) return l_boolRes;
        strcpy_P(l_strTemp, theExpectedResponse);
        l_boolRes = (NULL != strstr((char *)this->m_intCommBuffer, l_strTemp));
        if (NULL != l_strTemp) free(l_strTemp);
    }
    return l_boolRes;	
}

boolean CGSM::TurnOn(long theBaudRate) {
    boolean l_boolRes = false;
    this->SetCommLineStatus(LS_ATCMD);
    this->m_objSerial->begin(theBaudRate);
    AT_RESPONSE l_intRes;
    
    do {
        // Check power
        DBG(PSTR("DBG: Check: if GSM powered on\r\n"));
        l_intRes = this->SendCommandWaitResponse(_command_AT, 500, 100, _response_OK, 5);
        l_boolRes = (AT_RESPONSE_OK == l_intRes);
        if (l_boolRes) {
            // Отправили AT, модуль ответил OK - все в порядке: модуль включен, 
            // скорости UART совпадают - можно выходить
            DBG(PSTR("DBG: GSM is on\r\n"));            
            break;
        }
        if (AT_RESPONSE_ERR_NO_RESP == l_intRes) {
            // There is no response => turn on the module
            DBG(PSTR("DBG: GSM off, let's start it\r\n"));
            // Generate turn on pulse
            digitalWrite(GSM_PWR, HIGH);
            delay(1200);
            digitalWrite(GSM_PWR, LOW);
            delay(5000);
            // Попробуем еще раз связаться
            l_intRes = this->SendCommandWaitResponse(_command_AT, 500, 100, _response_OK, 5);
            l_boolRes = (AT_RESPONSE_OK == l_intRes);
            if (l_boolRes) {
                // Отправили AT, модуль ответил OK - все в порядке: модуль включен,
                // скорости UART совпадают - можно выходить
                DBG(PSTR("DBG: GSM is on\r\n"));
                break;
            } else if (AT_RESPONSE_ERR_NO_RESP == l_intRes) {
                // Попробовали включить, но Sim900 все равно не отвечает - прекращаем попытки
                DBG(PSTR("DBG: Failed to power GSM on\r\n"));
                break;
            }
        }
        // Sim900 что-то ответил (сразу или после программного включения питания), но это не OK. Возможно, просто не 
        // совпадают скорости UART. Попробуем подобрать скорость
        if (AT_RESPONSE_ERR_DIF_RESP == l_intRes) {
            // Sim900 что-то нам ответил, но явно не OK. Возможно, дело в несовместимости скоростей UART
            // Попробуем, переключаясь на разные варианты скоростей, установить требуемую
            const long l_intBaudRate[] = {4800, 9600, 19200, 38400, 57600, 115200};
            DBG(PSTR("DBG: GSM on, but answer is unknown, probably baud rate incorrect\r\n"));
            for (uint8_t i= 0 ; i < sizeof(l_intBaudRate) ; i++) {
                this->m_objSerial->begin(l_intBaudRate[i]);
                DBG(PSTR("DBG: Trying to change Sim900 baud rate: "));
                DBG(l_intBaudRate[i]);
                DBG(PSTR("\r\n"));
                delay(100);
                this->m_objSerial->write(l_intBaudRate[i]);
                this->m_objSerial->write("AT+IPR=");
                this->m_objSerial->write(theBaudRate);
                this->m_objSerial->write("\r"); // send <CR>
                delay(500);
                this->m_objSerial->begin(theBaudRate);
                delay(100);
                if (AT_RESPONSE_OK == this->SendCommandWaitResponse(_command_AT, 500, 100, _response_OK, 5)) {
                    DBG(PSTR("DBG: Successfully set Sim900 baud rate to "));
                    DBG(theBaudRate);
                    DBG(PSTR("\r\n"));
                    break;
                }
            }
            // Попытались автоматически настроить скорость - попробуем еще раз соединиться с Sim900
            l_boolRes = (AT_RESPONSE_OK == this->SendCommandWaitResponse(_command_AT, 500, 50, _response_OK, 5));
            if (l_boolRes) {
                DBG(PSTR("DBG: Baud OK\r\n"));
            } else {
                DBG(PSTR("DBG: No GSM answer\r\n"));
            }
        } else {
            // Произошло что-то странное: 
            DBG(PSTR("DBG: UNexpected return code\r\n"));
        }
    } while (false);
    this->SetCommLineStatus(LS_FREE);

    // Send collection of first initialization parameters for the GSM module
    // InitParam(PARAM_SET_0);
    return l_boolRes;
}

#ifdef DEBUG_PRINT

void CGSM::DebugPrint(PGM_P theString) {
    int i = 0;
    char l_chrChar;
    do {
        l_chrChar = pgm_read_byte_near(theString + i);
        if (l_chrChar != 0) Serial.write(l_chrChar);
        i++;
    } while (l_chrChar != 0);
}

void CGSM::DebugPrint(int theNumber) {
	Serial.print(theNumber);
}

#endif

AT_RESPONSE CGSM::SendArbitraryCommandWaitResponse(
    const char* theCommand, 
    uint16_t theStartTimeout, uint16_t theInterCharTimeout, 
    uint8_t* &theData, uint8_t theRetries) {
    return this->_SendArbitraryCommandWaitResponse(theCommand, theStartTimeout, theInterCharTimeout, NULL, theData, theRetries);
}

AT_RESPONSE CGSM::SendArbitraryCommandWaitResponse(
    const char* theCommand, 
    uint16_t theStartTimeout, uint16_t theInterCharTimeout, const char* theExpectedResponse, 
    uint8_t* &theData, uint8_t theRetries) {
	return this->_SendArbitraryCommandWaitResponse(theCommand, theStartTimeout, theInterCharTimeout, theExpectedResponse, theData, theRetries);
}

AT_RESPONSE CGSM::_SendCommandWaitResponse(PGM_P theCommand, uint16_t theStartTimeout, uint16_t theInterCharTimeout, PGM_P theExpectedResponse, uint8_t theRetries) {
	uint8_t l_intStatus;
	AT_RESPONSE l_intRes = AT_RESPONSE_ERR_NO_RESP;

	for (uint8_t i = 0; i < theRetries; i++) {
    	// Delay 500 ms before sending next repeated AT command
    	// so if we have no_of_attempts = 1 timeout will not occurred
    	if (i > 0) delay(500);
    	this->sendPGM(theCommand, true);

    	l_intStatus = this->WaitResponse(theStartTimeout, theInterCharTimeout);
    	if (RX_FINISHED == l_intStatus) {
        	// something was received but what was received?
        	// ---------------------------------------------
        	if ((NULL == theExpectedResponse) || (this->IsStringReceived(theExpectedResponse))) {
            	l_intRes = AT_RESPONSE_OK;
            	break;  // response is OK => finish
        	} else
                l_intRes = AT_RESPONSE_ERR_DIF_RESP;
        } else {
        	// nothing was received
        	// --------------------
        	l_intRes = AT_RESPONSE_ERR_NO_RESP;
    	}
	}
	return l_intRes;
}

AT_RESPONSE CGSM::_SendArbitraryCommandWaitResponse(
    const char* theCommand, uint16_t theStartTimeout, uint16_t theInterCharTimeout, const char* theExpectedResponse, 
    uint8_t* &theData, uint8_t theRetries) {
    theData = this->m_intCommBuffer;
    uint8_t l_intStatus;
    AT_RESPONSE l_intRes = AT_RESPONSE_ERR_NO_RESP;

    for (uint8_t i = 0; i < theRetries; i++) {
        // Delay 500 ms before sending next repeated AT command
        // so if we have no_of_attempts = 1 timeout will not occurred
        if (i > 0) delay(500);
        this->m_objSerial->write(theCommand);
        this->m_objSerial->write("\r");

        l_intStatus = this->WaitResponse(theStartTimeout, theInterCharTimeout);
        if (RX_FINISHED == l_intStatus) {
        	// something was received but what was received?
        	// ---------------------------------------------
        	if ((NULL == theExpectedResponse) || (this->IsStringReceived(theExpectedResponse))) {
            	l_intRes = AT_RESPONSE_OK;
            	break;  // response is OK => finish
        	} else
            	l_intRes = AT_RESPONSE_ERR_DIF_RESP;
        } else {
            // nothing was received
            // --------------------
            l_intRes = AT_RESPONSE_ERR_NO_RESP;
        }
    }
    return l_intRes;
}

void CGSM::TurnOff() {
    this->SetCommLineStatus(LS_ATCMD);
    // Check power
    AT_RESPONSE l_intRes = this->SendCommandWaitResponse(_command_AT, 500, 100, _response_OK, 5);

    if (AT_RESPONSE_ERR_NO_RESP != l_intRes) {
        // There is response => turn off the module
        // Generate turn off pulse
        digitalWrite(GSM_PWR, HIGH);
        delay(1200);
        digitalWrite(GSM_PWR, LOW);
        delay(5000);
    }
    this->SetCommLineStatus(LS_FREE);
}

/**********************************************************
  Sends parameters for initialization of GSM module

  group:  0 - parameters of group 0 - not necessary to be registered in the GSM
          1 - parameters of group 1 - it is necessary to be registered
**********************************************************/
void CGSM::InitParam(INIT_PARAM theParamGroup) {
    if (LS_FREE != this->GetCommLineStatus()) return;
    this->SetCommLineStatus(LS_ATCMD);
    
    if (PARAM_GROUP_0 == theParamGroup) {
        // Reset to the factory settings
        this->SendCommandWaitResponse(_command_AT_F, 1000, 50, _response_OK, 5);
    } else if (PARAM_GROUP_1 == theParamGroup) {
        // Request calling line identification
        this->SendCommandWaitResponse(_command_AT_CLIP1, 500, 50, _response_OK, 5);
        // Mobile Equipment Error Code
#ifdef DEBUG
        this->SendCommandWaitResponse(_command_AT_CMEE2, 500, 50, _response_OK, 5);
#else
        this->SendCommandWaitResponse(_command_AT_CMEE0, 500, 50, _response_OK, 5);
#endif
        // Set the SMS mode to text
        this->SendCommandWaitResponse(_command_AT_CMGF1, 500, 50, _response_OK, 5);
        // Initialize SMS storage
        this->InitSMSMemory();
        // Select phone book memory storage
        this->SendCommandWaitResponse(_command_AT_CPBS_SM, 1000, 50, _response_OK, 5);
    }
    this->SetCommLineStatus(LS_FREE);
}	

boolean CGSM::InitSMSMemory() {
    boolean l_boolRes = false;
    if (LS_FREE != this->GetCommLineStatus()) return l_boolRes;
    this->SetCommLineStatus(LS_ATCMD);

    // Disable messages about new SMS from the GSM module
    this->SendCommandWaitResponse(_command_AT_CNMI_20, 1000, 50, _response_OK, 2);

    // Send AT command to init memory for SMS in the SIM card
    // Response:
    // +CPMS: <usedr>,<totalr>,<usedw>,<totalw>,<useds>,<totals>
    l_boolRes = (AT_RESPONSE_OK == this->SendCommandWaitResponse(_command_AT_CNMI_20, 1000, 1000, _response_OK, 10));
    this->SetCommLineStatus(LS_FREE);
    return l_boolRes;	
}

int8_t CGSM::IsSMSPresent() {
    int8_t l_intRes = -1;
    RX_STATE l_intStatus;

    if (LS_FREE != this->GetCommLineStatus()) return l_intRes;
    this->SetCommLineStatus(LS_ATCMD);

    this->sendPGM(_command_AT_CMGL_ALL, true);

    // 5 sec. for initial comm timeout
    // and max. 1500 ms for inter character timeout
    this->RxInit(5000, 1500);
    // wait response is finished
    do {
        if (this->IsStringReceived(_response_OK)) {
            // perfect - we have some response, but what:
            //
            // there is either NO SMS:
            // <CR><LF>OK<CR><LF>
            //
            // or there is at least 1 SMS
            // +CMGL: <index>,<stat>,<oa/da>,,[,<tooa/toda>,<length>]
            // <CR><LF> <data> <CR><LF>OK<CR><LF>
            l_intStatus = RX_FINISHED;
            break; // so finish receiving immediately and let's go to check response
        }
        l_intStatus = IsRxFinished();
    } while (l_intStatus == RX_NOT_FINISHED);

    switch (l_intStatus) {
        case RX_TIMEOUT_ERR:
            // response was not received in specific time
            l_intRes = -2;
            break;
        case RX_FINISHED:
            // something was received but what was received?
            // ---------------------------------------------
            if (this->IsStringReceived(_response_CMGL)) {
                // there is some SMS with status => get its position
                // response is:
                // +CMGL: <index>,<stat>,<oa/da>,,[,<tooa/toda>,<length>]
                // <CR><LF> <data> <CR><LF>OK<CR><LF>
                Serial.println((char *)(this->m_intCommBuffer));
                char* l_ptrPos = strchr((char *)(this->m_intCommBuffer), ':');
                if (NULL != l_ptrPos) l_intRes = atoi(l_ptrPos + 1);
            } else 
                l_intRes = 0;
            // here we have WaitResp() just for generation timeout 20ms in case OK was detected
            // not due to receiving
            this->WaitResponse(20, 20);
            break;
        default:
            l_intRes = -1;
    }
    this->SetCommLineStatus(LS_FREE);
    return l_intRes;	
}

/**********************************************************
Method reads SMS from specified memory(SIM) position

position:     SMS position <1..20>
phone_number: a pointer where the phone number string of received SMS will be placed
              so the space for the phone number string must be reserved - see example
SMS_text  :   a pointer where SMS text will be placed
max_SMS_len:  maximum length of SMS text excluding also string terminating 0x00 character
              
return: 
        ERROR ret. val:
        ---------------
        SMS_LINE_BUSY_ERR - comm. line to the GSM module is not free
        SMS_TIMEOUT_ERR - GSM module didn't answer in timeout
        SMS_BAD_SYNTAX_ERR - specified position must be > 0
        SMS_INTERNAL_ERR - возникла непредвиденная ситуация в ходе работы (например, Sim900 вернул непредвиденный код возврата)

        OK ret val:
        -----------
        SMS_NO_SMS       - no SMS was not found at the specified position
        SMS_UNREAD_SMS   - new SMS was found at the specified position
        SMS_READ_SMS     - already read SMS was found at the specified position
        SMS_OTHER_SMS    - other type of SMS was found 


an example of usage:
        GSM gsm;
        char position;
        char phone_num[20]; // array for the phone number string
        char sms_text[100]; // array for the SMS text string

        position = gsm.IsSMSPresent(SMS_UNREAD);
        if (position) {
          // there is new SMS => read it
          gsm.GetGSM(position, phone_num, sms_text, 100);
          #ifdef DEBUG_PRINT
            gsm.DebugPrint("DEBUG SMS phone number: ", 0);
            gsm.DebugPrint(phone_num, 0);
            gsm.DebugPrint("\r\n          SMS text: ", 0);
            gsm.DebugPrint(sms_text, 1);
          #endif
        }        
**********************************************************/
SMS_TYPE CGSM::GetSMS(uint8_t thePosition, SMS &theSMS) {
    SMS_TYPE l_intRes = SMS_NO_SMS;
    /*
    if (thePosition <= 0) return SMS_BAD_SYNTAX_ERR;
    if (LS_FREE != this->GetCommLineStatus()) return SMS_LINE_BUSY_ERR;
    this->SetCommLineStatus(LS_ATCMD);
    theSMS->m_strSender[0] = 0;  // end of string for now
    l_intRes = SMS_NO_SMS; // still no SMS
  
    // Send "AT+CMGR=X" - where X = position
    this->sendPGM(_command_AT_CMGR, false); // m_objSerial->write("AT+CMGR=");
    this->m_objSerial->write(thePosition);  
    this->m_objSerial->write("\r");
    */
    char* l_ptrStartPos = NULL;
    char* l_ptrEndPos = NULL;

    // 5000 ms for initial comm timeout
    // 100 ms for inter character timeout
    switch (RX_FINISHED_STR_RECV/*this->WaitResponse(5000, 100, _response_CMGR)*/) {
        case RX_TIMEOUT_ERR:
            // Response was not received in specific time
            l_intRes = SMS_TIMEOUT_ERR;
            break;
        case RX_FINISHED_STR_NOT_RECV:
            // OK was received => there is NO SMS stored in this position
            if (this->IsStringReceived(_response_OK)) {
                // There is only response <CR><LF>OK<CR><LF> 
                // => there is NO SMS
                l_intRes = SMS_NO_SMS;
            } else if(IsStringReceived(_response_ERROR)) {
                // Error should not be here but for sure
                l_intRes = SMS_INTERNAL_ERR;
            }
            break;
        case RX_FINISHED_STR_RECV:
            // Find out what was received exactly
            //response for new SMS:
            //<CR><LF>+CMGR: "REC UNREAD","+XXXXXXXXXXXX",,"02/03/18,09:54:28+40"<CR><LF>
		    //There is SMS text<CR><LF>OK<CR><LF>
            if (this->IsStringReceived(_sms_state_REC_UNREAD)) 
                l_intRes = SMS_UNREAD_SMS;
            else if (this->IsStringReceived(_sms_state_REC_READ))
                l_intRes = SMS_READ_SMS;
            else { l_intRes = SMS_INTERNAL_ERR; break; }
            ////////////////////////////////////////////////////////////////////////////////////
            // Номер телефона      
            //              
            // Найдем первую запятую (после нее начинается номер телефона в кавычках)
            l_ptrStartPos = strchr((char *)(this->m_intCommBuffer), ',');
            if (NULL == l_ptrStartPos) { l_intRes = SMS_INTERNAL_ERR; break; }
            // Пропустим запятую и открывающую кавычку
            // Здесь рискуем выйти за рамки приемного буфера - надо проверить
            if (l_ptrStartPos + 2 > (char*)(&this->m_intCommBuffer[COMM_BUF_LEN])) { l_intRes = SMS_INTERNAL_ERR; break; }
            l_ptrStartPos += 2;
            // Найдем закрывающую кавычку
            l_ptrEndPos = strchr(l_ptrStartPos, '"');
            if (NULL == l_ptrEndPos) { l_intRes = SMS_INTERNAL_ERR; break; }
            if (SMS_FROM_MAXLEN < l_ptrEndPos - l_ptrStartPos) {
                strncpy(theSMS.m_strSender, l_ptrStartPos, SMS_FROM_MAXLEN);
                theSMS.m_strSender[SMS_FROM_MAXLEN] = '\0';
            } else {                
                strncpy(theSMS.m_strSender, l_ptrStartPos, l_ptrEndPos - l_ptrStartPos);
                theSMS.m_strSender[l_ptrEndPos - l_ptrStartPos] = '\0';
            }                
                        
            ////////////////////////////////////////////////////////////////////////////////////
            // Дата/время
            if (l_ptrEndPos + 1 > (char*)(&this->m_intCommBuffer[COMM_BUF_LEN])) { l_intRes = SMS_INTERNAL_ERR; break; }
            l_ptrStartPos = strchr(l_ptrEndPos + 1, ','); 
            if (NULL == l_ptrStartPos) { l_intRes = SMS_INTERNAL_ERR; break; }
                
            if (l_ptrStartPos + 1 > (char*)(&this->m_intCommBuffer[COMM_BUF_LEN])) { l_intRes = SMS_INTERNAL_ERR; break; }
            l_ptrStartPos = strchr(l_ptrStartPos + 1, ',');
            if (NULL == l_ptrStartPos) { l_intRes = SMS_INTERNAL_ERR; break; }
                
            if (l_ptrStartPos + 1 > (char*)(&this->m_intCommBuffer[COMM_BUF_LEN])) { l_intRes = SMS_INTERNAL_ERR; break; }
            l_ptrStartPos = strchr(l_ptrStartPos + 1, '"');
            if (NULL == l_ptrStartPos) { l_intRes = SMS_INTERNAL_ERR; break; }
                
            if (l_ptrStartPos + 1 > (char*)(&this->m_intCommBuffer[COMM_BUF_LEN])) { l_intRes = SMS_INTERNAL_ERR; break; }
            l_ptrStartPos++;    
            l_ptrEndPos = strchr(l_ptrStartPos, '"');
            if (NULL == l_ptrEndPos) { l_intRes = SMS_INTERNAL_ERR; break; }
            if (SMS_DATE_MAXLEN < l_ptrEndPos - l_ptrStartPos) {
                strncpy(theSMS.m_strDate, l_ptrStartPos, SMS_DATE_MAXLEN);
                theSMS.m_strDate[SMS_DATE_MAXLEN] = '\0';
            } else {
                strncpy(theSMS.m_strDate, l_ptrStartPos, l_ptrEndPos - l_ptrStartPos);
                theSMS.m_strDate[l_ptrEndPos - l_ptrStartPos] = '\0';
            }
                
            ////////////////////////////////////////////////////////////////////////////////////
            // Текст сообщения            
            if (l_ptrStartPos + 1 > (char*)(&this->m_intCommBuffer[COMM_BUF_LEN])) { l_intRes = SMS_INTERNAL_ERR; break; }
            l_ptrStartPos = strchr(l_ptrEndPos + 1, '\n'); // find <LF>
            if (NULL == l_ptrStartPos) { l_intRes = SMS_INTERNAL_ERR; break; }
            // Next character after <LF> is the first SMS character
            if (l_ptrStartPos + 1 > (char*)(&this->m_intCommBuffer[COMM_BUF_LEN])) { l_intRes = SMS_INTERNAL_ERR; break; }
            l_ptrStartPos++; // Now we are on the first SMS character
            // Find <CR> as the end of SMS string
            l_ptrEndPos = strchr(l_ptrStartPos, '\r');
            // Если SMS очень длинная - <CR> может и не влезть в буфер. 
            // Поскольку обработка таких команд в OpenHAB не предполагается, выходим из функции с кодом ошибки SMS_INTERNAL_ERR
            if (NULL == l_ptrEndPos) { l_intRes = SMS_INTERNAL_ERR; break; }
            if (SMS_BODY_MAXLEN < l_ptrEndPos - l_ptrStartPos) {
                strncpy(theSMS.m_strBody, l_ptrStartPos, SMS_BODY_MAXLEN);
                theSMS.m_strBody[SMS_BODY_MAXLEN] = '\0';
            } else {
                strncpy(theSMS.m_strBody, l_ptrStartPos, l_ptrEndPos - l_ptrStartPos);
                theSMS.m_strBody[l_ptrEndPos - l_ptrStartPos] = '\0';
            }
            break;
        default:
            l_intRes = SMS_INTERNAL_ERR;
    }

    this->SetCommLineStatus(LS_FREE);
    return l_intRes;
}

/**********************************************************
Method deletes SMS from the specified SMS position

position:     SMS position <1..20>

return: 
        ERROR ret. val:
        ---------------
        -1 - comm. line to the GSM module is not free
        -2 - GSM module didn't answer in timeout
        -3 - position must be > 0

        OK ret val:
        -----------
        0 - SMS was not deleted
        1 - SMS was deleted
**********************************************************/
RETURN_CODE CGSM::DeleteSMS(uint8_t thePosition) {
    RETURN_CODE l_intRes = RETURN_INTERNAL_ERR;

    if (0 == thePosition) return RETURN_BAD_SYNTAX;
    if (LS_FREE != this->GetCommLineStatus()) return RETURN_LINE_BUSY_ERR;
    this->SetCommLineStatus(LS_ATCMD);
  
    //send "AT+CMGD=XY" - where XY = position
    this->sendPGM(_command_AT_CMGD, false); 
    this->m_objSerial->write(thePosition);
    this->m_objSerial->write("\r");

    // 5000 ms for initial comm tmout
    // 50 ms for inter character timeout
    switch (this->WaitResponse(5000, 50, _response_OK)) {
        case RX_TIMEOUT_ERR:
            // response was not received in specific time
            l_intRes = RETURN_TIMEOUT_ERR;
            break;
        case RX_FINISHED_STR_RECV:
            // OK was received => SMS deleted
            l_intRes = RETURN_SUCCESS;
            break;
        case RX_FINISHED_STR_NOT_RECV:
            // other response: e.g. ERROR => SMS was not deleted
            l_intRes = RETURN_INTERNAL_ERR; 
            break;
        default:
            l_intRes = RETURN_INTERNAL_ERR;
            break;
    }
    this->SetCommLineStatus(LS_FREE);
    return l_intRes;
}


/******************	****************************************
Function to enable or disable echo
Echo(true)   enable echo mode
Echo(false)   disable echo mode
**********************************************************/
void CGSM::Echo(boolean theState) {
    this->SetCommLineStatus(LS_ATCMD);
    if (theState)
        this->sendPGM(_command_ATE1, true);
    else
        this->sendPGM(_command_ATE0, true);
    delay(500);
    this->SetCommLineStatus(LS_FREE);
}


