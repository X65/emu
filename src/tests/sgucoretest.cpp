#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <cstring>
#include <vector>

// Unlike sgu1test, which stubs the core to test the register wrapper, this
// suite links the real ext/sgu-1 core. It covers the two mechanisms the service
// bank leans on and that nothing else exercises: the deferred reset and the
// atomic read-to-clear status word.
extern "C" {
#include "sgu-1/sgu.h"
}

namespace {

struct core {
    struct SGU sgu;
    std::vector<int8_t> pcm;

    core() : pcm(SGU_PCM_BANK_SIZE, 0) {
        std::memset(&sgu, 0, sizeof(sgu));
        SGU_Init(&sgu, pcm.data(), pcm.size());
    }
};

// Write a byte into a channel's register file through the public bus API.
void write_ch(struct SGU* sgu, uint8_t ch, uint8_t reg, uint8_t data) {
    SGU_Write(sgu, static_cast<uint16_t>((ch << 6) | reg), data);
}

uint8_t read_ch(const struct SGU* sgu, uint8_t ch, uint8_t reg) {
    return reinterpret_cast<const uint8_t*>(sgu->chan)[(ch << 6) | reg];
}

}  // namespace

TEST_CASE("a requested reset is deferred to the next sample boundary") {
    core c;
    write_ch(&c.sgu, 0, SGU1_CHN_FLAGS0, SGU1_FLAGS0_CTL_GATE);
    REQUIRE(read_ch(&c.sgu, 0, SGU1_CHN_FLAGS0) != 0);

    SGU_RequestReset(&c.sgu, SGU_RESET_VOICES);
    // Nothing may happen yet: on hardware the other core could be mid-render.
    CHECK(read_ch(&c.sgu, 0, SGU1_CHN_FLAGS0) != 0);

    int32_t l = 0, r = 0;
    SGU_NextSample(&c.sgu, &l, &r);
    CHECK(read_ch(&c.sgu, 0, SGU1_CHN_FLAGS0) == 0);

    // The request is consumed, not standing.
    write_ch(&c.sgu, 0, SGU1_CHN_FLAGS0, SGU1_FLAGS0_CTL_GATE);
    SGU_NextSample(&c.sgu, &l, &r);
    CHECK(read_ch(&c.sgu, 0, SGU1_CHN_FLAGS0) != 0);
}

TEST_CASE("reset domains are independent") {
    core c;
    write_ch(&c.sgu, 0, SGU1_CHN_FLAGS0, SGU1_FLAGS0_CTL_GATE);
    c.sgu.L = c.sgu.R = 12345;
    c.sgu.sample_counter = 999;

    SGU_ResetParts(&c.sgu, SGU_RESET_MIX);
    CHECK(c.sgu.L == 0);
    CHECK(c.sgu.R == 0);
    CHECK(c.sgu.sample_counter == 999);               // TIMEBASE untouched
    CHECK(read_ch(&c.sgu, 0, SGU1_CHN_FLAGS0) != 0);  // VOICES untouched

    SGU_ResetParts(&c.sgu, SGU_RESET_TIMEBASE);
    CHECK(c.sgu.sample_counter == 0);
    CHECK(c.sgu.lfo_lfsr == 0x1FFFF);  // reseeded, not zeroed
    CHECK(read_ch(&c.sgu, 0, SGU1_CHN_FLAGS0) != 0);

    SGU_ResetParts(&c.sgu, SGU_RESET_VOICES);
    CHECK(read_ch(&c.sgu, 0, SGU1_CHN_FLAGS0) == 0);
}

TEST_CASE("PCM sample memory survives every reset") {
    core c;
    c.pcm[0x1234] = 0x42;
    SGU_Reset(&c.sgu);
    CHECK(c.pcm[0x1234] == 0x42);
    SGU_ResetParts(&c.sgu, SGU_RESET_ALL);
    CHECK(c.pcm[0x1234] == 0x42);
    CHECK(c.sgu.pcm == c.pcm.data());
    CHECK(c.sgu.pcm_size == c.pcm.size());
}

TEST_CASE("the status word latches and reads to clear") {
    core c;
    CHECK(SGU_GetFlags(&c.sgu) == 0);

    // Drive the output stage past the hardware limiter. The DC-removal filter
    // sits between the mix and the clip test, so feed it for a while.
    bool clipped = false;
    for (int i = 0; i < 64 && !clipped; i++) {
        int32_t l = 0, r = 0;
        SGU_NextSample_Setup(&c.sgu);
        SGU_NextSample_Finalize(&c.sgu, INT32_MAX / 2, INT32_MAX / 2, &l, &r);
        clipped = (SGU_GetFlags(&c.sgu) & SGU_FLAG_CLIP) != 0;
    }
    CHECK(clipped);
    CHECK(SGU_GetFlags(&c.sgu) == 0);  // the read cleared it
}

TEST_CASE("a mix reset clears the status latch") {
    core c;
    int32_t l = 0, r = 0;
    SGU_NextSample_Setup(&c.sgu);
    SGU_NextSample_Finalize(&c.sgu, INT32_MAX / 2, INT32_MAX / 2, &l, &r);

    SGU_ResetParts(&c.sgu, SGU_RESET_MIX);
    CHECK(SGU_GetFlags(&c.sgu) == 0);
}
