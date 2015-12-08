#include "portaudio.h"
#include "pa_defs.h"
#include <stdio.h>
#include "modstructs.h"
#include "player.h"
#include "util.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

//static int patestCallback(const void *, void *, unsigned long, const PaStreamCallbackTimeInfo *,
//        PaStreamCallbackFlags, void *);

static void loadfile(const char *, FILEDESC *);
static void unloadfile(FILEDESC *);

void die(const char *text) {
    fprintf(stderr, "%s\n", text);
    printf("error\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {

    if (argc < 2) die("No file specified");

    FILEDESC fileinfo;
    loadfile(argv[1], &fileinfo);

    play_mod(&fileinfo);

    unloadfile(&fileinfo);


}

static void loadfile(const char *filename, FILEDESC *fileinfo) {
    // check if file can be opened
    if (access(filename, F_OK) != 0) {
        perror("access");
        die("Specified file does not exist!");
    } else if (access(filename, R_OK) != 0) {
        perror("access");
        die("Read permission denied!");
    }
    struct stat sfile;
    if (stat(filename, &sfile) < 0) {
        perror("stat");
        die("Cannot get file stats");
    }
    fileinfo->filename = filename;
    fileinfo->dsize = (size_t) sfile.st_size;
    fileinfo->memaddr = xmalloc(fileinfo->dsize, __FILE__, __LINE__);

    if (fileinfo->memaddr == NULL) {
        perror("malloc");
        die("Cannot allocate file buffer: Out of memory");
    }
    // load file into buffer
    FILE *inputfile = fopen(filename, "r");
    if (inputfile == NULL) {
        perror("fopen");
        die("Cannot open file");
    }
    size_t bytesread = fread(fileinfo->memaddr, 1, fileinfo->dsize, inputfile);
    if ((size_t)sfile.st_size != bytesread) {
        perror("fread");
        die("Couldn't read all bytes from file");
    }
    return;
}

static void unloadfile(FILEDESC *memfile) {
    free(memfile->memaddr);
}
