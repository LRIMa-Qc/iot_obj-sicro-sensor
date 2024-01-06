/**
 * utils.h
 * 
 * Utility functions
 * 
 * Author: Nils Lahaye 2023
 * 
*/

#ifndef UTILS_H_
#define UTILS_H_

#include <math.h>
#include <stdint.h>

#define TO_STRING(x) #x
#define LOCATION __FILE__ ":" TO_STRING(__LINE__)

#define RET_IF_ERR(expr, msg)                                             \
    {                                                                                         \
        int ret = (expr);                                                              \
        if(ret) {                                                                            \
            LOG_ERR("Error %d: " msg " in " LOCATION, ret);      \
        }                                                                                       \
    }

#define RETRY_IF_ERR(expr, msg) { \
    int ret = (expr); \
    if(ret) ret = (expr); \
    if(ret) { \
        LOG_ERR("Error %d: " msg " in " LOCATION, ret); \
    } \
}

typedef struct {
	float temp;
	float hum;
	float lum;
	float gnd_temp;
	float gnd_hum;
	float bat;
} sensors_data_t;


/**
 * @brief A temperature/Humidity Sensor (eg. SHT20/AHT20)
 * 
 * @param label Label of the i2c device in the project config
 * @param address Address of the i2c device
 * @param initFuncPtr Function to execute the init commands 
 * @param readFuncPtr Function to execute the read temperature commands
 */
typedef struct
{
    const char *label;
    uint8_t address;
    int (*initFuncPtr)(void);
    int (*readFuncPtr)(float *temperature, float *humidity);
} temp_hum_i2c_device;



float mapRange(float value, float inMin, float inMax, float outMin, float outMax);

float evaluate_polynomial(float x, const int coefficients[3]);

int floatSeparator(float *val, uint8_t *whole, uint8_t *decimal);

#endif /* UTILS_H_ */