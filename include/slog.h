#ifndef SENSOR_LOGGER_H
#define SENSOR_LOGGER_H

#include <stdint.h>
#include <time.h>

#define SLOG_VERSION 1

#define SLOG_OK 0
#define SLOG_EMPTY 1
#define SLOG_ERR_NON_EMPTY 2
#define SLOG_ERR_FAIL 3
#define SLOG_ERR_NO_MEM 4
#define SLOG_ERR_UNSUPPORTED_VERSION 5
#define SLOG_ERR_ENUM_NOT_HANDLED 6

typedef enum {
  COUNTER = 0,
  GAUGE = 1
} slog_metric_type_t;

typedef enum {
  DAY = 0,
  MONTH = 1,
  YEAR = 2
} slog_partition_t;

typedef enum {
  ONE_MINUTE = 0,   // 1440 samples per day
  FIVE_MINUTES = 1, // 288 samples per day
  TEN_MINUTES = 2   // 144 samples per day
} slog_sample_interval_t;

typedef enum {
  LENGTH_8BITS = 0,
  LENGTH_12BITS = 1,
  LENGTH_16BITS = 2
} slog_sample_size_t; // normally correspond to ADC resolution

typedef struct {
  uint8_t version;
  slog_sample_interval_t sample_interval;
  slog_metric_type_t metric_type;
  slog_sample_size_t sample_size_bits;
} slog_header_t;

typedef struct slog_t slog;

/**
 * @brief Initialize database.
 *
 * @param current - used to determine number of missing samples since the last "append" operation. Also used to calculate internally the next sample and partition after "append" operation.
 * @param base_dir - base directory where metric partitions are stored. Each metric should have own root directory.
 * @param result - handle to sensor logger. Used in any other operations.
 * @return
 *      - SLOG_OK - if initialization is success
 *      - SLOG_EMPTY - if base directory is empty and database needs to be set up using "slog_setup" function
 *      - SLOG_FAIL - on any other failure
 */
int slog_create(struct tm current, const char *base_dir, slog **result);

/**
 * @brief Setup database in the empty base directory.
 *
 * @param header - configuration
 * @param handle - handle to the sensor logger
 * @return
 *      - SLOG_OK - if setup is success
 *      - SLOG_NON_EMPTY - if base directory is not empty and already initialized
 *      - SLOG_FAIL - on any other failure
 */
int slog_setup(struct tm current, slog_header_t *header, slog *handle);

/**
 * @brief Append new value to the database. Recalculate monthly and yearly aggregates. Caller should ensure this function is called periodically according to the "slog_sample_interval_t" configuration.
 *
 * @param value - value to append.
 * @param handle - handle to the sensor logger
 * @return
 *      - SLOG_OK - if setup is success
 *      - SLOG_FAIL - on any other failure
 */
int slog_append(void *value, slog *handle);

/**
 * @brief Read values from the database.
 *
 * @param partition - controls the data resolution. More granular resolution gives more data points.
 * @param from - Select from time.
 * @param to - Select to time.
 * @param result - Output values into the pre-allocated array. Data type is determined by the "slog_sample_size_t".
 * @param result_length - Number of data points in the result.
 * @param handle - handle to the sensor logger
 * @return
 *      - SLOG_OK - if setup is success
 *      - SLOG_FAIL - on any other failure
 */
int slog_read(slog_partition_t partition, struct tm from, struct tm to, void **result, uint8_t *result_length, slog *handle);

/**
 * @brief Destroy any internally allocated resources. Handle will become invalid after calling this function.
 *
 * @param handle
 */
void slog_destroy(slog *handle);

#endif //SENSOR_LOGGER_H
