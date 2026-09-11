#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdbool.h>

#include "oras.h"
#include "game_select.h"
#include "ui.h"

#define APP_VERSION "0.5-alpha"
#define APP_DIR "sdmc:/3ds/PokeBank-CFW"
#define BANK_FILE APP_DIR "/bank.dat"
#define BACKUP_DIR APP_DIR "/backups"
#define BANK_BACKUP_LATEST BACKUP_DIR "/bank-latest.bak"
#define BANK_BACKUP_PREVIOUS BACKUP_DIR "/bank-previous.bak"
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
static uint8_t s_file_copy_buffer[4096];

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


static bool copy_regular_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return false;
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return false;
    }

    bool ok = true;
    size_t got;
    while ((got = fread(s_file_copy_buffer, 1, sizeof(s_file_copy_buffer), in)) > 0) {
        if (fwrite(s_file_copy_buffer, 1, got, out) != got) {
            ok = false;
            break;
        }
    }
    if (ferror(in)) ok = false;
    fflush(out);
    fclose(out);
    fclose(in);
    if (!ok) remove(dst);
    return ok;
}

static bool bank_backup_before_write(char *detail, size_t detail_size) {
    if (!ensure_dir(BACKUP_DIR)) {
        snprintf(detail, detail_size, "Bank backup dir failed (%d)", errno);
        return false;
    }

    remove(BANK_BACKUP_PREVIOUS);
    if (rename(BANK_BACKUP_LATEST, BANK_BACKUP_PREVIOUS) != 0 &&
        errno != ENOENT) {
        snprintf(detail, detail_size, "Bank backup rotate failed (%d)", errno);
        return false;
    }

    if (!copy_regular_file(BANK_FILE, BANK_BACKUP_LATEST)) {
        snprintf(detail, detail_size, "Bank backup failed");
        return false;
    }
    return true;
}

static bool bank_read_slot(unsigned box, unsigned slot, BankSlot *out) {
    if (!out || box >= BANK_BOXES || slot >= SLOTS_PER_BOX) return false;
    FILE *f = fopen(BANK_FILE, "rb");
    if (!f) return false;
    const long offset = (long)sizeof(BankHeader) +
        (long)((box * SLOTS_PER_BOX + slot) * SLOT_SIZE);
    bool ok = fseek(f, offset, SEEK_SET) == 0 &&
              fread(out, 1, sizeof(*out), f) == sizeof(*out);
    fclose(f);
    return ok;
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

    if (!bank_backup_before_write(detail, detail_size)) {
        return false;
    }

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

    BankSlot verify;
    if (!bank_read_slot(box, slot, &verify) ||
        memcmp(&verify, &record, sizeof(record)) != 0) {
        snprintf(detail, detail_size, "Bank write verify failed");
        return false;
    }

    snprintf(detail, detail_size, "GAME->BANK species %u copied + backup",
             pk->species);
    return true;
}

static UiPokemon s_bank_ui[SLOTS_PER_BOX];
static UiPokemon s_game_ui[SLOTS_PER_BOX];
static UiPokemon s_detail_ui;

typedef enum {
    SCREEN_GAME_SELECTOR = 0,
    SCREEN_BANK
} AppScreen;

static void move_selection(unsigned *selected, u32 down)
{
    if ((down & KEY_LEFT) && (*selected % 6)) (*selected)--;
    if ((down & KEY_RIGHT) && (*selected % 6 < 5)) (*selected)++;
    if ((down & KEY_UP) && *selected >= 6) *selected -= 6;
    if ((down & KEY_DOWN) && *selected + 6 < SLOTS_PER_BOX) *selected += 6;
}

static void move_game_selector(int *selected, u32 down)
{
    if (down & KEY_LEFT) {
        if ((*selected % 2) == 1) (*selected)--;
    }
    if (down & KEY_RIGHT) {
        if ((*selected % 2) == 0) (*selected)++;
    }
    if (down & KEY_UP) {
        if (*selected >= 2) *selected -= 2;
    }
    if (down & KEY_DOWN) {
        if (*selected + 2 < (int)GAME_SELECTOR_COUNT) *selected += 2;
    }
}

static void pokemon_view_from_oras(UiPokemon *dst, const OrasSlotInfo *src)
{
    memset(dst, 0, sizeof(*dst));
    if (!src || !src->occupied) return;

    dst->occupied = true;
    dst->shiny = src->shiny;
    dst->species = src->species;
    dst->held_item = src->held_item;
    dst->tid = src->tid;
    dst->sid = src->sid;
    dst->pid = src->pid;
    dst->nature = src->nature;
    dst->ability = src->ability;
    dst->gender = src->gender;
    dst->form = src->form;
    memcpy(dst->ivs, src->ivs, sizeof(dst->ivs));
    memcpy(dst->nickname, src->nickname, sizeof(dst->nickname));
    memcpy(dst->ot_name, src->ot_name, sizeof(dst->ot_name));
}

static void rebuild_ui_views(bool bank_ok, unsigned bank_box,
                             bool game_box_ok,
                             bool game_focus,
                             unsigned bank_selected,
                             unsigned game_selected)
{
    memset(s_bank_ui, 0, sizeof(s_bank_ui));
    memset(s_game_ui, 0, sizeof(s_game_ui));
    memset(&s_detail_ui, 0, sizeof(s_detail_ui));

    if (bank_ok && bank_read_box(bank_box, s_bank_view)) {
        for (unsigned i = 0; i < SLOTS_PER_BOX; ++i) {
            BankSlot *b = &s_bank_view[i];
            if (!b->occupied) continue;
            s_bank_ui[i].occupied = true;
            s_bank_ui[i].species = b->species;
            s_bank_ui[i].shiny = (b->flags & BANK_FLAG_SHINY) != 0;
        }
    }

    if (game_box_ok) {
        for (unsigned i = 0; i < SLOTS_PER_BOX; ++i)
            pokemon_view_from_oras(&s_game_ui[i], &s_game_slots[i]);
    }

    if (game_focus) {
        s_detail_ui = s_game_ui[game_selected];
    } else if (s_bank_view[bank_selected].occupied) {
        BankSlot *b = &s_bank_view[bank_selected];
        if (b->generation == 6 &&
            b->payload_size == PK6_BOX_LENGTH &&
            b->checksum == payload_checksum(b->payload, b->payload_size)) {
            OrasSlotInfo decoded;
            memset(&decoded, 0, sizeof(decoded));
            if (oras_decode_pk6(b->payload, &decoded)) {
                pokemon_view_from_oras(&s_detail_ui, &decoded);
            } else {
                s_detail_ui.occupied = true;
                s_detail_ui.species = b->species;
                s_detail_ui.shiny = (b->flags & BANK_FLAG_SHINY) != 0;
            }
        } else {
            s_detail_ui.occupied = true;
            s_detail_ui.species = b->species;
            s_detail_ui.shiny = (b->flags & BANK_FLAG_SHINY) != 0;
        }
    }
}

static bool open_selected_game(const GameEntry *entry,
                               OrasSource *source,
                               unsigned *game_box,
                               unsigned *game_selected,
                               bool *game_box_ok,
                               bool *game_focus,
                               char *game_detail,
                               size_t game_detail_size,
                               char *action_detail,
                               size_t action_detail_size)
{
    if (!entry || !entry->present) {
        snprintf(action_detail, action_detail_size, "Game is not detected.");
        return false;
    }
    if (!entry->adapter_ready) {
        snprintf(action_detail, action_detail_size,
                 "%s detected; adapter is coming next.", entry->name);
        return false;
    }

    memset(source, 0, sizeof(*source));
    memset(s_game_slots, 0, sizeof(s_game_slots));

    if (!oras_open_selected(entry->title_id, entry->media,
                            source, game_detail, game_detail_size)) {
        snprintf(action_detail, action_detail_size,
                 "Could not open %s save.", entry->name);
        *game_box_ok = false;
        return false;
    }

    *game_box = source->current_box;
    *game_selected = 0;
    *game_box_ok = oras_read_box(source, *game_box, s_game_slots,
                                 game_detail, game_detail_size);
    *game_focus = *game_box_ok;

    snprintf(action_detail, action_detail_size,
             "%s connected. GAME and BANK are live.", entry->name);
    return *game_box_ok;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!ui_init()) return 1;

    char bank_detail[96];
    char game_detail[128] = "Choose a game.";
    char action_detail[128] = "Choose a detected game to open its boxes.";

    bool bank_ok = bank_validate(bank_detail, sizeof(bank_detail));

    Result am_res = oras_services_init();
    if (R_FAILED(am_res)) {
        snprintf(action_detail, sizeof(action_detail),
                 "AM service failed: %08lX", (unsigned long)(u32)am_res);
    }

    GameEntry games[GAME_SELECTOR_COUNT];
    memset(games, 0, sizeof(games));
    if (R_SUCCEEDED(am_res)) games_scan(games);
    int game_choice = games_first_ready(games);

    OrasSource source;
    memset(&source, 0, sizeof(source));
    memset(s_game_slots, 0, sizeof(s_game_slots));

    unsigned bank_box = 0, bank_selected = 0;
    unsigned game_box = 0, game_selected = 0;
    bool game_focus = false;
    bool game_box_ok = false;
    bool overwrite_armed = false;

    AppScreen screen = SCREEN_GAME_SELECTOR;

    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();

        if (down & KEY_START) break;

        if (screen == SCREEN_GAME_SELECTOR) {
            if (down & (KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN))
                move_game_selector(&game_choice, down);

            if (down & KEY_TOUCH) {
                touchPosition pos;
                hidTouchRead(&pos);
                int hit = ui_game_index_at_touch(pos);
                if (hit >= 0) game_choice = hit;
            }

            if (down & KEY_X) {
                games_scan(games);
                game_choice = games_first_ready(games);
                snprintf(action_detail, sizeof(action_detail), "Game list rescanned.");
            }

            if (down & KEY_A) {
                if (open_selected_game(&games[game_choice], &source,
                                       &game_box, &game_selected,
                                       &game_box_ok, &game_focus,
                                       game_detail, sizeof(game_detail),
                                       action_detail, sizeof(action_detail))) {
                    overwrite_armed = false;
                    screen = SCREEN_BANK;
                }
            }

            ui_render_game_selector(games, game_choice, action_detail);
            continue;
        }

        if (down & KEY_SELECT) {
            screen = SCREEN_GAME_SELECTOR;
            overwrite_armed = false;
            snprintf(action_detail, sizeof(action_detail),
                     "Choose another game.");
            ui_render_game_selector(games, game_choice, action_detail);
            continue;
        }

        if (down & KEY_TOUCH) {
            touchPosition pos;
            hidTouchRead(&pos);
            int slot = ui_game_slot_at_touch(pos);
            if (slot >= 0) {
                game_selected = (unsigned)slot;
                game_focus = true;
                overwrite_armed = false;
            }
        }

        if (down & KEY_Y) {
            if (source.found && game_box_ok) {
                game_focus = !game_focus;
                overwrite_armed = false;
                snprintf(action_detail, sizeof(action_detail),
                         "%s selected.", game_focus ? "Game box" : "Bank box");
            }
        }

        if (down & (KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN)) {
            if (game_focus && source.found) move_selection(&game_selected, down);
            else move_selection(&bank_selected, down);
            overwrite_armed = false;
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
        }

        if (down & KEY_B) {
            if (source.found) {
                game_box_ok = oras_read_box(&source, game_box, s_game_slots,
                                            game_detail, sizeof(game_detail));
                snprintf(action_detail, sizeof(action_detail),
                         "%s", game_box_ok ? "Game box refreshed." : "Game refresh failed.");
            }
            bank_ok = bank_validate(bank_detail, sizeof(bank_detail));
            overwrite_armed = false;
        }

        if (down & KEY_A) {
            if (game_focus && game_box_ok) {
                const OrasSlotInfo *g = &s_game_slots[game_selected];
                if (g->occupied) {
                    snprintf(action_detail, sizeof(action_detail),
                             "%s%s | %s | %s",
                             g->nickname[0] ? g->nickname : ui_species_name(g->species),
                             g->shiny ? " SHINY" : "",
                             ui_nature_name(g->nature),
                             ui_ability_name(g->ability));
                } else {
                    snprintf(action_detail, sizeof(action_detail), "Selected game slot is empty.");
                }
            } else if (bank_read_box(bank_box, s_bank_view) &&
                       s_bank_view[bank_selected].occupied) {
                BankSlot *b = &s_bank_view[bank_selected];
                snprintf(action_detail, sizeof(action_detail),
                         "Bank slot: %s%s",
                         ui_species_name(b->species),
                         (b->flags & BANK_FLAG_SHINY) ? " SHINY" : "");
            } else {
                snprintf(action_detail, sizeof(action_detail), "Selected Bank slot is empty.");
            }
        }

        if (down & KEY_X) {
            if (!bank_ok) {
                snprintf(action_detail, sizeof(action_detail), "Bank is not writable.");
                overwrite_armed = false;
            } else if (!source.found || !game_box_ok) {
                snprintf(action_detail, sizeof(action_detail), "No readable game save.");
                overwrite_armed = false;
            } else if (game_focus) {
                const OrasSlotInfo *g = &s_game_slots[game_selected];
                if (!g->occupied) {
                    snprintf(action_detail, sizeof(action_detail), "Selected GAME slot is empty.");
                    overwrite_armed = false;
                } else if (!g->checksum_valid) {
                    snprintf(action_detail, sizeof(action_detail),
                             "Refusing copy: PK6 checksum is invalid.");
                    overwrite_armed = false;
                } else {
                    bool dest_occupied = bank_read_box(bank_box, s_bank_view) &&
                                         s_bank_view[bank_selected].occupied;
                    if (dest_occupied && !overwrite_armed) {
                        overwrite_armed = true;
                        snprintf(action_detail, sizeof(action_detail),
                                 "Bank destination occupied.");
                    } else {
                        if (bank_write_pk6(bank_box, bank_selected, &source, g,
                                           action_detail, sizeof(action_detail))) {
                            bank_ok = bank_validate(bank_detail, sizeof(bank_detail));
                        }
                        overwrite_armed = false;
                    }
                }
            } else {
                if (!bank_read_box(bank_box, s_bank_view) ||
                    !s_bank_view[bank_selected].occupied) {
                    snprintf(action_detail, sizeof(action_detail), "Selected BANK slot is empty.");
                    overwrite_armed = false;
                } else {
                    BankSlot *b = &s_bank_view[bank_selected];
                    bool bank_payload_ok =
                        b->generation == 6 &&
                        b->payload_size == PK6_BOX_LENGTH &&
                        b->checksum == payload_checksum(b->payload, b->payload_size);

                    if (!bank_payload_ok) {
                        snprintf(action_detail, sizeof(action_detail),
                                 "Refusing withdrawal: Bank PK6 invalid.");
                        overwrite_armed = false;
                    } else if (s_game_slots[game_selected].occupied && !overwrite_armed) {
                        overwrite_armed = true;
                        snprintf(action_detail, sizeof(action_detail),
                                 "Game destination occupied.");
                    } else {
                        if (oras_write_slot_with_backup(&source, game_box, game_selected,
                                                        b->payload,
                                                        action_detail,
                                                        sizeof(action_detail))) {
                            game_box_ok = oras_read_box(&source, game_box, s_game_slots,
                                                        game_detail, sizeof(game_detail));
                        }
                        overwrite_armed = false;
                    }
                }
            }
        }

        rebuild_ui_views(bank_ok, bank_box, game_box_ok, game_focus,
                         bank_selected, game_selected);

        ui_render_bank(oras_game_name(source.game),
                       oras_media_name(source.media),
                       bank_box, game_box,
                       bank_selected, game_selected,
                       game_focus,
                       s_bank_ui, s_game_ui,
                       &s_detail_ui,
                       action_detail,
                       overwrite_armed);
    }

    oras_services_exit();
    ui_exit();
    return 0;
}
