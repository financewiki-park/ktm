#define _XOPEN_SOURCE 600
#define _DEFAULT_SOURCE
#define _DARWIN_C_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* A terminal child, not a second GUI. No GTK, X11, VTE or clipboard dependency.
 * The shell edits the pending input. Only the user's keystrokes can submit it.
 * Limit to 512 bytes: old firmware shells have small command editing buffers. */
#define INPUT_LIMIT 512
#ifndef KTM_SHELL
#define KTM_SHELL "/bin/sh"
#endif
static struct termios saved;
static int restore_needed;
static volatile sig_atomic_t stop, resized;
static void interrupted(int n) { if (n == SIGWINCH) resized = 1; else stop = n; }
static void restore(void) { if (restore_needed) tcsetattr(STDIN_FILENO, TCSANOW, &saved); }
static int write_all(int fd, const void *data, size_t len) {
    const char *p = data;
    while (len) {
        ssize_t n = write(fd, p, len);
        if (n < 0 && errno == EINTR && !stop) continue;
        if (n <= 0) return -1;
        p += n; len -= (size_t)n;
    }
    return 0;
}
static int load_message(const char *path, unsigned char *text, size_t *length) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("ktm: pending message"); return -1; }
    *length = fread(text, 1, INPUT_LIMIT + 1, f);
    int failed = ferror(f); fclose(f);
    if (failed || !*length || *length > INPUT_LIMIT) {
        fputs("ktm: direct input requires 1-512 bytes. Original message is kept in Inbox.\n", stderr);
        return -1;
    }
    for (size_t i = 0; i < *length; i++) {
        if (text[i] < 0x20 || text[i] == 0x7f) {
            fputs("ktm: direct input requires one line without tabs/control characters.\n"
                  "Original message (including newlines) is kept in Inbox; nothing was executed.\n", stderr);
            return -1;
        }
    }
    return 0;
}
int main(int argc, char **argv) {
    unsigned char text[INPUT_LIMIT + 1];
    size_t length;
    int check = argc == 3 && !strcmp(argv[1], "--check");
    if ((argc != 2 && !check) || load_message(argv[check ? 2 : 1], text, &length)) return 2;
    if (check) return 0;
    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &saved)) {
        fputs("ktm: open this command inside KTerm.\n", stderr); return 2;
    }
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0 || grantpt(master) || unlockpt(master)) {
        perror("ktm: cannot allocate terminal"); if (master >= 0) close(master); return 2;
    }
    char *name = ptsname(master);
    int slave = name ? open(name, O_RDWR | O_NOCTTY) : -1;
    if (slave < 0) { perror("ktm: cannot open terminal"); close(master); return 2; }
    struct winsize size;
    if (!ioctl(STDIN_FILENO, TIOCGWINSZ, &size)) ioctl(slave, TIOCSWINSZ, &size);
    if (tcsetattr(slave, TCSANOW, &saved)) {
        perror("ktm: terminal setup"); close(master); close(slave); return 2;
    }
    /* An exact prompt handshake replaces the old, racy 800ms sleep. */
    char prompt[64];
    snprintf(prompt, sizeof(prompt), "[ktm-%ld]$ ", (long)getpid());
    pid_t child = fork();
    if (child == -1) { perror("ktm: fork"); close(master); close(slave); return 2; }
    if (!child) {
        close(master);
        if (setsid() < 0 || ioctl(slave, TIOCSCTTY, 0) < 0) _exit(125);
        for (int fd = 0; fd < 3; fd++) if (dup2(slave, fd) < 0) _exit(125);
        if (slave > 2) close(slave);
        unsetenv("ENV"); unsetenv("BASH_ENV"); unsetenv("PROMPT_COMMAND");
        setenv("PS1", prompt, 1);
        setenv("PS2", "> ", 1);
        execl(KTM_SHELL, "sh", "-i", (char *)NULL);
        perror("ktm: cannot start shell"); _exit(127);
    }
    close(slave);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa)); sa.sa_handler = interrupted; sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL); sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL); sigaction(SIGWINCH, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
    struct termios raw = saved;
    cfmakeraw(&raw);
    atexit(restore);
    int result = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw)) { perror("ktm: raw terminal"); result = 2; }
    else restore_needed = 1;
    size_t match = 0, plen = strlen(prompt);
    int inserted = 0;
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    while (!stop && !result) {
        if (resized) {
            if (!ioctl(STDIN_FILENO, TIOCGWINSZ, &size)) ioctl(master, TIOCSWINSZ, &size);
            resized = 0;
        }
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (!inserted && now.tv_sec - start.tv_sec >= 10) {
            fputs("\r\nktm: shell did not become ready. Message retained.\r\n", stderr);
            result = 2; break;
        }
        struct pollfd fds[2] = {{master, POLLIN, 0}, {STDIN_FILENO, inserted ? POLLIN : 0, 0}};
        int n = poll(fds, 2, 250);
        if (n < 0) { if (errno == EINTR) continue; result = 2; break; }
        if (fds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            unsigned char buf[4096];
            ssize_t count = read(master, buf, sizeof(buf));
            if (count <= 0) break;
            if (write_all(STDOUT_FILENO, buf, (size_t)count)) { result = 2; break; }
            if (!inserted) {
                for (ssize_t i = 0; i < count; i++) {
                    if (buf[i] == (unsigned char)prompt[match]) match++;
                    else match = buf[i] == (unsigned char)prompt[0] ? 1 : 0;
                    if (match == plen) {
                        /* No newline, no Ctrl-U, no eval, no file deletion. */
                        if (write_all(master, text, length)) result = 2;
                        inserted = 1; break;
                    }
                }
            }
        }
        if (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            unsigned char buf[4096];
            ssize_t count = read(STDIN_FILENO, buf, sizeof(buf));
            if (count <= 0 || write_all(master, buf, (size_t)count)) break;
        }
    }
    restore(); restore_needed = 0;
    close(master);
    int status = 0;
    if (waitpid(child, &status, WNOHANG) == 0) {
        kill(child, SIGHUP);
        /* The relay owns this child. Never leave an orphan after KTerm closes. */
        struct timespec delay = {0, 20000000};
        int ended = 0;
        for (int i = 0; i < 25; i++) {
            if (waitpid(child, &status, WNOHANG) != 0) { ended = 1; break; }
            nanosleep(&delay, NULL);
        }
        if (!ended) { kill(child, SIGKILL); waitpid(child, &status, 0); }
    }
    if (stop) return 128 + stop;
    if (!inserted) return 2;
    return result;
}
