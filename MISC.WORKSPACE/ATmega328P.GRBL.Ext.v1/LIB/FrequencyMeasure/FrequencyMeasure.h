#ifndef FreqMeasure_h
#define FreqMeasure_h

class CFrequencyMeasure {
public:
	static void begin(void);
	static uint8_t available(void);
	static uint32_t read(void);
	static float countToFrequency(uint32_t theCount);
	static float countToNanoseconds(uint32_t theCount);
	static void end(void);
};

extern CFrequencyMeasure FreqMeasure;

extern "C" void FreqTimerInit(void);
extern "C" void Capture();
extern "C" float GetFrequency();

#endif

