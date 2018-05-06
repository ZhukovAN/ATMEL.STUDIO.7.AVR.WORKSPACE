/*
 * HD44780.h
 *
 * Created: 06.09.2017 18:31:23
 *  Author: Alexey N. Zhukov
 */ 


#ifndef HD44780_V1_H_
#define HD44780_V1_H_

#include <inttypes.h>
#include "IIC_ultimate.h"

class HD44780 {
	public:
		HD44780(uint8_t theAddr, uint8_t theCols, uint8_t theRows);
		~HD44780();
		void begin();
		static HD44780* g_ptrActiveLCD;
		bool command(uint8_t theCmd, bool the4BitOnlyMode = false);
		bool data(uint8_t theData, bool the4BitOnlyMode = false);
		bool display();
		bool clear();
		bool home();
		bool printAt(uint8_t theCol, uint8_t theRow, char* theData, uint8_t theLength);
	protected:
		uint8_t m_intAddr;
		uint8_t m_intCols;
		uint8_t m_intRows;
		// ����� ������ � ��� ������������ � ������� �������
		uint8_t* m_ptrBuffer;
		uint8_t m_intMaxBufferSize;
		uint8_t m_intBufferSize;
		// �������. ���� 0, ������ ���������� ������ (��� RS ������ ���� ���������� � 1
		uint8_t m_intCmd;		
		uint8_t m_intDisplayControl;
		uint8_t m_intDisplayMode;
		uint8_t m_intDisplayFunction;	
		uint8_t m_intCharSize;	
	private:
		// ������ I2C-�������� ������� (���� �� 0) � ������:
		// � ���������� � ����������� �������
		bool send();
		bool send(IIC_F onSendComplete);
		bool m_bool4BitOnlyMode;
		// Callback, ���������� �� ���������� ��������
		IIC_F m_ptrOnSendComplete;
		volatile bool m_boolSendComplete;
		// ������� ���������������� � ��������� ����� ���������� ��� ����, 
		// ����� ����� ����������� ������ � ����������� LCD-��������. 
		// �� ��������� ��� ��� ���������� ���� I2C-����, �� ��� ������� 
		// �������� � ��������������, ����� ���������� �������� ������ �� ������
		// ����� ����� ��� �� ����������� �������� �� ������, ��������� ������������ ��������
		// ������ ������ - �������� ������ ����� �� I2C �� �����, ���������� � 
		// ������������� ������, ������� ���������������� �������� ����� ���� ��-�� ����, ��� 
		// ������������ ����������� ����� En. ������� ���� ������ � ��� ��������� ��� ����������� 
		// � ����� ��� ����� I2C ��� ��������� �������� ������ �� ��������� ����������������
		// ��� ���� �������� �� �������� ����������������, ������� ���������� I2C �� Di HALT
		// ���� ���������: � ��� ��������� ����������� ������ callback-�������, ������������ 
		// ��������, ������� ���������� ��������. ��� ���������, ��-������, ������� ����� ������, 
		// ������������ �� I2C �� ����� �� ����������, �, ��-������, �������������� ������ ����� 
		// ������ �� ���� ��������� ���������� �������� ������������� ����� ������ "�� ����"
		bool lockI2C();
		bool releaseI2C();
		// �� ������� ������ ������ � getData ������-���� ����� getDataFunc
		friend uint8_t GetData(uint16_t theIdx);
		// ���������� � OnSendComplete
		friend void OnSendComplete();
		// �����, ���������� �� ���������� I2C-��������
		virtual void onSendComplete();
		uint8_t getData(uint16_t theIdx);
};

#endif /* HD44780_V1_H_ */