/* Run the reviewed snapshot with KTerm's stdin/stdout/stderr attached. */
#include <signal.h>
static int command_shell(const char *path, int check, const char *cwd) {
    struct sigaction ignore, old_int, old_quit;
    memset(&ignore, 0, sizeof(ignore));
    ignore.sa_handler = SIG_IGN; sigemptyset(&ignore.sa_mask);
    sigaction(SIGINT, &ignore, &old_int); sigaction(SIGQUIT, &ignore, &old_quit);
    pid_t child = fork();
    if (!child) {
        signal(SIGINT, SIG_DFL); signal(SIGQUIT, SIG_DFL);
        unsetenv("ENV"); unsetenv("BASH_ENV");
        if (chdir(cwd)) { perror("ktm: command working directory"); _exit(126); }
        if (check) execl("/bin/sh", "sh", "-n", path, (char *)NULL);
        else execl("/bin/sh", "sh", path, (char *)NULL);
        perror("ktm: cannot start shell"); _exit(127);
    }
    int status = 0, result = 1;
    if (child < 0) perror("ktm: fork");
    else {
        pid_t waited;
        do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
        if (waited > 0) result = WIFEXITED(status) ? WEXITSTATUS(status) :
            WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1;
    }
    sigaction(SIGINT, &old_int, NULL); sigaction(SIGQUIT, &old_quit, NULL);
    return result;
}
static int run_command(App *a) {
    if (!isatty(0) || !isatty(1)) {
        fprintf(stderr, "Open Run command in KTerm; interactive confirmation is required.\n"); return 2;
    }
    /* Do not read ahead and steal answers intended for an interactive installer. */
    setvbuf(stdin, NULL, _IONBF, 0);
    char pending[PATH_MAX], snapshot[PATH_MAX];
    snprintf(pending, sizeof(pending), "%s/current.txt", a->inbox);
    size_t length = 0;
    char *text = read_all(pending, &length);
    if (!text || !length) {
        free(text); fprintf(stderr, "No command received. Send it to your Telegram bot, then sync.\n"); return 2;
    }
    /* The stored message stays byte-exact. Only CRLF in the reviewed/executed
     * copy is normalized. Received text is never interpolated into an argv. */
    size_t used = 0;
    int normalized = 0;
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\r' && i + 1 < length && text[i + 1] == '\n') { normalized = 1; continue; }
        if ((c < 0x20 && c != '\n' && c != '\t') || c == 0x7f) {
            free(text); fprintf(stderr, "Command contains control characters; nothing executed.\n"); return 2;
        }
        text[used++] = (char)c;
    }
    const char *cwd = getenv("KTM_COMMAND_CWD");
    if (!cwd || !*cwd) cwd = "/mnt/us";
    if (snprintf(snapshot, sizeof(snapshot), "%s/command.XXXXXX", a->state) >= (int)sizeof(snapshot)) {
        free(text); return 2;
    }
    int fd = mkstemp(snapshot);
    if (fd < 0) { free(text); perror("ktm: command snapshot"); return 2; }
    FILE *f = fdopen(fd, "wb");
    if (!f) { close(fd); unlink(snapshot); free(text); return 2; }
    int failed = fwrite(text, 1, used, f) != used;
    if (fclose(f)) failed = 1;
    if (failed) { unlink(snapshot); free(text); fprintf(stderr, "Cannot save command snapshot.\n"); return 2; }
    printf("\nReceived command (%zu bytes)\nWorking directory: %s\n", used, cwd);
    if (normalized) puts("CRLF converted to LF for execution; original retained.");
    puts("---------------- COMMAND ----------------");
    fwrite(text, 1, used, stdout);
    puts("\n-----------------------------------------");
    free(text); fflush(stdout);
    if (command_shell(snapshot, 1, cwd)) {
        fprintf(stderr, "Shell syntax check failed. Nothing executed. Send corrected plain command text.\n");
        unlink(snapshot); return 2;
    }
    printf("Type r then Enter to RUN this command; Enter alone cancels: "); fflush(stdout);
    char answer[32];
    if (!fgets(answer, sizeof(answer), stdin) ||
        (strcmp(answer, "r\n") && strcmp(answer, "R\n"))) {
        puts("Cancelled. Nothing executed."); unlink(snapshot); return 0;
    }
    puts("\n--- Running in Kindle shell ---"); fflush(stdout);
    int result = command_shell(snapshot, 0, cwd);
    printf("\n--- Command finished: exit code %d ---\n", result);
    unlink(snapshot);
    return result;
}
