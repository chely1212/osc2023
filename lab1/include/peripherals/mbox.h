#ifndef _MBOX_H_
#define _MBOX_H_
#include "peripherals/base.h"

#define MBOX_BASE                       (MMIO_BASE + 0xb880)

// address map
#define MBOX_READ                       (unsigned int*)(MBOX_BASE)
#define MBOX_STATUS                     (unsigned int*)(MBOX_BASE + 0x18)
#define MBOX_WRITE                      (unsigned int*)(MBOX_BASE + 0x20)

// flag
#define MBOX_EMPTY                      0x40000000
#define MBOX_FULL                       0x80000000

// code
#define MBOX_CODE_BUF_REQ               0x00000000
#define MBOX_CODE_BUF_RES_SUCC          0x80000000
#define MBOX_CODE_TAG_REQ               0x00000000

// tag
#define MBOX_TAG_GET_BOARD_REVISION     0x00010002
#define MBOX_TAG_GET_ARM_MEMORY		0x00010005
#define MBOX_TAG_GET_VC_MEMORY          0x00010006
#define MBOX_TAG_SET_CLOCK_RATE         0x00038002
#define MBOX_TAG_SET_PHY_WIDTH_HEIGHT   0x00048003
#define MBOX_TAG_SET_VTL_WIDTH_HEIGHT   0x00048004
#define MBOX_TAG_SET_VTL_OFFSET         0x00048009
#define MBOX_TAG_SET_DEPTH              0x00048005
#define MBOX_TAG_SET_PIXEL_ORDER        0x00048006
#define MBOX_TAG_ALLOCATE_BUFFER        0x00040001
#define MBOX_TAG_GET_PITCH              0x00040008


//
#define MBOX_CH_POWER   0
#define MBOX_CH_FB      1
#define MBOX_CH_VUART   2
#define MBOX_CH_VCHIQ   3
#define MBOX_CH_LEDS    4
#define MBOX_CH_BTNS    5
#define MBOX_CH_TOUCH   6
#define MBOX_CH_COUNT   7
#define MBOX_CH_PROP    8


#endif


