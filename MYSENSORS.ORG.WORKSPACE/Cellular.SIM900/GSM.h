/*
 * GSM.h
 *
 * Created: 08.07.2015 22:04:16
 *  Author: bug
 */ 

#ifndef GSM_H_
#define GSM_H_

#include <SoftwareSerial.h>

#define GSM_PWR             6 // У GBoard включение питания модуля Sim900 управляется с PD6
#define GSM_RESET           7 // У GBoard сброс модуля Sim900 управляется с PD7

// Длина буфера для приема данных по UART с Sim900
#define COMM_BUF_LEN        200

// Ответы модуля Sim900 на AT-команду
enum AT_RESPONSE {
    AT_RESPONSE_ERR_NO_RESP,    // Ответа не пришло (возврат по тайм-ауту)
    AT_RESPONSE_ERR_DIF_RESP,   // Ответ пришел, но не тот, что ожидался
    AT_RESPONSE_OK              // Пришел ожидаемый ответ
};

// Состояния конечного автомата, обеспечивающего прием данных (ответов) от UART Sim900
enum RX_STATE {
    RX_NOT_FINISHED,            // Not finished yet
    RX_FINISHED,                // Finished, some character was received
    RX_FINISHED_STR_RECV,       // Finished and expected string received
    RX_FINISHED_STR_NOT_RECV,   // Finished, but expected string not received
    RX_TIMEOUT_ERR,             // Finished, no character received
    RX_NOT_STARTED,             // Just initialized, no data received yet
    RX_ALREADY_STARTED          // Just initialized, data received
};

enum LINE_STATUS {
    LS_FREE,    // Sim900 UART communication line is free - not used by the communication and can be used
    LS_ATCMD    // Sim900 UART communication line is used by AT commands, includes also time for response
};

enum INIT_PARAM {
    PARAM_GROUP_0,
    PARAM_GROUP_1    
};

enum RETURN_CODE {
    RETURN_SUCCESS,
    RETURN_INTERNAL_ERR,
    RETURN_BAD_SYNTAX,
    RETURN_LINE_BUSY_ERR,
    RETURN_TIMEOUT_ERR
};

enum SMS_TYPE {
    SMS_NO_SMS,
    SMS_UNREAD_SMS,
    SMS_READ_SMS,
    SMS_OTHER_SMS,
    SMS_BAD_SYNTAX_ERR,
    SMS_LINE_BUSY_ERR,
    SMS_TIMEOUT_ERR,
    SMS_INTERNAL_ERR
/*
    GETSMS_NOT_AUTH_SMS,
    GETSMS_AUTH_SMS,

    GETSMS_LAST_ITEM*/
};


#define SMS_FROM_MAXLEN 16
#define SMS_DATE_MAXLEN 32
#define SMS_BODY_MAXLEN 64
struct SMS {
    char m_strSender[SMS_FROM_MAXLEN + 1];
    char m_strDate[SMS_DATE_MAXLEN + 1];
    char m_strBody[SMS_BODY_MAXLEN + 1];
};

class CGSM {
protected:
    SoftwareSerial *m_objSerial;
    void sendPGM(PGM_P theString, bool theDoCR);    
    
    LINE_STATUS m_intCommLineStatus;
    // Set communication line status
    inline void SetCommLineStatus(LINE_STATUS theNewStatus) { this->m_intCommLineStatus = theNewStatus; };
    // Get communication line status
    inline LINE_STATUS GetCommLineStatus(void) { return this->m_intCommLineStatus; };

#ifdef DEBUG_PRINT
    void DebugPrint(PGM_P theString);
    void DebugPrint(int theNumber);
#endif

public:
    CGSM(void);
    
    uint8_t m_intCommBuffer[COMM_BUF_LEN + 1];  // Буфер, в который конечный автомат приема данных по UART с Sim900 будет сохранять данные
    uint8_t *m_ptrCommBuffer;                   // Указатель на текущий символ в буфере
    uint8_t m_intCommBufferLength;              // Длина принятых данных
    RX_STATE m_intRxState;                      // Текущее состояние конечного автомата приема данных по UART с Sim900
    uint16_t m_intStartTimeout;                 // Максимальный тайм-аут до начала приема данных по UART с Sim900. 
                                                // Если данные так и не поступили - вываливаемся из WaitResponse с кодом RX_TIMEOUT_ERR
    uint16_t m_intMaxIntercharTimeout;          // Максимальный тайм-аут между блоками даных, приходящих по UART с Sim900. 
                                                // Если тайм-аут превышен - считаем, что Sim900 передал все, что хотел и вываливаемся 
                                                // из WaitResponse с кодом RX_FINISHED
    unsigned long m_intLastRxTime;              // Метка времени последних принятых данных. Используется для определения факта тайм-аута

    void RxInit(uint16_t theStartTimeout, uint16_t theInterCharTimeout);
    RX_STATE WaitResponse(uint16_t theStartTimeout, uint16_t theInterCharTimeout);
    RX_STATE WaitResponse(uint16_t theStartTimeout, uint16_t theInterCharTimeout, PGM_P theExpectedResponse);
    boolean IsStringReceived(PGM_P theExpectedResponse);
    RX_STATE IsRxFinished(void);

protected:
    AT_RESPONSE _SendCommandWaitResponse(
        PGM_P theCommand,
        uint16_t theStartTimeout, uint16_t theInterCharTimeout,
        PGM_P theExpectedResponse,
        uint8_t theRetries);
    AT_RESPONSE _SendArbitraryCommandWaitResponse(
        const char* theCommand,
        uint16_t theStartTimeout, uint16_t theInterCharTimeout,
        const char* theExpectedResponse,
        uint8_t* &theData,
        uint8_t theRetries);
public:
    AT_RESPONSE SendCommandWaitResponse(
        PGM_P theCommand,
        uint16_t theStartTimeout, uint16_t theInterCharTimeout,
        PGM_P theExpectedResponse,
        uint8_t theRetries);    
    AT_RESPONSE SendCommandWaitResponse(
        PGM_P theCommand,
        uint16_t theStartTimeout, uint16_t theInterCharTimeout,
        uint8_t theRetries);
        
    AT_RESPONSE SendArbitraryCommandWaitResponse(
        const char* theCommand,
        uint16_t theStartTimeout, uint16_t theInterCharTimeout,
        const char* theExpectedResponse,
        uint8_t* &theData,
        uint8_t theRetries);
    AT_RESPONSE SendArbitraryCommandWaitResponse(
        const char* theCommand,
        uint16_t theStartTimeout, uint16_t theInterCharTimeout,
        uint8_t* &theData,
        uint8_t theRetries);
    
    boolean TurnOn(long theBaudRate);
    void TurnOff();
    
    void InitParam(INIT_PARAM theParamGroup);
    boolean InitSMSMemory();
    // Функция проверяет наличие SMS в памяти SIM-карты. Возвращает:
    //  -1  -   UART в настоящий момент занят
    //  -2  -   превышен тайм-аут для ответа со стороны Sim900
    //  0   -   в настоящий момент SMS отсутствуют
    //  >0  -   порядковый номер первой ячейки памяти SIM-карты, в которой присутствует SMS
    int8_t IsSMSPresent();
	SMS_TYPE GetSMS(uint8_t thePosition, SMS &theSMS);    
	void Echo(boolean theState);
	RETURN_CODE DeleteSMS(uint8_t thePosition);
};


#endif /* GSM_H_ */