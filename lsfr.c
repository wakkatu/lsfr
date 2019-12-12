#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define LOG_ERROR(...) fprintf(stderr, __VA_ARGS__)
#define LOG_DBG(...) do { if (debug) fprintf(stderr, __VA_ARGS__); } while (0)

static int debug = 0;

int popcount(uintmax_t x)
{
    int count;

    for (count = 0; x; count++)
        x &= x - 1;

    return count;
}

int parity(uintmax_t x)
{
    return popcount(x) & 0x1;
}

uintmax_t reverse(unsigned size, uintmax_t x)
{
    uintmax_t y = 0;
    int i;

    for (i = 0; i < size; i++) {
        y |= ((x >> i) & 0x1) << (size - i - 1);
    }

    return y;
}

uintmax_t feedback_polynomial_bits(unsigned size)
{
    switch (size)
    {
        case 2: return 0x3;  // x^2 + x + 1
        case 3: return 0x3;  // x^3 + x^2 + 1
        case 4: return 0x3;  // x^4 + x^3 + 1
        case 5: return 0x5;  // x^5 + x^3 + 1
        case 6: return 0x3;  // x^6 + x^5 + 1
        case 7: return 0x3;  // x^7 + x^6 + 1
        case 8: return 0x1d;  // x^8 + x^6 + x^5 + x^4 + 1
        case 9: return 0x11;  // x^9 + x^5 + 1
        case 10: return 0x9;  // x^10 + x^7 + 1
        case 11: return 0x5;  // x^11 + x^9 + 1
        case 12: return 0x107;  // x^12 + x^11 + x^10 + x^4 + 1
        case 13: return 0x27;  // x^13 + x^12 + x^11 + x^8 + 1
        case 14: return 0x1007;  // x^14 + x^13 + x^12 + x^2 + 1
        case 15: return 0x3;  // x^15 + x^14 + 1
        case 16: return 0x100b;  // x^16 + x^15 + x^13 + x^4 + 1
        case 17: return 0x9;  // x^17 + x^14 + 1
        case 18: return 0x81;  // x^18 + x^11 + 1
        case 19: return 0x27;  // x^19 + x^18 + x^17 + x^14 + 1
        case 20: return 0x9;  // x^20 + x^17 + 1
        case 21: return 0x5;  // x^21 + x^19 + 1
        case 22: return 0x3;  // x^22 + x^21 + 1
        case 23: return 0x21;  // x^23 + x^18 + 1
        case 24: return 0x87;  // x^24 + x^23 + x^22 + x^17 + 1
        case 31: return 0x9;  // x^31 + x^28 + 1
        default: ;
    }

    return 0;
}

int fibonacci_lsfr(unsigned size, int left_shift, uintmax_t taps, uintmax_t *state)
{
    int newbit = parity(taps & *state);
    int output;

    if (left_shift) {
        output = *state >> (size  - 1);
        *state = ((*state << 1) | newbit) & ((0x1 << size) - 1);
    } else {
        output = *state & 0x1;
        *state = (*state >> 1) | (newbit << (size - 1));
    }

    return output;
}

int galois_lsfr(unsigned size, int left_shift, uintmax_t taps, uintmax_t *state)
{
    int output;

    if (left_shift) {
        output = *state >> (size  - 1);
        *state = (*state << 1) ^ (output ? taps : 0);
    } else {
        output = *state & 0x1;
        *state = (*state >> 1) ^ (output ? taps : 0);
    }

    return output;
}

char *lsfr2str(char *buf, unsigned size, uintmax_t state)
{
    static char cache[sizeof(state)*8+1];
    char *p;
    int i;

    if (buf == NULL) {
        buf = cache;
    }
    p = buf;

    for (i = size - 1; i >= 0; i--) {
        *p++ = ((state >> i) & 0x1) ? '1' : '0';
    }
    *p = '\0';

    return buf;
}

void print_output(int period, unsigned size, uintmax_t state, int output)
{
    if (period == -1) {
        printf("\n");
        return;
    } else if (period == 0) {
        return;
    }

    printf("%c", output ? '1' : '0');
}

void print_state(int period, unsigned size, uintmax_t state, int output)
{
    if (period <= 0) {
        return;
    }

    printf("%s\n", lsfr2str(NULL, size, state));
}

void print_state_hex(int period, unsigned size, uintmax_t state, int output)
{
    if (period <= 0) {
        return;
    }

    printf("0x%0*" PRIxMAX "\n", (size+3)/4, state);
}

void print_full(int period, unsigned size, uintmax_t state, int output)
{
    const char *s;
    int w_period, w_state;
    int i;

    if (period <= 0) {
        return;
    }

    w_period = ((size+9)/10)*3 + 1;
    if (w_period < 6) {
        w_period = 6;
    }

    w_state = size % 4;
    if (w_state == 0) {
        w_state = 4;
    }

    s = lsfr2str(NULL, size, state);
    printf("%*d  output: %d  state:", w_period, period, output);

    for (i = 0; i < size; i+=w_state, w_state=4) {
        printf(" %-*.*s", w_state, w_state, s + i);
    }
    printf("\n");
}

void usage(FILE *f, char *name, int full)
{
    fprintf(f, "Usage: %s size=<size> [start=<start>] [taps=<taps>] [print=lsfr|prbs]\n",
        name);

    fprintf(f, "\n  help              this message");
    fprintf(f, "\n  size=2...%-4u     register size in bits", (unsigned)(sizeof(uintmax_t)*8));
    fprintf(f, "\n  start=<VALUE>     initial state, 0 for all-ones");
    fprintf(f, "\n  taps=<VALUE>      feedback polynomial, 0 to use pre-defined automatically");
    fprintf(f, "\n  lsfr=<LSFR_F>     type of LSFR");
    fprintf(f, "\n  period=<VALUE>    0 for 2^size - 1");
    fprintf(f, "\n  print=<PRINT_F>   print format");
    fprintf(f, "\n  errchk=0|1        error check, default is 1");
    fprintf(f, "\n  debug=0|1         display debug message, default is 0");
    fprintf(f, "\n");

    if (!full) {
        return;
    }

    fprintf(f, "\n");
    fprintf(f, "\nLSFR_F");
    fprintf(f, "\n  fibonacci|standard|external|many2one  (default)");
    fprintf(f, "\n  galois|modular|internal|one2many");
    fprintf(f, "\n");

    fprintf(f, "\n");
    fprintf(f, "\nPRINT_F");
    fprintf(f, "\n  output     output bit sequence");
    fprintf(f, "\n  state|bin  state in binary (default)");
    fprintf(f, "\n  hex        state in hex");
    fprintf(f, "\n  full       detail info");
    fprintf(f, "\n");

    fprintf(f, "\n");
    {
        uintmax_t taps;
        unsigned size;

        fprintf(f, "Pre-defined feedback polynomial: (for lsfr=fibonacci)\n");
        for (size = 2; size <= sizeof(taps)*8; size++) {
            taps = feedback_polynomial_bits(size);
            if (taps != 0) {
                fprintf(f, "  size=%u taps=0x%" PRIxMAX "\n", size, taps);
            }
        }
    }
}

int main(int argc, char* argv[])
{
    uintmax_t taps = 0;
    uintmax_t start = 0;
    unsigned size = 0;
    int left_shift = 0;
    int output = 0;
    int max_period = 0;
    int user_period = 0;
    int errchk = 1;
    int i = 0;
    int (*lsfr_f)() = fibonacci_lsfr;
    void (*print_f)() = print_state;

    for (i = 1; i < argc; i++) {
        if (!strcmp("help", argv[i])) {
            usage(stdout, argv[0], 1);
            return 0;
        } else
        if (!strncmp("size=", argv[i], 5)) {
            size = strtoul(argv[i]+5, NULL, 0);
        } else
        if (!strncmp("start=", argv[i], 6)) {
            start = strtoul(argv[i]+6, NULL, 0);
        } else
        if (!strncmp("taps=", argv[i], 5)) {
            taps = strtoul(argv[i]+5, NULL, 0);
        } else
        if (!strcmp("lsfr=fibonacci", argv[i]) ||
            !strcmp("lsfr=standard", argv[i]) ||
            !strcmp("lsfr=external", argv[i]) ||
            !strcmp("lsfr=many2one", argv[i])) {
            lsfr_f = fibonacci_lsfr;
        } else
        if (!strcmp("lsfr=galois", argv[i]) ||
            !strcmp("lsfr=modular", argv[i]) ||
            !strcmp("lsfr=internal", argv[i]) ||
            !strcmp("lsfr=one2many", argv[i])) {
            lsfr_f = galois_lsfr;
        } else
        if (!strncmp("period=", argv[i], 7)) {
            user_period = strtoul(argv[i]+7, NULL, 0);
        } else
        if (!strcmp("print=output", argv[i])) {
            print_f = print_output;
        } else
        if (!strcmp("print=state", argv[i]) ||
            !strcmp("print=bin", argv[i])) {
            print_f = print_state;
        } else
        if (!strcmp("print=hex", argv[i])) {
            print_f = print_state_hex;
        } else
        if (!strcmp("print=full", argv[i])) {
            print_f = print_full;
        } else
        if (!strncmp("errchk=", argv[i], 7)) {
            errchk = strtoul(argv[i]+7, NULL, 0);
        } else
        if (!strncmp("debug=", argv[i], 6)) {
            debug = strtoul(argv[i]+6, NULL, 0);
        } else
        {
            usage(stderr, argv[0], 0);
            return -1;
        }
    }

    if (size < 2 || size > sizeof(taps)*8) {
        LOG_ERROR("invalid size=%u\n", size);
        usage(stderr, argv[0], 0);
        return -1;
    }
    max_period = (1ULL << size) - 1;

    if (start == 0) {
        start = (0x1 << size) - 1;
    }

    if (taps == 0) {
        taps = feedback_polynomial_bits(size);

        if (lsfr_f != fibonacci_lsfr) {
            taps = reverse(size, taps);
        }
    }
    if (taps == 0) {
        LOG_ERROR("invalid taps=0x%" PRIxMAX "\n", taps);
        usage(stderr, argv[0], 1);
        return -1;
    }

    LOG_DBG("%s size=%u start=0x%" PRIxMAX " taps=0x%" PRIxMAX " lsfr=%s\n",
        argv[0],
        size, start, taps,
        (lsfr_f == fibonacci_lsfr) ? "fibonacci" : "galois");

    uintmax_t state = start;
    int period = 0;

    print_f(0, size);

    do {
        output = lsfr_f(size, left_shift, taps, &state);
        period++;
        print_f(period, size, state, output);

        if (errchk) {
            if (state == 0 ||
                (state == start && (period % max_period) != 0) ||
                (state != start && (period % max_period) == 0)) {
                print_f(-1);
                LOG_ERROR("invalid peroid=%d state=0x%" PRIxMAX "\n", period, state);
                return -1;
            }
        }

        if (user_period != 0) {
            if (period >= user_period) {
                break;
            }
        }
    } while (state != start || user_period != 0);

    print_f(-1);

    return 0;
}
