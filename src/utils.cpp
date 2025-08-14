#include "utils.h"


String toValveString(uint16_t value)
{
  String result = "";
  for (int i = 0; i < 12; i++)
  {
    result += (value & (1 << i)) ? 'o' : 'x';
    if ((i + 1) % 3 == 0 && i != 15)
    {
      result += ' ';
    }
  }
  return result;
}

uint16_t decodeBinaryString(const char *input)
{
  uint16_t result = 0;

  int idx = 0;
  int len = strlen(input);
  for (int i = 0; i < len; i++)
  {
    if (input[i] != ' ')
    {
      if (input[i] == 'o')
      {
        result |= (1 << idx); // bit i je nastaven na 1
      }
      idx++;
    }
  }
  return result;
}