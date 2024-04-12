## About

Data logger focused on storing sensor information. This library can also be used to read the data, so in some places "sensor logger" is also called "database".

## Design

It is very important to know what "sensor logger" IS and what IS NOT. While designing this library the following assumptions were made:

 * Primary use is microcontrollers with SD card attached. It is possible to store in the internal NVM memory, but the application should manage retention on its own.
 * Append-only. Current SD cards have capacity ~32Gb. Very rough estimations for 6 metrics 4 bytes each sampled every 10 minutes gives ~8000 years of data.
 * Cross-platform database. SD card can be inserted into any other POSIX operating system and database can be read from it.
 * Sensor-focused. ADC in the sensor is normally 8, 12 or 16bits. Thus, values can be maximum of 2 bytes.
 * Data is sampled at the very low rate. Currently, these rates are fixed and are: 1min, 5min and 10min.
 * Data is partitioned by day, month and year. This is mostly covering the basic use cases like: retrieve last day, month or year.

## Metrics

Metrics can be of 2 types:

 * GAUGE - the next value doesn't depend on the previous. For example: temperature.
 * COUNTER - the value is incrementing over time. Can overflow. For example: number of bytes sent.

It is very important to understand the difference and select proper type. "GAUGE" and "COUNTER" are stored differently. For example, monthly or yearly aggregates calculate "GAUGE" as simple moving average (SMA).

## File system structure

Sensor logger is storing the data on the file system in the binary files. Each metric should be stored on its own base directory. The sample layout might look like this:

```
/root
  /meta
  /2024/
    /data
    /2024.01/
      /data
      /2024.01.01/
        /data
      /2024.01.02/
        /data
    /2024.02/
      /data
      /2024.02.01/
```

Where ```meta``` file is binary and contains database configuration.

```data``` files are binary and store the actual data.


