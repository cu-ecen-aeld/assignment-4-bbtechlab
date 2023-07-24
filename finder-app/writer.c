#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>

int main(int argc, char*argv[]) {
    FILE    *fp;
    int     byteWrite;
    int     ret = 1;

    openlog("ASSIGNMENT-2-BBTECHLAB", 0, LOG_USER);

    if (argc < 3) {
        syslog(LOG_ERR, "No arguments, Usage: ./writer filename contents");
        closelog();
        goto exit;
    }

    fp = fopen(argv[1], "w");
    if (fp) {
        byteWrite = fwrite(argv[2], sizeof(char), strlen(argv[2]), fp);
        if (byteWrite != strlen(argv[2])) {
            syslog(LOG_ERR, "Failed to write the contents:%s to file:%s  with errno(%s)", argv[2], argv[1], strerror(errno));
        } else {
            syslog(LOG_DEBUG, "Succeed to write the contents:%s to file:%s", argv[2], argv[1]);
            ret = 0;
        }
        fclose(fp);
    } else {
        syslog(LOG_ERR, "can not open file: %s for writing with errno(%s)", argv[1], strerror(errno));
    }

exit:
    closelog();
    return ret;
}