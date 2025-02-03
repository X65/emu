#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define RB_BUFFER_SIZE 128

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t buffer[RB_BUFFER_SIZE];
    size_t head;
    size_t tail;
} ring_buffer_t;

void rb_init(ring_buffer_t* rb);
bool rb_is_empty(const ring_buffer_t* rb);
bool rb_is_full(const ring_buffer_t* rb);
bool rb_put(ring_buffer_t* rb, uint8_t data);
bool rb_get(ring_buffer_t* rb, uint8_t* data);

#ifdef __cplusplus
} /* extern "C" */
#endif
