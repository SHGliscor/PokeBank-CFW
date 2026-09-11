#pragma once

#include <3ds.h>
#include <stdbool.h>
#include <stddef.h>

#define ORAS_BOX_COUNT 31u
#define ORAS_SLOTS_PER_BOX 30u
#define PK6_BOX_LENGTH 232u

typedef enum {
    ORAS_GAME_NONE = 0,
    ORAS_GAME_OMEGA_RUBY,
    ORAS_GAME_ALPHA_SAPPHIRE
} OrasGame;

typedef struct {
    bool found;
    OrasGame game;
    u64 title_id;
    FS_MediaType media;
    u8 current_box;
    u64 save_size;
} OrasSource;

typedef struct {
    bool occupied;
    bool checksum_valid;
    bool shiny;
    u16 species;
    u16 tid;
    u16 sid;
    u32 pid;
    u8 nature;
    u8 ability;
    u8 raw[PK6_BOX_LENGTH];
} OrasSlotInfo;

Result oras_services_init(void);
void oras_services_exit(void);

bool oras_detect(OrasSource *out, char *detail, size_t detail_size);
bool oras_read_box(const OrasSource *source, unsigned box,
                   OrasSlotInfo out[ORAS_SLOTS_PER_BOX],
                   char *detail, size_t detail_size);

const char *oras_game_name(OrasGame game);
const char *oras_media_name(FS_MediaType media);
