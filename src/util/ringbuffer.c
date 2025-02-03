#include "./ringbuffer.h"

void rb_init(ring_buffer_t* rb) {
    rb->head = 0;
    rb->tail = 0;
}

bool rb_is_empty(const ring_buffer_t* rb) {
    return rb->head == rb->tail;
}

bool rb_is_full(const ring_buffer_t* rb) {
    return ((rb->head + 1) % RB_BUFFER_SIZE) == rb->tail;
}

bool rb_put(ring_buffer_t* rb, uint8_t data) {
    if (rb_is_full(rb)) {
        return false;
    }
    rb->buffer[rb->head] = data;
    rb->head = (rb->head + 1) % RB_BUFFER_SIZE;
    return true;
}

bool rb_get(ring_buffer_t* rb, uint8_t* data) {
    if (rb_is_empty(rb)) {
        return false;
    }
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RB_BUFFER_SIZE;
    return true;
}
