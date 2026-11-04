/* Host stub of the parts of furi.h used by the modules under test. */
#pragma once

#include <stdio.h>
#include <stdlib.h>

/* Not evaluated: sizeof only type-checks the format and its arguments. */
#define FURI_LOG_E(tag, ...) ((void)(tag), (void)sizeof(printf(__VA_ARGS__)))
#define FURI_LOG_I(tag, ...) ((void)(tag), (void)sizeof(printf(__VA_ARGS__)))
