#ifndef DLOADDEFH
#define DLOADDEFH
#define FALSE 0
#define TRUE 1

#define  WRITE_SIZ   8  /* Minimum size of the Write packet */
#define  ERASE_SIZ   9  /* Total size of the Erase packet   */
#define  GO_SIZ      7  /* Total size of the Go packet      */
#define  UNLOCK_SIZ  9  /* Total size of the unlock packet  */
#define  WRITE_32BIT_SIZ   9  /* Minimum size of the Write_32bit packet */

#define  MAX_RESPONSE_LEN  (0x2000)  // max HDLC packet size of HOST

/* area for download packet, etc. = 8K */
#define  PKT_BUF_SIZE           (0x1000)    //Response from TARGET

   /* Maximum size of a received packet. */

#define  MAX_PACKET_LEN    PKT_BUF_SIZE

   /* Minimum size of a received packet. */
#define  MIN_PACKET_LEN    3        /* 2 bytes CRC + 1 byte command code */

#define  DLOAD_BASE           0x41700000
#define  EDLOAD_BASE          0x2A000000
#define  DLOAD_LIMIT          0x00900000

/* REMOVE_BEGIN,===00001===,Owner : zjj, 2007-11-16, 11:08:05 */
#if 0
typedef char byte;
#endif
/* REMOVE_END,  ===00001===,Owner : zjj, 2007-11-16, 11:08:05 */
typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned char      boolean;
typedef unsigned long DWORD;

typedef enum tagCMDType
   {
   CMD_WRITE  = 0x01,   /* Write a block of data to memory (received)    */
   CMD_ACK    = 0x02,   /* Acknowledge receiving a packet  (transmitted) */
   CMD_NAK    = 0x03,   /* Acknowledge a bad packet        (transmitted) */
   CMD_ERASE  = 0x04,   /* Erase a block of memory         (received)    */
   CMD_GO     = 0x05,   /* Begin execution at an address   (received)    */
   CMD_NOP    = 0x06,   /* No operation, for debug         (received)    */
   CMD_PREQ   = 0x07,   /* Request implementation info     (received)    */
   CMD_PARAMS = 0x08,   /* Provide implementation info     (transmitted) */
   CMD_DUMP   = 0x09,   /* Debug: dump a block of memory   (received)    */
   CMD_RESET  = 0x0A,   /* Reset the phone                 (received)    */
   CMD_UNLOCK = 0x0B,   /* Unlock access to secured ops    (received)    */
   CMD_VERREQ = 0x0C,   /* Request software version info   (received)    */
   CMD_VERRSP = 0x0D,   /* Provide software version info   (transmitted) */
   CMD_PWROFF = 0x0E,   /* Turn phone power off            (received)    */
   CMD_WRITE_32BIT = 0x0F,  /* Write a block of data to 32-bit memory 
                               address (received)                        */
   CMD_MEM_DEBUG_QUERY = 0x10, /* Memory debug query       (received)    */
   CMD_MEM_DEBUG_INFO  = 0x11, /* Memory debug info        (transmitted) */
   CMD_MEM_READ_REQ    = 0x12, /* Memory read request      (received)    */
   CMD_MEM_READ_RESP   = 0x13, /* Memory read response     (transmitted) */
   CMD_DLOAD_SWITCH = 0x3A
   } CMD_Type;


typedef struct
  {
  DWORD length;                 /* Length of packet so far */
  boolean   broken;                 /* Set if the packet can't be built */
  byte      buf[MAX_RESPONSE_LEN];  /* The packet under construction */
  } pkt_buffer_type;

/* Async HDLC achieves data transparency at the byte level by using
   two special values. The first is a flag value which begins and ends
   every packet: */
#define  ASYNC_HDLC_FLAG      0x7e

/* The flag value might appear in the data.  If it does, it is sent as
   a two-byte sequence consisting of a special escape value followed by
   the flag value XORed with 0x20.  This gives a special meaning to the
   escape character, so if it appears in the data it is itself escaped
   in the same way. */
#define  ASYNC_HDLC_ESC       0x7d
#define  ASYNC_HDLC_ESC_MASK  0x20

   /* Other packets must be built up on the fly.  This data type and the
      associated macros support dynamic construction of a packet.  */

/* According to the protocol spec, a version string can be at most 20 bytes */
#define  MAX_VERSTRING_LEN    (20)

#define  ROOM_FOR_CRC   (4)         /* Allow this much room for the CRC,
                                       including worst-case expansion due
                                       to byte-stuffing */
#define  ROOM_FOR_FLAG  (1)         /* Allow this much room for trailing flag */

/* Max length for a memory debug packet is 2048 bytes, added in version 6 */
#define  MAX_MEM_DEBUG_LEN    (2048)

/* With the memory debug feature, the max data length is 2KB, plus 1 byte for
 * for command, 4 bytes for adress and 2 bytes for length of the data
 */
#define CRC_TAB_SIZE    256             /* 2^CRC_TAB_BITS      */

const word crc_16_l_table_dump[ CRC_TAB_SIZE ] = {
  0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
  0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
  0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
  0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
  0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
  0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
  0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
  0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
  0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
  0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
  0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
  0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
  0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
  0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
  0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
  0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
  0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
  0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
  0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
  0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
  0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
  0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
  0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
  0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
  0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
  0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
  0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
  0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
  0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
  0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
  0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
  0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};


static const byte rsp_ack[] =                   /* ACK */
  {
  CMD_ACK,             /* ACK type */
  0x6a,                /* CRC, MSB */
  0xd3,                /* CRC, LSB */
  ASYNC_HDLC_FLAG
  };

static const byte rsp_nak_invalid_fcs[] =       /* NAK_INVALID_FCS */
  {
  CMD_NAK,             /* NACK type */
  0x00,                /* Reason code, MSB */
  0x01,                /* Reason code, LSB */
  0x21,                /* CRC, MSB */
  0x38,                /* CRC, LSB */
  ASYNC_HDLC_FLAG
  };

static const byte rsp_nak_invalid_dest[] =      /* NAK_INVALID_DEST */
  {
  CMD_NAK,             /* NACK type */
  0x00,                /* Reason code, MSB */
  0x02,                /* Reason code, LSB */
  0xba,                /* CRC, MSB */
  0x0a,                /* CRC, LSB */
  ASYNC_HDLC_FLAG
  };

static const byte rsp_nak_invalid_len[] =       /* NAK_INVALID_LEN */
  {
  CMD_NAK,             /* NACK type */
  0x00,                /* Reason code, MSB */
  0x03,                /* Reason code, LSB */
  0x33,                /* CRC, MSB */
  0x1b,                /* CRC, LSB */
  ASYNC_HDLC_FLAG
  };

static const byte rsp_nak_early_end[] =         /* NAK_EARLY_END */
  {
  CMD_NAK,             /* NACK type */
  0x00,                /* Reason code, MSB */
  0x04,                /* Reason code, LSB */
  0x8c,                /* CRC, MSB */
  0x6f,                /* CRC, LSB */
  ASYNC_HDLC_FLAG
  };

static const byte rsp_nak_too_large[] =         /* NAK_TOO_LARGE */
  {
  CMD_NAK,                      /* NACK type */
  0x00,                         /* Reason code, MSB */
  0x05,                         /* Reason code, LSB */
  0x05,                         /* CRC, MSB */
  ASYNC_HDLC_ESC,
  0x7e ^ ASYNC_HDLC_ESC_MASK,   /* CRC, LSB */
  ASYNC_HDLC_FLAG
  };

static const byte rsp_nak_invalid_cmd[] =       /* NAK_INVALID_CMD */
  {
  CMD_NAK,             /* NACK type */
  0x00,                /* Reason code, MSB */
  0x06,                /* Reason code, LSB */
  0x9e,                /* CRC, MSB */
  0x4c,                /* CRC, LSB */
  ASYNC_HDLC_FLAG
  };

static const byte rsp_nak_failed[] =            /* NAK_FAILED */
  {
  CMD_NAK,             /* NACK type */
  0x00,                /* Reason code, MSB */
  0x07,                /* Reason code, LSB */
  0x17,                /* CRC, MSB */
  0x5d,                /* CRC, LSB */
  ASYNC_HDLC_FLAG
  };

static const byte rsp_nak_wrong_iid[] =         /* NAK_WRONG_IID */
  {
  CMD_NAK,             /* NACK type */
  0x00,                /* Reason code, MSB */
  0x08,                /* Reason code, LSB */
  0xe0,                /* CRC, MSB */
  0xa5,                /* CRC, LSB */
  ASYNC_HDLC_FLAG
  };

static const byte rsp_nak_bad_vpp[] =           /* NAK_BAD_VPP */
  {
  CMD_NAK,             /* NACK type */
  0x00,                /* Reason code, MSB */
  0x09,                /* Reason code, LSB */
  0x69,                /* CRC, MSB */
  0xb4,                /* CRC, LSB */
  ASYNC_HDLC_FLAG
  };

static const byte rsp_nak_verify_failed[] =     /* NAK_VERIFY_FAILED */
  {
  CMD_NAK,             /* NACK type */
  0x00,                /* Reason code, MSB */
  0x0a,                /* Reason code, LSB */
  0xf2,                /* CRC, MSB */
  0x86,                /* CRC, LSB */
  ASYNC_HDLC_FLAG
  };

/* REMOVE_BEGIN,===00001===,Owner : zjj, 2007-11-9, 11:24:00 */
#if 0
#ifndef FEATURE_HWTC
static const byte rsp_nak_no_sec_code[] =          /* NAK_NO_SEC_CODE */
  {
  CMD_NAK,    /* NACK type */
  0x000,      /* Reason code, MSB */
  0x00B,      /* Reason code, LSB */
  0x07B,      /* CRC, MSB */
  0x097,      /* CRC, LSB */
  ASYNC_HDLC_FLAG
  };

static const byte rsp_nak_bad_sec_code[] =        /* NAK_BAD_SEC_CODE */
  {
  CMD_NAK,    /* NACK type */
  0x000,      /* Reason code, MSB */
  0x00C,      /* Reason code, LSB */
  0x0C4,      /* CRC, MSB */
  0x0E3,      /* CRC, LSB */
  ASYNC_HDLC_FLAG
  };
#endif /* FEATURE_HWTC */
#endif
/* REMOVE_END,  ===00001===,Owner : zjj, 2007-11-9, 11:24:00 */


static const byte rsp_nak_op_not_permitted[] =      /* NAK_OP_NOT_PERMITTED */
  {
  CMD_NAK,    /* NACK type */
  0x000,      /* Reason code, MSB */
  0x00E,      /* Reason code, LSB */
  0x0,      /* CRC, MSB */
  0x0,      /* CRC, LSB */
  ASYNC_HDLC_FLAG
  };
  
static const byte rsp_nak_invalid_addr[] =          /* NAK_INVALID_ADDR */
  {
  CMD_NAK,    /* NACK type */
  0x000,      /* Reason code, MSB */
  0x00F,      /* Reason code, LSB */
  0x0,      /* CRC, MSB */
  0x0,      /* CRC, LSB */
  ASYNC_HDLC_FLAG
  };
/*===========================================================================

MACRO START_BUILDING_PACKET

DESCRIPTION
  This macro initializes the process of dynamically building a packet.

PARAMETERS
  pkt     A pkt_buffer_type struct in which the packet will be built.

DEPENDENCIES
  None.

RETURN VALUE
  None.

SIDE EFFECTS
  pkt is evaluated twice within this macro.
  This macro is not an expression, nor is it a single statement.  It
  must be called with a trailing semicolon.

===========================================================================*/

#define  START_BUILDING_PACKET(pkt)          \
               pkt.length = 0;               \
               pkt.broken = FALSE

/*===========================================================================

MACRO ADD_BYTE_TO_PACKET

DESCRIPTION
  This macro adds a single byte to a packet being built dynamically.

PARAMETERS
  pkt     A pkt_buffer_type struct in which the packet will be built.
  val     The byte to be added to the packet.

DEPENDENCIES
  START_BUILDING_PACKET must have been called on pkt before calling
  this macro.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/

#define  ADD_BYTE_TO_PACKET(pkt,val)         \
               add_byte_to_packet(&pkt, val)


/*===========================================================================

MACRO ADD_WORD_TO_PACKET

DESCRIPTION
  This macro adds a word (most significant byte first) to a packet
  being built dynamically.

PARAMETERS
  pkt     A pkt_buffer_type struct in which the packet will be built.
  val     The word to be added to the packet.

DEPENDENCIES
  START_BUILDING_PACKET must have been called on pkt before calling
  this macro.

RETURN VALUE
  None.

SIDE EFFECTS
  Each argument is evaluated twice within this macro.
  This macro is not an expression, not is it a single statement.  It
  must be called with a trailing semicolon.

===========================================================================*/

#define  ADD_WORD_TO_PACKET(pkt,val)         \
  /*lint -e778 val or (val >> 8) may evaluate to zero. */                    \
  add_byte_to_packet(&pkt, (const byte)((val >> 8) & 0xFF)); /* high byte */ \
  add_byte_to_packet(&pkt, (const byte)(val & 0xFF))         /* low  byte */ \
  /*lint +e778 */

/*===========================================================================

MACRO ADD_DWORD_TO_PACKET

DESCRIPTION
  This macro adds a dword (most significant byte first) to a packet
  being built dynamically.

PARAMETERS
  pkt     A pkt_buffer_type struct in which the packet will be built.
  val     The word to be added to the packet.

DEPENDENCIES
  START_BUILDING_PACKET must have been called on pkt before calling
  this macro.

RETURN VALUE
  None.

SIDE EFFECTS
  Each argument is evaluated twice within this macro.
  This macro is not an expression, not is it a single statement.  It
  must be called with a trailing semicolon.

===========================================================================*/

#define  ADD_DWORD_TO_PACKET(pkt,val)         \
  /*lint -e778 val or (val >> 8) may evaluate to zero. */                  \
  add_byte_to_packet(&pkt, (const byte)((val >> 24) & 0xFF)); /* byte 3 */ \
  add_byte_to_packet(&pkt, (const byte)((val >> 16) & 0xFF)); /* byte 2 */ \
  add_byte_to_packet(&pkt, (const byte)((val >> 8) & 0xFF));  /* byte 1 */ \
  add_byte_to_packet(&pkt, (const byte)(val & 0xFF))          /* byte 0 */ \
  /*lint +e778 */

/*===========================================================================

MACRO ADD_TARGET_DWORD_TO_PACKET

DESCRIPTION
  This macro adds a dword (least significant byte first) to a packet
  being built dynamically.

PARAMETERS
  pkt     A pkt_buffer_type struct in which the packet will be built.
  val     The word to be added to the packet.

DEPENDENCIES
  START_BUILDING_PACKET must have been called on pkt before calling
  this macro.

RETURN VALUE
  None.

SIDE EFFECTS
  Each argument is evaluated twice within this macro.
  This macro is not an expression, not is it a single statement.  It
  must be called with a trailing semicolon.

===========================================================================*/

#define  ADD_TARGET_DWORD_TO_PACKET(pkt,val)         \
  /*lint -e778 val or (val >> 8) may evaluate to zero. */                  \
  add_byte_to_packet(&pkt, (const byte)(val & 0xFF));         /* byte 0 */ \
  add_byte_to_packet(&pkt, (const byte)((val >> 8) & 0xFF));  /* byte 1 */ \
  add_byte_to_packet(&pkt, (const byte)((val >> 16) & 0xFF)); /* byte 2 */ \
  add_byte_to_packet(&pkt, (const byte)((val >> 24) & 0xFF))  /* byte 3 */ \
  /*lint +e778 */



/*===========================================================================

MACRO ADD_CRC_TO_PACKET

DESCRIPTION
  This macro adds a word (LEAST significant byte first) to a packet
  being built dynamically.  This should only be used for the CRC,
  since other words are supposed to be sent most significant byte
  first.

PARAMETERS
  pkt     A pkt_buffer_type struct in which the packet will be built.
  val     The word to be added to the packet.

DEPENDENCIES
  START_BUILDING_PACKET must have been called on pkt before calling
  this macro.

RETURN VALUE
  None.

SIDE EFFECTS
  Each argument is evaluated twice within this macro.
  This macro is not an expression, not is it a single statement.  It
  must be called with a trailing semicolon.

===========================================================================*/

#define  ADD_CRC_TO_PACKET(pkt,val)         \
  add_byte_to_packet(&pkt, (const byte)(val & 0xFF));        /* low  byte */ \
  add_byte_to_packet(&pkt, (const byte)((val >> 8) & 0xFF))  /* high byte */


/*===========================================================================

MACRO ADD_STUFF_TO_PACKET

DESCRIPTION
  This macro adds an arbitrary buffer of bytes to a packet being built
  dynamically.

PARAMETERS
  pkt     A pkt_buffer_type struct in which the packet will be built.
  data    A pointer to byte (or array of bytes) containing the data to be added
  len     The number of bytes to be added to the packet.

DEPENDENCIES
  START_BUILDING_PACKET must have been called on pkt before calling
  this macro.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/

#define  ADD_STUFF_TO_PACKET(pkt,data,len)   \
               /*lint -e717 Yes, this is a do...while(0). */\
               do {                                         \
               int   stuff_i;                               \
                                                            \
               for (stuff_i=0; stuff_i < len; stuff_i++)    \
                 {                                          \
                 add_byte_to_packet(&pkt, data[stuff_i]);   \
                 }                                          \
               } while (0)                                  \
               /*lint +e717 */


/*===========================================================================

MACRO ADD_STRING_TO_PACKET

DESCRIPTION
  This macro adds a text string (null terminated) to a packet being built
  dynamically.  If the string is not null terminated or is longer than 20
  characters, only the first 20 characters are added to the packet.

PARAMETERS
  pkt     A pkt_buffer_type struct in which the packet will be built.
  str     The string to be added to the packet.

DEPENDENCIES
  START_BUILDING_PACKET must have been called on pkt before calling
  this macro.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/

#define  ADD_STRING_TO_PACKET(pkt,str)       \
               /*lint -e717 Yes, this is a do...while(0). */ \
               do {                                          \
               byte count = 0;                               \
               byte stopping_point;                          \
                                                             \
           stopping_point = dload_str_len(str);          \
               while (count < stopping_point)                \
                 add_byte_to_packet(&pkt, str[count++]);     \
               } while (0)                                   \
               /*lint -e717 */


/*===========================================================================

MACRO FINISH_BUILDING_PACKET

DESCRIPTION
  This macro completes the process of building a packet dynamically.
  It just calls a function to do the work.

PARAMETERS
  pkt     A pkt_buffer_type struct in which the packet has been built.

DEPENDENCIES
  START_BUILDING_PACKET must have been called on pkt before calling
  this macro.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/

#define  FINISH_BUILDING_PACKET(pkt)         \
               finish_building_packet(&pkt)


/*-------------------------------------------------------------------------*/

/* Mask for CRC-16 polynomial:
**
**      x^16 + x^12 + x^5 + 1
**
** This is more commonly referred to as CCITT-16.
** Note:  the x^16 tap is left off, it's implicit.
*/
#define CRC_16_L_POLYNOMIAL     0x8408

/* Seed value for CRC register.  The all ones seed is part of CCITT-16, as
** well as allows detection of an entire data stream of zeroes.
*/
#define CRC_16_L_SEED           0xFFFF

/* Residual CRC value to compare against return value of a CRC_16_L_STEP().
** Use CRC_16_L_STEP() to calculate a 16 bit CRC, complement the result,
** and append it to the buffer.  When CRC_16_L_STEP() is applied to the
** unchanged entire buffer, and complemented, it returns CRC_16_L_OK.
** That is, it returns CRC_16_L_OK_NEG.
*/
#define CRC_16_L_OK             0x0F47
#define CRC_16_L_OK_NEG         0xF0B8



/*===========================================================================

MACRO CRC_16_L_STEP

DESCRIPTION
  This macro calculates one byte step of an LSB-first 16-bit CRC.
  It can be used to produce a CRC and to check a CRC.

PARAMETERS
  xx_crc  Current value of the CRC calculation, 16-bits
  xx_c    New byte to figure into the CRC, 8-bits

DEPENDENCIES
  None

RETURN VALUE
  The new CRC value, 16-bits.  If this macro is being used to check a
  CRC, and is run over a range of bytes, the return value will be equal
  to CRC_16_L_OK_NEG if the CRC checks correctly according to the DMSS
  Async Download Protocol Spec.

SIDE EFFECTS
  xx_crc is evaluated twice within this macro.

===========================================================================*/

/* REMOVE_BEGIN,===00001===,Owner : zjj, 2007-11-9, 11:30:27 */
#if 0
extern word crc_16_l_table_dump[];       /* Extern for macro (global) */
#endif
/* REMOVE_END,  ===00001===,Owner : zjj, 2007-11-9, 11:30:27 */

#define CRC_16_L_STEP(xx_crc,xx_c) \
  (((xx_crc) >> 8) ^ crc_16_l_table_dump[((xx_crc) ^ (xx_c)) & 0x00ff])


/*===========================================================================

FUNCTION add_byte_to_packet

DESCRIPTION
  This function adds a single byte to a packet that is being built
  dynamically.  It takes care of byte stuffing and checks for buffer
  overflow.

  This function is a helper function for the packet building macros
  and should not be called directly.

DEPENDENCIES
  The START_BUILDING_PACKET() macro should have been called on the
  packet buffer before calling this function.

RETURN VALUE
  None.

SIDE EFFECTS
  None.

===========================================================================*/
/* Status Code Enumeration
   This lists the status result codes passed around in the program.

   This enum is used to index a table of response packets, so these
   values map exactly to possible responses. */

typedef enum
  {
      ACK,                    /* Success. Send an acknowledgement.           */
      NAK_INVALID_FCS,        /* Failure: invalid frame check sequence.      */
      NAK_INVALID_DEST,       /* Failure: destination address is invalid.    */
      NAK_INVALID_LEN,        /* Failure: operation length is invalid.       */
      NAK_EARLY_END,          /* Failure: packet was too short for this cmd. */
      NAK_TOO_LARGE,          /* Failure: packet was too long for my buffer. */
      NAK_INVALID_CMD,        /* Failure: packet command code was unknown.   */
      NAK_FAILED,             /* Failure: operation did not succeed.         */
      NAK_WRONG_IID,          /* Failure: intelligent ID code was wrong.     */
      NAK_BAD_VPP,            /* Failure: programming voltage out of spec    */
      NAK_OP_NOT_PERMITTED,   /* Failure: memory dump not permitted          */
#ifdef FEATURE_DLOAD_MEM_DEBUG
      NAK_INVALID_ADDR,       /* Failure: invalid address for a memory read  */
#endif
      NAK_VERIFY_FAILED,      /* Failure: readback verify did not match      */
      NAK_NO_SEC_CODE,        /* Failure: not permitted without unlock       */
      NAK_BAD_SEC_CODE,        /* Failure: invalid security code              */
      FATAL_ERROR = 0xFF 
  
  } response_code_type;

/*-   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -   -*/
#endif

