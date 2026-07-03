#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <deque>
#include <cstdint>

extern "C" {
#include "util/ringbuffer.h"
}

// The buffer sacrifices one slot to distinguish full from empty.
static const size_t CAP = RB_BUFFER_SIZE - 1;

TEST_CASE("fresh buffer is empty and not full") {
    ring_buffer_t rb;
    rb_init(&rb);
    CHECK(rb_is_empty(&rb));
    CHECK_FALSE(rb_is_full(&rb));
    uint8_t out = 0xAB;
    CHECK_FALSE(rb_get(&rb, &out));  // get on empty fails...
    CHECK(out == 0xAB);              // ...and leaves the output untouched
}

TEST_CASE("single put/get round-trips a value in order") {
    ring_buffer_t rb;
    rb_init(&rb);
    CHECK(rb_put(&rb, 0x42));
    CHECK_FALSE(rb_is_empty(&rb));
    uint8_t out = 0;
    CHECK(rb_get(&rb, &out));
    CHECK(out == 0x42);
    CHECK(rb_is_empty(&rb));
}

TEST_CASE("holds exactly RB_BUFFER_SIZE-1 items") {
    ring_buffer_t rb;
    rb_init(&rb);
    for (size_t i = 0; i < CAP; i++) {
        CAPTURE(i);
        CHECK(rb_put(&rb, (uint8_t)i));
    }
    CHECK(rb_is_full(&rb));
    CHECK_FALSE(rb_is_empty(&rb));
    CHECK_FALSE(rb_put(&rb, 0xFF));  // one past capacity is rejected
    CHECK(rb_is_full(&rb));          // ...and the buffer is unchanged
}

TEST_CASE("drains a full buffer in FIFO order, then is empty") {
    ring_buffer_t rb;
    rb_init(&rb);
    for (size_t i = 0; i < CAP; i++)
        REQUIRE(rb_put(&rb, (uint8_t)i));

    for (size_t i = 0; i < CAP; i++) {
        CAPTURE(i);
        uint8_t out = 0;
        CHECK(rb_get(&rb, &out));
        CHECK(out == (uint8_t)i);
    }
    CHECK(rb_is_empty(&rb));
    uint8_t out;
    CHECK_FALSE(rb_get(&rb, &out));
}

TEST_CASE("a full buffer accepts a new item once one is removed") {
    ring_buffer_t rb;
    rb_init(&rb);
    for (size_t i = 0; i < CAP; i++)
        REQUIRE(rb_put(&rb, (uint8_t)i));
    REQUIRE(rb_is_full(&rb));

    uint8_t out = 0;
    REQUIRE(rb_get(&rb, &out));   // free one slot
    CHECK(out == 0);              // FIFO: oldest comes out
    CHECK_FALSE(rb_is_full(&rb));
    CHECK(rb_put(&rb, 0x99));     // now there is room
    CHECK(rb_is_full(&rb));
}

// Drive a long, deterministic mix of puts and gets and check the ring buffer
// against a std::deque model at every step. The put-heavy pattern repeatedly
// saturates the buffer, and 2000 steps over a 128-slot buffer forces the
// head/tail indices to wrap the modulo boundary many times.
TEST_CASE("matches a FIFO model across many wrap-arounds") {
    ring_buffer_t rb;
    rb_init(&rb);
    std::deque<uint8_t> model;

    uint8_t next = 0;
    for (int step = 0; step < 2000; step++) {
        CAPTURE(step);
        const bool do_put = (step % 3) != 0;  // ~2/3 puts: tends to fill up
        if (do_put) {
            const bool expect = model.size() < CAP;
            CHECK(rb_put(&rb, next) == expect);
            if (expect) model.push_back(next);
            next++;
        }
        else {
            uint8_t out = 0;
            const bool expect = !model.empty();
            CHECK(rb_get(&rb, &out) == expect);
            if (expect) {
                CHECK(out == model.front());
                model.pop_front();
            }
        }
        CHECK(rb_is_empty(&rb) == model.empty());
        CHECK(rb_is_full(&rb) == (model.size() == CAP));
    }
}
