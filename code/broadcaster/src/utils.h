/**
 * utils.h
 * 
 * Utility functions
 * 
 * Author: Nils Lahaye (2024)
 * 
*/

#ifndef UTILS_H_
#define UTILS_H_

#include <math.h>
#include <stdint.h>

#define TO_STRING(x) #x
#define LOCATION __FILE__ ":" TO_STRING(__LINE__)

#define LOG_IF_ERR(expr, msg)                                             \
    {                                                                                         \
        int ret = (expr);                                                              \
        if(ret) {                                                                            \
            LOG_ERR("Error %d: " msg " in " LOCATION, ret);      \
        }                                                                                       \
    }

/** Struct definition for the sensors data*/
typedef struct {
	float temp;
	float hum;
	float lum;
	float gnd_temp;
	float gnd_hum;
	float bat;
} sensors_data_t;

/**
 * @brief Map a value from a range to another
 * 
 * @param value Value to map
 * @param inMin Minimum value of the input range
 * @param inMax Maximum value of the input range
 * @param outMin Minimum value of the output range
 * @param outMax Maximum value of the output range
 * 
 * @return The mapped value
*/
float mapRange(float value, float inMin, float inMax, float outMin, float outMax);

/**
 * @brief Evaluate a polynomial
 * 
 * @param x Value to evaluate the polynomial at
 * @param coefficients[3] Array of coefficients of the polynomial
 * 
 * @return The value of the polynomial at x
*/
float evaluate_polynomial(float x, const int coefficients[3]);

/**
 * @brief Separate a float into its whole and decimal parts
 * 
 * @param val Float to separate
 * @param whole Pointer to the whole part
 * @param decimal Pointer to the decimal part
 * 
 * @return 0 if successful, -1 otherwise
*/
int floatSeparator(float *val, uint8_t *whole, uint8_t *decimal);

#endif /* UTILS_H_ */