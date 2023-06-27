#ifndef MANDR_H
#define MANDR_H     1

#ifndef PATH_MAX
    #error PATH_MAX is not defined. Please see "NOTES" section from https://man7.org/linux/man-pages/man3/realpath.3.html for more information.
#endif

#define PAGE_PATH_SIZE      100
#define BLUE_UN             "\033[4;94m"
#define WHITE               "\033[1;97m"
#define END                 "\033[0m"

#define ON_HASH             "\033[1;97m• "
#define END_HASH            "\n\033[0m"
#define ON_P_GREATER        "\033[0;92m► \033[0m"
#define ON_DASH             "\033[0;91m• \033[0m"
#define ON_BACKTICK         "   \033[0;92m$ \033[0m"
#define END_BACKTICK        "\n\n\033[0m"

#define ARCHIVE_FILE        "archive.zip"
#define ARCHIVE_URL         "https://github.com/tldr-pages/tldr/archive/main.zip"
#define TLDR_FOLDER         "tldr-main"
#define PROGRAM_NAME        "mandr"

#define ARRAY_SIZE(x)       (sizeof(x) / sizeof(x[0]))

#endif
