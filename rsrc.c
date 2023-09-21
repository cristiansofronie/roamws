#include <libwebsockets.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <cjson/cJSON.h>

static struct my_conn {
        lws_sorted_usec_list_t  sul;
        struct lws              *wsi;
        uint16_t                retry_count;
} mco;

static struct lws_context *context;

static const uint32_t backoff_ms[] = { 1000, 2000, 3000, 4000, 5000 };

static const lws_retry_bo_t retry = {
        .retry_ms_table                 = backoff_ms,
        .retry_ms_table_count           = LWS_ARRAY_SIZE(backoff_ms),
        .conceal_count                  = LWS_ARRAY_SIZE(backoff_ms),
        .secs_since_valid_ping          = 3,
        .secs_since_valid_hangup        = 10,
        .jitter_percent                 = 20,
};

static void
connect_client(lws_sorted_usec_list_t *sul)
{
        struct my_conn *m = lws_container_of(sul, struct my_conn, sul);
        struct lws_client_connect_info i;

        memset(&i, 0, sizeof(i));

        i.context = context;
        i.port = 8181;
        i.address = "localhost";
        i.path = "/";
        i.host = i.address;
        i.origin = i.address;
        i.pwsi = &m->wsi;
        i.retry_and_idle_policy = &retry;
        i.userdata = m;

        if (!lws_client_connect_via_info(&i))
                if (lws_retry_sul_schedule(context, 0, sul, &retry,
                                           connect_client, &m->retry_count)) {
                        lwsl_err("%s: connection attempts exhausted\n", __func__);
                }
}

const char *json_message_before = "{"
  "\"actionType\": \"openSrc\","
  "\"data\": {"
    "\"actionType\": \"openSrc\","
    "\"pattern\": \"";


const char *json_message_after = "\"}}";

char buffer[4096];

static int
callback_minimal(struct lws *wsi, enum lws_callback_reasons reason,
                 void *user, void *in, size_t len)
{
        struct my_conn *m = (struct my_conn *)user;

        switch (reason) {

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
                printf("Failed to connect\n");
                goto do_retry;
                break;

        case LWS_CALLBACK_ESTABLISHED:
            printf("Connected to server\n");
            break;

        case LWS_CALLBACK_CLIENT_ESTABLISHED:
        {
            printf("Client has connected to the server\n");

            int filedes[2];
            if (pipe(filedes) == -1) {
              perror("pipe");
              exit(1);
            }

            pid_t pid = fork();

            if (pid == -1) {
              perror("fork");
              exit(1);
            } else if (pid == 0) {
              while ((dup2(filedes[1], STDOUT_FILENO) == -1) && (errno == EINTR)) {}
              close(filedes[0]);

              // int devNull = open("/dev/null", O_RDONLY);
              // int dup2Result = dup2(devNull, STDIN_FILENO);

              int devNull = open("/home/crist/.local/share/nvim/features", O_RDONLY);
              int dup2Result = dup2(devNull, STDIN_FILENO);

              if(dup2Result == -1) {
                  perror("Error in dup2");
                  exit(EXIT_FAILURE);
              }

              execl("/usr/bin/dmenu", "dmenu", "-f", "-l", "50", "-i", NULL);

              // execl("/usr/bin/zenity", "zenity", "--entry", "--text=Query:", NULL);
              perror("execl");
              exit(1);
            }

            close(filedes[1]);

            size_t read_n = read(filedes[0], buffer, 4096);
            wait(0);

            if (read_n < 2) {
              exit(1);
            }

            buffer[read_n - 1] = '\0';

            unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + strlen(json_message_before) + read_n + strlen(json_message_after) + LWS_SEND_BUFFER_POST_PADDING];
            unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];
            size_t n = sprintf( (char *)p, "%s%s%s", json_message_before, buffer, json_message_after);
            lws_write( wsi, p, n, LWS_WRITE_TEXT );
            exit(0);
            break;
        }

        case LWS_CALLBACK_CLIENT_CLOSED:
                goto do_retry;

        default:
                break;
        }

        return lws_callback_http_dummy(wsi, reason, user, in, len);

do_retry:
        if (lws_retry_sul_schedule_retry_wsi(wsi, &m->sul, connect_client,
                                             &m->retry_count)) {
                lwsl_err("%s: connection attempts exhausted\n", __func__);
        }

        return 0;
}

static const struct lws_protocols protocols[] = {
        { "http-only", callback_minimal, 0, 0, 0, NULL, 0 },
        LWS_PROTOCOL_LIST_TERM
};

int main(int argc, const char **argv)
{
        struct lws_context_creation_info info;
        const char *p;
        int n = 0;

        memset(&info, 0, sizeof info);

        info.port = CONTEXT_PORT_NO_LISTEN;
        info.protocols = protocols;

        context = lws_create_context(&info);

        if (!context) {
                lwsl_err("lws init failed\n");
                return 1;
        }

        lws_sul_schedule(context, 0, &mco.sul, connect_client, 1);

        while (n >= 0)
                n = lws_service(context, 0);

        lws_context_destroy(context);
        return 0;
}

