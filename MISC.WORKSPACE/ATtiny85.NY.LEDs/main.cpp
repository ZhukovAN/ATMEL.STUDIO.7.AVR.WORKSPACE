/*
 * ATtiny85.NY.LEDs.cpp
 *
 * Created: 08.03.2017 13:26:48
 * Author : Alexey N. Zhukov
 */ 

#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
extern "C" {
	#include "Light_WS2812/light_ws2812.h"
};
#include "ws2812_config.h"

#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

#define LEDCOUNT   30       // Половина длины гирлянды. Для симметричного обзора лента складывается вдвое
#define BRIGHTNESS 95       // Max. brightness in %

#define LENGTH     7		// Длина сосульки. Должна быть меньше LEDCOUNT
const uint8_t g_intColors[LENGTH] = { 100, 100, 80, 60, 40, 30, 10 };

cRGB g_objLEDs[LEDCOUNT << 1];

// DEFINITIONS
// Colors are RGB
#define BLACK      0x000000  // LED color BLACK

// Init LED strand, WS2811/WS2912 specific
// These might work for other configurations:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)

void setAllLEDs(uint32_t color);
void H2R_HSBtoRGB(int hue, int sat, int bright, cRGB &colors);

int l_intFirstIdx = 0;
int l_intHue = random() % 360;

void setup() {
	DDRB |= _BV(ws2812_pin);
	setAllLEDs(BLACK);
}

void loop() {
	// "Рисуем" одну сосульку из требуемых двух (зеркальных)  
	for (int i = 0; i < l_intFirstIdx - LENGTH; i++) {
		g_objLEDs[i].r = 0;
		g_objLEDs[i].g = 0;
		g_objLEDs[i].b = 0;
	}
	cRGB l_objTemp;

	for (int j = l_intFirstIdx; j > l_intFirstIdx - LENGTH; j--) {
		if (j < 0)
			break;
		// Предполагаем, что цвет от головы до хвоста убывает
		// равномерно. Длина сосульки - 70% от длины гирлянды
		// Тогда вычислим разность цвета между соседними светодиодами в
		// сосульке
		int l_intValue = (int) (g_intColors[l_intFirstIdx - j]);
		H2R_HSBtoRGB(l_intHue, 100, l_intValue, l_objTemp);
		// l_intValue = 128;
		g_objLEDs[j].r = l_objTemp.r;
		g_objLEDs[j].g = l_objTemp.g;
		g_objLEDs[j].b = l_objTemp.b;

	}
	for (int j = l_intFirstIdx + 1; j < LEDCOUNT; j++) {
		g_objLEDs[j].r = 0;
		g_objLEDs[j].g = 0;
		g_objLEDs[j].b = 0;
	}

	l_intFirstIdx++;
	if (l_intFirstIdx >= LEDCOUNT + LENGTH) {
		l_intFirstIdx = 0;
		l_intHue = random() % 360;
	}


	for (int i = 0; i < LEDCOUNT; i++) {
		int j = LEDCOUNT * 2 - i - 1;
		g_objLEDs[j].r = g_objLEDs[i].r;
		g_objLEDs[j].g = g_objLEDs[i].g;
		g_objLEDs[j].b = g_objLEDs[i].b;
	}
	ws2812_sendarray((uint8_t *)g_objLEDs, LEDCOUNT * 3 * 2);
	_delay_ms(50);
}

// Sets the color of all LEDs in the strand to 'theColor'
// If 'theWait' > 0 then it will show a swipe from start to end
void setAllLEDs(uint32_t theColor) {
	for (int i = 0; i < LEDCOUNT; i++) {
		g_objLEDs[i].r = 0;
		g_objLEDs[i].g = 0;
		g_objLEDs[i].b = 0;
	}
	ws2812_sendarray((uint8_t *)g_objLEDs, LEDCOUNT * 3 * 2);
}

#define H2R_MAX_RGB_val 255.0

/* HSB to RGB conversion function
 INPUT PARAMETERS: Hue values should range from 0 to 360, Saturation and Brightness values should range from 0 to 100
 Colors pointer should resolve to an array with 3 elements
 RGB values are passed back using via the array. Each value will range between 0 and 255
 */
void H2R_HSBtoRGB(int hue, int sat, int bright, cRGB &colors) {

	// constrain all input variables to expected range
	hue = constrain(hue, 0, 360);
	sat = constrain(sat, 0, 100);
	bright = constrain(bright, 0, 100);

	// define maximum value for RGB array elements
	float max_rgb_val = H2R_MAX_RGB_val;

	// convert saturation and brightness value to decimals and init r, g, b variables
	float sat_f = float(sat) / 100.0;
	float bright_f = float(bright) / 100.0;
	int r, g, b;

	// If brightness is 0 then color is black (achromatic)
	// therefore, R, G and B values will all equal to 0
	if (bright <= 0) {
		colors.r = 0;
		colors.g = 0;
		colors.b = 0;
	}

	// If saturation is 0 then color is gray (achromatic)
	// therefore, R, G and B values will all equal the current brightness
	if (sat <= 0) {
		colors.r = bright_f * max_rgb_val;
		colors.g = bright_f * max_rgb_val;
		colors.b = bright_f * max_rgb_val;
	}

	// if saturation and brightness are greater than 0 then calculate
	// R, G and B values based on the current hue and brightness
	else {

		if (hue >= 0 && hue < 120) {
			float hue_primary = 1.0 - (float(hue) / 120.0);
			float hue_secondary = float(hue) / 120.0;
			float sat_primary = (1.0 - hue_primary) * (1.0 - sat_f);
			float sat_secondary = (1.0 - hue_secondary) * (1.0 - sat_f);
			float sat_tertiary = 1.0 - sat_f;
			r = (bright_f * max_rgb_val) * (hue_primary + sat_primary);
			g = (bright_f * max_rgb_val) * (hue_secondary + sat_secondary);
			b = (bright_f * max_rgb_val) * sat_tertiary;
		}

		else if (hue >= 120 && hue < 240) {
			float hue_primary = 1.0 - ((float(hue) - 120.0) / 120.0);
			float hue_secondary = (float(hue) - 120.0) / 120.0;
			float sat_primary = (1.0 - hue_primary) * (1.0 - sat_f);
			float sat_secondary = (1.0 - hue_secondary) * (1.0 - sat_f);
			float sat_tertiary = 1.0 - sat_f;
			r = (bright_f * max_rgb_val) * sat_tertiary;
			g = (bright_f * max_rgb_val) * (hue_primary + sat_primary);
			b = (bright_f * max_rgb_val) * (hue_secondary + sat_secondary);
		}

		else if (hue >= 240 && hue <= 360) {
			float hue_primary = 1.0 - ((float(hue) - 240.0) / 120.0);
			float hue_secondary = (float(hue) - 240.0) / 120.0;
			float sat_primary = (1.0 - hue_primary) * (1.0 - sat_f);
			float sat_secondary = (1.0 - hue_secondary) * (1.0 - sat_f);
			float sat_tertiary = 1.0 - sat_f;
			r = (bright_f * max_rgb_val) * (hue_secondary + sat_secondary);
			g = (bright_f * max_rgb_val) * sat_tertiary;
			b = (bright_f * max_rgb_val) * (hue_primary + sat_primary);
		}

		colors.r = r;
		colors.g = g;
		colors.b = b;
	}
}

int main(void)
{
	setup();
	/* Replace with your application code */
	while (1)
	{
		loop();
	}
}
