#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdbool.h>

#include "oras.h"

#define APP_VERSION "0.3-alpha"
#define APP_DIR "sdmc:/3ds/PokeBank-CFW"
#define BANK_FILE APP_DIR "/bank.dat"
#define BANK_MAGIC 0x46434250u
#define BANK_VERSION 1u
#define BANK_BOXES 100u
#define SLOTS_PER_BOX 30u
#define SLOT_SIZE 512u

#define BANK_FLAG_SHINY 0x0001u
#define BANK_FLAG_NATIVE_CHECKSUM_OK 0x0002u

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t box_count;
    uint32_t slots_per_box;
    uint32_t slot_size;
    uint32_t reserved[11];
} BankHeader;

typedef struct {
    uint8_t occupied;
    uint8_t generation;
    uint16_t payload_size;
    uint16_t species;
    uint16_t flags;
    uint32_t checksum;
    uint64_t source_title_id;
    uint8_t payload[SLOT_SIZE - 20];
} BankSlot;
#pragma pack(pop)

_Static_assert(sizeof(BankHeader) == 64, "BankHeader must be 64 bytes");
_Static_assert(sizeof(BankSlot) == SLOT_SIZE, "BankSlot must be 512 bytes");

/*
 * Keep the large box buffers out of the ARM11 thread stack.
 * A Bank box alone is 30 * 512 = 15 KiB, and the ORAS view adds
 * another ~7 KiB. Stacking both at once can cross the process stack
 * guard on real hardware even though the code compiles cleanly.
 */
static BankSlot s_bank_view[SLOTS_PER_BOX];
static OrasSlotInfo s_game_slots[ORAS_SLOTS_PER_BOX];

static bool ensure_dir(const char *path) {
    if (mkdir(path, 0777) == 0) return true;
    return errno == EEXIST;
}

static bool write_zeros(FILE *f, size_t bytes) {
    static uint8_t zeros[4096];
    while (bytes) {
        size_t chunk = bytes > sizeof(zeros) ? sizeof(zeros) : bytes;
        if (fwrite(zeros, 1, chunk, f) != chunk) return false;
        bytes -= chunk;
    }
    return true;
}

static bool bank_create(char *detail, size_t detail_size) {
    if (!ensure_dir("sdmc:/3ds") || !ensure_dir(APP_DIR)) {
        snprintf(detail, detail_size, "mkdir failed (%d)", errno);
        return false;
    }

    FILE *f = fopen(BANK_FILE, "wb");
    if (!f) {
        snprintf(detail, detail_size, "create failed (%d)", errno);
        return false;
    }

    BankHeader header;
    memset(&header, 0, sizeof(header));
    header.magic = BANK_MAGIC;
    header.version = BANK_VERSION;
    header.box_count = BANK_BOXES;
    header.slots_per_box = SLOTS_PER_BOX;
    header.slot_size = SLOT_SIZE;

    bool ok = fwrite(&header, 1, sizeof(header), f) == sizeof(header);
    if (ok) {
        ok = write_zeros(f, (size_t)BANK_BOXES * SLOTS_PER_BOX * SLOT_SIZE);
    }
    fflush(f);
    fclose(f);

    if (!ok) {
        snprintf(detail, detail_size, "bank write failed");
        return false;
    }

    snprintf(detail, detail_size, "created %u boxes", (unsigned)BANK_BOXES);
    return true;
}

static bool bank_validate(char *detail, size_t detail_size) {
    FILE *f = fopen(BANK_FILE, "rb");
    if (!f) {
        if (errno == ENOENT) return bank_create(detail, detail_size);
        snprintf(detail, detail_size, "open failed (%d)", errno);
        return false;
    }

    BankHeader h;
    size_t got = fread(&h, 1, sizeof(h), f);
    if (got != sizeof(h)) {
        fclose(f);
        snprintf(detail, detail_size, "header truncated");
        return false;
    }
    if (h.magic != BANK_MAGIC) {
        fclose(f);
        snprintf(detail, detail_size, "bad bank magic");
        return false;
    }
    if (h.version != BANK_VERSION || h.box_count != BANK_BOXES ||
        h.slots_per_box != SLOTS_PER_BOX || h.slot_size != SLOT_SIZE) {
        fclose(f);
        snprintf(detail, detail_size, "unsupported bank format");
        return false;
    }

    const long expected = (long)sizeof(BankHeader) +
        (long)(BANK_BOXES * SLOTS_PER_BOX * SLOT_SIZE);
    if (fseek(f, 0, SEEK_END) != 0 || ftell(f) != expected) {
        fclose(f);
        snprintf(detail, detail_size, "bank size mismatch");
        return false;
    }

    fclose(f);
    snprintf(detail, detail_size, "bank verified");
    return true;
}

static bool bank_read_box(unsigned box, BankSlot out[SLOTS_PER_BOX]) {
    if (!out || box >= BANK_BOXES) return false;
    FILE *f = fopen(BANK_FILE, "rb");
    if (!f) return false;

    const long offset = (long)sizeof(BankHeader) +
        (long)(box * SLOTS_PER_BOX * SLOT_SIZE);
    bool ok = fseek(f, offset, SEEK_SET) == 0 &&
              fread(out, sizeof(BankSlot), SLOTS_PER_BOX, f) == SLOTS_PER_BOX;
    fclose(f);
    return ok;
}

static uint32_t payload_checksum(const uint8_t *data, size_t size) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

static bool bank_write_pk6(unsigned box, unsigned slot,
                           const OrasSource *source,
                           const OrasSlotInfo *pk,
                           char *detail, size_t detail_size) {
    if (!source || !source->found || !pk || !pk->occupied ||
        box >= BANK_BOXES || slot >= SLOTS_PER_BOX) {
        snprintf(detail, detail_size, "Invalid deposit request");
        return false;
    }

    BankSlot record;
    memset(&record, 0, sizeof(record));
    record.occupied = 1;
    record.generation = 6;
    record.payload_size = PK6_BOX_LENGTH;
    record.species = pk->species;
    record.flags = (pk->shiny ? BANK_FLAG_SHINY : 0) |
                   (pk->checksum_valid ? BANK_FLAG_NATIVE_CHECKSUM_OK : 0);
    record.source_title_id = source->title_id;
    memcpy(record.payload, pk->raw, PK6_BOX_LENGTH);
    record.checksum = payload_checksum(record.payload, record.payload_size);

    FILE *f = fopen(BANK_FILE, "r+b");
    if (!f) {
        snprintf(detail, detail_size, "Bank open failed (%d)", errno);
        return false;
    }

    const long offset = (long)sizeof(BankHeader) +
        (long)((box * SLOTS_PER_BOX + slot) * SLOT_SIZE);
    bool ok = fseek(f, offset, SEEK_SET) == 0 &&
              fwrite(&record, 1, sizeof(record), f) == sizeof(record);
    fflush(f);
    fclose(f);

    if (!ok) {
        snprintf(detail, detail_size, "Bank slot write failed");
        return false;
    }

    snprintf(detail, detail_size, "Copied species %u to Bank %u/%u",
             pk->species, box + 1, slot + 1);
    return true;
}

static void move_selection(unsigned *selected, u32 down) {
    if ((down & KEY_LEFT) && (*selected % 6)) (*selected)--;
    if ((down & KEY_RIGHT) && (*selected % 6 < 5)) (*selected)++;
    if ((down & KEY_UP) && *selected >= 6) *selected -= 6;
    if ((down & KEY_DOWN) && *selected + 6 < SLOTS_PER_BOX) *selected += 6;
}

static void draw_grid_bank(const BankSlot slots[SLOTS_PER_BOX],
                           unsigned selected, bool focus) {
    for (unsigned row = 0; row < 5; ++row) {
        for (unsigned col = 0; col < 6; ++col) {
            unsigned s = row * 6 + col;
            char c = slots[s].occupied ? 'P' : '.';
            if (focus && s == selected) printf("[%c]", c);
            else if (!focus && s == selected) printf("(%c)", c);
            else printf(" %c ", c);
        }
        printf("\n");
    }
}

static void draw_grid_game(const OrasSlotInfo slots[ORAS_SLOTS_PER_BOX],
                           unsigned selected, bool focus) {
    for (unsigned row = 0; row < 5; ++row) {
        for (unsigned col = 0; col < 6; ++col) {
            unsigned s = row * 6 + col;
            char c = slots[s].occupied ? (slots[s].shiny ? '*' : 'P') : '.';
            if (focus && s == selected) printf("[%c]", c);
            else if (!focus && s == selected) printf("(%c)", c);
            else printf(" %c ", c);
        }
        printf("\n");
    }
}

static void draw_ui(PrintConsole *top, PrintConsole *bottom,
                    bool bank_ok, const char *bank_detail,
                    unsigned bank_box, unsigned bank_selected,
                    bool game_focus,
                    const OrasSource *source, bool game_box_ok,
                    const OrasSlotInfo game_slots[ORAS_SLOTS_PER_BOX],
                    unsigned game_box, unsigned game_selected,
                    const char *game_detail, const char *action_detail,
                    bool overwrite_armed) {
    memset(s_bank_view, 0, sizeof(s_bank_view));
    if (bank_ok) bank_read_box(bank_box, s_bank_view);

    consoleSelect(top);
    consoleClear();
    printf("\x1b[1;1HPOKEBANK-CFW v%s\n", APP_VERSION);
    printf("BANK BOX %03u/%u %s\n", bank_box + 1, (unsigned)BANK_BOXES,
           game_focus ? "" : "<FOCUS>");
    printf("%s\n\n", bank_ok ? bank_detail : "BANK ERROR");
    draw_grid_bank(s_bank_view, bank_selected, !game_focus);

    const BankSlot *b = &s_bank_view[bank_selected];
    printf("\nBank slot %u: ", bank_selected + 1);
    if (!b->occupied) {
        printf("empty\n");
    } else {
        printf("Gen %u species %u%s\n",
               b->generation, b->species,
               (b->flags & BANK_FLAG_SHINY) ? " SHINY" : "");
        printf("Native %u bytes | source %08lX\n",
               b->payload_size, (unsigned long)(u32)b->source_title_id);
    }

    printf("\n%s\n", action_detail);
    if (overwrite_armed) printf("OVERWRITE ARMED: press X again\n");

    consoleSelect(bottom);
    consoleClear();

    if (source && source->found) {
        printf("\x1b[1;1H%s - %s\n",
               oras_game_name(source->game), oras_media_name(source->media));
        printf("GAME BOX %02u/%u %s\n",
               game_box + 1, (unsigned)ORAS_BOX_COUNT,
               game_focus ? "<FOCUS>" : "");
        printf("%s\n\n", game_detail);

        if (game_box_ok) {
            draw_grid_game(game_slots, game_selected, game_focus);
            const OrasSlotInfo *g = &game_slots[game_selected];
            printf("\nGame slot %u: ", game_selected + 1);
            if (!g->occupied) {
                printf("empty\n");
            } else {
                printf("species %u%s\n", g->species, g->shiny ? " SHINY" : "");
                printf("Nature %u Ability %u PK6 %s\n",
                       g->nature, g->ability,
                       g->checksum_valid ? "OK" : "BAD CHECKSUM");
            }
        } else {
            printf("Unable to read game box.\n");
        }
    } else {
        printf("\x1b[1;1HORAS SOURCE: not connected\n\n");
        printf("%s\n", game_detail);
        printf("Press SELECT to scan cartridge/SD.\n");
    }

    printf("\nY Focus L/R Box D-Pad Slot\n");
    printf("A Inspect X Copy GAME->BANK\n");
    printf("B Refresh SELECT Detect START Exit\n");
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    gfxInitDefault();

    PrintConsole top, bottom;
    consoleInit(GFX_TOP, &top);
    consoleInit(GFX_BOTTOM, &bottom);

    char bank_detail[96];
    char game_detail[128];
    char action_detail[128] = "ORAS is read-only; only Bank is written.";

    bool bank_ok = bank_validate(bank_detail, sizeof(bank_detail));

    Result am_res = oras_services_init();
    if (R_FAILED(am_res)) {
        snprintf(game_detail, sizeof(game_detail),
                 "AM init failed: %08lX", (unsigned long)(u32)am_res);
    } else {
        snprintf(game_detail, sizeof(game_detail), "Scanning ORAS...");
    }

    OrasSource source;
    memset(&source, 0, sizeof(source));
    memset(s_game_slots, 0, sizeof(s_game_slots));

    unsigned bank_box = 0, bank_selected = 0;
    unsigned game_box = 0, game_selected = 0;
    bool game_focus = false, game_box_ok = false, overwrite_armed = false;

    if (R_SUCCEEDED(am_res) &&
        oras_detect(&source, game_detail, sizeof(game_detail))) {
        game_box = source.current_box;
        game_box_ok = oras_read_box(&source, game_box, s_game_slots,
                                    game_detail, sizeof(game_detail));
        game_focus = game_box_ok;
    }

    draw_ui(&top, &bottom, bank_ok, bank_detail,
            bank_box, bank_selected, game_focus,
            &source, game_box_ok, s_game_slots,
            game_box, game_selected,
            game_detail, action_detail, overwrite_armed);

    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        bool redraw = false;

        if (down & KEY_START) break;

        if (down & KEY_Y) {
            if (source.found && game_box_ok) {
                game_focus = !game_focus;
                overwrite_armed = false;
                snprintf(action_detail, sizeof(action_detail),
                         "Focus: %s", game_focus ? "GAME" : "BANK");
                redraw = true;
            }
        }

        if (down & (KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN)) {
            if (game_focus && source.found) move_selection(&game_selected, down);
            else move_selection(&bank_selected, down);
            overwrite_armed = false;
            redraw = true;
        }

        if (down & KEY_L) {
            if (game_focus && source.found) {
                game_box = (game_box + ORAS_BOX_COUNT - 1) % ORAS_BOX_COUNT;
                game_box_ok = oras_read_box(&source, game_box, s_game_slots,
                                            game_detail, sizeof(game_detail));
            } else {
                bank_box = (bank_box + BANK_BOXES - 1) % BANK_BOXES;
            }
            overwrite_armed = false;
            redraw = true;
        }

        if (down & KEY_R) {
            if (game_focus && source.found) {
                game_box = (game_box + 1) % ORAS_BOX_COUNT;
                game_box_ok = oras_read_box(&source, game_box, s_game_slots,
                                            game_detail, sizeof(game_detail));
            } else {
                bank_box = (bank_box + 1) % BANK_BOXES;
            }
            overwrite_armed = false;
            redraw = true;
        }

        if (down & KEY_SELECT) {
            memset(&source, 0, sizeof(source));
            memset(s_game_slots, 0, sizeof(s_game_slots));
            game_box_ok = false;
            if (oras_detect(&source, game_detail, sizeof(game_detail))) {
                game_box = source.current_box;
                game_selected = 0;
                game_box_ok = oras_read_box(&source, game_box, s_game_slots,
                                            game_detail, sizeof(game_detail));
                game_focus = game_box_ok;
                snprintf(action_detail, sizeof(action_detail),
                         "Connected to %s read-only.", oras_game_name(source.game));
            } else {
                game_focus = false;
                snprintf(action_detail, sizeof(action_detail), "ORAS source not available.");
            }
            overwrite_armed = false;
            redraw = true;
        }

        if (down & KEY_B) {
            if (source.found) {
                game_box_ok = oras_read_box(&source, game_box, s_game_slots,
                                            game_detail, sizeof(game_detail));
                snprintf(action_detail, sizeof(action_detail),
                         "%s", game_box_ok ? "Game box refreshed." : "Game refresh failed.");
            } else {
                bank_ok = bank_validate(bank_detail, sizeof(bank_detail));
                snprintf(action_detail, sizeof(action_detail), "Bank re-checked.");
            }
            overwrite_armed = false;
            redraw = true;
        }

        if (down & KEY_A) {
            if (game_focus && source.found && game_box_ok) {
                const OrasSlotInfo *g = &s_game_slots[game_selected];
                if (g->occupied) {
                    snprintf(action_detail, sizeof(action_detail),
                             "GAME %u/%u species %u PID %08lX%s",
                             game_box + 1, game_selected + 1, g->species,
                             (unsigned long)g->pid, g->shiny ? " SHINY" : "");
                } else {
                    snprintf(action_detail, sizeof(action_detail),
                             "GAME %u/%u is empty.", game_box + 1, game_selected + 1);
                }
            } else {
                BankSlot bank_slots[SLOTS_PER_BOX];
                if (bank_ok && bank_read_box(bank_box, bank_slots) &&
                    s_bank_view[bank_selected].occupied) {
                    BankSlot *b = &s_bank_view[bank_selected];
                    snprintf(action_detail, sizeof(action_detail),
                             "BANK %u/%u Gen %u species %u%s",
                             bank_box + 1, bank_selected + 1,
                             b->generation, b->species,
                             (b->flags & BANK_FLAG_SHINY) ? " SHINY" : "");
                } else {
                    snprintf(action_detail, sizeof(action_detail),
                             "BANK %u/%u is empty.", bank_box + 1, bank_selected + 1);
                }
            }
            redraw = true;
        }

        if (down & KEY_X) {
            if (!bank_ok) {
                snprintf(action_detail, sizeof(action_detail), "Bank is not writable.");
                overwrite_armed = false;
            } else if (!source.found || !game_box_ok) {
                snprintf(action_detail, sizeof(action_detail), "No readable ORAS source.");
                overwrite_armed = false;
            } else {
                const OrasSlotInfo *g = &s_game_slots[game_selected];
                if (!g->occupied) {
                    snprintf(action_detail, sizeof(action_detail), "Selected GAME slot is empty.");
                    overwrite_armed = false;
                } else if (!g->checksum_valid) {
                    snprintf(action_detail, sizeof(action_detail),
                             "Refusing copy: PK6 checksum is invalid.");
                    overwrite_armed = false;
                } else {
                    BankSlot bank_slots[SLOTS_PER_BOX];
                    bool dest_occupied = bank_read_box(bank_box, bank_slots) &&
                                         s_bank_view[bank_selected].occupied;
                    if (dest_occupied && !overwrite_armed) {
                        overwrite_armed = true;
                        snprintf(action_detail, sizeof(action_detail),
                                 "Destination occupied. X again to overwrite.");
                    } else {
                        if (bank_write_pk6(bank_box, bank_selected, &source, g,
                                           action_detail, sizeof(action_detail))) {
                            bank_ok = bank_validate(bank_detail, sizeof(bank_detail));
                        }
                        overwrite_armed = false;
                    }
                }
            }
            redraw = true;
        }

        if (redraw) {
            draw_ui(&top, &bottom, bank_ok, bank_detail,
                    bank_box, bank_selected, game_focus,
                    &source, game_box_ok, s_game_slots,
                    game_box, game_selected,
                    game_detail, action_detail, overwrite_armed);
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    oras_services_exit();
    gfxExit();
    return 0;
}
