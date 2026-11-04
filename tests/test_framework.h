/* Minimal host test framework. */
#pragma once

#include <math.h>
#include <stdio.h>
#include <string.h>

extern int pm_test_failures;
extern int pm_test_checks;

/* True only when |a - b| <= eps. Written so that NaN (and opposite
 * infinities) compare as NOT near, instead of slipping through. */
static inline int pm_test_near(double a, double b, double eps) {
    return fabs(a - b) <= eps;
}

#define CHECK(cond)                                                          \
    do {                                                                     \
        pm_test_checks++;                                                    \
        if(!(cond)) {                                                        \
            pm_test_failures++;                                              \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
        }                                                                    \
    } while(0)

#define CHECK_NEAR(a, b, eps)                                                \
    do {                                                                     \
        pm_test_checks++;                                                    \
        const double _a = (double)(a);                                       \
        const double _b = (double)(b);                                       \
        if(!pm_test_near(_a, _b, (double)(eps))) {                           \
            pm_test_failures++;                                              \
            printf(                                                          \
                "  FAIL %s:%d: %s = %.6f, expected %.6f\n",                  \
                __FILE__,                                                    \
                __LINE__,                                                    \
                #a,                                                          \
                _a,                                                          \
                _b);                                                         \
        }                                                                    \
    } while(0)

#define CHECK_STR(a, b)                                                      \
    do {                                                                     \
        pm_test_checks++;                                                    \
        if(strcmp((a), (b)) != 0) {                                          \
            pm_test_failures++;                                              \
            printf("  FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); \
        }                                                                    \
    } while(0)

#define RUN(test)                    \
    do {                             \
        printf("- %s\n", #test);     \
        test();                      \
    } while(0)

void test_energy_suite(void);
void test_stats_suite(void);
void test_history_suite(void);
void test_charge_suite(void);
void test_health_suite(void);
void test_format_suite(void);
void test_csv_suite(void);
void test_settings_suite(void);
