/***************************************************************************
 * mseedformat.h:
 *
 * Documentation and helpers for miniSEED structures.
 ***************************************************************************/

#ifndef MSEEDFORMAT_H
#define MSEEDFORMAT_H 1

#include <stdint.h>

/***************************************************************************
 * miniSEED 2.4 Fixed Section of Data Header
 * 48 bytes total
 *
 * FIELD               TYPE       OFFSET
 * sequence_number     char[6]       0
 * dataquality         char          6
 * reserved            char          7
 * station             char[5]       8
 * location            char[2]      13
 * channel             char[3]      15
 * network             char[2]      18
 * year                uint16_t     20
 * day                 uint16_t     22
 * hour                uint8_t      24
 * min                 uint8_t      25
 * sec                 uint8_t      26
 * unused              uint8_t      27
 * fract               uint16_t     28
 * numsamples          uint16_t     30
 * samprate_fact       int16_t      32
 * samprate_mult       int16_t      34
 * act_flags           uint8_t      36
 * io_flags            uint8_t      37
 * dq_flags            uint8_t      38
 * numblockettes       uint8_t      39
 * time_correct        int32_t      40
 * data_offset         uint16_t     44
 * blockette_offset    uint16_t     46
 *
 * Convenience macros for accessing the fields with typing follow:
 ***************************************************************************/
#define MS2FSDH_SEQNUM(BUFFER)          (char*)BUFFER
#define MS2FSDH_DATAQUALTIY(BUFFER)     (char)*((uint8_t)BUFFER+6)
#define MS2FSDH_RESERVED(BUFFER)        (char)*((uint8_t)BUFFER+7)
#define MS2FSDH_STATION(BUFFER)         (char*)((uint8_t)BUFFER+8)
#define MS2FSDH_LOCATION(BUFFER)        (char*)((uint8_t)BUFFER+13)
#define MS2FSDH_CHANNEL(BUFFER)         (char*)((uint8_t)BUFFER+15)
#define MS2FSDH_NETWORK(BUFFER)         (char*)((uint8_t)BUFFER+18)
#define MS2FSDH_YEAR(BUFFER)            (uint16_t)*((uint8_t)BUFFER+20)
#define MS2FSDH_DAY(BUFFER)             (uint16_t)*((uint8_t)BUFFER+22)
#define MS2FSDH_HOUR(BUFFER)            (uint8_t)*((uint8_t)BUFFER+24)
#define MS2FSDH_MIN(BUFFER)             (uint8_t)*((uint8_t)BUFFER+25)
#define MS2FSDH_SEC(BUFFER)             (uint8_t)*((uint8_t)BUFFER+26)
#define MS2FSDH_UNUSED(BUFFER)          (uint8_t)*((uint8_t)BUFFER+27)
#define MS2FSDH_FSEC(BUFFER)            (uint16_t)*((uint8_t)BUFFER+28)
#define MS2FSDH_NUMSAMPLES(BUFFER)      (uint16_t)*((uint8_t)BUFFER+30)
#define MS2FSDH_SAMPLERATEFACT(BUFFER)  (int16_t)*((uint8_t)BUFFER+32)
#define MS2FSDH_SAMPLERATEMULT(BUFFER)  (int16_t)*((uint8_t)BUFFER+34)
#define MS2FSDH_ACTFLAGS(BUFFER)        (uint8_t)*((uint8_t)BUFFER+36)
#define MS2FSDH_IOFLAGS(BUFFER)         (uint8_t)*((uint8_t)BUFFER+37)
#define MS2FSDH_DQFLAGS(BUFFER)         (uint8_t)*((uint8_t)BUFFER+38)
#define MS2FSDH_NUMBLOCKETTES(BUFFER)   (uint8_t)*((uint8_t)BUFFER+39)
#define MS2FSDH_TIMECORRECT(BUFFER)     (int32_t)*((uint8_t)BUFFER+40)
#define MS2FSDH_DATAOFFSET(BUFFER)      (uint16_t)*((uint8_t)BUFFER+44)
#define MS2FSDH_BLOCKETTEOFFSET(BUFFER) (uint16_t)*((uint8_t)BUFFER+46)

/***************************************************************************
 * miniSEED 2.4 Blockette 100 - sample rate
 *
 * FIELD               TYPE       OFFSET
 * type                uint16_t      0
 * next offset         uint16_t      2
 * samprate            float         4
 * flags               uint8_t       8
 * reserved            uint8_t[3]    9
 *
 * Convenience macros for accessing the fields with typing follow:
 ***************************************************************************/
#define MS2B100_TYPE(BUFFER)            (uint16_t)*(BUFFER)
#define MS2B100_NEXT(BUFFER)            (uint16_t)*((uint8_t)BUFFER+2)
#define MS2B100_SAMPRATE(BUFFER)        (float)*((uint8_t)BUFFER+4)
#define MS2B100_FLAGS(BUFFER)           (uint8_t)*((uint8_t)BUFFER+8)
#define MS2B100_RESERVED(BUFFER)        (uint8_t)*((uint8_t)BUFFER+9)

/***************************************************************************
 * miniSEED 2.4 Blockette 200 - generic event detection
 *
 * FIELD               TYPE       OFFSET
 * type                uint16_t      0
 * next offset         uint16_t      2
 * amplitude           float         4
 * period              float         8
 * background_est      float        12
 * flags               uint8_t      16
 * reserved            uint8_t      17
 * year                uint16_t     18
 * day                 uint16_t     20
 * hour                uint8_t      22
 * min                 uint8_t      23
 * sec                 uint8_t      24
 * unused              uint8_t      25
 * fract               uint16_t     26
 * detector            char[24]     28
 *
 * Convenience macros for accessing the fields with typing follow:
 ***************************************************************************/
#define MS2B200_TYPE(BUFFER)            (uint16_t)*(BUFFER)
#define MS2B200_NEXT(BUFFER)            (uint16_t)*((uint8_t)BUFFER+2)
#define MS2B200_AMPLITUDE(BUFFER)       (float)*((uint8_t)BUFFER+4)
#define MS2B200_PERIOD(BUFFER)          (float)*((uint8_t)BUFFER+8)
#define MS2B200_BACKGROUNDEST(BUFFER)   (float)*((uint8_t)BUFFER+12)
#define MS2B200_FLAGS(BUFFER)           (uint8_t)*((uint8_t)BUFFER+16)
#define MS2B200_RESERVED(BUFFER)        (uint8_t)*((uint8_t)BUFFER+17)
#define MS2B200_YEAR(BUFFER)            (uint16_t)*((uint8_t)BUFFER+18)
#define MS2B200_DAY(BUFFER)             (uint16_t)*((uint8_t)BUFFER+20)
#define MS2B200_HOUR(BUFFER)            (uint8_t)*((uint8_t)BUFFER+22)
#define MS2B200_MIN(BUFFER)             (uint8_t)*((uint8_t)BUFFER+23)
#define MS2B200_SEC(BUFFER)             (uint8_t)*((uint8_t)BUFFER+24)
#define MS2B200_UNUSED(BUFFER)          (uint8_t)*((uint8_t)BUFFER+25)
#define MS2B200_FSEC(BUFFER)            (uint16_t)*((uint8_t)BUFFER+26)
#define MS2B200_DETECTOR(BUFFER)        (char*)((uint8_t)BUFFER+28)

/***************************************************************************
 * miniSEED 2.4 Blockette 201 - Murdock event detection
 *
 * FIELD               TYPE       OFFSET
 * type                uint16_t      0
 * next offset         uint16_t      2
 * amplitude           float         4
 * period              float         8
 * background_est      float        12
 * flags               uint8_t      16
 * reserved            uint8_t      17
 * year                uint16_t     18
 * day                 uint16_t     20
 * hour                uint8_t      22
 * min                 uint8_t      23
 * sec                 uint8_t      24
 * unused              uint8_t      25
 * fract               uint16_t     26
 * snr_values          uint8_t[6]   28
 * loopback            uint8_t      34
 * pick_algorithm      uint8_t      35
 * detector            char[24]     36
 *
 * Convenience macros for accessing the fields with typing follow:
 ***************************************************************************/
#define MS2B201_TYPE(BUFFER)            (uint16_t)*(BUFFER)
#define MS2B201_NEXT(BUFFER)            (uint16_t)*((uint8_t)BUFFER+2)
#define MS2B201_AMPLITUDE(BUFFER)       (float)*((uint8_t)BUFFER+4)
#define MS2B201_PERIOD(BUFFER)          (float)*((uint8_t)BUFFER+8)
#define MS2B201_BACKGROUNDEST(BUFFER)   (float)*((uint8_t)BUFFER+12)
#define MS2B201_FLAGS(BUFFER)           (uint8_t)*((uint8_t)BUFFER+16)
#define MS2B201_RESERVED(BUFFER)        (uint8_t)*((uint8_t)BUFFER+17)
#define MS2B201_YEAR(BUFFER)            (uint16_t)*((uint8_t)BUFFER+18)
#define MS2B201_DAY(BUFFER)             (uint16_t)*((uint8_t)BUFFER+20)
#define MS2B201_HOUR(BUFFER)            (uint8_t)*((uint8_t)BUFFER+22)
#define MS2B201_MIN(BUFFER)             (uint8_t)*((uint8_t)BUFFER+23)
#define MS2B201_SEC(BUFFER)             (uint8_t)*((uint8_t)BUFFER+24)
#define MS2B201_UNUSED(BUFFER)          (uint8_t)*((uint8_t)BUFFER+25)
#define MS2B201_FSEC(BUFFER)            (uint16_t)*((uint8_t)BUFFER+26)
#define MS2B201_SNRVALUES(BUFFER)       (uint8_t)*((uint8_t)BUFFER+28)
#define MS2B201_LOOPBACK(BUFFER)        (uint8_t)*((uint8_t)BUFFER+34)
#define MS2B201_PICKALGORITHM(BUFFER)   (uint8_t)*((uint8_t)BUFFER+35)
#define MS2B201_DETECTOR(BUFFER)        (char*)((uint8_t)BUFFER+36)

/***************************************************************************
 * miniSEED 2.4 Blockette 300 - step calibration
 *
 * FIELD               TYPE       OFFSET
 * type                uint16_t      0
 * next offset         uint16_t      2
 * year                uint16_t      4
 * day                 uint16_t      6
 * hour                uint8_t       8
 * min                 uint8_t       9
 * sec                 uint8_t      10
 * unused              uint8_t      11
 * fract               uint16_t     12
 * num calibrations    uint8_t      14
 * flags               uint8_t      15
 * step duration       uint32_t     16
 * interval duration   uint32_t     20
 * amplitude           float        24
 * input channel       char[3]      28
 * reserved            uint8_t      31
 * reference amplitude uint32_t     32
 * coupling            char[12]     36
 * rolloff             char[12]     48
 *
 * Convenience macros for accessing the fields with typing follow:
 ***************************************************************************/
#define MS2B300_TYPE(BUFFER)            (uint16_t)*(BUFFER)
#define MS2B300_NEXT(BUFFER)            (uint16_t)*((uint8_t)BUFFER+2)
#define MS2B300_YEAR(BUFFER)            (uint16_t)*((uint8_t)BUFFER+4)
#define MS2B300_DAY(BUFFER)             (uint16_t)*((uint8_t)BUFFER+6)
#define MS2B300_HOUR(BUFFER)            (uint8_t)*((uint8_t)BUFFER+8)
#define MS2B300_MIN(BUFFER)             (uint8_t)*((uint8_t)BUFFER+9)
#define MS2B300_SEC(BUFFER)             (uint8_t)*((uint8_t)BUFFER+10)
#define MS2B300_UNUSED(BUFFER)          (uint8_t)*((uint8_t)BUFFER+11)
#define MS2B300_FSEC(BUFFER)            (uint16_t)*((uint8_t)BUFFER+12)
#define MS2B300_NUMCALIBRATIONS(BUFFER) (uint8_t)*((uint8_t)BUFFER+14)
#define MS2B300_FLAGS(BUFFER)           (uint8_t)*((uint8_t)BUFFER+15)
#define MS2B300_STEPDURATION(BUFFER)    (uint32_t)*((uint8_t)BUFFER+16)
#define MS2B300_INTERVALDURATION(BUFFER) (uint32_t)*((uint8_t)BUFFER+20)
#define MS2B300_AMPLITUDE(BUFFER)       (float)*((uint8_t)BUFFER+24)
#define MS2B300_INPUTCHANNEL(BUFFER)    (char *)((uint8_t)BUFFER+28)
#define MS2B300_RESERVED(BUFFER)        (uint8_t)*((uint8_t)BUFFER+31)
#define MS2B300_REFERENCEAMPLITUDE(BUFFER) (uint32_t)*((uint8_t)BUFFER+32)
#define MS2B300_COUPLING(BUFFER)        (char*)((uint8_t)BUFFER+36)
#define MS2B300_ROLLOFF(BUFFER)         (char*)((uint8_t)BUFFER+48)

/***************************************************************************
 * miniSEED 2.4 Blockette 310 - sine calibration
 *
 * FIELD               TYPE       OFFSET
 * type                uint16_t      0
 * next offset         uint16_t      2
 * year                uint16_t      4
 * day                 uint16_t      6
 * hour                uint8_t       8
 * min                 uint8_t       9
 * sec                 uint8_t      10
 * unused              uint8_t      11
 * fract               uint16_t     12
 * reserved1           uint8_t      14
 * flags               uint8_t      15
 * duration            uint32_t     16
 * period              float        20
 * amplitude           float        24
 * input channel       char[3]      28
 * reserved2           uint8_t      31
 * reference amplitude uint32_t     32
 * coupling            char[12]     36
 * rolloff             char[12]     48
 *
 * Convenience macros for accessing the fields with typing follow:
 ***************************************************************************/
#define MS2B310_TYPE(BUFFER)            (uint16_t)*(BUFFER)
#define MS2B310_NEXT(BUFFER)            (uint16_t)*((uint8_t)BUFFER+2)
#define MS2B310_YEAR(BUFFER)            (uint16_t)*((uint8_t)BUFFER+4)
#define MS2B310_DAY(BUFFER)             (uint16_t)*((uint8_t)BUFFER+6)
#define MS2B310_HOUR(BUFFER)            (uint8_t)*((uint8_t)BUFFER+8)
#define MS2B310_MIN(BUFFER)             (uint8_t)*((uint8_t)BUFFER+9)
#define MS2B310_SEC(BUFFER)             (uint8_t)*((uint8_t)BUFFER+10)
#define MS2B310_UNUSED(BUFFER)          (uint8_t)*((uint8_t)BUFFER+11)
#define MS2B310_FSEC(BUFFER)            (uint16_t)*((uint8_t)BUFFER+12)
#define MS2B310_RESERVED1(BUFFER)       (uint8_t)*((uint8_t)BUFFER+14)
#define MS2B310_FLAGS(BUFFER)           (uint8_t)*((uint8_t)BUFFER+15)
#define MS2B310_DURATION(BUFFER)        (uint32_t)*((uint8_t)BUFFER+16)
#define MS2B310_PERIOD(BUFFER)          (float)*((uint8_t)BUFFER+20)
#define MS2B310_AMPLITUDE(BUFFER)       (float)*((uint8_t)BUFFER+24)
#define MS2B310_INPUTCHANNEL(BUFFER)    (char *)((uint8_t)BUFFER+28)
#define MS2B310_RESERVED2(BUFFER)       (uint8_t)*((uint8_t)BUFFER+31)
#define MS2B310_REFERENCEAMPLITUDE(BUFFER) (uint32_t)*((uint8_t)BUFFER+32)
#define MS2B310_COUPLING(BUFFER)        (char*)((uint8_t)BUFFER+36)
#define MS2B310_ROLLOFF(BUFFER)         (char*)((uint8_t)BUFFER+48)

/***************************************************************************
 * miniSEED 2.4 Blockette 320 - pseudo-random calibration
 *
 * FIELD               TYPE       OFFSET
 * type                uint16_t      0
 * next offset         uint16_t      2
 * year                uint16_t      4
 * day                 uint16_t      6
 * hour                uint8_t       8
 * min                 uint8_t       9
 * sec                 uint8_t      10
 * unused              uint8_t      11
 * fract               uint16_t     12
 * reserved1           uint8_t      14
 * flags               uint8_t      15
 * duration            uint32_t     16
 * PtP amplitude       float        20
 * input channel       char[3]      24
 * reserved2           uint8_t      27
 * reference amplitude uint32_t     28
 * coupling            char[12]     32
 * rolloff             char[12]     44
 * noise type          char[8]      56
 *
 * Convenience macros for accessing the fields with typing follow:
 ***************************************************************************/
#define MS2B320_TYPE(BUFFER)            (uint16_t)*(BUFFER)
#define MS2B320_NEXT(BUFFER)            (uint16_t)*((uint8_t)BUFFER+2)
#define MS2B320_YEAR(BUFFER)            (uint16_t)*((uint8_t)BUFFER+4)
#define MS2B320_DAY(BUFFER)             (uint16_t)*((uint8_t)BUFFER+6)
#define MS2B320_HOUR(BUFFER)            (uint8_t)*((uint8_t)BUFFER+8)
#define MS2B320_MIN(BUFFER)             (uint8_t)*((uint8_t)BUFFER+9)
#define MS2B320_SEC(BUFFER)             (uint8_t)*((uint8_t)BUFFER+10)
#define MS2B320_UNUSED(BUFFER)          (uint8_t)*((uint8_t)BUFFER+11)
#define MS2B320_FSEC(BUFFER)            (uint16_t)*((uint8_t)BUFFER+12)
#define MS2B320_RESERVED1(BUFFER)       (uint8_t)*((uint8_t)BUFFER+14)
#define MS2B320_FLAGS(BUFFER)           (uint8_t)*((uint8_t)BUFFER+15)
#define MS2B320_DURATION(BUFFER)        (uint32_t)*((uint8_t)BUFFER+16)
#define MS2B320_PTPAMPLITUDE(BUFFER)    (float)*((uint8_t)BUFFER+20)
#define MS2B320_INPUTCHANNEL(BUFFER)    (char *)((uint8_t)BUFFER+24)
#define MS2B320_RESERVED2(BUFFER)       (uint8_t)*((uint8_t)BUFFER+27)
#define MS2B320_REFERENCEAMPLITUDE(BUFFER) (uint32_t)*((uint8_t)BUFFER+28)
#define MS2B320_COUPLING(BUFFER)        (char*)((uint8_t)BUFFER+32)
#define MS2B320_ROLLOFF(BUFFER)         (char*)((uint8_t)BUFFER+44)
#define MS2B320_NOISETYPE(BUFFER)       (char*)((uint8_t)BUFFER+56)

/***************************************************************************
 * miniSEED 2.4 Blockette 390 - generic calibration
 *
 * FIELD               TYPE       OFFSET
 * type                uint16_t      0
 * next offset         uint16_t      2
 * year                uint16_t      4
 * day                 uint16_t      6
 * hour                uint8_t       8
 * min                 uint8_t       9
 * sec                 uint8_t      10
 * unused              uint8_t      11
 * fract               uint16_t     12
 * reserved1           uint8_t      14
 * flags               uint8_t      15
 * duration            uint32_t     16
 * amplitude           float        20
 * input channel       char[3]      24
 * reserved2           uint8_t      27
 *
 * Convenience macros for accessing the fields with typing follow:
 ***************************************************************************/
#define MS2B390_TYPE(BUFFER)            (uint16_t)*(BUFFER)
#define MS2B390_NEXT(BUFFER)            (uint16_t)*((uint8_t)BUFFER+2)
#define MS2B390_YEAR(BUFFER)            (uint16_t)*((uint8_t)BUFFER+4)
#define MS2B390_DAY(BUFFER)             (uint16_t)*((uint8_t)BUFFER+6)
#define MS2B390_HOUR(BUFFER)            (uint8_t)*((uint8_t)BUFFER+8)
#define MS2B390_MIN(BUFFER)             (uint8_t)*((uint8_t)BUFFER+9)
#define MS2B390_SEC(BUFFER)             (uint8_t)*((uint8_t)BUFFER+10)
#define MS2B390_UNUSED(BUFFER)          (uint8_t)*((uint8_t)BUFFER+11)
#define MS2B390_FSEC(BUFFER)            (uint16_t)*((uint8_t)BUFFER+12)
#define MS2B390_RESERVED1(BUFFER)       (uint8_t)*((uint8_t)BUFFER+14)
#define MS2B390_FLAGS(BUFFER)           (uint8_t)*((uint8_t)BUFFER+15)
#define MS2B390_DURATION(BUFFER)        (uint32_t)*((uint8_t)BUFFER+16)
#define MS2B390_AMPLITUDE(BUFFER)       (float)*((uint8_t)BUFFER+20)
#define MS2B390_INPUTCHANNEL(BUFFER)    (char *)((uint8_t)BUFFER+24)
#define MS2B390_RESERVED2(BUFFER)       (uint8_t)*((uint8_t)BUFFER+27)

/***************************************************************************
 * miniSEED 2.4 Blockette 395 - calibration abort
 *
 * FIELD               TYPE       OFFSET
 * type                uint16_t      0
 * next offset         uint16_t      2
 * year                uint16_t      4
 * day                 uint16_t      6
 * hour                uint8_t       8
 * min                 uint8_t       9
 * sec                 uint8_t      10
 * unused              uint8_t      11
 * fract               uint16_t     12
 * reserved            uint8_t[2]   14
 *
 * Convenience macros for accessing the fields with typing follow:
 ***************************************************************************/
#define MS2B395_TYPE(BUFFER)            (uint16_t)*(BUFFER)
#define MS2B395_NEXT(BUFFER)            (uint16_t)*((uint8_t)BUFFER+2)
#define MS2B395_YEAR(BUFFER)            (uint16_t)*((uint8_t)BUFFER+4)
#define MS2B395_DAY(BUFFER)             (uint16_t)*((uint8_t)BUFFER+6)
#define MS2B395_HOUR(BUFFER)            (uint8_t)*((uint8_t)BUFFER+8)
#define MS2B395_MIN(BUFFER)             (uint8_t)*((uint8_t)BUFFER+9)
#define MS2B395_SEC(BUFFER)             (uint8_t)*((uint8_t)BUFFER+10)
#define MS2B395_UNUSED(BUFFER)          (uint8_t)*((uint8_t)BUFFER+11)
#define MS2B395_FSEC(BUFFER)            (uint16_t)*((uint8_t)BUFFER+12)
#define MS2B395_RESERVED1(BUFFER)       (uint8_t)*((uint8_t)BUFFER+14)

/***************************************************************************
 * miniSEED 2.4 Blockette 400 - beam
 *
 * FIELD               TYPE       OFFSET
 * type                uint16_t      0
 * next offset         uint16_t      2
 * azimuth             float         4
 * slowness            float         8
 * configuration       uint16_t     12
 * reserved            uint8_t[2]   14
 *
 * Convenience macros for accessing the fields with typing follow:
 ***************************************************************************/
#define MS2B400_TYPE(BUFFER)            (uint16_t)*(BUFFER)
#define MS2B400_NEXT(BUFFER)            (uint16_t)*((uint8_t)BUFFER+2)
#define MS2B400_AZIMUTH(BUFFER)         (float)*((uint8_t)BUFFER+4)
#define MS2B400_SLOWNESS(BUFFER)        (float)*((uint8_t)BUFFER+8)
#define MS2B400_CONFIGURATION(BUFFER)   (uint16_t)*((uint8_t)BUFFER+12)
#define MS2B400_RESERVED(BUFFER)        (uint8_t)*((uint8_t)BUFFER+14)

/***************************************************************************
 * miniSEED 2.4 Blockette 405 - beam delay
 *
 * FIELD               TYPE       OFFSET
 * type                uint16_t      0
 * next offset         uint16_t      2
 * delay values        uint16_t[1]   4
 *
 * Convenience macros for accessing the fields with typing follow:
 ***************************************************************************/
#define MS2B405_TYPE(BUFFER)            (uint16_t)*(BUFFER)
#define MS2B405_NEXT(BUFFER)            (uint16_t)*((uint8_t)BUFFER+2)
#define MS2B405_DELAYVALUES(BUFFER)     (uint16_t)*((uint8_t)BUFFER+4)

/***************************************************************************
 * miniSEED 2.4 Blockette 500 - timing
 *
 * FIELD               TYPE       OFFSET
 * type                uint16_t      0
 * next offset         uint16_t      2
 * VCO correction      float         4
 * year                uint16_t      8
 * day                 uint16_t     10
 * hour                uint8_t      12
 * min                 uint8_t      13
 * sec                 uint8_t      14
 * unused              uint8_t      15
 * fract               uint16_t     16
 * microsecond         int8_t       18
 * reception quality   uint8_t      19
 * exception count     uint32_t     20
 * exception type      char[16]     24
 * clock model         char[32]     40
 * clock status        char[128]    72
 *
 * Convenience macros for accessing the fields with typing follow:
 ***************************************************************************/
#define MS2B500_TYPE(BUFFER)            (uint16_t)*(BUFFER)
#define MS2B500_NEXT(BUFFER)            (uint16_t)*((uint8_t)BUFFER+2)
#define MS2B500_VCOCORRECTION(BUFFER)   (float)*((uint8_t)BUFFER+4)
#define MS2B500_YEAR(BUFFER)            (uint16_t)*((uint8_t)BUFFER+8)
#define MS2B500_DAY(BUFFER)             (uint16_t)*((uint8_t)BUFFER+10)
#define MS2B500_HOUR(BUFFER)            (uint8_t)*((uint8_t)BUFFER+12)
#define MS2B500_MIN(BUFFER)             (uint8_t)*((uint8_t)BUFFER+13)
#define MS2B500_SEC(BUFFER)             (uint8_t)*((uint8_t)BUFFER+14)
#define MS2B500_UNUSED(BUFFER)          (uint8_t)*((uint8_t)BUFFER+15)
#define MS2B500_FSEC(BUFFER)            (uint16_t)*((uint8_t)BUFFER+16)
#define MS2B500_MICROSECOND(BUFFER)     (int8_t)*((uint8_t)BUFFER+18)
#define MS2B500_RECEPTIONQUALITY(BUFFER) (uint8_t)*((uint8_t)BUFFER+19)
#define MS2B500_EXCEPTIONCOUNT(BUFFER)  (uint32_t)*((uint8_t)BUFFER+20)
#define MS2B500_EXCEPTIONTYPE(BUFFER)   (char*)((uint8_t)BUFFER+24)
#define MS2B500_CLOCKMODEL(BUFFER)      (char*)((uint8_t)BUFFER+40)
#define MS2B500_CLOCKSTATUS(BUFFER)     (char*)((uint8_t)BUFFER+72)

/***************************************************************************
 * miniSEED 2.4 Blockette 1000 - data only SEED (miniSEED)
 *
 * FIELD               TYPE       OFFSET
 * type                uint16_t      0
 * next offset         uint16_t      2
 * encoding            uint8_t       4
 * byteorder           uint8_t       5
 * reclen              uint8_t       6
 * reserved            uint8_t       7
 *
 * Convenience macros for accessing the fields with typing follow:
 ***************************************************************************/
#define MS2B1000_TYPE(BUFFER)            (uint16_t)*(BUFFER)
#define MS2B1000_NEXT(BUFFER)            (uint16_t)*((uint8_t)BUFFER+2)
#define MS2B1000_ENCODING(BUFFER)        (uint8_t)*((uint8_t)BUFFER+4)
#define MS2B1000_BYTEORDER(BUFFER)       (uint8_t)*((uint8_t)BUFFER+5)
#define MS2B1000_RECLEN(BUFFER)          (uint8_t)*((uint8_t)BUFFER+6)
#define MS2B1000_RESERVED(BUFFER)        (uint8_t)*((uint8_t)BUFFER+7)

/***************************************************************************
 * miniSEED 2.4 Blockette 1001 - data extension
 *
 * FIELD               TYPE       OFFSET
 * type                uint16_t      0
 * next offset         uint16_t      2
 * timing quality      uint8_t       4
 * microsecond         int8_t        5
 * reserved            uint8_t       6
 * frame count         uint8_t       7
 *
 * Convenience macros for accessing the fields with typing follow:
 ***************************************************************************/
#define MS2B1001_TYPE(BUFFER)            (uint16_t)*(BUFFER)
#define MS2B1001_NEXT(BUFFER)            (uint16_t)*((uint8_t)BUFFER+2)
#define MS2B1001_TIMINGQUALITY(BUFFER)   (uint8_t)*((uint8_t)BUFFER+4)
#define MS2B1001_MICROSECOND(BUFFER)     (int8_t)*((uint8_t)BUFFER+5)
#define MS2B1001_RESERVED(BUFFER)        (uint8_t)*((uint8_t)BUFFER+7)
#define MS2B1001_FRAMECOUNT(BUFFER)      (uint8_t)*((uint8_t)BUFFER+6)

/***************************************************************************
 * miniSEED 2.4 Blockette 2000 - opaque data
 *
 * FIELD               TYPE       OFFSET
 * type                uint16_t      0
 * next offset         uint16_t      2
 * length              uint16_t      4
 * data offset         uint16_t      6
 * recnum              uint32_t      8
 * byteorder           uint8_t      12
 * flags               uint8_t      13
 * numheaders          uint8_t      14
 * payload             char[1]      15
 *
 * Convenience macros for accessing the fields with typing follow:
 ***************************************************************************/
#define MS2B2000_TYPE(BUFFER)            (uint16_t)*(BUFFER)
#define MS2B2000_NEXT(BUFFER)            (uint16_t)*((uint8_t)BUFFER+2)
#define MS2B2000_LENGTH(BUFFER)          (uint16_t)*((uint8_t)BUFFER+4)
#define MS2B2000_DATAOFFSET(BUFFER)      (uint16_t)*((uint8_t)BUFFER+6)
#define MS2B2000_RECNUM(BUFFER)          (uint32_t)*((uint8_t)BUFFER+8)
#define MS2B2000_BYTEORDER(BUFFER)       (uint8_t)*((uint8_t)BUFFER+12)
#define MS2B2000_FLAGS(BUFFER)           (uint8_t)*((uint8_t)BUFFER+13)
#define MS2B2000_NUMHEADERS(BUFFER)      (uint8_t)*((uint8_t)BUFFER+14)
#define MS2B2000_PAYLOAD(BUFFER)         (char*)((uint8_t)BUFFER+15)

/* Macro to test a character for miniSEED 2.x data record indicators */
#define MS2_ISDATAINDICATOR(X) (X=='D' || X=='R' || X=='Q' || X=='M')

/* Macro to test for sane year and day values, used primarily to
 * determine if byte order swapping is needed.
 *
 * Year : between 1900 and 2100
 * Day  : between 1 and 366
 *
 * This test is non-unique (non-deterministic) for days 1, 256 and 257
 * in the year 2056 because the swapped values are also within range.
 */
#define MS_ISVALIDYEARDAY(Y,D) (Y >= 1900 && Y <= 2100 && D >= 1 && D <= 366)


#ifdef __cplusplus
}
#endif

#endif
