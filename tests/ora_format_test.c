#include <assert.h>
#include <stdio.h>
#include "ora.h"

int main(void)
{
    struct ora_header header = {
        ORA_MAGIC, ORA_VERSION, ORA_ARCH_X86_64, ORA_HEADER_SIZE, 0,
        0, 4, 4, 8, 16
    };

    assert(sizeof(header) == ORA_HEADER_SIZE);
    assert(ora_validate_header(&header, ORA_HEADER_SIZE + 8) == ORA_OK);
    header.magic = 0;
    assert(ora_validate_header(&header, ORA_HEADER_SIZE + 8) == ORA_ERR_FORMAT);
    header.magic = ORA_MAGIC;
    header.alignment = 3;
    assert(ora_validate_header(&header, ORA_HEADER_SIZE + 8) == ORA_ERR_FORMAT);
    header.alignment = 16;
    header.text_size = 20;
    assert(ora_validate_header(&header, ORA_HEADER_SIZE + 8) == ORA_ERR_RANGE);

    puts("ora format test passed");
    return 0;
}