#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <getopt.h>
#include <unistd.h>
#include <curl/curl.h>
#include <dirent.h>
#include <stdarg.h>
#include <zip.h>

#include "mandr.h"

static void pxerr(const char *err)
{
    perror(err);
    exit(EXIT_FAILURE);
}

static void fxerr(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    #ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wformat-nonliteral"
    #endif
    vfprintf(stderr, fmt, ap);
    #ifdef __clang__
    #pragma clang diagnostic pop
    #endif
    va_end(ap);
    exit(EXIT_FAILURE);
}

static void *xcalloc(size_t nmem)
{
    void *buf;

    buf = calloc(nmem, sizeof(char));
    if (buf == NULL)
        pxerr("calloc()");

    return buf;
}

static char *trim_end_char(char *src, const char del)
{
    size_t sz = strlen(src);

    while (sz--)
        if (*(src + sz) == del)
            *(src + sz) = '\0';

    return src;
}

static void update_archive(void)
{
    FILE *fp;
    CURL *curl;
    CURLcode ret;

    curl = curl_easy_init();
    if (curl == NULL)
        pxerr("curl_easy_init()");

    fp = fopen(ARCHIVE_FILE, "wb");
    if (fp == NULL) {
        curl_easy_cleanup(curl);
        pxerr("fopen()");
    }

    curl_easy_setopt(curl, CURLOPT_URL, ARCHIVE_URL);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, PROGRAM_NAME);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

    ret = curl_easy_perform(curl);
    if (ret != CURLE_OK) {
        fclose(fp);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        pxerr("curl_easy_perform()");
    }

    fclose(fp);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
}

static void unzip_archive(void)
{
    FILE *fp;
    zip_t *zz;
    struct zip_stat zs;
    struct zip_file *zf;
    size_t len, max_sz, i;
    unsigned char buf[4024] = {0};

    zz = zip_open("archive.zip", 0, NULL);

    if (zz == NULL)
        pxerr("zip_open()");

    for (i = 0; i < (size_t)zip_get_num_entries(zz, 0); i++) {
        /*
            Casting, since zip_get_num_entries returns a zip_int64_t
        */
        if (zip_stat_index(zz, (uint64_t)i, 0, &zs) == 0) {
            len = strlen(zs.name);

            if (zs.name[len - 1] == '/') {
                mkdir(zs.name, 0755);
            } else  {
                zf = zip_fopen_index(zz, (uint64_t)i, 0);
                if (zf == NULL)
                    pxerr("zip_fopen_index()");

                fp = fopen(zs.name, "wb");
                if (fp == NULL)
                    pxerr("fopen()");

                max_sz = 0;
                while (max_sz != zs.size) {
                    len = (size_t)zip_fread(zf, buf, sizeof(buf));

                    fwrite(&buf, 1UL, len, fp);
                    max_sz += len;
                }

                fclose(fp);
                zip_fclose(zf);
            }
        }
    }

    zip_close(zz);
}

static char *read_tldr(const char *file)
{
    FILE *fp;
    size_t i, k, sz;
    char *fbuf, *xbuf;

    fp = fopen(file, "r");
    if (fp == NULL)
        pxerr("fopen()");

    fseek(fp, 0L, SEEK_END);
    sz = (size_t)ftell(fp);
    rewind(fp);

    /* we need 2x to hold the buffer */
    xbuf = xcalloc(sz << 1);
    fbuf = xcalloc(sz + 1);

    fread(fbuf, 1L, sz, fp);
    fclose(fp);

    for (i = 0; i < sz; i++) {
        switch (fbuf[i]) {
        case '#':
            memcpy(xbuf + strlen(xbuf), ON_HASH, 12UL);
            for (k = 0; k < 2; k++)
                fbuf[i + k] = '\0';

            k = 1;
            while (fbuf[i] != '\n') {
                memcpy(xbuf + strlen(xbuf), &fbuf[i], 1UL);
                i = k += 1;
            }

            memcpy(xbuf + strlen(xbuf), "\n", 2UL);
            for (; k; k--)
                memcpy(xbuf + strlen(xbuf), "-", 2UL);

            memcpy(xbuf + strlen(xbuf), END_HASH, 6UL);
            break;

        case '>':
            memcpy(xbuf + strlen(xbuf), ON_P_GREATER, 16UL);
            fbuf[i + 1] = '\0';

            while (fbuf[i] != '\n') {
                switch (fbuf[i]) {
                case '<':
                    fbuf[i] = '\0';
                    memcpy(xbuf + strlen(xbuf), BLUE_UN, 8UL);
                    break;

                case '>':
                    fbuf[i] = '\0';
                    memcpy(xbuf + strlen(xbuf), END, 5UL);
                    break;

                case '`':
                    fbuf[i] = '\"';
                    break;

                default:
                    break;
                }

                memcpy(xbuf + strlen(xbuf), &fbuf[i], 1UL);
                i++;
            }

            if (fbuf[i + 2] == '-')
                memcpy(xbuf + strlen(xbuf), "\n\n", 3UL);
            else
                memcpy(xbuf + strlen(xbuf), "\n", 2UL);
            break;

        case '-':
            memcpy(xbuf + strlen(xbuf), ON_DASH, 16UL);
            for (k = 0; k < 2; k++)
                fbuf[i + k] = '\0';

            while (fbuf[i] != '\n') {
                if (fbuf[i] == '`')
                    fbuf[i] = '\"';

                memcpy(xbuf + strlen(xbuf), &fbuf[i], 1);
                i++;
            }

            memcpy(xbuf + strlen(xbuf), "\n", 2);
            break;

        case '`':
            memcpy(xbuf + strlen(xbuf), ON_BACKTICK, 17);
            fbuf[i] = '\0';

            while (fbuf[i] != '\n') {
                if (fbuf[i] == '`')
                    fbuf[i] = '\0';

                memcpy(xbuf + strlen(xbuf), &fbuf[i], 1);
                i++;
            }

            memcpy(xbuf + strlen(xbuf), END_BACKTICK, 7);
            break;

        default:
            break;
        }
    }

    free(fbuf);

    return xbuf;
}

static int rrmdir(const char *path)
{
    DIR *dir;
    struct stat st;
    struct dirent *det;
    char buf[300] = {0};

    dir = opendir(path);
    if (dir == NULL)
        return -1;

    while ((det = readdir(dir)) != NULL) {
        if (strcmp(det->d_name, ".") == 0 ||
            strcmp(det->d_name, "..") == 0)
            continue;

        if (det) {
            snprintf(buf, sizeof(buf), "%s/%s", path, det->d_name);

            if (stat(buf, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    rrmdir(buf);
                } else {
                    if (unlink(buf) == -1) {
                        return -1;
                    }
                }
            }
        }
    }

    /* Ignore errors */
    closedir(dir);

    if (rmdir(path) == -1)
        return -1;

    return 0;
}

static void usage(void)
{
    fprintf(stdout,
        "%s - A CLI TL;DR pages viewer\n"
        "Usage:\n"
        "   [--lang]        -- Select TL;DR page language\n"
        "   [--platform]    -- Select platform specific TL;DR page\n"
        "   [--cmd]         -- Command name\n"
        "   [--symlink]     -- create executable symlink in /bin\n"
        "   [--update]      -- update all TL;DR pages\n"
        "   [--update-all]  -- sync and update all TL;DR pages\n"
        "   [--help]        -- Shows this help menu\n"
        , PROGRAM_NAME
    );
}

int main(int argc, char **argv)
{
    /* compat with < C89 */
    int opt;
    DIR *dr;
    size_t i;
    char tbuf[PAGE_PATH_SIZE / 2] = {0};
    struct dirent *der = {0};
    int langf, platf, cmdf, listf;
    char *lang, *platform, *cmd, *tldr,
        rpath[PATH_MAX] = {0}, wpage[PAGE_PATH_SIZE] = {0};
    const char *platforms[] = {
        "android", "common", "linux", "osx", "sunos", "windows"
    };
    struct option long_options[] = {
        { "lang",       required_argument, 0, 0 },
        { "platform",   required_argument, 0, 1 },
        { "cmd",        required_argument, 0, 2 },
        { "list-cmds",  no_argument,       0, 3 },
        { "symlink",    no_argument,       0, 4 },
        { "update",     no_argument,       0, 5 },
        { "update-all", no_argument,       0, 6 },
        { "help",       no_argument,       0, 7 },
        { 0,            0,                 0, 0 }
    };

    if (argc < 2) {
        usage();
        exit(EXIT_SUCCESS);
    }

    if (argv[1][0] != '-') {
        usage();
        exit(EXIT_FAILURE);
    }

    lang = cmd = platform = NULL;
    langf = platf = cmdf = listf = 0;

    while ((opt = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch (opt) {
        /* lang argument */
        case 0:
            langf = 1;
            lang = optarg;
            break;

        /* platform argument */
        case 1:
            platf = 1;
            platform = optarg;
            break;

        /* cmd argument */
        case 2:
            cmdf = 1;
            cmd = optarg;
            break;

        case 3:
            listf = 1;
            break;

        /* symlink argument */
        case 4:
            if (getuid() != 0 || getuid() != geteuid())
                fxerr(
                    "Error: To symlink, you must need to invoke the command as root\n"
                );

            if (realpath(argv[0], rpath) == NULL)
                pxerr("realpath()");

            /* Ignore errors if it doesn't exist */
            remove("/bin/mandr");

            if (symlink(rpath, "/bin/mandr") == -1)
                pxerr("symlink()");
            break;

        /* update argument */
        case 5:
            if (access(ARCHIVE_FILE, F_OK) == -1)
                fxerr("Error: archive.zip wasn't found, please sync with git repo first\n");

            rrmdir(TLDR_FOLDER);
            fprintf(stdout, "Decompressing...\n");
            unzip_archive();
            fprintf(stdout, "Archive decompressed\n");
            break;

        /* update-all argument */
        case 6:
            remove(ARCHIVE_FILE);
            rrmdir(TLDR_FOLDER);
            fprintf(stdout, "Syncing...\n");
            update_archive();
            fprintf(stdout, "Archive synced\nDecompressing...\n");
            unzip_archive();
            fprintf(stdout, "Archive decompressed\n");
            break;

        /* help argument */
        case 7:
            usage();
            break;

        /* unknown argument */
        default:
            exit(EXIT_FAILURE);
        }
    }

    if (langf) {
        if ((!platf || !cmdf) && !listf)
            fxerr("Error: Insufficient arguments\n");

        if (strlen(lang) > 2)
            fxerr(
                "Error: Language code can be only two charecters long\n"
            );
    }

    if (platf) {
         if (platform == NULL || platform[0] == '-')
            fxerr("Error: No platform was specified\n");

        if ((listf == 0 && cmd == NULL) || (listf == 0 && cmd[0] == '-'))
            fxerr("Error: No command name was specified\n");

        if (strcmp(platform, "any") == 0)
            goto do_nothing;

        memcpy(wpage + strlen(wpage), TLDR_FOLDER, 10UL);

        /* Check for the directory */
        if (access(wpage, F_OK) == -1)
            fxerr(
                "Error: No TL;DR directory was found."
                "Have to synced the repo?\n"
            );

        if (lang == NULL || (lang != NULL && strcmp(lang, "en") == 0)) {
            memcpy(wpage + strlen(wpage), "/pages", 7UL);
        } else {
            memcpy(wpage + strlen(wpage), "/pages.", 8UL);
            memcpy(wpage + strlen(wpage), lang, 3UL);
        }

        /* Check for the file */
        if (access(wpage, F_OK) == -1)
            fxerr(
                "Error: No TL;DR pages were found with language code %s\n",
                lang
            );

        /* Create /[platform]/ string */
        memcpy(wpage + strlen(wpage), "/", 2UL);
        memcpy(wpage + strlen(wpage), platform, 8UL);
        memcpy(wpage + strlen(wpage), "/", 2UL);

        do_nothing: /* Ignore */;
    }

    if (cmdf) {
        if (cmd == NULL || cmd[0] == '-')
            fxerr("Error: No command name was specified\n");

        if (platform == NULL || platform[0] == '-')
            fxerr("Error: No platform was specified\n");

        /* evaluate, if platform is "any" */
        if (strcmp(platform, "any") == 0) {
            for (i = 0; i < ARRAY_SIZE(platforms); i++) {
                if (lang == NULL || (lang != NULL && strcmp(lang, "en") == 0))
                    snprintf(tbuf, sizeof(tbuf),
                        "%s/pages/%s/%s.md", TLDR_FOLDER, platforms[i],
                        trim_end_char(cmd, '.')
                    );
                else
                    snprintf(tbuf, sizeof(tbuf),
                        "%s/pages.%s/%s/%s.md", TLDR_FOLDER, lang,
                        platforms[i], trim_end_char(cmd, '.')
                    );

                if (access(tbuf, F_OK) == 0)
                    break;

                if (i == ARRAY_SIZE(platforms) - 1)
                    fxerr("Error: No command found called %s\n",
                        strlen(cmd) == 0 ? "(unknown characters)" : cmd
                    );
            }
        } else {
            /* When a specific platform is provided */
            memcpy(wpage + strlen(wpage), trim_end_char(cmd, '.'), 10UL);
            memcpy(wpage + strlen(wpage), ".md", 4UL);

            if (access(wpage, F_OK) == -1)
                fxerr("Error: No command found called %s\n",
                    strlen(cmd) == 0 ? "(unknown characters)" : cmd
                );
        }

        i = strlen(wpage);
        if (i == 0 && strlen(tbuf) == 0)
            fxerr("Error: Argument parsing failed, unknown error\n");

        tldr = read_tldr(i == 0 ? tbuf : wpage);
        if (tldr == NULL)
            fxerr(
                "Error: No information is written to %s TL;DR page\n"    
                , cmd
            );

        fprintf(stdout, "%s", tldr);
        free(tldr);
    }

    if (listf) {
        memset(tbuf, '\0', sizeof(tbuf));

        if (lang == NULL || (lang != NULL && strcmp(lang, "en") == 0))
            snprintf(
                tbuf, sizeof(tbuf), "%s/pages/%s",
                TLDR_FOLDER, platform
            );
        else
            snprintf(
                tbuf, sizeof(tbuf), "%s/pages.%s/%s",
                TLDR_FOLDER, lang, platform
            );

        i = 0;
        dr = opendir(tbuf);

        if (dr == NULL)
            fxerr("Error: Unknown platform was specified\n");

        while ((der = readdir(dr)) != NULL) {
            if (strcmp(der->d_name, ".") == 0 || strcmp(der->d_name, "..") == 0)
                continue;

            fprintf(stdout,
                "%s%ld. %s\n", ON_P_GREATER, i, der->d_name
            );

            i++;
        }

        closedir(dr);
    }

    exit(EXIT_SUCCESS);
}
