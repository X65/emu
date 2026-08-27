#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cmath>
#include <cstdint>
#include <cstring>

extern "C" {
#include "chips/sgu1.h"
}

static int32_t next_left;
static int32_t next_right;
// Reset domains the wrapper asked the core for, accumulated by the stub below.
static uint32_t requested_reset_parts;
// Status word the stubbed core hands back on the next SGU_GetFlags call.
static uint32_t pending_flags;

extern "C" {
void SGU_Init(struct SGU* sgu, int8_t* pcm, size_t pcm_size) {
    std::memset(sgu, 0, sizeof(*sgu));
    sgu->pcm = pcm;
    sgu->pcm_size = pcm_size;
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

void SGU_RequestReset(struct SGU*, uint32_t parts) {
    requested_reset_parts |= parts;
}

uint32_t SGU_GetFlags(struct SGU*) {
    const uint32_t flags = pending_flags;
    pending_flags = 0;
    return flags;
}
}

static sgu1_t make_sgu() {
    requested_reset_parts = 0;
    pending_flags = 0;
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
    CHECK(sgu.sgu.pcm_size == SGU1_PCM_BANKS * SGU_PCM_BANK_SIZE);
    std::memset(sgu.sgu.pcm, 0, sgu.sgu.pcm_size);
    select_bank(sgu, 0xFF);
    sgu1_reg_write(&sgu, 0x1E, 0);
    set_offset(sgu, 0xFFFE);

    sgu1_reg_write(&sgu, 0x1F, 0x12);
    sgu1_reg_write(&sgu, 0x1F, 0x34);
    sgu1_reg_write(&sgu, 0x1F, 0x56);
    CHECK(static_cast<uint8_t>(sgu.sgu.pcm[0xFFFE]) == 0x12);
    CHECK(static_cast<uint8_t>(sgu.sgu.pcm[0xFFFF]) == 0x34);
    CHECK(static_cast<uint8_t>(sgu.sgu.pcm[0]) == 0x56);
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
    CHECK(static_cast<uint8_t>(sgu.sgu.pcm[0xFFFE]) == 0x12);
    set_offset(sgu, 0xFFFE);
    CHECK(sgu1_reg_read(&sgu, 0x1F) == 0x99);
    CHECK(sgu1_reg_read(&sgu, 0x1C) == 0xFF);

    sgu1_reg_write(&sgu, 0x1E, SGU1_PCM_BANKS);
    set_offset(sgu, 0xFFFE);
    sgu1_reg_write(&sgu, 0x1F, 0x77);
    set_offset(sgu, 0xFFFE);
    CHECK(sgu1_reg_read(&sgu, 0x1F) == 0);
    sgu1_discard(&sgu);
}

TEST_CASE("service and reserved banks cannot alias channel registers") {
    auto sgu = make_sgu();
    uint8_t before[sizeof(sgu.sgu.chan)];
    std::memset(sgu.sgu.chan, 0xA5, sizeof(sgu.sgu.chan));
    std::memcpy(before, sgu.sgu.chan, sizeof(before));

    select_bank(sgu, 0xFF);
    sgu1_reg_write(&sgu, 0x20, 0x80);
    // Sweep the whole service window, identification block and control
    // registers included, and assert none of it reaches the channel file.
    for (uint8_t reg = 0x00; reg <= 0x20; reg++) {
        sgu1_reg_write(&sgu, reg, 0x11);
    }
    CHECK(sgu1_reg_read(&sgu, 0x00) == 'S');  // read-only, the write bounced
    CHECK(std::memcmp(before, sgu.sgu.chan, sizeof(before)) == 0);

    select_bank(sgu, SGU_CHNS);
    sgu1_reg_write(&sgu, 0x00, 0x22);
    CHECK(sgu1_reg_read(&sgu, 0x00) == 0xFF);
    CHECK(sgu1_reg_read(&sgu, 0x3F) == SGU_CHNS);
    CHECK(std::memcmp(before, sgu.sgu.chan, sizeof(before)) == 0);
    sgu1_discard(&sgu);
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
    CHECK(sgu.svc_master_vol == 0);  // muted at reset - the OS unmutes
    sgu1_discard(&sgu);
}

TEST_CASE("master volume linearly scales final stereo output") {
    auto sgu = make_sgu();
    sgu.svc_master_vol = 0xFF;
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
    sgu1_discard(&sgu);
}

TEST_CASE("service bank identifies the chip") {
    auto sgu = make_sgu();
    select_bank(sgu, 0xFF);

    CHECK(sgu1_reg_read(&sgu, 0x00) == 'S');
    CHECK(sgu1_reg_read(&sgu, 0x01) == 'G');
    CHECK(sgu1_reg_read(&sgu, 0x02) == 'U');
    CHECK(sgu1_reg_read(&sgu, 0x03) == '1');
    CHECK(sgu1_reg_read(&sgu, 0x04) == 0x01);  // version major
    CHECK(sgu1_reg_read(&sgu, 0x05) == 0x00);  // version minor

    // No board id under emulation, and silicon never reports an all-zero one.
    for (uint8_t reg = 0x06; reg <= 0x0D; reg++) {
        CHECK(sgu1_reg_read(&sgu, reg) == 0);
    }

    CHECK(sgu1_reg_read(&sgu, 0x0E) == SGU1_PCM_BANKS);
    CHECK(sgu1_reg_read(&sgu, 0x0F) == 0);  // no service banks beyond $FF yet

    // The whole block is read-only.
    for (uint8_t reg = 0x00; reg <= 0x0F; reg++) {
        const uint8_t before = sgu1_reg_read(&sgu, reg);
        sgu1_reg_write(&sgu, reg, 0x5A);
        CHECK(sgu1_reg_read(&sgu, reg) == before);
    }
    sgu1_discard(&sgu);
}

TEST_CASE("STATUS reports the clip latch and clears on read") {
    auto sgu = make_sgu();
    select_bank(sgu, 0xFF);

    CHECK(sgu1_reg_read(&sgu, 0x10) == 0);

    pending_flags = SGU_FLAG_CLIP;
    CHECK((sgu1_reg_read(&sgu, 0x10) & SGU_FLAG_CLIP) != 0);
    CHECK(sgu1_reg_read(&sgu, 0x10) == 0);  // read-to-clear

    // Writes never set status bits.
    pending_flags = 0;
    sgu1_reg_write(&sgu, 0x10, 0xFF);
    CHECK(sgu1_reg_read(&sgu, 0x10) == 0);
    sgu1_discard(&sgu);
}

TEST_CASE("CHIP_RESET requires the magic nybble and selects reset domains") {
    auto sgu = make_sgu();
    select_bank(sgu, 0xFF);

    // Without the $A high nybble nothing happens, however tempting the low bits.
    for (const uint8_t data : { 0x00, 0x07, 0x0F, 0x5A, 0xB7, 0xFF }) {
        requested_reset_parts = 0;
        sgu1_reg_write(&sgu, 0x18, data);
        CHECK(requested_reset_parts == 0);
    }

    requested_reset_parts = 0;
    sgu1_reg_write(&sgu, 0x18, 0xA0);  // magic, but no domains
    CHECK(requested_reset_parts == 0);

    requested_reset_parts = 0;
    sgu1_reg_write(&sgu, 0x18, 0xA1);
    CHECK(requested_reset_parts == SGU_RESET_VOICES);

    requested_reset_parts = 0;
    sgu1_reg_write(&sgu, 0x18, 0xA2);
    CHECK(requested_reset_parts == SGU_RESET_TIMEBASE);

    requested_reset_parts = 0;
    sgu1_reg_write(&sgu, 0x18, 0xA4);
    CHECK(requested_reset_parts == SGU_RESET_MIX);

    requested_reset_parts = 0;
    sgu1_reg_write(&sgu, 0x18, 0xA7);  // the whole core, as SGU_Reset() does
    CHECK(requested_reset_parts == SGU_RESET_ALL);

    CHECK(sgu1_reg_read(&sgu, 0x18) == 0);  // write-only
    sgu1_discard(&sgu);
}

TEST_CASE("CHIP_RESET SVC bit clears service registers but not the window") {
    auto sgu = make_sgu();
    select_bank(sgu, 0xFF);
    set_offset(sgu, 0x1234);
    sgu1_reg_write(&sgu, 0x1E, 2);
    sgu1_reg_write(&sgu, 0x20, 0x40);

    requested_reset_parts = 0;
    sgu1_reg_write(&sgu, 0x18, 0xA8);  // SVC only

    CHECK(requested_reset_parts == 0);  // the core is left alone
    CHECK(sgu.svc_sample_offset == 0);
    CHECK(sgu.svc_sample_bank == 0);
    CHECK(sgu.svc_master_vol == 0);  // muted, as at power-on
    // The select is the register window, not service state - it must survive,
    // or the very sequence issuing the reset loses its bank.
    CHECK(sgu.selected_channel == 0xFF);
    CHECK(sgu1_reg_read(&sgu, 0x3F) == 0xFF);

    // $AF is the full chip reset: every core domain plus the service registers.
    sgu1_reg_write(&sgu, 0x20, 0x40);
    requested_reset_parts = 0;
    sgu1_reg_write(&sgu, 0x18, 0xAF);
    CHECK(requested_reset_parts == SGU_RESET_ALL);
    CHECK(sgu.svc_master_vol == 0);
    sgu1_discard(&sgu);
}

TEST_CASE("reserved service offsets read zero and ignore writes") {
    auto sgu = make_sgu();
    select_bank(sgu, 0xFF);

    for (uint8_t reg = 0x11; reg <= 0x3E; reg++) {
        if (reg >= 0x1C && reg <= 0x20) {
            continue;  // sample window and master volume
        }
        if (reg == 0x18) {
            continue;  // CHIP_RESET, tested above
        }
        sgu1_reg_write(&sgu, reg, 0x5A);
        CHECK(sgu1_reg_read(&sgu, reg) == 0);
    }
    sgu1_discard(&sgu);
}
