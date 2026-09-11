#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/stat.h>

#define APP_VERSION "0.1-smoketest"
#define APP_DIR "sdmc:/3ds/PokeBank-CFW"
#define TEST_FILE APP_DIR "/storage_test.dat"

static bool ensure_dir(const char *path) {
    if (mkdir(path, 0777) == 0) return true;
    return errno == EEXIST;
}

static bool run_storage_test(char *detail, size_t detail_size) {
    static const char payload[] = "POKEBANK_CFW_STORAGE_TEST_V1\n";
    char readback[sizeof(payload)] = {0};

    if (!ensure_dir("sdmc:/3ds")) {
        snprintf(detail, detail_size, "mkdir sdmc:/3ds failed (%d)", errno);
        return false;
    }

    if (!ensure_dir(APP_DIR)) {
        snprintf(detail, detail_size, "mkdir app dir failed (%d)", errno);
        return false;
    }

    FILE *out = fopen(TEST_FILE, "wb");
    if (!out) {
        snprintf(detail, detail_size, "open for write failed (%d)", errno);
        return false;
    }

    size_t wanted = sizeof(payload) - 1;
    size_t written = fwrite(payload, 1, wanted, out);
    fflush(out);
    fclose(out);

    if (written != wanted) {
        snprintf(detail, detail_size, "short write: %lu/%lu",
                 (unsigned long)written, (unsigned long)wanted);
        return false;
    }

    FILE *in = fopen(TEST_FILE, "rb");
    if (!in) {
        snprintf(detail, detail_size, "open for read failed (%d)", errno);
        return false;
    }

    size_t got = fread(readback, 1, wanted, in);
    fclose(in);

    if (got != wanted || memcmp(readback, payload, wanted) != 0) {
        snprintf(detail, detail_size, "readback mismatch");
        return false;
    }

    snprintf(detail, detail_size, "read/write verified");
    return true;
}

static void draw_status(PrintConsole *top, PrintConsole *bottom, bool ok, const char *detail) {
    consoleSelect(top);
    consoleClear();
    printf("\x1b[1;1HPOKEBANK-CFW\n");
    printf("Native Nintendo 3DS storage\n");
    printf("Version: %s\n\n", APP_VERSION);
    printf("Gen 1 - Gen 7 Bank Project\n");
    printf("-----------------------------\n");
    printf("SD storage test: %s\n", ok ? "PASS" : "FAIL");
    printf("Result: %s\n\n", detail);
    printf("Path:\n%s\n", TEST_FILE);

    consoleSelect(bottom);
    consoleClear();
    printf("\x1b[1;1HHardware smoke test\n\n");
    if (ok) {
        printf("PASS - SD storage is working.\n\n");
        printf("Next milestone:\n");
        printf("Bank file + 30-slot box UI.\n");
    } else {
        printf("FAIL - do not continue to save\n");
        printf("editing until this is fixed.\n");
    }
    printf("\n[A] Run test again\n");
    printf("[START] Exit\n");
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    gfxInitDefault();
    PrintConsole top;
    PrintConsole bottom;
    consoleInit(GFX_TOP, &top);
    consoleInit(GFX_BOTTOM, &bottom);

    char detail[96];
    bool ok = run_storage_test(detail, sizeof(detail));
    draw_status(&top, &bottom, ok, detail);

    while (aptMainLoop()) {
        hidScanInput();
        u32 down = hidKeysDown();

        if (down & KEY_START) break;

        if (down & KEY_A) {
            ok = run_storage_test(detail, sizeof(detail));
            draw_status(&top, &bottom, ok, detail);
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
