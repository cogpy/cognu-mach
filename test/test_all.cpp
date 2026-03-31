// test/test_all.cpp — Minimal C++11 test suite for cognu-mach headers
// Tests core types and mach (Mach microkernel cognitive) modules.
// SPDX-License-Identifier: MIT

#include <cog/cog.hpp>
#include <iostream>
#include <cassert>
#include <cmath>
#include <sstream>

// ─────────────────────────────────────────────────────────────────────────────
// Simple test framework
// ─────────────────────────────────────────────────────────────────────────────
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                                                        \
    do {                                                                  \
        ++tests_run;                                                      \
        std::cout << "  [" << tests_run << "] " << #name << "... ";       \
        try {                                                             \
            test_##name();                                                \
            ++tests_passed;                                               \
            std::cout << "PASS" << std::endl;                             \
        } catch (const std::exception& e) {                               \
            std::cout << "FAIL: " << e.what() << std::endl;               \
        } catch (...) {                                                   \
            std::cout << "FAIL: unknown exception" << std::endl;          \
        }                                                                 \
    } while (0)

#define ASSERT_TRUE(cond) \
    if (!(cond)) throw std::runtime_error("assertion failed: " #cond)

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        std::ostringstream ss; \
        ss << "expected " << (a) << " == " << (b); \
        throw std::runtime_error(ss.str()); \
    }

// ─────────────────────────────────────────────────────────────────────────────
// Core module tests
// ─────────────────────────────────────────────────────────────────────────────

void test_handle_creation() {
    cog::Handle h1 = 1;
    cog::Handle h2 = 2;
    ASSERT_TRUE(h1 != h2);
    ASSERT_TRUE(h1 == static_cast<cog::Handle>(1));
    ASSERT_TRUE(cog::UNDEFINED_HANDLE == 0);
}

void test_atomspace_add_node() {
    cog::AtomSpace as;
    auto h = as.add_node(cog::AtomType::CONCEPT_NODE, "hello");
    ASSERT_TRUE(h != cog::UNDEFINED_HANDLE);
    auto* atom = as.get_atom(h);
    ASSERT_TRUE(atom != nullptr);
    ASSERT_EQ(atom->name, std::string("hello"));
    ASSERT_TRUE(atom->type == cog::AtomType::CONCEPT_NODE);
}

void test_atomspace_add_link() {
    cog::AtomSpace as;
    auto h1 = as.add_node(cog::AtomType::CONCEPT_NODE, "A");
    auto h2 = as.add_node(cog::AtomType::CONCEPT_NODE, "B");
    auto link = as.add_link(cog::AtomType::INHERITANCE_LINK, {h1, h2});
    ASSERT_TRUE(link != cog::UNDEFINED_HANDLE);
    auto* latom = as.get_atom(link);
    ASSERT_TRUE(latom != nullptr);
    ASSERT_EQ(latom->outgoing.size(), static_cast<size_t>(2));
}

// ─────────────────────────────────────────────────────────────────────────────
// Mach module tests — Fixed-point arithmetic
// ─────────────────────────────────────────────────────────────────────────────

void test_fixed_point_q16_16() {
    using Q = cog::mach::Q16_16;
    Q a = Q::from_int(3);
    Q b = Q::from_int(4);
    Q sum = a + b;
    ASSERT_EQ(sum.to_int(), 7);
}

void test_fixed_point_multiply() {
    using Q = cog::mach::Q16_16;
    Q a = Q::from_int(3);
    Q b = Q::from_int(5);
    Q prod = a * b;
    ASSERT_EQ(prod.to_int(), 15);
}

void test_fixed_point_from_float() {
    using Q = cog::mach::Q16_16;
    Q a = Q::from_float(2.5f);
    // 2.5 in Q16.16 = 2.5 * 65536 = 163840
    float back = a.to_float();
    ASSERT_TRUE(std::abs(back - 2.5f) < 0.001f);
}

void test_port_space() {
    cog::mach::PortSpace ps;
    auto p1 = ps.allocate();
    auto p2 = ps.allocate();
    ASSERT_TRUE(p1 != p2);
    // Send and receive a message
    cog::mach::MachMessage msg;
    msg.msgh_id = 42;
    ASSERT_TRUE(ps.send(p1, msg));
    cog::mach::MachMessage recv;
    ASSERT_TRUE(ps.receive(p1, recv));
    ASSERT_EQ(recv.msgh_id, static_cast<uint32_t>(42));
}

void test_kernel_atomspace() {
    cog::mach::KernelAtomSpace kas;
    auto h = kas.add_node(cog::AtomType::CONCEPT_NODE, "mach_test");
    ASSERT_TRUE(h != cog::UNDEFINED_HANDLE);
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "cognu-mach C++ test suite" << std::endl;
    std::cout << "========================" << std::endl;

    std::cout << "\n--- Core Module ---" << std::endl;
    TEST(handle_creation);
    TEST(atomspace_add_node);
    TEST(atomspace_add_link);

    std::cout << "\n--- Mach Module (Fixed-Point / IPC) ---" << std::endl;
    TEST(fixed_point_q16_16);
    TEST(fixed_point_multiply);
    TEST(fixed_point_from_float);
    TEST(port_space);
    TEST(kernel_atomspace);

    std::cout << "\n========================" << std::endl;
    std::cout << tests_passed << "/" << tests_run << " tests passed" << std::endl;

    return (tests_passed == tests_run) ? 0 : 1;
}
