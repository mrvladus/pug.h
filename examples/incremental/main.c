#include "add.h"
#include "subtract.h"

#include <stdio.h>

int main() {
  int sum = add(1, 1);
  printf("Sum is: %d\n", sum);
  int diff = subtract(1, 1);
  printf("Difference is: %d\n", diff);
  return 0;
}
