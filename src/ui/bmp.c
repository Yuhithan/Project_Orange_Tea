#include "imp.h"
#include "imp_b.h"

int bmp_test() {
    int drawing[5][5] = {
        {1, 1, 1, 1, 1},
        {1, 0, 0, 0, 1},
        {1, 0, 0, 0, 1},
        {1, 0, 0, 0, 1},
        {1, 1, 1, 1, 1}
    };

    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            //printf("%d ", drawing[i][j]);
            if (drawing[i][j] == 1)
                imp_text("#");
            else
            imp_text(" ");
        }
        imp_text("\n");
    }

    return 0;
}