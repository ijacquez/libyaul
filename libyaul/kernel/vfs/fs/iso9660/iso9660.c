/*
 * Copyright (c) 2012-2019 Israel Jacquez
 * See LICENSE for details.
 *
 * Israel Jacquez <mrkotfw@gmail.com>
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <cd-block.h>

#include <scu/map.h>

#include "iso9660-internal.h"

#include "iso9660.h"

static struct {
        iso9660_pvd_t pvd;
} _state;

static inline uint32_t __always_inline
_length_sector_round(uint32_t length)
{
        return ((length + 0x07FF) >> 11);
}

static void _dirent_root_walk(iso9660_filelist_walk_t, void *);
static void _dirent_walk(iso9660_filelist_walk_t, uint32_t, void *);
static bool _dirent_interleave(const iso9660_dirent_t *);

static void _filelist_initialize(iso9660_filelist_t *, int32_t);
static void _filelist_entry_populate(const iso9660_dirent_t *, iso9660_entry_type_t,
    iso9660_filelist_entry_t *);
static void _filelist_read_walker(const iso9660_filelist_entry_t *, void *);

static void _bread(uint32_t, void *);

static uint8_t _sector_buffer[ISO9660_SECTOR_SIZE] __aligned(ISO9660_SECTOR_SIZE);

void
iso9660_filelist_root_read(iso9660_filelist_t *filelist, int32_t count)
{
        _filelist_initialize(filelist, count);

        iso9660_filelist_walk(NULL, _filelist_read_walker, filelist);
}

void
iso9660_filelist_read(const iso9660_filelist_entry_t root_entry,
    iso9660_filelist_t *filelist, int32_t count)
{
        _filelist_initialize(filelist, count);

        iso9660_filelist_walk(&root_entry, _filelist_read_walker, filelist);
}

void
iso9660_filelist_walk(const iso9660_filelist_entry_t *root_entry,
    iso9660_filelist_walk_t walker, void *args)
{
        if (root_entry == NULL) {
                /* Skip IP.BIN (16 sectors) */
                _bread(16, &_state.pvd);

                /* We are interested in the Primary Volume Descriptor, which
                 * points us to the root directory and path tables, which both
                 * allow us to find any file on the CD */

                /* Must be a "Primary Volume Descriptor" */
                assert(isonum_711(_state.pvd.type) == ISO_VD_PRIMARY);

                /* Must match 'CD001' */
#ifdef DEBUG
                const char *cd001_str;
                cd001_str = (const char *)_state.pvd.id;
                size_t cd001_len;
                cd001_len = sizeof(_state.pvd.id) + 1;

                assert((strncmp(cd001_str, ISO_STANDARD_ID, cd001_len)) != 0);
#endif /* DEBUG */

                /* Logical block size must be ISO9660_SECTOR_SIZE bytes */
                assert(isonum_723(_state.pvd.logical_block_size) == ISO9660_SECTOR_SIZE);

                _dirent_root_walk(walker, args);
        } else {
                const uint32_t sector = FAD2LBA(root_entry->starting_fad);

                _dirent_walk(walker, sector, args);
        }
}

static void
_filelist_read_walker(const iso9660_filelist_entry_t *entry, void *args)
{
        iso9660_filelist_t *filelist;
        filelist = args;

        if (filelist->entries_count >= filelist->entries_pooled_count) {
                return;
        }

        iso9660_filelist_entry_t *this_entry;
        this_entry = &filelist->entries[filelist->entries_count];

        (void)memcpy(this_entry, entry, sizeof(iso9660_filelist_entry_t));

        filelist->entries_count++;
}

static bool __unused
_dirent_interleave(const iso9660_dirent_t *dirent)
{
        return ((isonum_711(dirent->interleave)) != 0x00);
}

static void
_dirent_root_walk(iso9660_filelist_walk_t walker, void *args)
{
        iso9660_dirent_t dirent_root;

        /* Populate filesystem mount structure */
        /* Copy of directory record */
        (void)memcpy(&dirent_root, _state.pvd.root_directory_record, sizeof(dirent_root));

        /* Start walking the root directory */
        const uint32_t sector = isonum_733(dirent_root.extent);

        _dirent_walk(walker, sector, args);
}

static void
_dirent_walk(iso9660_filelist_walk_t walker, uint32_t sector, void *args)
{
        const iso9660_dirent_t *dirent;
        dirent = NULL;

        int32_t dirent_sectors;
        dirent_sectors = INT32_MAX;

        /* Keep track of where we are within the sector */
        uint32_t dirent_offset;
        dirent_offset = 0;

        while (true) {
                uint8_t dirent_length =
                    (dirent != NULL) ? isonum_711(dirent->length) : 0;

                if ((dirent == NULL) ||
                    (dirent_length == 0) ||
                    ((dirent_offset + dirent_length) >= ISO9660_SECTOR_SIZE)) {
                        dirent_sectors--;

                        if (dirent_sectors == 0) {
                                break;
                        }

                        dirent_offset = 0;

                        _bread(sector, _sector_buffer);

                        dirent = (const iso9660_dirent_t *)&_sector_buffer[0];
                        dirent_length = isonum_711(dirent->length);

                        if (dirent->name[0] == '\0') {
                                uint32_t data_length;
                                data_length = isonum_733(dirent->data_length);

                                dirent_sectors = _length_sector_round(data_length);
                        }

                        /* Interleave mode must be disabled */
                        assert(!(_dirent_interleave(dirent)));

                        /* Next sector */
                        sector++;
                }

                /* Check for Current directory ('\0') or parent directory ('\1') */
                if ((dirent->name[0] != '\0') && (dirent->name[0] != '\1')) {
                        /* Interleave mode must be disabled */
                        assert(!(_dirent_interleave(dirent)));

                        if (walker != NULL) {
                                const uint8_t file_flags = isonum_711(dirent->file_flags);

                                iso9660_entry_type_t type;
                                type = ISO9660_ENTRY_TYPE_FILE;

                                if ((file_flags & DIRENT_FILE_FLAGS_DIRECTORY) == DIRENT_FILE_FLAGS_DIRECTORY) {
                                        type = ISO9660_ENTRY_TYPE_DIRECTORY;
                                }

                                iso9660_filelist_entry_t filelist_entry;

                                _filelist_entry_populate(dirent, type, &filelist_entry);

                                walker(&filelist_entry, args);
                        }
                }

                if (dirent != NULL) {
                        const uintptr_t p = (uintptr_t)dirent + dirent_length;

                        dirent = (const iso9660_dirent_t *)p;

                        dirent_offset += dirent_length;
                }
        }
}

static void
_filelist_initialize(iso9660_filelist_t *filelist, int32_t count)
{
        int32_t clamped_count;
        clamped_count = count;

        if (clamped_count <= 0) {
                clamped_count = ISO9660_FILELIST_ENTRIES_COUNT;
        } else if (clamped_count > ISO9660_FILELIST_ENTRIES_COUNT) {
                clamped_count = ISO9660_FILELIST_ENTRIES_COUNT;
        }

        if (filelist->entries == NULL) {
                filelist->entries = malloc(sizeof(iso9660_filelist_entry_t) * clamped_count);
        }

        assert(filelist->entries != NULL);

        filelist->entries_pooled_count = (uint32_t)clamped_count;
        filelist->entries_count = 0;
}

static void
_filelist_entry_populate(const iso9660_dirent_t *dirent, iso9660_entry_type_t type,
    iso9660_filelist_entry_t *filelist_entry)
{
        filelist_entry->type = type;
        filelist_entry->size = isonum_733(dirent->data_length);
        filelist_entry->starting_fad = LBA2FAD(isonum_733(dirent->extent));
        filelist_entry->sector_count = _length_sector_round(filelist_entry->size);

        uint8_t name_len;
        name_len = isonum_711(dirent->file_id_len);

        if (type == ISO9660_ENTRY_TYPE_FILE) {
                /* Minus the ';1' */
                name_len -= 2;

                /* Empty extension */
                if (name_len > 0 && dirent->name[name_len - 1] == '.')
                        name_len--;
        }

        (void)memcpy(filelist_entry->name, dirent->name, name_len);
        filelist_entry->name[name_len] = '\0';
}

static void
_bread(uint32_t sector, void *ptr)
{
        int ret __unused;
        ret = cd_block_sector_read(LBA2FAD(sector), ptr);
        assert(ret == 0);
}
