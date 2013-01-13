#include <Arduino.h>
#include "buffer.h"
#include "common.h"

read_buffer::read_buffer() {
    _available = 0;
    _start = 0;
}

int read_buffer::available() {
    return _available;
}

uint8_t read_buffer::read() {
    uint8_t return_value = 0;

    if(_available > 0) {
        return_value = _buffer[_start];
        _start = (_start + 1) % BUFFER_LENGTH;
        _available--;
    }
    return return_value;
}

void read_buffer::store(uint8_t value) {
    _buffer[(_start + _available) % BUFFER_LENGTH] = value;
    _available++;
}

