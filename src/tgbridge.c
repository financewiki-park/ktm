#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define VERSION "0.1.5"
#define MAX_RESPONSE (8U * 1024U * 1024U)

typedef struct {
    char *root, config[PATH_MAX], state[PATH_MAX], inbox[PATH_MAX], outbox[PATH_MAX], logs[PATH_MAX];
    char token[512], pairing_state[32], pairing_code[32];
    long long allowed_user_id, allowed_chat_id, offset, pairing_expires;
    int poll_timeout;
} App;

static void die(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); fputc('\n', stderr); exit(1); }

static void log_msg(App *a, const char *fmt, ...) {
    char msg[2048]; va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof(msg), fmt, ap); va_end(ap);
    fprintf(stderr, "%s\n", msg);
    char p[PATH_MAX]; snprintf(p, sizeof(p), "%s/bridge.log", a->logs); FILE *f = fopen(p, "a");
    if (f) { fprintf(f, "%lld %s\n", (long long)time(NULL), msg); fclose(f); chmod(p, 0600); }
}

static int mkdir_p(const char *path) {
    char b[PATH_MAX]; size_t n = strlen(path); if (!n || n >= sizeof(b)) return -1; memcpy(b, path, n + 1);
    for (char *p = b + 1; *p; p++) if (*p == '/') { *p = 0; if (mkdir(b, 0700) && errno != EEXIST) return -1; *p = '/'; }
    return mkdir(b, 0700) && errno != EEXIST ? -1 : 0;
}

static int set_root(App *a, const char *root) {
    if (strlen(root) >= PATH_MAX) return -1; free(a->root); a->root = strdup(root); if (!a->root) return -1;
    if (snprintf(a->config, PATH_MAX, "%s/config", root) >= PATH_MAX || snprintf(a->state, PATH_MAX, "%s/state", root) >= PATH_MAX || snprintf(a->inbox, PATH_MAX, "%s/inbox", root) >= PATH_MAX || snprintf(a->outbox, PATH_MAX, "%s/outbox", root) >= PATH_MAX || snprintf(a->logs, PATH_MAX, "%s/logs", root) >= PATH_MAX) return -1;
    return 0;
}
static int ensure_dirs(App *a) { return mkdir_p(a->root) || mkdir_p(a->config) || mkdir_p(a->state) || mkdir_p(a->inbox) || mkdir_p(a->outbox) || mkdir_p(a->logs); }
static const char *root_from_env(void) { const char *p = getenv("KTM_DATA_DIR"); if (p && *p) return p; return access("/var/local", W_OK) == 0 ? "/var/local/ktm" : "/mnt/us/ktm"; }

static char *read_all(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb"); if (!f) return NULL; if (fseek(f, 0, SEEK_END)) { fclose(f); return NULL; }
    long z = ftell(f); if (z < 0 || (unsigned long)z > MAX_RESPONSE) { fclose(f); return NULL; } rewind(f);
    char *b = calloc((size_t)z + 1, 1); if (!b) { fclose(f); return NULL; } size_t got = fread(b, 1, (size_t)z, f); fclose(f);
    if (got != (size_t)z) { free(b); return NULL; } if (len) *len = got; return b;
}
static int write_atomic(const char *path, const char *data, size_t len, mode_t mode) {
    char p[PATH_MAX]; if (snprintf(p, sizeof(p), "%s.tmp.XXXXXX", path) >= (int)sizeof(p)) return -1; int fd = mkstemp(p); if (fd < 0) return -1; fchmod(fd, mode);
    size_t off = 0; while (off < len) { ssize_t n = write(fd, data + off, len - off); if (n <= 0) { close(fd); unlink(p); return -1; } off += (size_t)n; }
    fsync(fd); close(fd); if (rename(p, path)) { unlink(p); return -1; } return 0;
}
static void trim(char *s) { size_t n = strlen(s); while (n && isspace((unsigned char)s[n-1])) s[--n] = 0; char *p = s; while (*p && isspace((unsigned char)*p)) p++; if (p != s) memmove(s, p, strlen(p)+1); }
static void app_defaults(App *a) { memset(a, 0, sizeof(*a)); a->allowed_user_id = a->allowed_chat_id = -1; strcpy(a->pairing_state, "none"); }
static void load_num(const char *p, long long *v, long long d) { char *s = read_all(p, NULL); if (!s) { *v = d; return; } char *e; long long x = strtoll(s, &e, 10); *v = e == s ? d : x; free(s); }
static int save_num(const char *p, long long v) { char b[64]; int n = snprintf(b, sizeof(b), "%lld\n", v); return write_atomic(p, b, (size_t)n, 0600); }
static void mark_sync(App *a) { char p[PATH_MAX]; snprintf(p, sizeof(p), "%s/last_sync", a->state); save_num(p, (long long)time(NULL)); }

static void load_config(App *a) {
    char p[PATH_MAX]; snprintf(p, sizeof(p), "%s/bridge.conf", a->config); FILE *f = fopen(p, "r");
    if (f) { char l[2048]; while (fgets(l, sizeof(l), f)) { trim(l); if (!*l || *l == '#') continue; char *e = strchr(l, '='); if (!e) continue; *e++ = 0; if (!strcmp(l, "bot_token")) snprintf(a->token, sizeof(a->token), "%s", e); else if (!strcmp(l, "allowed_user_id")) a->allowed_user_id = strtoll(e, NULL, 10); else if (!strcmp(l, "allowed_chat_id")) a->allowed_chat_id = strtoll(e, NULL, 10); else if (!strcmp(l, "poll_timeout")) a->poll_timeout = atoi(e); } fclose(f); }
    snprintf(p, sizeof(p), "%s/update_offset", a->state); load_num(p, &a->offset, 0);
    snprintf(p, sizeof(p), "%s/pairing_expires", a->state); load_num(p, &a->pairing_expires, 0);
    snprintf(p, sizeof(p), "%s/pairing_state", a->state); char *s = read_all(p, NULL); if (s) { trim(s); snprintf(a->pairing_state, sizeof(a->pairing_state), "%s", s); free(s); }
    snprintf(p, sizeof(p), "%s/pairing_code", a->state); s = read_all(p, NULL); if (s) { trim(s); snprintf(a->pairing_code, sizeof(a->pairing_code), "%s", s); free(s); }
}
static int save_config(App *a) { char p[PATH_MAX], b[2048]; snprintf(p, sizeof(p), "%s/bridge.conf", a->config); int n = snprintf(b, sizeof(b), "# mode 0600\nbot_token=%s\nallowed_user_id=%lld\nallowed_chat_id=%lld\npoll_timeout=%d\n", a->token, a->allowed_user_id, a->allowed_chat_id, a->poll_timeout); return n < 0 || (size_t)n >= sizeof(b) ? -1 : write_atomic(p, b, (size_t)n, 0600); }
static int save_pairing(App *a) { char p[PATH_MAX]; snprintf(p, sizeof(p), "%s/pairing_state", a->state); if (write_atomic(p, a->pairing_state, strlen(a->pairing_state), 0600)) return -1; snprintf(p, sizeof(p), "%s/pairing_code", a->state); if (write_atomic(p, a->pairing_code, strlen(a->pairing_code), 0600)) return -1; snprintf(p, sizeof(p), "%s/pairing_expires", a->state); return save_num(p, a->pairing_expires); }

/* Structure-aware JSON scanner. It never extracts values with grep/sed. */
static const char *jskip(const char *, const char *);
static const char *jstr(const char *p, const char *e, char **out) {
    if (p >= e || *p != '"') return NULL; p++; size_t cap = 64, n = 0; char *s = malloc(cap); if (!s) return NULL;
    while (p < e && *p != '"') { unsigned char c = (unsigned char)*p++; if (c == '\\') { if (p >= e) { free(s); return NULL; } char x = *p++; if (x == '"' || x == '\\' || x == '/') c = (unsigned char)x; else if (x == 'n') c = '\n'; else if (x == 'r') c = '\r'; else if (x == 't') c = '\t'; else if (x == 'b') c = '\b'; else if (x == 'f') c = '\f'; else if (x == 'u') { unsigned v = 0; for (int i=0;i<4;i++) { if (p>=e || !isxdigit((unsigned char)*p)) { free(s); return NULL; } v = v*16 + (unsigned)(isdigit((unsigned char)*p) ? *p-'0' : tolower((unsigned char)*p)-'a'+10); p++; } c = v < 128 ? (unsigned char)v : '?'; } else { free(s); return NULL; } } if (n+2 > cap) { cap*=2; s=realloc(s,cap); if (!s) return NULL; } s[n++]=(char)c; }
    if (p >= e) { free(s); return NULL; } s[n]=0; *out=s; return p+1;
}
static const char *jskip(const char *p, const char *e) {
    while (p<e && isspace((unsigned char)*p)) p++; if (p>=e) return NULL;
    if (*p == '"') { char *s=NULL; const char *q=jstr(p,e,&s); free(s); return q; }
    if (*p == '{') { p++; for (;;) { while(p<e && (isspace((unsigned char)*p)||*p==',')) p++; if(p<e&&*p=='}') return p+1; char *k=NULL; p=jstr(p,e,&k); free(k); if(!p) return NULL; while(p<e&&isspace((unsigned char)*p))p++; if(p>=e||*p++!=':')return NULL; p=jskip(p,e); if(!p)return NULL; } }
    if (*p == '[') { p++; for (;;) { while(p<e&&(isspace((unsigned char)*p)||*p==','))p++; if(p<e&&*p==']')return p+1; p=jskip(p,e); if(!p)return NULL; } }
    while(p<e && !strchr(",}]", *p)) p++; return p;
}
static int jget(const char *o, const char *e, const char *key, const char **vs, const char **ve) {
    while(o<e&&isspace((unsigned char)*o))o++; if(o>=e||*o!='{')return 0; const char *p=o+1;
    for (;;) { while(p<e&&(isspace((unsigned char)*p)||*p==','))p++; if(p<e&&*p=='}')return 0; char *k=NULL; p=jstr(p,e,&k); if(!p)return 0; while(p<e&&isspace((unsigned char)*p))p++; if(p>=e||*p++!=':'){free(k);return 0;} while(p<e&&isspace((unsigned char)*p))p++; const char *v=p,*q=jskip(p,e); if(!q){free(k);return 0;} int yes=!strcmp(k,key); free(k); if(yes){*vs=v;*ve=q;return 1;} p=q; }
}
static int jlong(const char *s, const char *e, long long *out) { char b[64]; size_t n=0; while(s<e&&n+1<sizeof(b)&&!strchr(",}]",*s)&&!isspace((unsigned char)*s))b[n++]=*s++; b[n]=0; if(!n)return 0; char *z; long long v=strtoll(b,&z,10); if(z==b)return 0;*out=v;return 1; }
static int jbool(const char *s, int *out) { if(!strncmp(s,"true",4)){*out=1;return 1;}if(!strncmp(s,"false",5)){*out=0;return 1;}return 0; }
static char *jescape(const char *s) { size_t cap=strlen(s)*2+32,n=0; char *o=malloc(cap); if(!o)return NULL; for(;*s;s++){unsigned char c=(unsigned char)*s; const char *r=NULL; if(c=='"')r="\\\"";else if(c=='\\')r="\\\\";else if(c=='\n')r="\\n";else if(c=='\r')r="\\r";else if(c=='\t')r="\\t";if(r){size_t m=strlen(r);while(n+m+1>cap)cap*=2;o=realloc(o,cap);memcpy(o+n,r,m);n+=m;}else{while(n+2>cap)cap*=2;o=realloc(o,cap);o[n++]=(char)c;}}o[n]=0;return o; }

static int temp_file(const char *dir, const char *name, char *out) { if(snprintf(out,PATH_MAX,"%s/%s.XXXXXX",dir,name)>=PATH_MAX)return -1; int fd=mkstemp(out); if(fd<0)return -1; close(fd); chmod(out,0600); return 0; }
/* Token lives in curl's mode-0600 config file, never in curl argv or logs. */
static int api(App *a, const char *method, const char *body, char **resp, int *http, int *curl, char *detail, size_t ds) {
    char cfg[PATH_MAX], dat[PATH_MAX], out[PATH_MAX], status[PATH_MAX], err[PATH_MAX], url[1024];
    if (temp_file(a->state, "curlcfg", cfg) || temp_file(a->state, "curlbody", dat) || temp_file(a->state, "curlout", out) || temp_file(a->state, "curlstatus", status) || temp_file(a->state, "curlerr", err)) return -1;
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/%s", a->token, method);
    if (write_atomic(dat, body, strlen(body), 0600)) goto done;
    FILE *f = fopen(cfg, "w");
    if (!f) goto done;
    /* curl config values containing whitespace must be quoted.  Without it,
       curl sends a malformed Content-Type header and Telegram rejects sends. */
    fprintf(f, "url = %s\nrequest = POST\nheader = \"Content-Type: application/json\"\ndata-binary = @%s\noutput = %s\nstderr = %s\nsilent\nconnect-timeout = 15\nmax-time = 45\nwrite-out = %%{http_code}\n", url, dat, out, err);
    fclose(f); chmod(cfg, 0600);
    int fd = open(status, O_WRONLY | O_TRUNC);
    if (fd < 0) goto done;
    pid_t pid = fork();
    if (pid == 0) {
        dup2(fd, STDOUT_FILENO); close(fd);
        const char *cp = getenv("TG_CURL"); if (!cp || !*cp) cp = "curl";
        execlp(cp, cp, "--config", cfg, (char *)NULL); _exit(127);
    }
    close(fd); int st = 0;
    if (pid < 0 || waitpid(pid, &st, 0) < 0) { *curl = 127; snprintf(detail, ds, "curl process failed"); goto done; }
    *curl = WIFEXITED(st) ? WEXITSTATUS(st) : 128;
    char *ss = read_all(status, NULL); *http = ss ? atoi(ss) : 0; free(ss);
    if (*curl) { snprintf(detail, ds, "curl exit %d", *curl); goto done; }
    *resp = read_all(out, NULL);
    if (!*resp) { *curl = 1; snprintf(detail, ds, "empty curl response"); goto done; }
    unlink(cfg); unlink(dat); unlink(out); unlink(status); unlink(err); return 0;
done:
    unlink(cfg); unlink(dat); unlink(out); unlink(status); unlink(err); return -1;
}
static void transport_error(int cc,int hc,const char *d){if(cc==6)fprintf(stderr,"DNS failure: %s\n",d);else if(cc==7)fprintf(stderr,"Telegram API unreachable: %s\n",d);else if(cc==28)fprintf(stderr,"HTTPS timeout: %s\n",d);else if(cc==35||cc==51||cc==60)fprintf(stderr,"TLS/HTTPS failure: %s\n",d);else if(cc)fprintf(stderr,"HTTPS transport failure: %s\n",d);else fprintf(stderr,"Telegram HTTP error: %d\n",hc);}
static int api_ok(const char *s,long long *ec,long long *retry){const char *a,*b;int ok=0;if(!jget(s,s+strlen(s),"ok",&a,&b)||!jbool(a,&ok))return -1;if(ok)return 1;if(ec){*ec=0;if(jget(s,s+strlen(s),"error_code",&a,&b))jlong(a,b,ec);}if(retry){*retry=0;const char *p,*q;if(jget(s,s+strlen(s),"parameters",&p,&q)&&jget(p,q,"retry_after",&a,&b))jlong(a,b,retry);}return 0;}

static void pair_code(char *out) { static const char al[]="ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; unsigned char r[8]; int fd=open("/dev/urandom",O_RDONLY); if(fd>=0){read(fd,r,8);close(fd);}else{for(int i=0;i<8;i++)r[i]=(unsigned char)rand();} for(int i=0;i<4;i++)out[i]=al[r[i]%(sizeof(al)-1)];out[4]='-';for(int i=0;i<4;i++)out[5+i]=al[r[4+i]%(sizeof(al)-1)];out[9]=0; }
static int setup(App *a) { char t[512]; fprintf(stderr,"Paste BotFather token on stdin; it will not be printed: "); if(!fgets(t,sizeof(t),stdin))return 1;trim(t);if(!*t)return 1;snprintf(a->token,sizeof(a->token),"%s",t);char *r=NULL,d[128]={0};int hc=0,cc=0;if(api(a,"getMe","{}",&r,&hc,&cc,d,sizeof(d))||api_ok(r,NULL,NULL)!=1){transport_error(cc,hc,d);free(r);return 1;}free(r);a->allowed_user_id=a->allowed_chat_id=-1;strcpy(a->pairing_state,"none");a->pairing_code[0]=0;a->pairing_expires=0;if(save_config(a)||save_pairing(a))die("validated token but could not save configuration");puts("Bot token validated and saved. Run: ktm pair");return 0;}
static int pair(App *a) { if(!a->token[0])die("run tgbridge setup first");pair_code(a->pairing_code);a->pairing_expires=(long long)time(NULL)+600;strcpy(a->pairing_state,"pending");if(save_pairing(a))die("cannot save pairing state");printf("Pairing code (expires in 10 minutes): %s\nSend this exact command to your Bot: /pair %s\n",a->pairing_code,a->pairing_code);return 0; }

static int append_message(App *a,long long uid,long long mid,long long from,long long chat,long long date,const char *text){char *e=jescape(text);if(!e)return -1;char l[16384];int n=snprintf(l,sizeof(l),"{\"update_id\":%lld,\"message_id\":%lld,\"from_id\":%lld,\"chat_id\":%lld,\"timestamp\":%lld,\"text\":\"%s\",\"received_at\":%lld}\n",uid,mid,from,chat,date,e,(long long)time(NULL));free(e);if(n<0||(size_t)n>=sizeof(l))return -1;char p[PATH_MAX];snprintf(p,sizeof(p),"%s/messages.jsonl",a->inbox);FILE *f=fopen(p,"a");if(!f)return -1;fchmod(fileno(f),0600);int ok=fwrite(l,1,(size_t)n,f)==(size_t)n;fflush(f);fsync(fileno(f));fclose(f);return ok?0:-1;}

static int outbox(App *a) { char p[PATH_MAX];snprintf(p,sizeof(p),"%s/pending.jsonl",a->outbox);FILE *f=fopen(p,"r");if(!f)return 0;char tmp[PATH_MAX];snprintf(tmp,sizeof(tmp),"%s/pending.jsonl.tmp.XXXXXX",a->outbox);int fd=mkstemp(tmp);if(fd<0){fclose(f);return 1;}fchmod(fd,0600);FILE *w=fdopen(fd,"w");char l[16384];int fail=0;while(fgets(l,sizeof(l),f)){const char *s,*e;char *t=NULL;if(!jget(l,l+strlen(l),"text",&s,&e)||!jstr(s,e,&t)){fputs(l,w);continue;}char *x=jescape(t);free(t);char b[16384];snprintf(b,sizeof(b),"{\"chat_id\":%lld,\"text\":\"%s\"}",a->allowed_chat_id,x);free(x);char *r=NULL,d[128]={0};int hc=0,cc=0;if(api(a,"sendMessage",b,&r,&hc,&cc,d,sizeof(d))||api_ok(r,NULL,NULL)!=1){transport_error(cc,hc,d);fputs(l,w);fail=1;free(r);break;}free(r);}if(fail)while(fgets(l,sizeof(l),f))fputs(l,w);fclose(f);fflush(w);fsync(fileno(w));fclose(w);if(fail){rename(tmp,p);chmod(p,0600);}else{unlink(tmp);unlink(p);}return fail;}

static int sync_updates(App *a) {
    char b[256];snprintf(b,sizeof(b),"{\"offset\":%lld,\"timeout\":%d,\"allowed_updates\":[\"message\"]}",a->offset,a->poll_timeout);char *r=NULL,d[128]={0};int hc=0,cc=0;if(api(a,"getUpdates",b,&r,&hc,&cc,d,sizeof(d))){transport_error(cc,hc,d);return 1;}long long ec=0,ra=0;int ok=api_ok(r,&ec,&ra);if(ok!=1){if(ra)fprintf(stderr,"Telegram rate limit; retry after %lld seconds\n",ra);else fprintf(stderr,"Telegram API error code=%lld\n",ec);free(r);return 1;}const char *as,*ae;if(!jget(r,r+strlen(r),"result",&as,&ae)||*as!='['){free(r);fprintf(stderr,"malformed Telegram JSON: result is not an array\n");return 1;}const char *p=as+1;while(p<ae&&*p!=']'){while(p<ae&&(isspace((unsigned char)*p)||*p==','))p++;if(p>=ae||*p==']')break;const char *ue=jskip(p,ae);if(!ue){free(r);fprintf(stderr,"malformed update JSON; offset unchanged\n");return 1;}const char *s,*e;long long uid=0;if(!jget(p,ue,"update_id",&s,&e)||!jlong(s,e,&uid)){free(r);return 1;}const char *ms,*me;if(!jget(p,ue,"message",&ms,&me)){a->offset=uid+1;char op[PATH_MAX];snprintf(op,sizeof(op),"%s/update_offset",a->state);if(save_num(op,a->offset)){free(r);return 1;}p=ue;continue;}char *text=NULL;long long mid=0,from=0,chat=0,date=0;if(!jget(ms,me,"text",&s,&e)||!jstr(s,e,&text)||!jget(ms,me,"message_id",&s,&e)||!jlong(s,e,&mid)||!jget(ms,me,"date",&s,&e)||!jlong(s,e,&date)){free(text);a->offset=uid+1;char op[PATH_MAX];snprintf(op,sizeof(op),"%s/update_offset",a->state);save_num(op,a->offset);p=ue;continue;}const char *os,*oe;if(jget(ms,me,"from",&os,&oe)&&jget(os,oe,"id",&s,&e))jlong(s,e,&from);if(jget(ms,me,"chat",&os,&oe)&&jget(os,oe,"id",&s,&e))jlong(s,e,&chat);
        if(!strcmp(a->pairing_state,"pending")){char want[64];snprintf(want,sizeof(want),"/pair %s",a->pairing_code);if(time(NULL)<=a->pairing_expires&&!strcmp(text,want)){a->allowed_user_id=from;a->allowed_chat_id=chat;strcpy(a->pairing_state,"paired");a->pairing_code[0]=0;a->pairing_expires=0;if(save_config(a)||save_pairing(a)){free(text);free(r);return 1;}puts("Pairing succeeded.");}else log_msg(a,"pairing message discarded");}
        else if(!strcmp(a->pairing_state,"paired")){if(from!=a->allowed_user_id||chat!=a->allowed_chat_id)log_msg(a,"unauthorized message discarded from user/chat %lld/%lld",from,chat);else if(append_message(a,uid,mid,from,chat,date,text)){free(text);free(r);return 1;}}
        else log_msg(a,"message discarded before pairing");free(text);a->offset=uid+1;char op[PATH_MAX];snprintf(op,sizeof(op),"%s/update_offset",a->state);if(save_num(op,a->offset)){free(r);return 1;}p=ue;}
    mark_sync(a); free(r);return outbox(a);
}

static int extract_last(App *a,long long wanted,char **out){char p[PATH_MAX];snprintf(p,sizeof(p),"%s/messages.jsonl",a->inbox);FILE *f=fopen(p,"r");if(!f)return -1;char l[16384],*found=NULL;while(fgets(l,sizeof(l),f)){const char *s,*e;long long id=0;if(wanted&&(!jget(l,l+strlen(l),"message_id",&s,&e)||!jlong(s,e,&id)||id!=wanted))continue;if(jget(l,l+strlen(l),"text",&s,&e)){char *v=NULL;if(jstr(s,e,&v)){free(found);found=v;}}}fclose(f);if(!found)return -1;*out=found;return 0;}
static int inbox(App *a){char p[PATH_MAX];snprintf(p,sizeof(p),"%s/messages.jsonl",a->inbox);FILE *f=fopen(p,"r");if(!f){puts("Telegram Inbox\n(no messages)");return 0;}char l[16384];puts("Telegram Inbox");while(fgets(l,sizeof(l),f)){const char*s,*e;long long id=0,ts=0;char*t=NULL;if(jget(l,l+strlen(l),"message_id",&s,&e))jlong(s,e,&id);if(jget(l,l+strlen(l),"timestamp",&s,&e))jlong(s,e,&ts);if(jget(l,l+strlen(l),"text",&s,&e))jstr(s,e,&t);printf("\nmessage_id=%lld time=%lld\n--------------------------------\n%s\n--------------------------------\n",id,ts,t?t:"");free(t);}fclose(f);return 0;}
static int last(App *a){char*t=NULL;if(extract_last(a,0,&t)){fprintf(stderr,"inbox is empty\n");return 1;}fwrite(t,1,strlen(t),stdout);free(t);return 0;}
static int clipboard(const char *text){const char *cmds[2] = {"xclip", "xsel"}; for(int i=0;i<2;i++){int q[2];if(pipe(q))continue;pid_t p=fork();if(p==0){dup2(q[0],0);close(q[0]);close(q[1]);if(i==0)execlp(cmds[i],cmds[i],"-selection","clipboard",(char*)NULL);else execlp(cmds[i],cmds[i],"--clipboard","--input",(char*)NULL);_exit(127);}close(q[0]);write(q[1],text,strlen(text));close(q[1]);int st=0;waitpid(p,&st,0);if(WIFEXITED(st)&&WEXITSTATUS(st)==0)return 1;}return 0;}
static int copy_msg(App *a,int n,char **v){long long id=n?strtoll(v[0],NULL,10):0;char*t=NULL;if(extract_last(a,id,&t)){fprintf(stderr,"message not found\n");return 1;}char p[PATH_MAX];snprintf(p,sizeof(p),"%s/latest.txt",a->inbox);if(write_atomic(p,t,strlen(t),0600)){free(t);return 1;}printf("Saved exact message to %s%s\n",p,clipboard(t)?" and system clipboard":"");free(t);return 0;}
static int send_msg(App *a,int n,char **v){if(!a->token[0]||strcmp(a->pairing_state,"paired")||a->allowed_chat_id<0)die("setup and pair before sending");size_t cap=4096,z=0;char*t=n?strdup(v[0]):malloc(cap);if(!n){int c;while((c=fgetc(stdin))!=EOF){if(z+2>cap){cap*=2;t=realloc(t,cap);}t[z++]=(char)c;}t[z]=0;}if(!t||!*t)die("empty message");char*e=jescape(t);free(t);char l[16384];int k=snprintf(l,sizeof(l),"{\"id\":%lld,\"text\":\"%s\"}\n",(long long)time(NULL)*1000+(long long)(getpid()%1000),e);free(e);if(k<0||(size_t)k>=sizeof(l))die("message too long");char p[PATH_MAX];snprintf(p,sizeof(p),"%s/pending.jsonl",a->outbox);FILE*f=fopen(p,"a");if(!f)die("cannot open outbox");fchmod(fileno(f),0600);fwrite(l,1,(size_t)k,f);fflush(f);fsync(fileno(f));fclose(f);int rc=outbox(a);if(rc)fprintf(stderr,"message kept in outbox for a later sync\n");else puts("sent");return rc;}
static int status_cmd(App*a){printf("version=%s\ndata_dir=%s\npairing=%s\noffset=%lld\nallowed_user_id=%s\nallowed_chat_id=%s\n",VERSION,a->root,a->pairing_state,a->offset,a->allowed_user_id>=0?"set":"unset",a->allowed_chat_id>=0?"set":"unset");char p[PATH_MAX];snprintf(p,sizeof(p),"%s/pending.jsonl",a->outbox);struct stat st;printf("outbox_pending=%s\n",stat(p,&st)==0&&st.st_size>0?"yes":"no");snprintf(p,sizeof(p),"%s/last_sync",a->state);long long when=0;load_num(p,&when,0);if(when&&time(NULL)-when>86400)printf("WARNING: Kindle has not synced for more than 24h; Telegram may no longer retain older updates.\n");return 0;}
static int diagnose(App*a){printf("ktm %s\ndata_dir=%s\nconfig=%s (%s)\ncurl=%s\nplatform=%s\n",VERSION,a->root,a->config,access(a->config,W_OK)==0?"writable":"not writable",getenv("TG_CURL")&&*getenv("TG_CURL")?getenv("TG_CURL"):"PATH/curl",getenv("KINDLE_PLATFORM")&&*getenv("KINDLE_PLATFORM")?getenv("KINDLE_PLATFORM"):"unknown");puts("Run curl --version and kpm --version on the Kindle; no rootfs changes were made.");return 0;}
static int reset_pairing(App*a){a->allowed_user_id=a->allowed_chat_id=-1;strcpy(a->pairing_state,"none");a->pairing_code[0]=0;a->pairing_expires=0;if(save_config(a)||save_pairing(a))return 1;puts("pairing reset; run ktm pair to create a new code");return 0;}

int main(int argc,char**argv){App a;app_defaults(&a);if(set_root(&a,root_from_env())||ensure_dirs(&a))die("data directory is not writable");char vp[PATH_MAX];snprintf(vp,sizeof(vp),"%s/version",a.state);if(access(vp,F_OK)!=0)write_atomic(vp,VERSION,strlen(VERSION),0600);load_config(&a);if(argc<2){fprintf(stderr,"usage: ktm {setup|pair|sync|inbox|last|copy|send|status|diagnose|reset-pairing}\n");return 2;}int r=0;if(!strcmp(argv[1],"setup"))r=setup(&a);else if(!strcmp(argv[1],"pair"))r=pair(&a);else if(!strcmp(argv[1],"sync"))r=sync_updates(&a);else if(!strcmp(argv[1],"inbox"))r=inbox(&a);else if(!strcmp(argv[1],"last"))r=last(&a);else if(!strcmp(argv[1],"copy"))r=copy_msg(&a,argc-2,argv+2);else if(!strcmp(argv[1],"send"))r=send_msg(&a,argc-2,argv+2);else if(!strcmp(argv[1],"status"))r=status_cmd(&a);else if(!strcmp(argv[1],"diagnose"))r=diagnose(&a);else if(!strcmp(argv[1],"reset-pairing"))r=reset_pairing(&a);else{fprintf(stderr,"unknown command: %s\n",argv[1]);r=2;}free(a.root);return r;}
