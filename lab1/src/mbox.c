#include "peripherals/mbox.h"
#include "uart.h"

int mbox_call(unsigned int* mbox, unsigned char channel) {
    unsigned int r = (unsigned int)(((unsigned long)mbox) & (~0xF)) | (channel & 0xF);
    // wait until full flag unset
    while (*MBOX_STATUS & MBOX_FULL) {
    }
    // write address of message + channel to mailbox
    *MBOX_WRITE = r;
    // wait until response
    while (1) {
        // wait until empty flag unset
        while (*MBOX_STATUS & MBOX_EMPTY) {
        }
        // is it a response to our msg?
        if (r == *MBOX_READ) {
            // check is response success
            return mbox[1] == MBOX_CODE_BUF_RES_SUCC;
        }
    }
    return 0;
}


void mbox_get_board_revision() {
    unsigned int __attribute__((aligned(16))) mbox[7];

    mbox[0] = 7 * 4;  // buffer size in bytes
    mbox[1] = MBOX_CODE_BUF_REQ;
    // tags begin
    mbox[2] = MBOX_TAG_GET_BOARD_REVISION;  // tag identifier
    mbox[3] = 4;                            // maximum of request and response value buffer's length.
    mbox[4] = MBOX_CODE_TAG_REQ;            // tag code
    mbox[5] = 0;                            // value buffer
    mbox[6] = 0x0;                          // end tag
    // tags end
    mbox_call(mbox, 8);
    uart_send_string("Board Revision: ");
    uart_hex(mbox[5]);
    uart_send_string("\n");
}


void mbox_get_arm_memory() {
    unsigned int __attribute__((aligned(16))) mbox[8];
    mbox[0] = 8 * 4;  // buffer size in bytes
    mbox[1] = MBOX_CODE_BUF_REQ;
    // tags begin
    mbox[2] = MBOX_TAG_GET_ARM_MEMORY;  // tag identifier
    mbox[3] = 8;                       // maximum of request and response value buffer's length.
    mbox[4] = MBOX_CODE_TAG_REQ;       // tag code
    mbox[5] = 0;                       // base address
    mbox[6] = 0;                       // size in bytes
    mbox[7] = 0x0;                     // end tag
    // tags end
    mbox_call(mbox, 8);
    uart_send_string("ARM core base address: 0x");
    uart_hex(mbox[5]);
    uart_send_string(";SIZE:");
    uart_hex(mbox[6]);
    uart_send_string("\n");

}
