#include "slog.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define META_FILENAME "meta"

#define ERROR_CHECK(x)        \
  do {                        \
    int __err_rc = (x); \
    if (__err_rc != SLOG_OK) { \
      return __err_rc;        \
    }                         \
  } while (0)

static const uint16_t points_per_interval[] = {
    [ONE_MINUTE] = 1440,
    [FIVE_MINUTES] = 288,
    [TEN_MINUTES] = 144
};

static const uint16_t minutes_per_point[] = {
    [ONE_MINUTE] = 1,
    [FIVE_MINUTES] = 5,
    [TEN_MINUTES] = 10
};

static const uint8_t sample_size_to_bytes[] = {
    [LENGTH_8BITS] = 1,
    [LENGTH_12BITS] = 2, //TODO be smart here
    [LENGTH_16BITS] = 2
};

typedef struct {
  FILE *file;
  uint16_t current_sample;
} slog_aggregate_t;

struct slog_t {
  const char *base_dir;
  char *meta_filename;
  slog_aggregate_t day;
  slog_aggregate_t month;
  slog_aggregate_t year;
  slog_header_t *header;
};

uint16_t slog_bytes_per_sample(slog_partition_t partition, slog_header_t *header) {
  switch (header->metric_type) {
    case COUNTER: {
      switch (partition) {
        case DAY: {
          return 1 + sample_size_to_bytes[header->sample_size_bits];
        }
        case YEAR:
        case MONTH: {
          return 1 + 2 * sample_size_to_bytes[header->sample_size_bits]; //FIXME calculate overflow rate carefully
        }
      }
    }
    case GAUGE: {
      switch (partition) {
        case YEAR:
        case DAY: {
          return 1 + sample_size_to_bytes[header->sample_size_bits];
        }
        case MONTH: { //store sum and count. calculate avg when querying. used for recovery for the year
          return 4 + 2;
        }
      }
    }
    default:
      return 0;
  }
}

int slog_read_header(char *meta_filename, slog_header_t *header) {
  FILE *meta = fopen(meta_filename, "rb");
  if (meta == NULL) {
    //TODO can be caused by other failures. check errno
    return SLOG_EMPTY;
  }
  if (!fread(&header->version, sizeof(uint8_t), 1, meta)) {
    fclose(meta);
    return SLOG_ERR_FAIL;
  }
  if (header->version != SLOG_VERSION) {
    fclose(meta);
    return SLOG_ERR_UNSUPPORTED_VERSION;
  }
  if (!fread(&header->sample_interval, sizeof(uint8_t), 1, meta)) {
    fclose(meta);
    return SLOG_ERR_FAIL;
  }
  if (!fread(&header->metric_type, sizeof(uint8_t), 1, meta)) {
    fclose(meta);
    return SLOG_ERR_FAIL;
  }
  if (!fread(&header->sample_size_bits, sizeof(uint8_t), 1, meta)) {
    fclose(meta);
    return SLOG_ERR_FAIL;
  }
  fclose(meta);
  return SLOG_OK;
}

int slog_write_header(char *meta_filename, slog_header_t *header) {
  FILE *meta = fopen(meta_filename, "wb");
  if (meta == NULL) {
    return SLOG_ERR_FAIL;
  }
  uint8_t buffer[] = {header->version, header->sample_interval, header->metric_type, header->sample_size_bits};
  size_t buffer_length = sizeof(buffer);
  if (fwrite(buffer, sizeof(uint8_t), buffer_length, meta) != buffer_length) {
    fclose(meta);
    return SLOG_ERR_FAIL;
  }
  fclose(meta);
  return SLOG_OK;
}

int slog_append_internal(slog_partition_t partition, uint8_t presence, void *value, slog *handle) {
  switch (partition) {
    case DAY: {
      fwrite(&presence, sizeof(uint8_t), 1, handle->day.file);
      fwrite(value, sizeof(uint8_t), sample_size_to_bytes[handle->header->sample_size_bits], handle->day.file);
      break;
    }
      //FIXME other types
    default:
      return SLOG_ERR_ENUM_NOT_HANDLED;
  }
  return SLOG_OK;
}

int slog_resume_day(struct tm current, slog *handle) {
  size_t day_length = strlen(handle->base_dir) + 1 + 10 + 1;
  char *filename = malloc(sizeof(char) * day_length);
  if (filename == NULL) {
    return SLOG_ERR_NO_MEM;
  }
  sprintf(filename, "%s/%4d.%2d.%2d", handle->base_dir, current.tm_year, current.tm_mon, current.tm_mday);
  handle->day.file = fopen(filename, "a+");
  if (handle->day.file == NULL) {
    free(filename);
    return SLOG_ERR_FAIL;
  }
  free(filename);
  long file_size = ftell(handle->day.file);
  uint16_t bytes_per_sample = slog_bytes_per_sample(DAY, handle->header);
  long rounded_file_size = (file_size / bytes_per_sample) * bytes_per_sample;
  if (rounded_file_size != file_size) { // partial write
    fseek(handle->day.file, rounded_file_size, SEEK_SET);
  }
  handle->day.current_sample = rounded_file_size / bytes_per_sample;
  uint16_t expected_sample = ((current.tm_hour * 60 + current.tm_min) / minutes_per_point[handle->header->sample_interval]);
  long expected_file_size = bytes_per_sample * expected_sample;
  if (expected_file_size < rounded_file_size) { //some kind overflow? override
    fseek(handle->day.file, expected_file_size, SEEK_SET);
  } else {
    // append empty points up till current timestamp
    uint64_t empty_value = 0x00;
    while (handle->day.current_sample <= expected_sample) {
      slog_append_internal(DAY, 0x00, &empty_value, handle);
    }
  }
  return SLOG_OK;
}

int slog_create(struct tm current, const char *base_dir, slog **output) {
  struct slog_t *result = malloc(sizeof(struct slog_t));
  if (result == NULL) {
    return SLOG_ERR_NO_MEM;
  }
  *result = (struct slog_t) {0};
  result->base_dir = base_dir;

  size_t meta_filename_length = strlen(base_dir) + 1 + strlen(META_FILENAME) + 1;
  result->meta_filename = malloc(sizeof(char) * meta_filename_length);
  if (result->meta_filename == NULL) {
    slog_destroy(result);
    return SLOG_ERR_NO_MEM;
  }
  sprintf(result->meta_filename, "%s/%s", base_dir, META_FILENAME);
  result->header = malloc(sizeof(slog_header_t));
  if (result->header == NULL) {
    slog_destroy(result);
    return SLOG_ERR_NO_MEM;
  }
  int code = slog_read_header(result->meta_filename, result->header);
  if (code == SLOG_EMPTY) {
    *output = result;
    return code;
  }
  if (code != SLOG_OK) {
    slog_destroy(result);
    return code;
  }
  code = slog_resume_day(current, result);
  if (code != SLOG_OK) {
    slog_destroy(result);
    return code;
  }

  *output = result;
  return SLOG_OK;
}

int slog_setup(struct tm current, slog_header_t *header, slog *handle) {
  ERROR_CHECK(slog_write_header(handle->meta_filename, header));
  handle->header = header;
  ERROR_CHECK(slog_resume_day(current, handle));
  return SLOG_OK;
}

int slog_append(void *value, slog *handle) {
  if (handle->header == NULL) {
    return SLOG_EMPTY;
  }
  ERROR_CHECK(slog_append_internal(DAY, 0x01, value, handle));
  ERROR_CHECK(slog_append_internal(MONTH, 0x01, value, handle));
  ERROR_CHECK(slog_append_internal(YEAR, 0x01, value, handle));
  return SLOG_OK;
}

int slog_read(slog_partition_t partition, struct tm from, struct tm to, void **result, uint8_t *result_length, slog *handle) {
  if (handle->header == NULL) {
    return SLOG_EMPTY;
  }
  //FIXME implement
  return SLOG_OK;
}

void slog_destroy(slog *handle) {
  if (handle == NULL) {
    return;
  }
  if (handle->meta_filename != NULL) {
    free(handle->meta_filename);
  }
  if (handle->header != NULL) {
    free(handle->header);
  }
  if (handle->day.file != NULL) {
    fclose(handle->day.file);
  }
  free(handle);
}

