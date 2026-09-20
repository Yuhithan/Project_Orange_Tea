#include "ora.h"

static int ora_add_overflows(uint64_t left, uint64_t right)
{
    return left > UINT64_MAX - right;
}

int ora_validate_header(const struct ora_header *header, uint64_t file_size)
{
    uint64_t payload_size;

    if (!header || header->magic != ORA_MAGIC || header->version != ORA_VERSION ||
        header->architecture != ORA_ARCH_X86_64 || header->header_size != ORA_HEADER_SIZE ||
        header->entry_point >= header->text_size || header->alignment == 0 ||
        (header->alignment & (header->alignment - 1)) != 0) {
        return ORA_ERR_FORMAT;
    }

    if (ora_add_overflows(header->text_size, header->data_size) ||
        ora_add_overflows(ORA_HEADER_SIZE, header->text_size + header->data_size)) {
        return ORA_ERR_RANGE;
    }

    payload_size = ORA_HEADER_SIZE + header->text_size + header->data_size;
    if (payload_size > file_size ||
        header->bss_size > UINT64_MAX - header->text_size - header->data_size) {
        return ORA_ERR_RANGE;
    }

    return ORA_OK;
}

int ora_load(const struct ora_loader *loader, uint64_t file_size)
{
    struct ora_header header;
    uint64_t image_size;
    unsigned char scratch[256];
    uint64_t offset;

    if (!loader || !loader->read || !loader->load || !loader->zero || !loader->enter) {
        return ORA_ERR_ARGUMENT;
    }
    if (sizeof(header) != ORA_HEADER_SIZE ||
        loader->read(loader->context, 0, &header, sizeof(header)) != (int)sizeof(header)) {
        return ORA_ERR_IO;
    }
    if (ora_validate_header(&header, file_size) != ORA_OK) return ORA_ERR_FORMAT;

    image_size = header.text_size + header.data_size + header.bss_size;
    if (image_size > loader->memory_limit ||
        loader->image_base > loader->memory_limit - image_size) {
        return ORA_ERR_RANGE;
    }

    offset = ORA_HEADER_SIZE;
    while (offset < ORA_HEADER_SIZE + header.text_size + header.data_size) {
        uint64_t remaining = ORA_HEADER_SIZE + header.text_size + header.data_size - offset;
        size_t chunk = remaining > sizeof(scratch) ? sizeof(scratch) : (size_t)remaining;
        uint64_t address = loader->image_base + offset - ORA_HEADER_SIZE;
        if (loader->read(loader->context, offset, scratch, chunk) != (int)chunk ||
            loader->load(loader->context, address, scratch, chunk) != ORA_OK) {
            return ORA_ERR_IO;
        }
        offset += chunk;
    }
    if (header.bss_size && loader->zero(loader->context,
        loader->image_base + header.text_size + header.data_size,
        (size_t)header.bss_size) != ORA_OK) return ORA_ERR_IO;

    return loader->enter(loader->context, loader->image_base + header.entry_point);
}
