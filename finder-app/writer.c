/* writer.c */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <path/to/file> <string>\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];
    const char *text = argv[2];

    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);

    int fd = open(path, O_APPEND | O_CREAT | O_RDWR, 0644);
    if (fd == -1)
    {
        perror("open");
        syslog(LOG_DEBUG, "Failed to open file %s: %s", path, strerror(errno));
        return 1;
    }
    ssize_t wr_ret = 0;
    int count = 0;
    wr_ret = write(fd, argv[2] , strlen(argv[2]));

    if(wr_ret == -1)
    {
        perror("write");
        syslog(LOG_DEBUG, "Failed to write to file: %s", path);
        close(fd);
        return 1;
    }

    syslog(LOG_DEBUG, "Successfully wrote '%s' to %s", text, path);
    closelog();
    close(fd);

    return 0;
}