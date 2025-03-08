#ifndef _CWC_IPC_H
#define _CWC_IPC_H

/* The ipc messaging format is:
 *
 * ```
 * cwc-ipc\n
 * <opcode>\n
 * body...
 * ```
 *
 * The first line contain the signature which is "cwc-ipc", the second line
 * contain a byte of opcode, and the rest is the message body.
 */

#define IPC_HEADER  "cwc-ipc"
#define HEADER_SIZE (sizeof(IPC_HEADER) + 2)

/* Opcode is 1 byte after header */
enum cwc_ipc_opcode {
    /* the compositor will evaluate received string and send returned value  */
    IPC_EVAL = 1,
    /* the compositor response of IPC_EVAL */
    IPC_EVAL_RESPONSE,

    /* compositor object signal such client, screen, etc. */
    IPC_SIGNAL,
};

/* check if msg header is valid */
bool check_header(const char *msg);

/* create message with known string length */
int ipc_create_message_n(char *dest,
                         int maxlen,
                         enum cwc_ipc_opcode opcode,
                         const char *body,
                         int n);

/* create message with c string */
int ipc_create_message(char *dest,
                       int maxlen,
                       enum cwc_ipc_opcode opcode,
                       const char *body);

/* return a pointer to the message body (a slice) in msg.  */
const char *ipc_get_body(const char *msg, enum cwc_ipc_opcode *opcode);

#endif // !_CWC_IPC_H
