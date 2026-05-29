// Copyright (c) 2026 Sondre Sjølyst
// Semantic version comparison extracted into a header so it can be
// unit-tested natively without pulling in any Arduino/ESP32 headers.

#pragma once
#include <cstdio>

// Returns negative if v1 < v2, zero if equal, positive if v1 > v2.
// Both strings are expected to use the format "vMAJOR.MINOR.PATCH".
// Malformed or missing components default to 0.
inline int versionCompare(const char *v1, const char *v2) {
  int maj1 = 0, min1 = 0, pat1 = 0;
  int maj2 = 0, min2 = 0, pat2 = 0;
  sscanf(v1, "v%d.%d.%d", &maj1, &min1, &pat1);
  sscanf(v2, "v%d.%d.%d", &maj2, &min2, &pat2);
  if (maj1 != maj2)
    return maj1 - maj2;
  if (min1 != min2)
    return min1 - min2;
  return pat1 - pat2;
}
