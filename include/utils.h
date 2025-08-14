#ifndef UTILS_H
#define UTILS_H

#include <WString.h>

String toValveString(uint16_t value);
uint16_t decodeBinaryString(const char *input);

#endif