#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>

#define APP_VERSION "0.2-alpha"
#define APP_DIR "sdmc:/3ds/PokeBank-CFW"
#define BANK_FILE APP_DIR "/bank.dat"
#define BANK_MAGIC 0x46434250u /* PBCF */
#define BANK_VERSION 1u
#define BANK_BOXES 100u
#define SLOTS_PER_BOX 30u
#define SLOT_SIZE 512u

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
        const size_t payload_bytes = (size_t)BANK_BOXES * SLOTS_PER_BOX * SLOT_SIZE;
        ok = write_zeros(f, payload_bytes);
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
    fclose(f);

    if (got != sizeof(h)) {
        snprintf(detail, detail_size, "header truncated");
        return false;
    }
    if (h.magic != BANK_MAGIC) {
        snprintf(detail, detail_size, "bad bank magic");
        return false;
    }
    if (h.version != BANK_VERSION || h.box_count != BANK_BOXES ||
        h.slots_per_box != SLOTS_PER_BOX || h.slot_size != SLOT_SIZE) {
        snprintf(detail, detail_size, "unsupported bank format");
        return false;
    }

    snprintf(detail, detail_size, "bank verified");
    return true;
}

static bool read_slot(unsigned box, unsigned slot, BankSlot *out) {
    if (!out || box >= BANK_BOXES || slot >= SLOTS_PER_BOX) return false;
    FILE *f = fopen(BANK_FILE, "rb");
    if (!f) return false;
    const long offset = (long)sizeof(BankHeader) +
        (long)((box * SLOTS_PER_BOX + slot) * SLOT_SIZE);
    if (fseek(f, offset, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }
    bool ok = fread(out, 1, sizeof(*out), f) == sizeof(*out);
    fclose(f);
    return ok;
}

static void draw_box(PrintConsole *top, PrintConsole *bottom,
                     bool bank_ok, const char *detail,
                     unsigned box, unsigned selected) {
    consoleSelect(top);
    consoleClear();
    printf("\x1b[1;1HPOKEBANK-CFW   v%s\n", APP_VERSION);
    printf("Local Gen 1-7 Pokemon storage\n");
    printf("Bank: %s (%s)\n\n", bank_ok ? "READY" : "ERROR", detail);
    printf("BOX %03u / %u\n", box + 1, (unsigned)BANK_BOXES);
    printf("------------------------------\n");

    for (unsigned row = 0; row < 5; ++row) {
        for (unsigned col = 0; col < 6; ++col) {
            unsigned s = row * 6 + col;
            BankSlot slot;
            bool occupied = false;
            if (bank_ok && read_slot(box, s, &slot)) occupied = slot.occupied != 0;
            if (s == selected) printf("[%c]", occupied ? 'P' : '*');
            else               printf(" %c ", occupied ? 'P' : '.');
        }
        printf("\n");
    }

    printf("\nSelected slot: %u\n", selected + 1);

    consoleSelect(bottom);
    consoleClear();
    printf("\x1b[1;1HBank controls\n\n");
    printf("D-Pad   Select slot\n");
    printf("L / R   Change box\n");
    printf("A       Inspect slot\n");
    printf("X       Re-check bank\n");
    printf("START   Exit\n\n");
    printf("Storage:\n%s\n", BANK_FILE);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    gfxInitDefault();
    PrintConsole top, bottom;
    consoleInit(GFX_TOP, &top);
    consoleInit(GFX_BOTTOM, &bottom);

    char detail[96];
    bool bank_ok = bank_validate(detail, sizeof(detail));
    unsigned box = 0;
    unsigned selected = 0;
    draw_box(&top, &bottom, bank_ok, detail, box, selected);

    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();
        bool redraw = false;

        if (down & KEY_START) break;
        if (down & KEY_LEFT)  { if (selected % 6) selected--; redraw = true; }
        if (down & KEY_RIGHT) { if (selected % 6 < 5) selected++; redraw = true; }
        if (down & KEY_UP)    { if (selected >= 6) selected -= 6; redraw = true; }
        if (down & KEY_DOWN)  { if (selected + 6 < SLOTS_PER_BOX) selected += 6; redraw = true; }
        if (down & KEY_L)     { box = (box + BANK_BOXES - 1) % BANK_BOXES; redraw = true; }
        if (down & KEY_R)     { box = (box + 1) % BANK_BOXES; redraw = true; }
        if (down & KEY_X)     { bank_ok = bank_validate(detail, sizeof(detail)); redraw = true; }
        if (down & KEY_A) {
            BankSlot slot;
            if (bank_ok && read_slot(box, selected, &slot)) {
                if (slot.occupied) {
                    snprintf(detail, sizeof(detail), "Gen %u species %u", slot.generation, slot.species);
                } else {
                    snprintf(detail, sizeof(detail), "Box %u slot %u is empty", box + 1, selected + 1);
                }
            } else {
                snprintf(detail, sizeof(detail), "slot read failed");
            }
            redraw = true;
        }

        if (redraw) draw_box(&top, &bottom, bank_ok, detail, box, selected);
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
