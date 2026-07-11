#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cmath>
#include <cstdint>
#include <cstring>

extern "C" {
#include "chips/sgu1.h"
}

static int8_t pcm[SGU_PCM_RAM_SIZE];
static int32_t next_left;
static int32_t next_right;

extern "C" {
void SGU_Init(struct SGU* sgu, size_t) {
    std::memset(sgu, 0, sizeof(*sgu));
    sgu->pcm = pcm;
}

void SGU_Reset(struct SGU* sgu) {
    std::memset(sgu->chan, 0, sizeof(sgu->chan));
}

void SGU_NextSample(struct SGU*, int32_t* left, int32_t* right) {
    *left = next_left;
    *right = next_right;
}

int32_t SGU_GetSample(struct SGU*, uint8_t) {
    return 0;
}

void SGU_Write(struct SGU* sgu, uint16_t reg, uint8_t data) {
    reinterpret_cast<uint8_t*>(sgu->chan)[reg] = data;
}
}

static sgu1_t make_sgu() {
    sgu1_t sgu;
    sgu1_desc_t desc = { .tick_hz = SGU_CHIP_CLOCK, .magnitude = 1.0f, .dump_file = nullptr };
    sgu1_init(&sgu, &desc);
    sgu1_reset(&sgu);
    return sgu;
}

static void select_bank(sgu1_t& sgu, uint8_t bank) {
    sgu1_reg_write(&sgu, 0x3F, bank);
}

static void set_offset(sgu1_t& sgu, uint16_t offset) {
    sgu1_reg_write(&sgu, 0x1C, static_cast<uint8_t>(offset));
    sgu1_reg_write(&sgu, 0x1D, static_cast<uint8_t>(offset >> 8));
}

TEST_CASE("service bank uploads and reads PCM with wrapping auto-increment") {
    auto sgu = make_sgu();
    std::memset(pcm, 0, sizeof(pcm));
    select_bank(sgu, 0xFF);
    sgu1_reg_write(&sgu, 0x1E, 0);
    set_offset(sgu, 0xFFFE);

    sgu1_reg_write(&sgu, 0x1F, 0x12);
    sgu1_reg_write(&sgu, 0x1F, 0x34);
    sgu1_reg_write(&sgu, 0x1F, 0x56);
    CHECK(static_cast<uint8_t>(pcm[0xFFFE]) == 0x12);
    CHECK(static_cast<uint8_t>(pcm[0xFFFF]) == 0x34);
    CHECK(static_cast<uint8_t>(pcm[0]) == 0x56);
    CHECK(sgu1_reg_read(&sgu, 0x1C) == 1);
    CHECK(sgu1_reg_read(&sgu, 0x1D) == 0);

    set_offset(sgu, 0xFFFE);
    CHECK(sgu1_reg_read(&sgu, 0x1F) == 0x12);
    CHECK(sgu1_reg_read(&sgu, 0x1F) == 0x34);
    CHECK(sgu1_reg_read(&sgu, 0x1F) == 0x56);
    CHECK(sgu1_reg_read(&sgu, 0x1C) == 1);

    sgu1_reg_write(&sgu, 0x1E, 1);
    set_offset(sgu, 0xFFFE);
    sgu1_reg_write(&sgu, 0x1F, 0x99);
    CHECK(static_cast<uint8_t>(pcm[0xFFFE]) == 0x12);
    set_offset(sgu, 0xFFFE);
    CHECK(sgu1_reg_read(&sgu, 0x1F) == 0);
    CHECK(sgu1_reg_read(&sgu, 0x1C) == 0xFF);
}

TEST_CASE("service and reserved banks cannot alias channel registers") {
    auto sgu = make_sgu();
    uint8_t before[sizeof(sgu.sgu.chan)];
    std::memset(sgu.sgu.chan, 0xA5, sizeof(sgu.sgu.chan));
    std::memcpy(before, sgu.sgu.chan, sizeof(before));

    select_bank(sgu, 0xFF);
    sgu1_reg_write(&sgu, 0x20, 0x80);
    sgu1_reg_write(&sgu, 0x00, 0x11);
    CHECK(sgu1_reg_read(&sgu, 0x00) == 0);
    CHECK(std::memcmp(before, sgu.sgu.chan, sizeof(before)) == 0);

    select_bank(sgu, SGU_CHNS);
    sgu1_reg_write(&sgu, 0x00, 0x22);
    CHECK(sgu1_reg_read(&sgu, 0x00) == 0xFF);
    CHECK(sgu1_reg_read(&sgu, 0x3F) == SGU_CHNS);
    CHECK(std::memcmp(before, sgu.sgu.chan, sizeof(before)) == 0);
}

TEST_CASE("channel selection and reset retain wrapper semantics") {
    auto sgu = make_sgu();
    select_bank(sgu, 3);
    sgu1_reg_write(&sgu, 7, 0x42);
    CHECK(sgu1_reg_read(&sgu, 7) == 0x42);
    CHECK(sgu1_reg_read(&sgu, 0x3F) == 3);

    select_bank(sgu, 0xFF);
    set_offset(sgu, 0x1234);
    sgu1_reg_write(&sgu, 0x1E, 7);
    sgu1_reg_write(&sgu, 0x20, 0x40);
    sgu1_reset(&sgu);
    CHECK(sgu.selected_channel == 0);
    CHECK(sgu.svc_sample_offset == 0);
    CHECK(sgu.svc_sample_bank == 0);
    CHECK(sgu.svc_master_vol == 0xFF);
}

TEST_CASE("master volume linearly scales final stereo output") {
    auto sgu = make_sgu();
    next_left = 16384;
    next_right = -8192;
    sgu1_tick(&sgu, 0);
    const float full_left = sgu.sample[0];
    const float full_right = sgu.sample[1];

    select_bank(sgu, 0xFF);
    sgu1_reg_write(&sgu, 0x20, 128);
    sgu1_tick(&sgu, 0);
    CHECK(sgu.sample[0] == doctest::Approx(full_left * 128.0f / 255.0f));
    CHECK(sgu.sample[1] == doctest::Approx(full_right * 128.0f / 255.0f));
}
