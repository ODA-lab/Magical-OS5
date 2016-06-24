#ifndef __MIKOOS_TYPES_H
#define __MIKOOS_TYPES_H

typedef enum {
	false = 0,
	true,
} bool;

#define NULL (void *)0

typedef signed char           int8_t;
typedef unsigned char        uint8_t;
typedef short                int16_t;
typedef unsigned short      uint16_t;
typedef long                 int32_t;
typedef unsigned long       uint32_t;
typedef long long            int64_t;
typedef unsigned long long  uint64_t;
typedef unsigned int*      uintptr_t;

typedef int                  ssize_t;
typedef uint32_t             size_t;

#define SECTOR_SIZE 256
#define BLOCK_SIZE (SECTOR_SIZE * 2)
typedef uint16_t sector_t;
typedef union block_data_ {
	sector_t sector[SECTOR_SIZE];
	char data[BLOCK_SIZE];
} block_data_t;


#endif // __MIKOOS_TYPES_H
