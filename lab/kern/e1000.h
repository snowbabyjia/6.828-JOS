#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include<kern/pci.h>

#define E1000_STATUS   0x00008  /* Device Status - RO */
#define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDBAH    0x03804  /* TX Descriptor Base Address High - RW */
#define E1000_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818  /* TX Descripotr Tail - RW */
#define NTXDESC        32       /* Total number of transmit descriptors*/
#define PKTSIZE        1518

/* Transmit Control */
#define E1000_TCTL     0x00400  /* TX Control - RW */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */

#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */

#define E1000_TXD_STAT_DD    0x00000001 /* Descriptor Done */
#define E1000_TXD_CMD_RS     0x08000000 /* Report Status */
#define E1000_TXD_CMD_EOP    0x01000000 /* End of Packet */

/* Receiving packets*/
#define NRXDESC         128
#define E1000_RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define E1000_RDBAH    0x02804  /* RX Descriptor Base Address High - RW */
#define E1000_RDBAL0   E1000_RDBAL /* RX Desc Base Address Low (0) - RW */
#define E1000_RDBAH0   E1000_RDBAH /* RX Desc Base Address High (0) - RW */
// hard-coded mac addr
#define MACL            0x12005452
#define MACH            0x5634

#define E1000_EERD     0x00014  /* EEPROM Read - RW */
#define E1000_EEPROM_RW_REG_START  1    /* First bit for telling part to start operation */
#define E1000_EEPROM_RW_ADDR_SHIFT 8    /* Shift to the address bits */
#define E1000_EEPROM_RW_REG_DATA   16   /* Offset to data in EEPROM read/write registers */
#define E1000_EEPROM_RW_REG_DONE   0x10 /* Offset to READ/WRITE done bit */

#define E1000_RAH_AV   0x80000000 
#define E1000_MTA      0x05200  /* Multicast Table Array - RW Array */
#define E1000_RDLEN    0x02808  /* RX Descriptor Length - RW */
#define E1000_RAL       0x5400
#define E1000_RAH       0x5404
#define E1000_RDH      0x02810  /* RX Descriptor Head - RW */
#define E1000_RDT      0x02818  /* RX Descriptor Tail - RW */

#define E1000_RCTL     0x00100  /* RX Control - RW */
#define E1000_RXD_STAT_DD       0x01    /* Descriptor Done */
#define E1000_RXD_STAT_EOP      0x02    /* End of Packet */


int E1000_attach(struct pci_func *pcif);
int packet_send(void *packet, uint16_t size);
int packet_recv(void *dest_buf, uint16_t *buf_len);
void get_mac_addr(void *addr_buf, int raw);

struct e1000_tx_desc {
    uint64_t buffer_addr;       /* Address of the descriptor's data buffer */
    union {
        uint32_t data;
        struct {
            uint16_t length;    /* Data buffer length */
            uint8_t cso;        /* Checksum offset */
            uint8_t cmd;        /* Descriptor control */
        } flags;
    } lower;
    union {
        uint32_t data;
        struct {
            uint8_t status;     /* Descriptor status */
            uint8_t css;        /* Checksum start */
            uint16_t special;
        } fields;
    } upper;
};

struct e1000_rx_desc {
    uint64_t buffer_addr; /* Address of the descriptor's data buffer */
    uint16_t gth;     /* Length of data DMAed into data buffer */
    uint16_t csum;       /* Packet checksum */
    uint8_t status;      /* Descriptor status */
    uint8_t errors;      /* Descriptor Errors */
    uint16_t special;
};
/*
extern struct e1000_tx_desc *tx_queue;
extern void *tx_packet_buf;
extern struct e1000_rx_desc *rx_queue;
extern void *rx_packet_buf;
*/
extern struct e1000_tx_desc tx_queue[NTXDESC];
extern char tx_packet_buf[NTXDESC * PKTSIZE];

extern struct e1000_rx_desc rx_queue[NRXDESC];
extern char rx_packet_buf[NRXDESC * 2048];

#endif	// JOS_KERN_E1000_H
