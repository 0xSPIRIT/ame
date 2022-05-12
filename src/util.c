#include "util.h"

int sign(int n) {
    if (n < 0) {
        return -1;
    } else if (n > 0) {
        return 1;
    }
    return 0;
}

float lerp(float a, float b, float n) {
    return a + n*(b-a);
}