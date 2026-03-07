// cog/mach/mach.hpp — Mach Microkernel Cognitive Extensions
// Fixed-point tensors, IPC ports, VM map, kernel-safe AtomSpace
// Header-only, C++11, zero external dependencies
// SPDX-License-Identifier: MIT
#ifndef COG_MACH_HPP
#define COG_MACH_HPP

#include "../core/core.hpp"
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cassert>
#include <type_traits>
#include <limits>

namespace cog { namespace mach {

// ─────────────────────────────────────────────────────────────────────────────
// Fixed-Point Arithmetic — Q16.16 and Q8.24 formats
// Kernel-safe: no floating point, no heap allocation
// ─────────────────────────────────────────────────────────────────────────────
template<int IntBits, int FracBits>
struct FixedPoint {
    static_assert(IntBits + FracBits == 32, "Must be 32-bit total");
    int32_t raw;

    FixedPoint() : raw(0) {}
    explicit FixedPoint(int32_t r) : raw(r) {}

    static FixedPoint from_int(int32_t v) {
        return FixedPoint(v << FracBits);
    }

    static FixedPoint from_float(float v) {
        return FixedPoint(static_cast<int32_t>(v * (1 << FracBits)));
    }

    float to_float() const {
        return static_cast<float>(raw) / static_cast<float>(1 << FracBits);
    }

    int32_t to_int() const {
        return raw >> FracBits;
    }

    FixedPoint operator+(const FixedPoint& o) const { return FixedPoint(raw + o.raw); }
    FixedPoint operator-(const FixedPoint& o) const { return FixedPoint(raw - o.raw); }

    FixedPoint operator*(const FixedPoint& o) const {
        int64_t product = static_cast<int64_t>(raw) * static_cast<int64_t>(o.raw);
        return FixedPoint(static_cast<int32_t>(product >> FracBits));
    }

    FixedPoint operator/(const FixedPoint& o) const {
        assert(o.raw != 0);
        int64_t dividend = static_cast<int64_t>(raw) << FracBits;
        return FixedPoint(static_cast<int32_t>(dividend / o.raw));
    }

    FixedPoint& operator+=(const FixedPoint& o) { raw += o.raw; return *this; }
    FixedPoint& operator-=(const FixedPoint& o) { raw -= o.raw; return *this; }
    FixedPoint& operator*=(const FixedPoint& o) { *this = *this * o; return *this; }

    bool operator==(const FixedPoint& o) const { return raw == o.raw; }
    bool operator!=(const FixedPoint& o) const { return raw != o.raw; }
    bool operator<(const FixedPoint& o) const  { return raw < o.raw; }
    bool operator>(const FixedPoint& o) const  { return raw > o.raw; }
    bool operator<=(const FixedPoint& o) const { return raw <= o.raw; }
    bool operator>=(const FixedPoint& o) const { return raw >= o.raw; }

    FixedPoint abs() const { return FixedPoint(raw < 0 ? -raw : raw); }

    // Fixed-point tanh approximation: tanh(x) ≈ x / (1 + |x|)
    FixedPoint tanh_approx() const {
        auto one = from_int(1);
        auto ax = this->abs();
        auto denom = one + ax;
        return *this / denom;
    }

    // Fixed-point sigmoid: 1 / (1 + exp(-x)) ≈ 0.5 + 0.5 * tanh(x/2)
    FixedPoint sigmoid_approx() const {
        auto half = from_float(0.5f);
        auto half_x = FixedPoint(raw >> 1);
        return half + half * half_x.tanh_approx();
    }
};

using Q16_16 = FixedPoint<16, 16>;
using Q8_24  = FixedPoint<8, 24>;

// ─────────────────────────────────────────────────────────────────────────────
// Tensor — Fixed-point N-dimensional tensor (arena-allocated)
// ─────────────────────────────────────────────────────────────────────────────
template<typename Scalar = Q16_16, size_t MaxDims = 4>
class Tensor {
public:
    Tensor() : ndim_(0), size_(0), data_(nullptr), owned_(false) {}

    // Create tensor with given shape, allocating from arena
    Tensor(Arena& arena, const std::array<size_t, MaxDims>& shape, size_t ndim)
        : ndim_(ndim), owned_(false)
    {
        assert(ndim <= MaxDims);
        shape_ = shape;
        size_ = 1;
        for (size_t i = 0; i < ndim; ++i) {
            size_ *= shape[i];
            strides_[ndim - 1 - i] = (i == 0) ? 1 : strides_[ndim - i] * shape[ndim - i];
        }
        data_ = arena.create<Scalar>(/* dummy */);
        // Actually allocate array
        if (size_ > 0) {
            void* mem = arena.alloc(sizeof(Scalar) * size_, alignof(Scalar));
            data_ = static_cast<Scalar*>(mem);
            if (data_) {
                for (size_t i = 0; i < size_; ++i) {
                    new(&data_[i]) Scalar();
                }
            }
        }
    }

    // Create from vector (heap-allocated, for convenience)
    explicit Tensor(const std::vector<Scalar>& data, size_t rows, size_t cols)
        : ndim_(2), owned_(true)
    {
        shape_[0] = rows;
        shape_[1] = cols;
        strides_[0] = cols;
        strides_[1] = 1;
        size_ = rows * cols;
        data_ = new Scalar[size_];
        for (size_t i = 0; i < size_ && i < data.size(); ++i) {
            data_[i] = data[i];
        }
    }

    ~Tensor() {
        if (owned_ && data_) delete[] data_;
    }

    // Element access
    Scalar& at(size_t i) { assert(i < size_); return data_[i]; }
    const Scalar& at(size_t i) const { assert(i < size_); return data_[i]; }

    Scalar& at(size_t r, size_t c) {
        assert(ndim_ >= 2);
        return data_[r * strides_[0] + c * strides_[1]];
    }
    const Scalar& at(size_t r, size_t c) const {
        assert(ndim_ >= 2);
        return data_[r * strides_[0] + c * strides_[1]];
    }

    // Matrix-vector multiply: y = A * x
    static void matvec(const Tensor& A, const Tensor& x, Tensor& y) {
        assert(A.ndim_ >= 2 && x.ndim_ >= 1 && y.ndim_ >= 1);
        size_t rows = A.shape_[0];
        size_t cols = A.shape_[1];
        assert(x.size_ >= cols && y.size_ >= rows);
        for (size_t i = 0; i < rows; ++i) {
            Scalar sum;
            for (size_t j = 0; j < cols; ++j) {
                sum += A.at(i, j) * x.at(j);
            }
            y.at(i) = sum;
        }
    }

    // Element-wise tanh
    void apply_tanh() {
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = data_[i].tanh_approx();
        }
    }

    // Fill with value
    void fill(Scalar val) {
        for (size_t i = 0; i < size_; ++i) data_[i] = val;
    }

    size_t ndim() const { return ndim_; }
    size_t size() const { return size_; }
    const std::array<size_t, MaxDims>& shape() const { return shape_; }
    Scalar* data() { return data_; }
    const Scalar* data() const { return data_; }

private:
    size_t ndim_;
    size_t size_;
    std::array<size_t, MaxDims> shape_;
    std::array<size_t, MaxDims> strides_;
    Scalar* data_;
    bool owned_;
};

// ─────────────────────────────────────────────────────────────────────────────
// Embedding — Fixed-point embedding table
// ─────────────────────────────────────────────────────────────────────────────
template<typename Scalar = Q16_16>
class Embedding {
public:
    Embedding() : vocab_size_(0), embed_dim_(0) {}

    Embedding(size_t vocab_size, size_t embed_dim)
        : vocab_size_(vocab_size), embed_dim_(embed_dim),
          table_(vocab_size * embed_dim)
    {
        // Initialize with small random-ish values from hash
        for (size_t i = 0; i < table_.size(); ++i) {
            uint32_t h = hash32(static_cast<uint32_t>(i));
            float val = (static_cast<float>(h & 0xFFFF) / 65536.0f - 0.5f) * 0.1f;
            table_[i] = Scalar::from_float(val);
        }
    }

    // Lookup embedding for token id
    const Scalar* lookup(size_t token_id) const {
        assert(token_id < vocab_size_);
        return &table_[token_id * embed_dim_];
    }

    // Copy embedding into output buffer
    void lookup(size_t token_id, Scalar* out) const {
        const Scalar* src = lookup(token_id);
        for (size_t i = 0; i < embed_dim_; ++i) out[i] = src[i];
    }

    size_t vocab_size() const { return vocab_size_; }
    size_t embed_dim() const { return embed_dim_; }

private:
    size_t vocab_size_;
    size_t embed_dim_;
    std::vector<Scalar> table_;

    static uint32_t hash32(uint32_t x) {
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = (x >> 16) ^ x;
        return x;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// IPC Port — Mach-style inter-process communication
// ─────────────────────────────────────────────────────────────────────────────
struct PortRight {
    enum Type : uint8_t { SEND = 1, RECEIVE = 2, SEND_ONCE = 3 };
    Type type;
    uint32_t port_name;
};

struct MachMessage {
    uint32_t msgh_bits;
    uint32_t msgh_size;
    uint32_t msgh_remote_port;
    uint32_t msgh_local_port;
    uint32_t msgh_id;
    std::vector<uint8_t> body;

    MachMessage() : msgh_bits(0), msgh_size(0),
                    msgh_remote_port(0), msgh_local_port(0), msgh_id(0) {}
};

class PortSpace {
public:
    PortSpace() : next_name_(1) {}

    // Allocate a new port with receive right
    uint32_t allocate() {
        uint32_t name = next_name_++;
        ports_[name] = PortState();
        return name;
    }

    // Send a message to a port
    bool send(uint32_t port_name, const MachMessage& msg) {
        auto it = ports_.find(port_name);
        if (it == ports_.end()) return false;
        it->second.queue.push_back(msg);
        return true;
    }

    // Receive a message from a port
    bool receive(uint32_t port_name, MachMessage& msg) {
        auto it = ports_.find(port_name);
        if (it == ports_.end() || it->second.queue.empty()) return false;
        msg = it->second.queue.front();
        it->second.queue.erase(it->second.queue.begin());
        return true;
    }

    // Deallocate a port
    bool deallocate(uint32_t port_name) {
        return ports_.erase(port_name) > 0;
    }

    size_t queue_size(uint32_t port_name) const {
        auto it = ports_.find(port_name);
        return (it != ports_.end()) ? it->second.queue.size() : 0;
    }

private:
    struct PortState {
        std::vector<MachMessage> queue;
    };
    uint32_t next_name_;
    std::unordered_map<uint32_t, PortState> ports_;
};

// ─────────────────────────────────────────────────────────────────────────────
// VMMap — Virtual memory map with cognitive annotations
// ─────────────────────────────────────────────────────────────────────────────
struct VMEntry {
    uint64_t start;
    uint64_t end;
    uint32_t protection;  // PROT_READ | PROT_WRITE | PROT_EXEC
    std::string name;
    Handle atom_handle;   // Associated atom in AtomSpace

    static constexpr uint32_t PROT_READ  = 0x01;
    static constexpr uint32_t PROT_WRITE = 0x02;
    static constexpr uint32_t PROT_EXEC  = 0x04;
    static constexpr uint32_t PROT_COG   = 0x80; // Cognitive region flag

    uint64_t size() const { return end - start; }
    bool is_cognitive() const { return (protection & PROT_COG) != 0; }
};

class VMMap {
public:
    VMMap() : next_addr_(0x10000) {}

    // Allocate a region
    VMEntry allocate(uint64_t size, uint32_t prot, const std::string& name = "",
                     Handle atom = UNDEFINED_HANDLE) {
        uint64_t aligned_addr = (next_addr_ + 0xFFF) & ~0xFFFULL;
        uint64_t aligned_size = (size + 0xFFF) & ~0xFFFULL;

        VMEntry entry;
        entry.start = aligned_addr;
        entry.end = aligned_addr + aligned_size;
        entry.protection = prot;
        entry.name = name;
        entry.atom_handle = atom;

        entries_.push_back(entry);
        next_addr_ = entry.end;
        return entry;
    }

    // Find region containing address
    const VMEntry* find(uint64_t addr) const {
        for (auto& e : entries_) {
            if (addr >= e.start && addr < e.end) return &e;
        }
        return nullptr;
    }

    // Find all cognitive regions
    std::vector<const VMEntry*> cognitive_regions() const {
        std::vector<const VMEntry*> result;
        for (auto& e : entries_) {
            if (e.is_cognitive()) result.push_back(&e);
        }
        return result;
    }

    // Deallocate region
    bool deallocate(uint64_t start) {
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it->start == start) {
                entries_.erase(it);
                return true;
            }
        }
        return false;
    }

    const std::vector<VMEntry>& entries() const { return entries_; }

private:
    uint64_t next_addr_;
    std::vector<VMEntry> entries_;
};

// ─────────────────────────────────────────────────────────────────────────────
// KernelAtomSpace — Arena-based AtomSpace for kernel contexts
// No heap allocation after construction
// ─────────────────────────────────────────────────────────────────────────────
class KernelAtomSpace {
public:
    static constexpr size_t MAX_ATOMS = 4096;
    static constexpr size_t MAX_OUTGOING = 8;

    struct KAtom {
        Handle handle;
        AtomType type;
        char name[64];
        Handle outgoing[MAX_OUTGOING];
        uint8_t outgoing_count;
        Q16_16 tv_strength;
        Q16_16 tv_confidence;
        int16_t av_sti;
        bool active;

        KAtom() : handle(UNDEFINED_HANDLE), type(AtomType::NOTYPE),
                  outgoing_count(0), av_sti(0), active(false) {
            name[0] = '\0';
            for (auto& h : outgoing) h = UNDEFINED_HANDLE;
            tv_strength = Q16_16::from_float(1.0f);
            tv_confidence = Q16_16::from_float(0.0f);
        }
    };

    KernelAtomSpace() : count_(0), next_handle_(1) {
        for (auto& a : atoms_) a.active = false;
    }

    Handle add_node(AtomType type, const char* name,
                    Q16_16 strength = Q16_16::from_float(1.0f),
                    Q16_16 confidence = Q16_16::from_float(0.0f)) {
        if (count_ >= MAX_ATOMS) return UNDEFINED_HANDLE;
        // Find free slot
        for (size_t i = 0; i < MAX_ATOMS; ++i) {
            if (!atoms_[i].active) {
                KAtom& a = atoms_[i];
                a.handle = next_handle_++;
                a.type = type;
                std::strncpy(a.name, name, 63);
                a.name[63] = '\0';
                a.outgoing_count = 0;
                a.tv_strength = strength;
                a.tv_confidence = confidence;
                a.active = true;
                ++count_;
                return a.handle;
            }
        }
        return UNDEFINED_HANDLE;
    }

    Handle add_link(AtomType type, const Handle* outgoing, uint8_t count,
                    Q16_16 strength = Q16_16::from_float(1.0f),
                    Q16_16 confidence = Q16_16::from_float(0.0f)) {
        if (count_ >= MAX_ATOMS || count > MAX_OUTGOING) return UNDEFINED_HANDLE;
        for (size_t i = 0; i < MAX_ATOMS; ++i) {
            if (!atoms_[i].active) {
                KAtom& a = atoms_[i];
                a.handle = next_handle_++;
                a.type = type;
                a.name[0] = '\0';
                a.outgoing_count = count;
                for (uint8_t j = 0; j < count; ++j) a.outgoing[j] = outgoing[j];
                a.tv_strength = strength;
                a.tv_confidence = confidence;
                a.active = true;
                ++count_;
                return a.handle;
            }
        }
        return UNDEFINED_HANDLE;
    }

    const KAtom* get(Handle h) const {
        for (size_t i = 0; i < MAX_ATOMS; ++i) {
            if (atoms_[i].active && atoms_[i].handle == h) return &atoms_[i];
        }
        return nullptr;
    }

    bool remove(Handle h) {
        for (size_t i = 0; i < MAX_ATOMS; ++i) {
            if (atoms_[i].active && atoms_[i].handle == h) {
                atoms_[i].active = false;
                --count_;
                return true;
            }
        }
        return false;
    }

    size_t count() const { return count_; }

private:
    KAtom atoms_[MAX_ATOMS];
    size_t count_;
    Handle next_handle_;
};

}} // namespace cog::mach

#endif // COG_MACH_HPP
