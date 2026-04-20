#pragma once
#include <iostream>

#define LOG_DEBUG(msg, ...) fprintf(stdout, "[DEBUG] %s:%d | " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(msg, ...) fprintf(stderr, "[ERROR] %s:%d | " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__)