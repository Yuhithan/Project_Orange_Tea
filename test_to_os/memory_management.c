#include <stdlib.h>

int main(void) {
    int *a = malloc(sizeof(int) * 10);

    for (int i = 0; i < 10; i++) a[i] = 10 - i;

    for (int i = 0; i < 10; i++)
        printf("a[%d] = %d\n", i, a[i]);
    printf("a: %p\n", a);

    free(a);

    return 0;
}