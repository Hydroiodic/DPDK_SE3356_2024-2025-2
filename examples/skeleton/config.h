#pragma once

#include <stdint.h>

// 192.168.122.88
static const uint32_t src_ip = 0xc0a87a58;
// 192.168.122.114
static const uint32_t dst_ip = 0xc0a87a72;

// 52:54:00:fe:94:f2
static const unsigned char src_mac[6] = {0x52, 0x54, 0x00, 0xfe, 0x94, 0xf2};
// 52:54:00:cf:25:d3
static const unsigned char dst_mac[6] = {0x52, 0x54, 0x00, 0xcf, 0x25, 0xd3};
