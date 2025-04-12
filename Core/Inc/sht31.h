#ifndef INC_SHT31_H_
#define INC_SHT31_H_

#include "main.h" // Includes HAL drivers via CubeMX generation

/**
 * @brief Reads Temperature and Humidity from SHT31 Sensor via I2C.
 * @param hi2c Pointer to I2C_HandleTypeDef structure that contains
 * the configuration information for the specified I2C.
 * @param temp_c Pointer to a float variable where the temperature in Celsius will be stored.
 * @param humidity_rh Pointer to a float variable where the relative humidity in %RH will be stored.
 * @retval HAL status (HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT)
 */
HAL_StatusTypeDef SHT31_Read(I2C_HandleTypeDef *hi2c, float *temp_c, float *humidity_rh);

#endif /* INC_SHT31_H_ */
