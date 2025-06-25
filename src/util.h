#pragma once
#include <stddef.h>

typedef struct {
  int x, y;
} Position;

typedef struct {
  int width, height;
} Size;

typedef struct {
  int x, y, width, height;
} Box;
