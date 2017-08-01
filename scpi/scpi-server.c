/**
 * @brief Red Pitaya Scpi server implementation
 * @Author Red Pitaya
 * (c) Red Pitaya  http://www.redpitaya.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>

#include "common.h"
#include "scpi-commands.h"
#include "scpi_gen.h"
//#include "scpi_osc.h"
//#include "scpi_lg.h"
#include "scpi_la.h"

#include "scpi/parser.h"
#include "redpitaya/rp1.h"

/**
 * process signal handlers
 */

static bool app_exit = false;

static void handleCloseChildEvents() {
    struct sigaction sigchld_action = {
            .sa_handler = SIG_DFL,
            .sa_flags = SA_NOCLDWAIT
    };
    sigaction(SIGCHLD, &sigchld_action, NULL);
}

static void termSignalHandler(int signum) {
    app_exit = true;
    syslog (LOG_NOTICE, "Received terminate signal: %i. Exiting...", signum);
}

static void installTermSignalHandler() {
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = termSignalHandler;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
}

/**
 * interface specific functions used by SCPI server
 */

size_t SCPI_Write(scpi_t * context, const char * data, size_t len) {
    if (context->user_context == NULL) {
        return SCPI_RES_ERR;
    }
    rpscpi_context_t *rp = (rpscpi_context_t *) context->user_context;
    if (rp->connfd != 0) {
        return write(rp->connfd, data, len);
    }
    return SCPI_RES_OK;
}

scpi_result_t SCPI_Flush(scpi_t * context) {
    (void) context;

    return SCPI_RES_OK;
}

int SCPI_Error(scpi_t * context, int_fast16_t err) {
    (void) context;
    /* BEEP */
    syslog(LOG_ERR, "**ERROR: %d, \"%s\".", (int16_t) err, SCPI_ErrorTranslate(err));
    return 0;
}

scpi_result_t SCPI_Control(scpi_t * context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val) {
    (void) context;

    if (SCPI_CTRL_SRQ == ctrl) {
        syslog(LOG_ERR, "**SRQ: 0x%X (%d).", val, val);
    } else {
        syslog(LOG_ERR, "**CTRL %02x: 0x%X (%d).", ctrl, val, val);
    }
    return SCPI_RES_OK;
}

scpi_result_t SCPI_Reset(scpi_t * context) {
    (void) context;

    syslog(LOG_ERR, "**Reset.");
    return SCPI_RES_OK;
}

scpi_result_t SCPI_SystemCommTcpipControlQ(scpi_t * context) {
    (void) context;

    return SCPI_RES_ERR;
}

static int createServer(int port) {
    int fd;
    int rc;
    struct sockaddr_in servaddr;

    /* Configure TCP Server */
    memset(&servaddr, 0, sizeof (servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    /* Create socket */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "socket() failed");
        exit(EXIT_FAILURE);
    }

    /* Bind to socket */
    rc = bind(fd, (struct sockaddr *) &servaddr, sizeof (servaddr));
    if (rc < 0) {
        syslog(LOG_ERR, "bind() failed");
        close(fd);
        exit(EXIT_FAILURE);
    }

    /* Listen on socket */
    listen(fd, 1);
    if (rc < 0) {
        syslog(LOG_ERR, "listen() failed");
        close(fd);
        exit(EXIT_FAILURE);
    }

    return fd;
}

static int waitServer(int fd) {
    fd_set fds;
    struct timeval timeout;
    int rc;
    int max_fd;

    FD_ZERO(&fds);
    max_fd = fd;
    FD_SET(fd, &fds);

    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    rc = select(max_fd + 1, &fds, NULL, NULL, &timeout);

    return rc;
}

#define LISTEN_PORT 5000
#define MAX_BUFF_SIZE 1024

/**
 * Main daemon entrance point. Opens a socket and listens for any incoming connection.
 * When client connects, if forks the conversation into a new socket and the daemon (parent process)
 * waits for another connection. It can handle multiple connections simultaneously.
 * @param argc  not used
 * @param argv  not used
 * @return
 */

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    // register process signal handlers
    installTermSignalHandler();
    handleCloseChildEvents();

    // Open logging into "/var/log/messages" or /var/log/syslog" or other configured...
    setlogmask (LOG_UPTO (LOG_INFO));
    openlog("scpi-server", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    syslog(LOG_NOTICE, "scpi-server started");

    SCPI_Init(&scpi_context,
            scpi_commands,
            &scpi_interface,
            scpi_units_def,
            SCPI_IDN1, SCPI_IDN2, SCPI_IDN3, SCPI_IDN4,
            scpi_input_buffer, SCPI_INPUT_BUFFER_LENGTH,
            scpi_error_queue_data, SCPI_ERROR_QUEUE_SIZE);
    syslog(LOG_NOTICE, "scpi-parser initialized");

    // prepare redpitaya specific context
    scpi_context.user_context = malloc(1 * sizeof(rpscpi_context_t));
    rpscpi_context_t *rp = (rpscpi_context_t *) scpi_context.user_context;

    // initialize API
    if (!rpscpi_gen_init(rp, 2))  return (EXIT_FAILURE);
//  if (!rpscpi_osc_init(rp, 2))  return (EXIT_FAILURE);
//  if (!rpscpi_lg_init(rp))      return (EXIT_FAILURE);
    if (!rpscpi_la_init(rp))      return (EXIT_FAILURE);

    rp->connfd = 0;

    int listenfd = createServer(LISTEN_PORT);

    syslog(LOG_INFO, "Server is listening on port %d\n", LISTEN_PORT);

    // Socket is opened and listening on port. Now we can accept connections
    while (1) {
        if (app_exit == true)
            break;

        struct sockaddr_in cliaddr;
        socklen_t clilen = sizeof(cliaddr);
        rp->connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);

        if (rp->connfd == -1) {
            syslog(LOG_ERR, "Failed to accept connection (%s)", strerror(errno));
            return (EXIT_FAILURE);
        }

        while (1) {
            char smbuffer[MAX_BUFF_SIZE];
            int rc = waitServer(rp->connfd);
            if (rc < 0) { /* failed */
                perror("  recv() failed");
                break;
            }
            if (rc == 0) { /* timeout */
                SCPI_Input(&scpi_context, NULL, 0);
            }
            if (rc > 0) { /* something to read */
                rc = recv(rp->connfd, smbuffer, sizeof (smbuffer), 0);
                if (rc < 0) {
                    if (errno != EWOULDBLOCK) {
                        perror("  recv() failed");
                        break;
                    }
                } else if (rc == 0) {
                    syslog(LOG_INFO, "Connection closed.");
                    break;
                } else {
                    SCPI_Input(&scpi_context, smbuffer, rc);
                }
            }
        }

        close(rp->connfd);
    }
    close(listenfd);

    // release API
    if (!rpscpi_gen_release(rp))  return (EXIT_FAILURE);
//  if (!rpscpi_osc_release(rp))  return (EXIT_FAILURE);
//  if (!rpscpi_lg_release(rp))   return (EXIT_FAILURE);
    if (!rpscpi_la_release(rp))   return (EXIT_FAILURE);
    // free user context
    free(scpi_context.user_context);

    syslog(LOG_INFO, "scpi-server stopped.");
    closelog();
    return (EXIT_SUCCESS);
}

