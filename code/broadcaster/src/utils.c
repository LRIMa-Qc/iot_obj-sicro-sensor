/**
 * utils.c
 * 
 * Utility functions
 * 
 * Author: Nils Lahaye (2024)
 * 
*/
#include "utils.h"

float mapRange(float value, float inMin, float inMax, float outMin, float outMax) {
    return fmaxf(fminf((value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin, outMax), outMin);
}

float evaluate_polynomial(float x, const int coefficients[3]) {
    return coefficients[0] + coefficients[1] * x + coefficients[2] * x * x;
}

int floatSeparator(float *val, uint8_t* whole, uint8_t* decimal) {
	*whole = (int)*val;
	*decimal = (int)((*val - *whole) * 100);

    if (*decimal < 0) *decimal *= -1;  // Make the decimal part positive

    return 0;
}