#ifndef TEST_STM32_H
#define TEST_STM32_H
#include <stdint.h>
extern uint32_t test_ipsr;
static inline uint32_t __get_IPSR(void) { return test_ipsr; }
#endif
