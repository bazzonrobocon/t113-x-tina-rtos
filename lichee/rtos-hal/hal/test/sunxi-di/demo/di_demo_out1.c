/*
* This confidential and proprietary software should be used
* under the licensing agreement from Allwinner Technology.

* Copyright (C) 2020-2030 Allwinner Technology Limited
* All rights reserved.

* Author:zhengwanyu <zhengwanyu@allwinnertech.com>

* The entire notice above must be reproduced on all authorised
* copies and copies may only be made to the extent permitted
* by a licensing agreement from Allwinner Technology Limited.
*/

#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <memory.h>
#include <stdlib.h>
#include <hal_mem.h>
#include <hal_cache.h>
#include <hal_cmd.h>

#include "../div3xx_uapi.h"
#include "../div3xx_hal.h"
#include "test_file.h"

//#define OUTPUT_RESULT_ENABLE

extern int IRQ_COUNT;

int buffer_count = 0;
int buffer_size = 0;

char f[] =
{
0x8B, 0x89, 0x81, 0x84, 0x8A, 0x98, 0xAC, 0xBD, 0xBB, 0xB0, 0xA3, 0xA0, 0xA0, 0xA4, 0xA8, 0xA6, 0x9A, 0x91, 0x8F, 0x90, 0x90, 0x90, 0x8E, 0x8B, 0x88, 0x86, 0x86, 0x85, 0x82, 0x82, 0x82, 0x85,
0xA7, 0xAA, 0xAC, 0xAC, 0xA9, 0xAB, 0xB5, 0xBA, 0xB1, 0xA2, 0x97, 0x99, 0x97, 0x96, 0x9D, 0xA6, 0xA7, 0xA1, 0x9F, 0x9A, 0x97, 0x98, 0x97, 0x94, 0x91, 0x8F, 0x8F, 0x96, 0xA1, 0xA8, 0xA3, 0x98,
0x97, 0x97, 0x97, 0x99, 0xA2, 0xB0, 0xBB, 0xBA, 0xB0, 0xA7, 0xA7, 0xAF, 0xB3, 0xB2, 0xB0, 0xAC, 0xA4, 0x9A, 0x9A, 0x96, 0x92, 0x8F, 0x8E, 0x8B, 0x8B, 0x8D, 0x8D, 0x96, 0x9E, 0x9A, 0x8D, 0x85,
0x9F, 0xA3, 0x8F, 0x92, 0x9F, 0xAE, 0xB1, 0xA5, 0x99, 0x95, 0xA3, 0xAF, 0xB8, 0xB5, 0xAA, 0x9D, 0x91, 0x88, 0x83, 0x80, 0x7F, 0x80, 0x7F, 0x7F, 0x81, 0x85, 0x8A, 0x8D, 0x8C, 0x86, 0x81, 0x82,
0xA6, 0xA8, 0xAC, 0xA6, 0xA1, 0x9D, 0x98, 0x94, 0x97, 0x9D, 0x9A, 0xA4, 0xA9, 0xA4, 0x9A, 0x8F, 0x89, 0x85, 0x7D, 0x7D, 0x7F, 0x82, 0x83, 0x82, 0x82, 0x83, 0x87, 0x88, 0x87, 0x83, 0x84, 0x89,
0x95, 0x8B, 0x99, 0x98, 0x95, 0x8C, 0x88, 0x8C, 0x92, 0x95, 0x98, 0x9A, 0x9A, 0x95, 0x8F, 0x8C, 0x8C, 0x8C, 0x8B, 0x88, 0x88, 0x8A, 0x8B, 0x88, 0x86, 0x85, 0x81, 0x89, 0x8C, 0x87, 0x83, 0x84,
0x93, 0x8E, 0x8E, 0x92, 0x90, 0x8A, 0x89, 0x8E, 0x8F, 0x8C, 0x97, 0x96, 0x93, 0x91, 0x8E, 0x8E, 0x8D, 0x8E, 0x91, 0x8E, 0x8D, 0x8D, 0x8D, 0x8B, 0x8D, 0x8F, 0x8E, 0x8F, 0x8E, 0x8C, 0x8D, 0x90,
0xA2, 0xA8, 0xB5, 0xA9, 0x99, 0x8D, 0x8D, 0x95, 0x9D, 0xA1, 0x94, 0x92, 0x92, 0x92, 0x91, 0x90, 0x8E, 0x8C, 0x8E, 0x8F, 0x91, 0x92, 0x8F, 0x8E, 0x93, 0x99, 0x99, 0x91, 0x8B, 0x8E, 0x96, 0x99,
0xA2, 0xA6, 0xA7, 0x98, 0x8A, 0x88, 0x8C, 0x8D, 0x8E, 0x91, 0x85, 0x84, 0x85, 0x87, 0x88, 0x87, 0x86, 0x85, 0x80, 0x86, 0x8D, 0x8D, 0x87, 0x82, 0x87, 0x8E, 0x86, 0x80, 0x7D, 0x7F, 0x7E, 0x79,
0x87, 0x84, 0x80, 0x79, 0x73, 0x73, 0x75, 0x78, 0x79, 0x79, 0x7C, 0x7C, 0x7B, 0x7A, 0x7A, 0x7A, 0x76, 0x72, 0x7B, 0x7D, 0x7F, 0x7C, 0x76, 0x74, 0x74, 0x76, 0x72, 0x72, 0x73, 0x73, 0x72, 0x72,
0x78, 0x77, 0x73, 0x6A, 0x62, 0x61, 0x63, 0x68, 0x6C, 0x70, 0x74, 0x74, 0x70, 0x6D, 0x6D, 0x6E, 0x6C, 0x69, 0x71, 0x6E, 0x6A, 0x6A, 0x6D, 0x6B, 0x66, 0x61, 0x5F, 0x61, 0x64, 0x67, 0x69, 0x69,
0x6A, 0x6A, 0x65, 0x62, 0x5E, 0x5D, 0x5D, 0x5A, 0x59, 0x5A, 0x64, 0x62, 0x5D, 0x58, 0x57, 0x59, 0x5B, 0x5A, 0x64, 0x60, 0x5D, 0x5D, 0x5F, 0x5D, 0x57, 0x51, 0x54, 0x55, 0x58, 0x5B, 0x5C, 0x5D,
0x6C, 0x6C, 0x6D, 0x6D, 0x70, 0x75, 0x76, 0x75, 0x76, 0x78, 0x75, 0x74, 0x6F, 0x6B, 0x6B, 0x6F, 0x71, 0x72, 0x73, 0x76, 0x78, 0x75, 0x70, 0x6D, 0x6D, 0x6E, 0x74, 0x74, 0x73, 0x74, 0x74, 0x75,
0x8C, 0x8C, 0x89, 0x86, 0x84, 0x84, 0x83, 0x85, 0x89, 0x8E, 0x8C, 0x8C, 0x8B, 0x89, 0x89, 0x8D, 0x91, 0x91, 0x85, 0x89, 0x8D, 0x89, 0x84, 0x83, 0x88, 0x8E, 0x92, 0x92, 0x93, 0x94, 0x95, 0x94,
0x8C, 0x8A, 0x95, 0x92, 0x8E, 0x8A, 0x84, 0x7E, 0x7C, 0x7D, 0x78, 0x7A, 0x7B, 0x7A, 0x7A, 0x7D, 0x80, 0x81, 0x83, 0x83, 0x82, 0x82, 0x85, 0x88, 0x8C, 0x8E, 0x8D, 0x90, 0x96, 0x9A, 0x9C, 0x9B,
0x65, 0x63, 0x68, 0x67, 0x6A, 0x6D, 0x6A, 0x64, 0x5D, 0x5B, 0x5D, 0x60, 0x61, 0x5F, 0x5E, 0x61, 0x62, 0x62, 0x5F, 0x5B, 0x59, 0x5B, 0x60, 0x64, 0x65, 0x64, 0x60, 0x62, 0x68, 0x6B, 0x6D, 0x6C,
0x65, 0x64, 0x67, 0x66, 0x65, 0x66, 0x66, 0x62, 0x5F, 0x5E, 0x57, 0x59, 0x59, 0x56, 0x54, 0x56, 0x57, 0x57, 0x5D, 0x5D, 0x5C, 0x5B, 0x5B, 0x5C, 0x5C, 0x5C, 0x5A, 0x5A, 0x59, 0x59, 0x59, 0x58,
0x6B, 0x6A, 0x6B, 0x6F, 0x6F, 0x67, 0x5E, 0x59, 0x57, 0x55, 0x55, 0x59, 0x59, 0x55, 0x57, 0x60, 0x67, 0x68, 0x61, 0x63, 0x67, 0x69, 0x6A, 0x68, 0x64, 0x61, 0x57, 0x5B, 0x5D, 0x5C, 0x5C, 0x5E,
0x6E, 0x6E, 0x60, 0x66, 0x6B, 0x6B, 0x6A, 0x68, 0x66, 0x64, 0x61, 0x64, 0x66, 0x67, 0x6B, 0x6F, 0x6D, 0x66, 0x6F, 0x6C, 0x67, 0x63, 0x5F, 0x5E, 0x5B, 0x5A, 0x63, 0x63, 0x63, 0x60, 0x60, 0x64,
0x7C, 0x7B, 0x7D, 0x7A, 0x73, 0x69, 0x65, 0x68, 0x6C, 0x6E, 0x6F, 0x70, 0x6F, 0x6C, 0x6D, 0x73, 0x75, 0x74, 0x74, 0x73, 0x73, 0x73, 0x76, 0x77, 0x79, 0x79, 0x73, 0x73, 0x73, 0x70, 0x70, 0x74,
0x72, 0x72, 0x66, 0x67, 0x64, 0x5D, 0x57, 0x56, 0x56, 0x56, 0x5D, 0x5F, 0x5E, 0x5A, 0x58, 0x60, 0x6B, 0x71, 0x75, 0x78, 0x7C, 0x82, 0x86, 0x86, 0x82, 0x7F, 0x7F, 0x81, 0x83, 0x83, 0x84, 0x87,
0x5F, 0x60, 0x62, 0x66, 0x67, 0x62, 0x5C, 0x59, 0x5B, 0x5D, 0x5A, 0x5C, 0x5C, 0x59, 0x57, 0x55, 0x55, 0x54, 0x5D, 0x60, 0x68, 0x73, 0x7D, 0x83, 0x84, 0x83, 0x79, 0x7B, 0x7F, 0x80, 0x82, 0x83,
0x5C, 0x5C, 0x63, 0x65, 0x63, 0x5D, 0x56, 0x57, 0x5A, 0x5D, 0x63, 0x60, 0x5E, 0x5F, 0x63, 0x63, 0x5D, 0x57, 0x5C, 0x59, 0x55, 0x55, 0x58, 0x5C, 0x5D, 0x5C, 0x5D, 0x5E, 0x5E, 0x5E, 0x61, 0x64,
0x62, 0x5E, 0x5B, 0x61, 0x67, 0x6A, 0x6C, 0x6B, 0x67, 0x63, 0x5D, 0x58, 0x56, 0x59, 0x60, 0x66, 0x68, 0x69, 0x65, 0x61, 0x5D, 0x5E, 0x64, 0x6A, 0x6D, 0x6F, 0x69, 0x68, 0x67, 0x66, 0x69, 0x6F,
0x6C, 0x67, 0x70, 0x70, 0x6F, 0x70, 0x73, 0x77, 0x74, 0x70, 0x6E, 0x6E, 0x6B, 0x64, 0x59, 0x52, 0x54, 0x58, 0x60, 0x5E, 0x5F, 0x64, 0x6A, 0x6D, 0x6D, 0x6A, 0x64, 0x64, 0x63, 0x63, 0x67, 0x6B,
0x6A, 0x67, 0x74, 0x77, 0x74, 0x6D, 0x6A, 0x6C, 0x69, 0x63, 0x6E, 0x6C, 0x69, 0x63, 0x62, 0x61, 0x5C, 0x54, 0x54, 0x54, 0x5D, 0x65, 0x63, 0x5F, 0x5C, 0x56, 0x53, 0x56, 0x5B, 0x5C, 0x5C, 0x5D,
0x71, 0x6D, 0x65, 0x6D, 0x70, 0x6A, 0x64, 0x64, 0x64, 0x63, 0x5C, 0x5F, 0x60, 0x61, 0x62, 0x64, 0x61, 0x5C, 0x5E, 0x59, 0x59, 0x5C, 0x59, 0x5A, 0x5D, 0x5B, 0x61, 0x60, 0x5D, 0x59, 0x56, 0x55,
0x60, 0x61, 0x5E, 0x6B, 0x76, 0x75, 0x6C, 0x66, 0x62, 0x61, 0x5D, 0x5F, 0x62, 0x61, 0x62, 0x64, 0x63, 0x61, 0x61, 0x5B, 0x5B, 0x5D, 0x5A, 0x5B, 0x5D, 0x5A, 0x56, 0x58, 0x5B, 0x5D, 0x5F, 0x61,
0x68, 0x70, 0x7C, 0x81, 0x83, 0x7C, 0x71, 0x66, 0x60, 0x5D, 0x68, 0x67, 0x65, 0x61, 0x5E, 0x5D, 0x5F, 0x61, 0x5F, 0x59, 0x5B, 0x5F, 0x5E, 0x5E, 0x5E, 0x5A, 0x5D, 0x5D, 0x5E, 0x61, 0x65, 0x68,
0x8A, 0x91, 0x81, 0x7B, 0x73, 0x6D, 0x6A, 0x69, 0x6B, 0x6D, 0x6D, 0x6C, 0x68, 0x64, 0x5F, 0x5E, 0x60, 0x62, 0x67, 0x60, 0x5F, 0x61, 0x61, 0x64, 0x69, 0x68, 0x64, 0x60, 0x5B, 0x5B, 0x5F, 0x66,
0x86, 0x86, 0x7F, 0x79, 0x76, 0x7C, 0x82, 0x86, 0x89, 0x8B, 0x81, 0x80, 0x7F, 0x7C, 0x78, 0x73, 0x73, 0x74, 0x7A, 0x75, 0x78, 0x7B, 0x79, 0x7B, 0x80, 0x80, 0x7C, 0x7B, 0x7D, 0x80, 0x87, 0x8C,
0x9E, 0x9F, 0xA1, 0x9F, 0xA1, 0xA6, 0xA6, 0x9E, 0x9B, 0x9B, 0x9E, 0x9D, 0x9C, 0x9D, 0x9B, 0x98, 0x97, 0x98, 0x99, 0x97, 0x9B, 0x9E, 0x99, 0x99, 0x9F, 0xA0, 0x9B, 0x9C, 0x9E, 0xA1, 0xA1, 0xA0,
0x85, 0x83, 0x82, 0x80, 0x80, 0x80, 0x81, 0x83, 0x85, 0x86, 0x87, 0x88, 0x88, 0x89, 0x89, 0x89, 0x83, 0x81, 0x7F, 0x7E, 0x7E, 0x7F, 0x80, 0x82, 0x84, 0x89, 0x8B, 0x8B, 0x8C, 0x8D, 0x8D, 0x8E,
0x85, 0x86, 0x85, 0x85, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8B, 0x8C, 0x8C, 0x8C, 0x8D, 0x8D, 0x8D, 0x87, 0x88, 0x87, 0x87, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8B, 0x8B, 0x8B, 0x8B, 0x8A, 0x8A, 0x8A,
0x87, 0x87, 0x87, 0x87, 0x88, 0x89, 0x89, 0x8A, 0x8A, 0x8C, 0x8C, 0x8B, 0x8B, 0x8B, 0x8A, 0x8A, 0x86, 0x85, 0x85, 0x86, 0x87, 0x88, 0x88, 0x88, 0x87, 0x86, 0x86, 0x86, 0x86, 0x85, 0x85, 0x85,
0x84, 0x82, 0x83, 0x84, 0x85, 0x86, 0x86, 0x86, 0x85, 0x85, 0x85, 0x86, 0x86, 0x87, 0x87, 0x87, 0x83, 0x83, 0x85, 0x86, 0x87, 0x88, 0x88, 0x87, 0x86, 0x87, 0x88, 0x88, 0x89, 0x8A, 0x8B, 0x8B,
0x82, 0x81, 0x81, 0x82, 0x83, 0x84, 0x84, 0x84, 0x84, 0x84, 0x85, 0x86, 0x86, 0x87, 0x87, 0x87, 0x81, 0x80, 0x80, 0x80, 0x80, 0x81, 0x82, 0x82, 0x83, 0x84, 0x85, 0x85, 0x85, 0x86, 0x87, 0x87,
0x82, 0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x81, 0x82, 0x84, 0x85, 0x85, 0x86, 0x87, 0x88, 0x89, 0x80, 0x81, 0x82, 0x82, 0x81, 0x81, 0x80, 0x7F, 0x80, 0x83, 0x83, 0x84, 0x85, 0x87, 0x88, 0x89,
0x7F, 0x83, 0x84, 0x84, 0x83, 0x82, 0x81, 0x7F, 0x7F, 0x81, 0x82, 0x82, 0x83, 0x85, 0x86, 0x86, 0x7F, 0x85, 0x86, 0x86, 0x86, 0x85, 0x83, 0x81, 0x81, 0x81, 0x82, 0x82, 0x83, 0x83, 0x84, 0x85,
0x84, 0x86, 0x88, 0x89, 0x8A, 0x88, 0x87, 0x85, 0x83, 0x84, 0x85, 0x85, 0x85, 0x85, 0x86, 0x86, 0x88, 0x88, 0x89, 0x8B, 0x8C, 0x8C, 0x8A, 0x88, 0x87, 0x89, 0x89, 0x89, 0x89, 0x89, 0x88, 0x88,
0x7A, 0x7C, 0x7D, 0x7E, 0x7E, 0x7D, 0x7B, 0x79, 0x78, 0x76, 0x76, 0x76, 0x75, 0x74, 0x74, 0x73, 0x7E, 0x7E, 0x7F, 0x7F, 0x7F, 0x7E, 0x7C, 0x7A, 0x78, 0x72, 0x71, 0x71, 0x71, 0x70, 0x70, 0x70,
0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x7A, 0x78, 0x77, 0x75, 0x71, 0x70, 0x70, 0x70, 0x6F, 0x6E, 0x6E, 0x7B, 0x7A, 0x79, 0x79, 0x79, 0x78, 0x77, 0x76, 0x76, 0x76, 0x76, 0x75, 0x75, 0x76, 0x75, 0x75,
0x79, 0x78, 0x77, 0x77, 0x76, 0x75, 0x75, 0x74, 0x74, 0x77, 0x77, 0x77, 0x77, 0x77, 0x76, 0x76, 0x77, 0x76, 0x76, 0x75, 0x74, 0x74, 0x74, 0x74, 0x74, 0x77, 0x77, 0x77, 0x77, 0x77, 0x76, 0x76,
0x79, 0x78, 0x77, 0x76, 0x75, 0x75, 0x75, 0x76, 0x76, 0x73, 0x72, 0x72, 0x72, 0x71, 0x71, 0x71, 0x78, 0x78, 0x76, 0x75, 0x74, 0x74, 0x75, 0x76, 0x76, 0x74, 0x73, 0x74, 0x74, 0x73, 0x73, 0x73,
0x7A, 0x7A, 0x79, 0x79, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x77, 0x77, 0x77, 0x77, 0x7B, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x7A, 0x79, 0x79, 0x78, 0x77, 0x75, 0x75, 0x73, 0x74, 0x74,
0x7A, 0x7A, 0x7A, 0x7A, 0x7B, 0x7B, 0x7B, 0x7B, 0x7B, 0x79, 0x78, 0x76, 0x75, 0x74, 0x73, 0x73, 0x7B, 0x7A, 0x79, 0x79, 0x7A, 0x7B, 0x7C, 0x7C, 0x7D, 0x7B, 0x7A, 0x78, 0x76, 0x76, 0x74, 0x75,
0x7E, 0x79, 0x78, 0x78, 0x78, 0x79, 0x7A, 0x7C, 0x7C, 0x7C, 0x7B, 0x7A, 0x78, 0x77, 0x76, 0x76, 0x7E, 0x77, 0x75, 0x74, 0x74, 0x75, 0x76, 0x78, 0x7A, 0x7B, 0x7A, 0x79, 0x78, 0x77, 0x76, 0x77,
0x77, 0x74, 0x72, 0x70, 0x70, 0x70, 0x71, 0x74, 0x74, 0x75, 0x75, 0x74, 0x73, 0x72, 0x73, 0x73, 0x6F, 0x70, 0x6F, 0x6E, 0x6C, 0x6C, 0x6D, 0x6F, 0x70, 0x6E, 0x6E, 0x6D, 0x6D, 0x6D, 0x6E, 0x6F
};

enum {
	YUV420_PIXEL_FMT = 0,
	YUV422_PIXEL_FMT = 1,
};

struct dma_mem
{
	unsigned int phy;     /* physical address */
	unsigned char *virt;  /* virtual address */
	unsigned long size;   /* ion buffer size */
	int dmafd;            /* ion dmabuf fd */
};

struct di_demo {
	DIDevice di_dev;
	bool hal_debug;
	int ion_fd;
	bool is_single_field;
	bool is_fmd_test;
	unsigned int fmd_test_mode;

	unsigned int dimode;

	unsigned int src_w, src_h;
	unsigned int dst_w, dst_h;

	struct di_rect crop;
	struct di_rect democrop;

	unsigned int src_fmt, dst_fmt;

	struct dma_mem tnrBuffer[3];

	bool fmd_en;
	struct di_tnr_work_mode tnrMode;

	struct dma_mem inFb[3];
	struct dma_mem inFbNf[3];

	struct dma_mem outFb[2];

	const char **src_pic_y;
	const char **src_pic_c;
	const char **src_pic_cr;

	const char **dst_pic;
	char result_dir[50];
};

static inline int getSubSampleFormat(unsigned int fmt)
{
	if (fmt == DI_FORMAT_YUV420_PLANNER
		|| fmt == DI_FORMAT_YUV420_NV21
		|| fmt == DI_FORMAT_YUV420_NV12)
		return YUV420_PIXEL_FMT;
	else if (fmt == DI_FORMAT_YUV422_PLANNER
		|| fmt == DI_FORMAT_YUV422_NV61
		|| fmt == DI_FORMAT_YUV422_NV16)
		return YUV422_PIXEL_FMT;

	return -1;
}

static int dmabuf_alloc(int dma_fd, struct dma_mem *mem, unsigned int size)
{
	int ret = 0;
	void *virt_addr = NULL;

	virt_addr = hal_malloc(size);

	memset(virt_addr, 50, size);
	hal_dcache_clean((unsigned long)virt_addr, size);

	mem->phy = __va_to_pa((unsigned long)virt_addr);
	mem->virt = (unsigned char *)virt_addr;
	mem->size = size;

	return ret;
}

static void ion_free(int ion_fd, struct dma_mem *mem)
{
	hal_free(mem->virt);
}

static void setDiBuffer(struct di_buffer *dibuf, struct dma_mem *mem,
		unsigned char field_type)
{
	dibuf->phyaddr = mem->phy;
	dibuf->field_type = field_type;
}

int initTnrTempMem(struct di_demo *para, unsigned int di_mode)
{
	int i;
	unsigned int dst_size = 0;
	unsigned int temp_num;

	if (getSubSampleFormat(para->dst_fmt) == YUV420_PIXEL_FMT)
		dst_size = para->dst_w * para->dst_h * 3 / 2;
	else
		dst_size = para->dst_w * para->dst_h * 2;

	if (di_mode == DIV3X_MODE_OUT2_TNR)
		temp_num = 3;
	else if (di_mode == DIV3X_MODE_OUT1_TNR)
		temp_num = 2;
	else
		return -1;

	for (i = 0; i < temp_num; i++) {
		if (dmabuf_alloc(para->ion_fd,
			&para->tnrBuffer[i], dst_size) < 0) {
			printf("dmabuf_alloc for tnrBuffer[%d] failed\n", i);
			return -1;
		}
	}

	return 0;
}

int loadFile(struct dma_mem *mem, unsigned int ion_offset,
			const char *path, unsigned int size)
{
	memcpy(mem->virt + ion_offset, f + ion_offset, size);
/*
	void *file_addr;
	int file_fd;

	file_fd = open(path, O_RDWR);
	if (file_fd < 0) {
		printf("open %s err.\n", path);
		return -1;
	}

	file_addr = (void *)mmap((void *)0, size,
				 PROT_READ | PROT_WRITE, MAP_SHARED,
				 file_fd, 0);

	memcpy(mem->virt + ion_offset, file_addr, size);
	munmap(file_addr, size);
	//printf("[loadFile] %s offset=%u size=%u\n", path, ion_offset, size);

	close(file_fd);
*/

	return 0;
}

int getInterlaceData_out2(int ion_fd, struct dma_mem *mem, unsigned int size,
	const char *y_path, unsigned int y_offset, unsigned int y_size,
	const char *cb_path, unsigned int cb_offset, unsigned int cb_size,
	const char *cr_path, unsigned int cr_offset, unsigned int cr_size,
	bool is_fmd_test)
{
	if (dmabuf_alloc(ion_fd, mem, size) < 0) {
		printf("dmabuf_alloc for interlace data failed\n");
		return -1;
	}

	if (loadFile(mem, y_offset, y_path, y_size) < 0) {
		printf("load file for %s failed\n", y_path);
		return -1;
	}

	if (loadFile(mem, cb_offset, cb_path, cb_size) < 0) {
		printf("load file for %s failed\n", cb_path);
		return -1;
	}

	if (cr_path && cr_size
		&& loadFile(mem, cr_offset, cr_path, cr_size) < 0) {
		printf("load file for %s failed\n", cr_path);
		return -1;
	}
	return 0;
}


int getInterlaceData(int ion_fd, struct dma_mem *mem, unsigned int size,
	const char *y_path, unsigned int y_offset, unsigned int y_size,
	const char *cb_path, unsigned int cb_offset, unsigned int cb_size,
	const char *cr_path, unsigned int cr_offset, unsigned int cr_size)
{

	if (dmabuf_alloc(ion_fd, mem, size) < 0) {
		printf("dmabuf_alloc for interlace data failed\n");
		return -1;
	}

	if (loadFile(mem, y_offset, y_path, y_size) < 0) {
		printf("load file for %s failed\n", y_path);
		return -1;
	}

	if (loadFile(mem, cb_offset, cb_path, cb_size) < 0) {
		printf("load file for %s failed\n", cb_path);
		return -1;
	}

	if (cr_path && cr_size
		&& loadFile(mem, cr_offset, cr_path, cr_size) < 0) {
		printf("load file for %s failed\n", cr_path);
		return -1;
	}

	return 0;
}

void releaseInterlaceData(int ion_fd, struct dma_mem *mem)
{
	ion_free(ion_fd, mem);
}

int getDiOutMem(int ion_fd, struct dma_mem *mem, unsigned int size)
{
	return dmabuf_alloc(ion_fd, mem, size);
}

void releaseDiOutMem(int ion_fd, struct dma_mem *mem)
{
	ion_free(ion_fd, mem);
}
void dumpOutputBuffer(struct dma_mem *mem)
{
	for (int i = 0; i < 32; i++)
		printf("0x%02X ", ((u8 *)mem->phy)[i]);
	printf("\n");
}

int setDiOnlyTnrInit(DIDevice *di_dev, struct di_demo *para)
{
	struct init_para_only_tnr inittnr;

	memset(&inittnr, 0, sizeof(inittnr));

	inittnr.debug_en = para->hal_debug;

	inittnr.src_w = para->src_w;
	inittnr.src_h = para->src_h;
	inittnr.dst_w = para->dst_w;
	inittnr.dst_h = para->dst_h;

	memcpy(&inittnr.crop, &para->crop, sizeof(para->crop));
	memcpy(&inittnr.democrop, &para->democrop, sizeof(para->democrop));

	if (diInit(di_dev, para->dimode, (void *)&inittnr) < 0) {
		printf("diInit failed\n");
		return -1;
	}

	return 0;
}

int setDiBobInit(DIDevice *di_dev, struct di_demo *para)
{
	struct init_para_bob initbob;

	memset(&initbob, 0, sizeof(initbob));

	initbob.debug_en = para->hal_debug;

	initbob.src_w = para->src_w;
	initbob.src_h = para->src_h;
	initbob.dst_w = para->dst_w;
	initbob.dst_h = para->dst_h;

	memcpy(&initbob.crop, &para->crop, sizeof(para->crop));

	if (diInit(di_dev, para->dimode, (void *)&initbob) < 0) {
		printf("diInit failed\n");
		return -1;
	}

	return 0;
}

int setDi1OutTnrInit(DIDevice *di_dev, struct di_demo *para)
{
	int i;
	struct init_para_out1_tnr initout1tnr;

	memset(&initout1tnr, 0, sizeof(initout1tnr));
	if (initTnrTempMem(para, para->dimode) < 0) {
		printf("initTnrTempMem failed!\n");
		return -1;
	}

	initout1tnr.debug_en = para->hal_debug;

	initout1tnr.src_w = para->src_w;
	initout1tnr.src_h = para->src_h;
	initout1tnr.dst_w = para->dst_w;
	initout1tnr.dst_h = para->dst_h;

	memcpy(&initout1tnr.crop, &para->crop, sizeof(para->crop));
	memcpy(&initout1tnr.democrop, &para->democrop, sizeof(para->democrop));

	for (i = 0; i < 2; i++)
		setDiBuffer(&initout1tnr.tnrBuffer[i], &para->tnrBuffer[i], 0);

	initout1tnr.tnrFormat = para->dst_fmt;
	initout1tnr.fmd_en = 1;

	if (diInit(di_dev, para->dimode, (void *)&initout1tnr) < 0) {
		printf("diInit failed\n");
		return -1;
	}

    return 0;
}

int setDi1OutInit(DIDevice *di_dev, struct di_demo *para)
{
	struct init_para_out1 initout1;

	memset(&initout1, 0, sizeof(initout1));

	initout1.debug_en = para->hal_debug;

	initout1.src_w = para->src_w;
	initout1.src_h = para->src_h;
	initout1.dst_w = para->dst_w;
	initout1.dst_h = para->dst_h;

	memcpy(&initout1.crop, &para->crop, sizeof(para->crop));

	initout1.fmd_en = 1;

	if (diInit(di_dev, para->dimode, (void *)&initout1) < 0) {
		printf("diInit failed\n");
		return -1;
	}

	return 0;
}

int setDi2OutTnrInit(DIDevice *di_dev, struct di_demo *para)
{
	int i;
	struct init_para_out2_tnr initout2tnr;

	memset(&initout2tnr, 0, sizeof(initout2tnr));
	if (initTnrTempMem(para, DIV3X_MODE_OUT2_TNR) < 0) {
		printf("initTnrTempMem failed!\n");
		return -1;
	}

	initout2tnr.debug_en = para->hal_debug;
	initout2tnr.src_w = para->src_w;
	initout2tnr.src_h = para->src_h;
	initout2tnr.dst_w = para->dst_w;
	initout2tnr.dst_h = para->dst_h;

	memcpy(&initout2tnr.crop, &para->crop, sizeof(para->crop));
	memcpy(&initout2tnr.democrop, &para->democrop, sizeof(para->democrop));

	for (i = 0; i < 3; i++)
		setDiBuffer(&initout2tnr.tnrBuffer[i], &para->tnrBuffer[i], 0);

	initout2tnr.tnrFormat = para->dst_fmt;
	initout2tnr.fmd_en = 1;

	if (diInit(di_dev, DIV3X_MODE_OUT2_TNR, (void *)&initout2tnr) < 0) {
		printf("diInit failed\n");
		return -1;
	}

	return 0;
}

int setDi2OutInit(DIDevice *di_dev, struct di_demo *para)
{
	struct init_para_out2 initout2;

	memset(&initout2, 0, sizeof(initout2));

	initout2.debug_en = para->hal_debug;
	initout2.src_w = para->src_w;
	initout2.src_h = para->src_h;
	initout2.dst_w = para->dst_w;
	initout2.dst_h = para->dst_h;

	memcpy(&initout2.crop, &para->crop, sizeof(para->crop));

	initout2.fmd_en = 1;

	if (diInit(di_dev, DIV3X_MODE_OUT2, (void *)&initout2) < 0) {
		printf("diInit failed\n");
		return -1;
	}

	return 0;
}

int diOut1Init(struct di_demo *demo)
{
    printf("diOut1Init \n");
	if (demo->dimode == DIV3X_MODE_OUT1_TNR) {
		if (setDi1OutTnrInit(&demo->di_dev, demo) < 0) {
			printf("setDi2OutTnrInit failed\n");
			return -1;
		}
	} else if (demo->dimode == DIV3X_MODE_OUT1) {
		if (setDi1OutInit(&demo->di_dev, demo) < 0) {
			printf("setDi2OutInit failed\n");
			return -1;
		}
	} else if (demo->dimode == DIV3X_MODE_ONLY_TNR) {
		if (setDiOnlyTnrInit(&demo->di_dev, demo) < 0) {
			printf("setDiOnlyTnrInit failed\n");
			return -1;
		}
	} else if (demo->dimode == DIV3X_MODE_BOB) {
		if (setDiBobInit(&demo->di_dev, demo) < 0) {
			printf("setDiBobInit failed\n");
			return -1;
		}
	}
    return 0;
}

int diOut2Init(struct di_demo *demo)
{
	if (demo->dimode == DIV3X_MODE_OUT2_TNR) {
		if (setDi2OutTnrInit(&demo->di_dev, demo) < 0) {
			printf("setDi2OutTnrInit failed\n");
			return -1;
		}
	} else if (demo->dimode == DIV3X_MODE_OUT2) {
		if (setDi2OutInit(&demo->di_dev, demo) < 0) {
			printf("setDi2OutInit failed\n");
			return -1;
		}
	} else {
		printf("unsupport format %d, please entry 1 or 2 \n", demo->dimode);
		return -1;
	}

	return 0;
}

int parse_params(struct di_demo *demo, int argc, char **argv)
{
	demo->dimode = atoi(argv[1]);

	demo->src_w = atoi(argv[2]);
	demo->src_h = atoi(argv[3]);
	demo->src_fmt = atoi(argv[4]);

	demo->dst_w = atoi(argv[5]);
	demo->dst_h = atoi(argv[6]);
	demo->dst_fmt = atoi(argv[7]);

	demo->crop.left = atoi(argv[8]);
	demo->crop.top = atoi(argv[9]);
	demo->crop.right = atoi(argv[10]);
	demo->crop.bottom = atoi(argv[11]);

	demo->democrop.left = atoi(argv[12]);
	demo->democrop.top = atoi(argv[13]);
	demo->democrop.right = atoi(argv[14]);
	demo->democrop.bottom = atoi(argv[15]);

	demo->is_single_field = atoi(argv[16]);

	switch (demo->src_fmt) {
	case DI_FORMAT_YUV420_PLANNER:
		if (demo->is_single_field) {
			printf("single field haven't support format:%d\n", demo->src_fmt);
			return -1;
		} else {
			demo->src_pic_y = test_pics_yv12_y;
			demo->src_pic_c = test_pics_yv12_c;
			demo->src_pic_cr = test_pics_yv12_cr;
		}
		break;
	case DI_FORMAT_YUV420_NV21:
		if (demo->is_single_field) {
			demo->src_pic_y = test_pics_nv21_y_sf;
			demo->src_pic_c = test_pics_nv21_c_sf;
		} else if (demo->src_w == 1920 && demo->src_h == 1080) {
			demo->src_pic_y = test_pics_nv21_1080_y;
			demo->src_pic_c = test_pics_nv21_1080_c;
		} else {
			demo->src_pic_y = test_pics_nv21_y;
			demo->src_pic_c = test_pics_nv21_c;
		}
		break;
	case DI_FORMAT_YUV420_NV12:
		printf("haven't support format:%d\n", demo->src_fmt);
		return -1;
	case DI_FORMAT_YUV422_PLANNER:
		if (demo->is_single_field) {
			printf("single field haven't support format:%d\n", demo->src_fmt);
			return -1;
		} else {
			demo->src_pic_y = test_pics_yv16_y;
			demo->src_pic_c = test_pics_yv16_c;
			demo->src_pic_cr = test_pics_yv16_cr;
		}
		break;
	case DI_FORMAT_YUV422_NV61:
		if (demo->is_single_field) {
			demo->src_pic_y = test_pics_nv61_y_sf;
			demo->src_pic_c = test_pics_nv61_c_sf;
		} else {
			demo->src_pic_y = test_pics_nv61_y;
			demo->src_pic_c = test_pics_nv61_c;
		}
		break;

	case DI_FORMAT_YUV422_NV16:
		printf("haven't support format:%d\n", demo->src_fmt);
		return -1;
		break;
	default:
		printf("invalid input format\n");
		return -1;
	}

	switch (demo->dst_fmt) {
	case DI_FORMAT_YUV420_PLANNER:
		demo->dst_pic = output_out1_yv12;
		if (demo->src_w == 1920 && demo->src_h == 1080)
			demo->dst_pic = output_out1_1080_yv12;
		break;
	case DI_FORMAT_YUV420_NV21:
		printf("haven't support format:%d\n", demo->dst_fmt);
		return -1;
	case DI_FORMAT_YUV420_NV12:
		printf("haven't support format:%d\n", demo->dst_fmt);
		return -1;

	case DI_FORMAT_YUV422_PLANNER:
		demo->dst_pic = output_out1_yv16;
		break;
	case DI_FORMAT_YUV422_NV61:
		printf("haven't support format:%d\n", demo->dst_fmt);
		return -1;
	case DI_FORMAT_YUV422_NV16:
		printf("haven't support format:%d\n", demo->dst_fmt);
		return -1;
	default:
		printf("invalid output format\n");
		return -1;
	}

	if (demo->is_single_field)
		sprintf(demo->result_dir, "%s/%s_%s_in_sf_result",
			result_folder,
			getDiModeName(demo->dimode),
			getDiPixelFormatName(demo->src_fmt));
	else
		sprintf(demo->result_dir, "%s/%s_%s_in_result",
			result_folder,
			getDiModeName(demo->dimode),
			getDiPixelFormatName(demo->src_fmt));

	return 0;
}

int parse_params_out2(struct di_demo *demo, int argc, char **argv)
{
	printf("start to parse argv\n");
	demo->dimode = atoi(argv[1]);

	demo->src_w = atoi(argv[2]);
	demo->src_h = atoi(argv[3]);
	demo->src_fmt = atoi(argv[4]);

	demo->dst_w = atoi(argv[5]);
	demo->dst_h = atoi(argv[6]);
	demo->dst_fmt = atoi(argv[7]);

	demo->crop.left = atoi(argv[8]);
	demo->crop.top = atoi(argv[9]);
	demo->crop.right = atoi(argv[10]);
	demo->crop.bottom = atoi(argv[11]);

	demo->democrop.left = atoi(argv[12]);
	demo->democrop.top = atoi(argv[13]);
	demo->democrop.right = atoi(argv[14]);
	demo->democrop.bottom = atoi(argv[15]);

	demo->is_single_field = atoi(argv[16]);
	demo->is_fmd_test = atoi(argv[17]);
	demo->fmd_test_mode = atoi(argv[18]);
	printf("parse argv finish\n");

	switch (demo->src_fmt) {
	case DI_FORMAT_YUV420_PLANNER:
		if (demo->is_single_field) {
			printf("single field haven't support format:%d\n", demo->src_fmt);
			return -1;
		} else {
			demo->src_pic_y = test_pics_yv12_y;
			demo->src_pic_c = test_pics_yv12_c;
			demo->src_pic_cr = test_pics_yv12_cr;
		}
		break;
	case DI_FORMAT_YUV420_NV21:
		if (demo->is_single_field) {
			demo->src_pic_y = test_pics_nv21_y_sf;
			demo->src_pic_c = test_pics_nv21_c_sf;
		} else if (demo->src_w == 1920 && demo->src_h == 1080) {
			demo->src_pic_y = test_pics_nv21_1080_y;
			demo->src_pic_c = test_pics_nv21_1080_c;
		} else if (demo->is_fmd_test) {
			switch (demo->fmd_test_mode) {
			case 0:
				demo->src_pic_y = test_pics_fmd22_nv21;
				demo->src_pic_c = test_pics_fmd22_nv21;
				break;
			case 1:
				demo->src_pic_y = test_pics_fmd2332_nv21;
				demo->src_pic_c = test_pics_fmd2332_nv21;
				break;
			case 2:
				demo->src_pic_y = test_pics_fmd32_nv21;
				demo->src_pic_c = test_pics_fmd32_nv21;
				break;
			}
		} else {
			demo->src_pic_y = test_pics_nv21_y;
			demo->src_pic_c = test_pics_nv21_c;
		}
		break;
	case DI_FORMAT_YUV420_NV12:
		printf("haven't support format:%d\n", demo->src_fmt);
		return -1;
	case DI_FORMAT_YUV422_PLANNER:
		if (demo->is_single_field) {
			printf("single field haven't support format:%d\n", demo->src_fmt);
			return -1;
		} else {
			demo->src_pic_y = test_pics_yv16_y;
			demo->src_pic_c = test_pics_yv16_c;
			demo->src_pic_cr = test_pics_yv16_cr;
		}
		break;
	case DI_FORMAT_YUV422_NV61:
		if (demo->is_single_field) {
			demo->src_pic_y = test_pics_nv61_y_sf;
			demo->src_pic_c = test_pics_nv61_c_sf;
		} else {
			demo->src_pic_y = test_pics_nv61_y;
			demo->src_pic_c = test_pics_nv61_c;
		}
		break;

	case DI_FORMAT_YUV422_NV16:
		printf("haven't support format:%d\n", demo->src_fmt);
		return -1;
		break;
	default:
		printf("invalid input format\n");
		return -1;
	}

	switch (demo->dst_fmt) {
	case DI_FORMAT_YUV420_PLANNER:
		demo->dst_pic = output_out2_yv12;
		if (demo->is_fmd_test)
			demo->dst_pic = NULL;
		else if (demo->src_w == 1920 && demo->src_h == 1080)
			demo->dst_pic = output_out2_1080_yv12;
		break;
	case DI_FORMAT_YUV420_NV21:
		printf("haven't support format:%d\n", demo->dst_fmt);
		return -1;
	case DI_FORMAT_YUV420_NV12:
		printf("haven't support format:%d\n", demo->dst_fmt);
		return -1;

	case DI_FORMAT_YUV422_PLANNER:
		demo->dst_pic = output_out2_yv16;
		break;
	case DI_FORMAT_YUV422_NV61:
		printf("haven't support format:%d\n", demo->dst_fmt);
		return -1;
	case DI_FORMAT_YUV422_NV16:
		printf("haven't support format:%d\n", demo->dst_fmt);
		return -1;
	default:
		printf("invalid output format\n");
		return -1;
	}

	/* complete resource and result path */
	if (demo->is_single_field)
		sprintf(demo->result_dir, "%s/%s_%s_in_sf_result",
			result_folder,
			getDiModeName(demo->dimode),
			getDiPixelFormatName(demo->src_fmt));
	else
		sprintf(demo->result_dir, "%s/%s_%s_in_result",
			result_folder,
			getDiModeName(demo->dimode),
			getDiPixelFormatName(demo->src_fmt));

	return 0;
}


//src_cb_size表示cb分量有多少个
int getSizes(struct di_demo *demo,
	     unsigned int *src_size,
	     unsigned int *src_y_size,
	     unsigned int *src_cb_size,
	     unsigned int *dst_size)
{
	*src_y_size = demo->src_w * demo->src_h;
	switch (demo->src_fmt) {
	case DI_FORMAT_YUV420_PLANNER:
		*src_size = demo->src_w * demo->src_h * 3 / 2;
		*src_cb_size = demo->src_w * demo->src_h / 4;
		break;
	case DI_FORMAT_YUV420_NV21:
	case DI_FORMAT_YUV420_NV12:
		*src_size = demo->src_w * demo->src_h * 3 / 2;
		/*如果按实际的cbcr分量来说，cb和cr的数量确实都是demo->src_w * demo->src_h / 4
		* 但这里除以2是因为NV12/NV21是半平面保存数据，src_cb_size是用来从文件中读取数据长度用的，所以一个文件里面同时包含了cb和cr的数据，所以这里就除以2就行了
		*/
		*src_cb_size = demo->src_w * demo->src_h / 2;
		break;

	case DI_FORMAT_YUV422_PLANNER:
		*src_size = demo->src_w * demo->src_h * 2;
		*src_cb_size = demo->src_w * demo->src_h / 2;
		break;
	case DI_FORMAT_YUV422_NV61:
	case DI_FORMAT_YUV422_NV16:
		*src_size = demo->src_w * demo->src_h * 2;
		/*如果按实际的cbcr分量来说，cb和cr的数量确实都是demo->src_w * demo->src_h / 2
		* 但这里除以2是因为NV12/NV21是半平面保存数据，src_cb_size是用来从文件中读取数据长度用的，所以一个文件里面同时包含了cb和cr的数据，所以这里就不用除
		*/
		*src_cb_size = demo->src_w * demo->src_h;
		break;
	default:
		printf("invalid input format\n");
		return -1;
	}

	switch (demo->dst_fmt) {
	case DI_FORMAT_YUV420_PLANNER:
		*dst_size = demo->dst_w * demo->dst_h * 3 / 2;
		break;
	case DI_FORMAT_YUV420_NV21:
	case DI_FORMAT_YUV420_NV12:
		*dst_size = demo->dst_w * demo->dst_h * 3 / 2;
		break;

	case DI_FORMAT_YUV422_PLANNER:
		*dst_size = demo->dst_w * demo->dst_h * 2;
		break;
	case DI_FORMAT_YUV422_NV61:
	case DI_FORMAT_YUV422_NV16:
		*dst_size = demo->dst_w * demo->dst_h * 2;
		break;
	default:
		printf("invalid input format\n");
		return -1;
	}

	return 0;
}


static int cmd_di300_test(int argc, char **argv)
{
#if 0
	//some test code
#else
    int i;
	struct di_demo demo;
	unsigned int src_size, dst_size;
	unsigned int src_y_size;
	unsigned int src_cb_size;
	unsigned int src_pic_max;
	struct fb_para_out1_tnr fbout1tnr;

	memset(&demo, 0, sizeof(demo));
	memset(&fbout1tnr, 0, sizeof(fbout1tnr));

	if (argc < 17) {
		printf("Invalid params input! you should input the params in the format:\n");
		printf("test_di300_30HZ diMode "
				"srcW srcH srcFormat"
				"dstW dstH dstFormat"
				"cropLeft cropTop cropRight cropBottom "
				"demoCropLeft demoCropTop demoCropRight demoCropBottom isSingleField\n");
		return -1;
	}

	demo.hal_debug = 1;

	if (parse_params(&demo, argc, argv) < 0)
		exit(-1);
	if (getSizes(&demo, &src_size, &src_y_size, &src_cb_size,
			&dst_size) < 0)
		exit(-1);

#ifdef OUTPUT_RESULT_ENABLE
	if(!opendir(demo.result_dir)) {
		if(!opendir(result_folder)) {
			if(mkdir(result_folder, S_IRWXU | S_IRWXG | S_IRWXO)) {
				printf("mkdir directory './result' failed, "
				"maybe you have creat it, please delete it before running that program\n");
				exit(-1);
			}
		}
		if(mkdir(demo.result_dir, S_IRWXU | S_IRWXG | S_IRWXO)) {
			printf("mkdir directory %s failed, "
			"maybe you have creat it, please delete it before running that program\n", demo.result_dir);
			exit(-1);
		}
	}
#endif

	src_pic_max = 12; //test 12 source interlaced pictures totally
	if (demo.src_w == 1920 && demo.src_h == 1080)
		src_pic_max = 6; //we only have 6 pictures for 1080p to test

	if (diOut1Init(&demo) < 0)
		exit(-1);

	if (demo.dimode == DIV3X_MODE_BOB)
	{
		/* just test YV12 input with bottom_field */
		if (demo.src_fmt == DI_FORMAT_YUV420_PLANNER)
			fbout1tnr.base_field = 1;
	}

	if (demo.is_single_field) {
/*
		if (getInterlaceData(demo.ion_fd, &demo.inFb[0], src_size / 2,
				demo.src_pic_y[0], 0, src_y_size / 2,
				demo.src_pic_c[0], src_y_size / 2, src_cb_size / 2,
				demo.src_pic_cr ? demo.src_pic_cr[0] : NULL,
				(src_y_size + src_cb_size) / 2, src_cb_size / 2) < 0) {
			printf("getInterlaceData infb[0] failed!\n");
			exit(-1);
		}

		if (getInterlaceData(demo.ion_fd, &demo.inFbNf[0], src_size / 2,
				demo.src_pic_y[1], 0, src_y_size / 2,
				demo.src_pic_c[1], src_y_size / 2, src_cb_size / 2,
				demo.src_pic_cr ? demo.src_pic_cr[1] : NULL,
				(src_y_size + src_cb_size) / 2, src_cb_size / 2) < 0) {
			printf("getInterlaceData infb[0] failed!\n");
			exit(-1);
		}

		setDiBuffer(&fbout1tnr.inFb[0], &demo.inFb[0], DI_FIELD_TYPE_TOP_FIELD);
		setDiBuffer(&fbout1tnr.inFbNf[0], &demo.inFbNf[0], DI_FIELD_TYPE_BOTTOM_FIELD);
*/
	} else {
        if (getInterlaceData(demo.ion_fd, &demo.inFb[0], src_size,
			demo.src_pic_y[0], 0, src_y_size,
			demo.src_pic_c[0], src_y_size, src_cb_size,
			demo.src_pic_cr ? demo.src_pic_cr[0] : NULL,
				src_y_size + src_cb_size, src_cb_size) < 0) {
			printf("getInterlaceData infb[0] failed!\n");
			exit(-1);
		}

        setDiBuffer(&fbout1tnr.inFb[0], &demo.inFb[0], 0);
    }

    fbout1tnr.inFormat[0] = demo.src_fmt;

	i = 1;
	while (i < src_pic_max) {
		fbout1tnr.topFieldFirst = true;
		fbout1tnr.tnrMode.mode = TNR_MODE_ADAPTIVE;
		fbout1tnr.tnrMode.level = 0;
		if (demo.is_single_field) {
/*			//get the new picture for the next deinterlacing
			if (getInterlaceData(demo.ion_fd, &demo.inFb[1], src_size / 2,
				demo.src_pic_y[i * 2], 0, src_y_size / 2,
				demo.src_pic_c[i * 2], src_y_size / 2, src_cb_size / 2,
				demo.src_pic_cr ? demo.src_pic_cr[i * 2] : NULL,
				(src_y_size + src_cb_size) / 2, src_cb_size / 2) < 0) {
				printf("getInterlaceData for infb[1] failed!\n");
				exit(-1);
			}

			if (getInterlaceData(demo.ion_fd, &demo.inFbNf[1], src_size / 2,
				demo.src_pic_y[i * 2 + 1], 0, src_y_size / 2,
				demo.src_pic_c[i * 2 + 1], src_y_size / 2, src_cb_size / 2,
				demo.src_pic_cr ? demo.src_pic_cr[i * 2 + 1] : NULL,
				(src_y_size + src_cb_size) / 2, src_cb_size / 2) < 0) {
				printf("getInterlaceData for infbNf[2] failed!\n");
				exit(-1);
			}

			setDiBuffer(&fbout1tnr.inFb[1], &demo.inFb[1], DI_FIELD_TYPE_TOP_FIELD);
			setDiBuffer(&fbout1tnr.inFbNf[1], &demo.inFbNf[1], DI_FIELD_TYPE_BOTTOM_FIELD);
*/
		} else {
			//get the new picture for the next deinterlacing
			if (getInterlaceData(demo.ion_fd, &demo.inFb[1], src_size,
				demo.src_pic_y[i], 0, src_y_size,
				demo.src_pic_c[i], src_y_size, src_cb_size,
				demo.src_pic_cr ? demo.src_pic_cr[i] : NULL,
				src_y_size + src_cb_size, src_cb_size) < 0) {
				printf("getInterlaceData for infb[2] failed!\n");
				exit(-1);
			}
			setDiBuffer(&fbout1tnr.inFb[1], &demo.inFb[1], 0);
		}

		fbout1tnr.inFormat[1] = demo.src_fmt;

		//set out_fb0/out_fb1
		if (getDiOutMem(demo.ion_fd, &demo.outFb[0], dst_size) < 0) {
			printf("getDiOutMem for out fb0 failed\n");
			exit(-1);
		}

		setDiBuffer(&fbout1tnr.outFb, &demo.outFb[0], 0);
		fbout1tnr.outFormat = demo.dst_fmt;
		if (diFrameBuffer(demo.di_dev, (void *)&fbout1tnr) < 0) {
			printf("diFrameBuffer failed\n");
			exit(-1);
		}

		//the memory of in_fb0 is free, you can use it to do other things or release it
		releaseInterlaceData(demo.ion_fd, &demo.inFb[0]);
		if (demo.is_single_field)
			releaseInterlaceData(demo.ion_fd, &demo.inFbNf[0]);

		//set the infb0/infb1 for the next deinterlace processing
		memcpy(&fbout1tnr.inFb[0], &fbout1tnr.inFb[1],
			sizeof(struct di_buffer));
		if (demo.is_single_field)
			memcpy(&fbout1tnr.inFbNf[0], &fbout1tnr.inFbNf[1],
				sizeof(struct di_buffer));
		memcpy(&demo.inFb[0], &demo.inFb[1], sizeof(struct dma_mem));

#ifdef OUTPUT_RESULT_ENABLE
		//you can use the out memorys to display them or do other things
		outputRenderResult(&demo, demo.ion_fd, &demo.outFb[0],
			demo.dst_pic[i - 1],
			dst_size);
#endif
		dumpOutputBuffer(&demo.outFb[0]);
		releaseDiOutMem(demo.ion_fd, &demo.outFb[0]);

        i++;
    }

	releaseInterlaceData(demo.ion_fd, &demo.inFb[1]);

	if (demo.dimode == DIV3X_MODE_OUT1_TNR) {
		releaseInterlaceData(0, &demo.tnrBuffer[0]);
		releaseInterlaceData(0, &demo.tnrBuffer[1]);
	}

	diExit(demo.di_dev);

	printf("Test DI out1 finish irq count %d\n", IRQ_COUNT);
	printf("buffer count %d buffer size %d \n", buffer_count, buffer_size);
	return 0;
#endif
}

static int cmd_di300_out2(int argc, char **argv)
{
	int i;
	struct di_demo demo;
	unsigned int src_size, dst_size;
	unsigned int src_y_size;
	unsigned int src_cb_size;
	unsigned int src_pic_max;
	struct fb_para_out2_tnr fbout2tnr;

	memset(&demo, 0, sizeof(demo));
	memset(&fbout2tnr, 0, sizeof(fbout2tnr));

	demo.hal_debug = 1;
	if (argc < 17) {
		printf("Invalid params input! you should input the params in the format:\n");
		printf("test_di300_60HZ diMode "
				"srcW srcH srcFormat"
				"dstW dstH dstFormat"
				"cropLeft cropTop cropRight cropBottom "
				"demoCropLeft demoCropTop demoCropRight demoCropBottom "
				"isSingleField isFMDTest FMDTestMode\n");
		return -1;
	}

	if (parse_params_out2(&demo, argc, argv) < 0)
		exit(-1);
	if (getSizes(&demo, &src_size, &src_y_size, &src_cb_size,
			&dst_size) < 0)
		exit(-1);

	if (demo.src_w == 1920 && demo.src_h == 1080)
		src_pic_max = 6; //we only have 6 pictures for 1080p to test
	else if (demo.is_fmd_test)
		src_pic_max = 40; //FMD need 30 picture to test
	else
		src_pic_max = 12; //test 12 source interlaced pictures totally

	if (diOut2Init(&demo) < 0)
		exit(-1);

	if (demo.is_single_field) {
		if (getInterlaceData_out2(demo.ion_fd, &demo.inFb[0], src_size / 2,
				demo.src_pic_y[0], 0, src_y_size / 2,
				demo.src_pic_c[0], src_y_size / 2, src_cb_size / 2,
				demo.src_pic_cr ? demo.src_pic_cr[0] : NULL,
				(src_y_size + src_cb_size) / 2, src_cb_size / 2,
				demo.is_fmd_test) < 0) {
			printf("getInterlaceData infb[0] failed!\n");
			exit(-1);
		}

		if (getInterlaceData_out2(demo.ion_fd, &demo.inFbNf[0], src_size / 2,
				demo.src_pic_y[1], 0, src_y_size / 2,
				demo.src_pic_c[1], src_y_size / 2, src_cb_size / 2,
				demo.src_pic_cr ? demo.src_pic_cr[1] : NULL,
				(src_y_size + src_cb_size) / 2, src_cb_size / 2,
				demo.is_fmd_test) < 0) {
			printf("getInterlaceData infb[0] failed!\n");
			exit(-1);
		}

		setDiBuffer(&fbout2tnr.inFb[0], &demo.inFb[0], DI_FIELD_TYPE_TOP_FIELD);
		setDiBuffer(&fbout2tnr.inFbNf[0], &demo.inFbNf[0], DI_FIELD_TYPE_BOTTOM_FIELD);

		if (getInterlaceData_out2(demo.ion_fd, &demo.inFb[1], src_size / 2,
				demo.src_pic_y[2], 0, src_y_size / 2,
				demo.src_pic_c[2], src_y_size / 2, src_cb_size / 2,
				demo.src_pic_cr ? demo.src_pic_cr[2] : NULL,
				(src_y_size + src_cb_size) / 2, src_cb_size / 2,
				demo.is_fmd_test) < 0) {
			printf("getInterlaceData infb[1] failed!\n");
			exit(-1);
		}

		if (getInterlaceData_out2(demo.ion_fd, &demo.inFbNf[1], src_size / 2,
				demo.src_pic_y[3], 0, src_y_size / 2,
				demo.src_pic_c[3], src_y_size / 2, src_cb_size / 2,
				demo.src_pic_cr ? demo.src_pic_cr[3] : NULL,
				(src_y_size + src_cb_size) / 2, src_cb_size / 2,
				demo.is_fmd_test) < 0) {
			printf("getInterlaceData infb[1] failed!\n");
			exit(-1);
		}

		setDiBuffer(&fbout2tnr.inFb[1], &demo.inFb[1], DI_FIELD_TYPE_TOP_FIELD);
		setDiBuffer(&fbout2tnr.inFbNf[1], &demo.inFbNf[1], DI_FIELD_TYPE_BOTTOM_FIELD);

	} else {
		if (getInterlaceData_out2(demo.ion_fd, &demo.inFb[0], src_size,
				demo.src_pic_y[0], 0, src_y_size,
				demo.src_pic_c[0], src_y_size, src_cb_size,
				demo.src_pic_cr ? demo.src_pic_cr[0] : NULL,
				src_y_size + src_cb_size, src_cb_size,
				demo.is_fmd_test) < 0) {
			printf("getInterlaceData infb[0] failed!\n");
			return -1;
		}

		if (getInterlaceData_out2(demo.ion_fd, &demo.inFb[1], src_size,
				demo.src_pic_y[1], 0, src_y_size,
				demo.src_pic_c[1], src_y_size, src_cb_size,
				demo.src_pic_cr ? demo.src_pic_cr[1] : NULL,
				src_y_size + src_cb_size, src_cb_size,
				demo.is_fmd_test) < 0) {
			printf("getInterlaceData infb[1] failed!\n");
			return -1;
		}

		setDiBuffer(&fbout2tnr.inFb[0], &demo.inFb[0], 0);
		setDiBuffer(&fbout2tnr.inFb[1], &demo.inFb[1], 0);
	}

	fbout2tnr.inFormat[0] = demo.src_fmt;
	fbout2tnr.inFormat[1] = demo.src_fmt;

	i = 2;
	while (i < src_pic_max) {
		fbout2tnr.tnrMode.mode = TNR_MODE_ADAPTIVE;
		fbout2tnr.tnrMode.level = 0;
		fbout2tnr.topFieldFirst = true;
		if (demo.is_fmd_test)
			fbout2tnr.topFieldFirst = false;//FMD test source is BFF

		if (demo.is_single_field) {
			//get the new picture for the next deinterlacing
			if (getInterlaceData_out2(demo.ion_fd, &demo.inFb[2], src_size / 2,
				demo.src_pic_y[i * 2], 0, src_y_size / 2,
				demo.src_pic_c[i * 2], src_y_size / 2, src_cb_size / 2,
				demo.src_pic_cr ? demo.src_pic_cr[i * 2] : NULL,
				(src_y_size + src_cb_size) / 2, src_cb_size / 2,
				demo.is_fmd_test) < 0) {
				printf("getInterlaceData for infb[1] failed!\n");
				exit(-1);
			}

			if (getInterlaceData_out2(demo.ion_fd, &demo.inFbNf[2], src_size / 2,
				demo.src_pic_y[i * 2 + 1], 0, src_y_size / 2,
				demo.src_pic_c[i * 2 + 1], src_y_size / 2, src_cb_size / 2,
				demo.src_pic_cr ? demo.src_pic_cr[i * 2 + 1] : NULL,
				(src_y_size + src_cb_size) / 2, src_cb_size / 2,
				demo.is_fmd_test) < 0) {
				printf("getInterlaceData for infbNf[2] failed!\n");
				exit(-1);
			}

			setDiBuffer(&fbout2tnr.inFb[2], &demo.inFb[2], DI_FIELD_TYPE_TOP_FIELD);
			setDiBuffer(&fbout2tnr.inFbNf[2], &demo.inFbNf[2], DI_FIELD_TYPE_BOTTOM_FIELD);
		} else {
			//get the new picture for the next deinterlacing
			if (getInterlaceData_out2(demo.ion_fd, &demo.inFb[2], src_size,
				demo.src_pic_y[i], 0, src_y_size,
				demo.src_pic_c[i], src_y_size, src_cb_size,
				demo.src_pic_cr ? demo.src_pic_cr[i] : NULL,
				src_y_size + src_cb_size, src_cb_size,
				demo.is_fmd_test) < 0) {
				printf("getInterlaceData for infb[2] failed!\n");
				return -1;
			}
			setDiBuffer(&fbout2tnr.inFb[2], &demo.inFb[2], 0);
		}

		fbout2tnr.inFormat[2] = demo.src_fmt;

		//set out_fb0/out_fb1
		if (getDiOutMem(demo.ion_fd, &demo.outFb[0], dst_size) < 0) {
			printf("getDiOutMem for out fb0 failed\n");
			return -1;
		}

		if (getDiOutMem(demo.ion_fd, &demo.outFb[1], dst_size) < 0) {
			printf("getDiOutMem for out fb1 failed\n");
			return -1;
		}

		setDiBuffer(&fbout2tnr.outFb[0], &demo.outFb[0], 0);
		setDiBuffer(&fbout2tnr.outFb[1], &demo.outFb[1], 0);

		fbout2tnr.outFormat[0] = demo.dst_fmt;
		fbout2tnr.outFormat[1] = demo.dst_fmt;

		if (diFrameBuffer(demo.di_dev, (void *)&fbout2tnr) < 0) {
			printf("diFrameBuffer failed\n");
			return -1;
		}

		//the memory of in_fb0 is free, you can use it to do other things or release it
		releaseInterlaceData(demo.ion_fd, &demo.inFb[0]);
		if (demo.is_single_field)
			releaseInterlaceData(demo.ion_fd, &demo.inFbNf[0]);

		//set the infb0/infb1 for the next deinterlace processing
		memcpy(&fbout2tnr.inFb[0], &fbout2tnr.inFb[1],
			sizeof(struct di_buffer));
		memcpy(&demo.inFb[0], &demo.inFb[1], sizeof(struct dma_mem));
		if (demo.is_single_field)
			memcpy(&fbout2tnr.inFbNf[0], &fbout2tnr.inFbNf[1],
						sizeof(struct di_buffer));

		memcpy(&fbout2tnr.inFb[1], &fbout2tnr.inFb[2],
			sizeof(struct di_buffer));
		memcpy(&demo.inFb[1], &demo.inFb[2], sizeof(struct dma_mem));
		if (demo.is_single_field)
			memcpy(&fbout2tnr.inFbNf[1], &fbout2tnr.inFbNf[2],
						sizeof(struct di_buffer));

		dumpOutputBuffer(&demo.outFb[0]);
		dumpOutputBuffer(&demo.outFb[1]);

		releaseDiOutMem(demo.ion_fd, &demo.outFb[0]);
		releaseDiOutMem(demo.ion_fd, &demo.outFb[1]);
		i++;
	}

	diExit(demo.di_dev);

	releaseInterlaceData(0, &demo.inFb[0]);
	releaseInterlaceData(0, &demo.inFb[1]);

	if (demo.dimode == DIV3X_MODE_OUT2_TNR) {
		releaseInterlaceData(0, &demo.tnrBuffer[0]);
		releaseInterlaceData(0, &demo.tnrBuffer[1]);
		releaseInterlaceData(0, &demo.tnrBuffer[2]);
	}


	printf("Test DI out2 finish irq count %d\n", IRQ_COUNT);
	return 0;
}

FINSH_FUNCTION_EXPORT_CMD(cmd_di300_out2, test_di300_60HZ, test di 60 HZ mode);
FINSH_FUNCTION_EXPORT_CMD(cmd_di300_test, test_di300_30HZ, test di 30 HZ mode);