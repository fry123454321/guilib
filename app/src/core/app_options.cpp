#include "app_options.h"

#include <cstdio>
#include <cstring>

AppOptions AppOptions::parse(int argc, char ** argv) {
    AppOptions options;
    int positional = 0;

    for (int i = 1; i < argc; ++i) {
        const char * arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            options.showUsage = true;
            return options;
        }
        if (strcmp(arg, "--ssl") == 0 || strcmp(arg, "-s") == 0) {
            options.wsInit.useSsl = true;
            continue;
        }
        if (strcmp(arg, "--key") == 0 && i + 1 < argc) {
            options.wsInit.sslKey = argv[++i];
            continue;
        }
        if (strcmp(arg, "--cert") == 0 && i + 1 < argc) {
            options.wsInit.sslCert = argv[++i];
            continue;
        }
        if (arg[0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", arg);
            options.showUsage = true;
            options.usageIsError = true;
            return options;
        }
        if (positional == 0) {
            options.port = atoi(arg);
        }
        ++positional;
    }

    return options;
}

void AppOptions::printUsage(const char * prog) const {
    printf("Usage: %s [options] [port]\n", prog);
    printf("Paths: LIBGUI_ROOT_DIR (default .) with data/ and static/ subdirectories.\n");
    printf("Options:\n");
    printf("  --ssl, -s           Use HTTPS/WSS (requires key.pem and cert.pem)\n");
    printf("  --key <path>        TLS private key (default: key.pem)\n");
    printf("  --cert <path>       TLS certificate (default: cert.pem)\n");
    printf("  -h, --help          Show this help\n");
#ifdef IMGUI_WS_HAVE_SSL
    printf("\nWSS support: enabled (built with LIBGUI_BUILD_SSL=ON)\n");
#else
    printf("\nWSS support: not compiled in (rebuild with -DLIBGUI_BUILD_SSL=ON)\n");
#endif
}
