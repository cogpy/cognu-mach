// cog/mach/expression_ipc.hpp — Mach IPC Expression Transport
// Fixed-point tensor IPC for MetaHuman DNA expression pipeline
// Header-only, C++11, zero external dependencies
// SPDX-License-Identifier: MIT
//
// Implements Mach-style message passing for expression state transport
// between cognitive kernel modules. Uses fixed-point Q15 representation
// for efficient IPC without floating-point overhead.
//
#ifndef COG_MACH_EXPRESSION_IPC_HPP
#define COG_MACH_EXPRESSION_IPC_HPP

#include <cstdint>
#include <cstring>
#include <array>
#include <vector>

namespace cog { namespace mach {

// ─────────────────────────────────────────────────────────────────────────────
// Q15 Fixed-Point — 16-bit signed fixed-point for AU values
// Range: [-1.0, +0.999969] with precision of 1/32768
// ─────────────────────────────────────────────────────────────────────────────
struct Q15 {
    int16_t raw;

    Q15() : raw(0) {}
    explicit Q15(float v) : raw(float_to_q15(v)) {}

    float to_float() const {
        return static_cast<float>(raw) / 32768.0f;
    }

    static int16_t float_to_q15(float v) {
        if (v >= 1.0f) return 32767;
        if (v <= -1.0f) return -32768;
        return static_cast<int16_t>(v * 32768.0f);
    }

    Q15 operator+(Q15 other) const {
        int32_t sum = static_cast<int32_t>(raw) + other.raw;
        if (sum > 32767) sum = 32767;
        if (sum < -32768) sum = -32768;
        Q15 result;
        result.raw = static_cast<int16_t>(sum);
        return result;
    }

    Q15 operator*(Q15 other) const {
        int32_t prod = (static_cast<int32_t>(raw) * other.raw) >> 15;
        if (prod > 32767) prod = 32767;
        if (prod < -32768) prod = -32768;
        Q15 result;
        result.raw = static_cast<int16_t>(prod);
        return result;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Expression Message Types (Mach-style message IDs)
// ─────────────────────────────────────────────────────────────────────────────
enum ExprMsgType : uint32_t {
    EXPR_MSG_AU_STATE       = 0x45585031, // "EXP1" — Full AU state
    EXPR_MSG_MORPH_TARGETS  = 0x45585032, // "EXP2" — MetaHuman morph targets
    EXPR_MSG_MATERIAL       = 0x45585033, // "EXP3" — Material parameters
    EXPR_MSG_ENDOCRINE      = 0x45585034, // "EXP4" — Hormone concentrations
    EXPR_MSG_COGNITIVE      = 0x45585035, // "EXP5" — Cognitive mode
    EXPR_MSG_CHAOS_STATE    = 0x45585036, // "EXP6" — Lorenz attractor state
};

// ─────────────────────────────────────────────────────────────────────────────
// ExpressionMessage — Mach-style IPC message for expression data
// ─────────────────────────────────────────────────────────────────────────────
struct ExpressionMessage {
    // Header (16 bytes)
    uint32_t msg_type;
    uint32_t msg_size;    // Total message size in bytes
    uint32_t frame_number;
    uint32_t reserved;

    // Payload follows header
    static constexpr size_t HEADER_SIZE = 16;
    static constexpr size_t MAX_PAYLOAD = 256;

    uint8_t payload[MAX_PAYLOAD];

    ExpressionMessage() : msg_type(0), msg_size(HEADER_SIZE),
                          frame_number(0), reserved(0) {
        std::memset(payload, 0, MAX_PAYLOAD);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// AU State Message — 20 AU values in Q15 format (40 bytes payload)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr size_t NUM_AUS = 20;

struct AUStatePayload {
    Q15 au_values[NUM_AUS]; // 40 bytes
};

inline ExpressionMessage pack_au_state(
    uint32_t frame, const float au_values[NUM_AUS])
{
    ExpressionMessage msg;
    msg.msg_type = EXPR_MSG_AU_STATE;
    msg.frame_number = frame;
    msg.msg_size = ExpressionMessage::HEADER_SIZE + sizeof(AUStatePayload);

    AUStatePayload* p = reinterpret_cast<AUStatePayload*>(msg.payload);
    for (size_t i = 0; i < NUM_AUS; ++i) {
        p->au_values[i] = Q15(au_values[i]);
    }
    return msg;
}

inline void unpack_au_state(
    const ExpressionMessage& msg, float au_values[NUM_AUS])
{
    const AUStatePayload* p =
        reinterpret_cast<const AUStatePayload*>(msg.payload);
    for (size_t i = 0; i < NUM_AUS; ++i) {
        au_values[i] = p->au_values[i].to_float();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Endocrine State Message — 10 hormone concentrations in Q15
// ─────────────────────────────────────────────────────────────────────────────
static constexpr size_t NUM_HORMONES = 10;

struct EndocrinePayload {
    Q15 concentrations[NUM_HORMONES]; // 20 bytes
    uint8_t cognitive_mode;           // 1 byte
    uint8_t padding[3];               // Alignment
};

inline ExpressionMessage pack_endocrine(
    uint32_t frame, const float conc[NUM_HORMONES], uint8_t mode)
{
    ExpressionMessage msg;
    msg.msg_type = EXPR_MSG_ENDOCRINE;
    msg.frame_number = frame;
    msg.msg_size = ExpressionMessage::HEADER_SIZE + sizeof(EndocrinePayload);

    EndocrinePayload* p = reinterpret_cast<EndocrinePayload*>(msg.payload);
    for (size_t i = 0; i < NUM_HORMONES; ++i) {
        p->concentrations[i] = Q15(conc[i]);
    }
    p->cognitive_mode = mode;
    return msg;
}

// ─────────────────────────────────────────────────────────────────────────────
// Chaos State Message — Lorenz attractor state (12 bytes)
// ─────────────────────────────────────────────────────────────────────────────
struct ChaosPayload {
    float x, y, z; // 12 bytes (full precision needed for chaotic state)
};

inline ExpressionMessage pack_chaos_state(
    uint32_t frame, float x, float y, float z)
{
    ExpressionMessage msg;
    msg.msg_type = EXPR_MSG_CHAOS_STATE;
    msg.frame_number = frame;
    msg.msg_size = ExpressionMessage::HEADER_SIZE + sizeof(ChaosPayload);

    ChaosPayload* p = reinterpret_cast<ChaosPayload*>(msg.payload);
    p->x = x; p->y = y; p->z = z;
    return msg;
}

// ─────────────────────────────────────────────────────────────────────────────
// MessagePort — Simple message queue for IPC simulation
// ─────────────────────────────────────────────────────────────────────────────
class MessagePort {
public:
    explicit MessagePort(size_t capacity = 64) : capacity_(capacity) {}

    bool send(const ExpressionMessage& msg) {
        if (queue_.size() >= capacity_) return false;
        queue_.push_back(msg);
        return true;
    }

    bool receive(ExpressionMessage& msg) {
        if (queue_.empty()) return false;
        msg = queue_.front();
        queue_.erase(queue_.begin());
        return true;
    }

    bool empty() const { return queue_.empty(); }
    size_t pending() const { return queue_.size(); }

private:
    size_t capacity_;
    std::vector<ExpressionMessage> queue_;
};

}} // namespace cog::mach

#endif // COG_MACH_EXPRESSION_IPC_HPP
