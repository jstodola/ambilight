#ifndef BUFFER_H
#define BUFFER_H

#include "common.h"


const int BUFFER_LENGTH = CHANNELS * 3 * 2;

class read_buffer {
  public:
    read_buffer();
    uint8_t read();
    int available();
    void store(uint8_t value);
  private:
    int _available;
    int _start;
    uint8_t _buffer[BUFFER_LENGTH];
};

#endif
