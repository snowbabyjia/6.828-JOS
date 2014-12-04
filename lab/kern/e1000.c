#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/string.h>

volatile uint32_t *e1000;

struct e1000_tx_desc tx_queue[NTXDESC];
char tx_packet_buf[NTXDESC * PKTSIZE];

struct e1000_rx_desc rx_queue[NRXDESC];
char rx_packet_buf[NRXDESC * 2048];
/*
struct e1000_tx_desc *tx_queue = NULL;
void * tx_packet_buf = NULL;

struct e1000_rx_desc *rx_queue = NULL;
void* rx_packet_buf = NULL;
*/
/*
 * Return 0 on success, -1 on fail.
 */
int packet_send(void * packet, uint16_t length)
{
    uint32_t tail = *(uint32_t *)((void*)e1000 + E1000_TDT);
    if (tx_queue[tail].upper.fields.status & E1000_TXD_STAT_DD)
    {
        if (length > 1518)
        {
            cprintf("Giant packet. Dropped it.\n");
            return -1;
        }
        memcpy((void *)tx_packet_buf + tail*PKTSIZE, (void *)packet, length);
        tx_queue[tail].lower.flags.length = length;
        tx_queue[tail].upper.data &= ~E1000_TXD_STAT_DD;
        tx_queue[tail].lower.data |= E1000_TXD_CMD_RS;
        tx_queue[tail].lower.data |= E1000_TXD_CMD_EOP;
        tail = (tail+1) % NTXDESC;
        *(uint32_t *)((void*)e1000 + E1000_TDT) = tail;
        return 0;
    }
    cprintf("Transmit queue full. OOM\n\n");
    return -1;
}

/*
 * return 0 on success
 * return -1 on "try again"
 */
int packet_recv(void *dest_buf, uint16_t* buf_len)
{
    uint32_t tail = (*(uint32_t *)((void*)e1000 + E1000_RDT) + 1) % NRXDESC;
    if (rx_queue[tail].status & E1000_RXD_STAT_DD)
    {
        *buf_len = (uint16_t)rx_queue[tail].gth;

        memcpy(dest_buf, (void *)rx_packet_buf + tail * 2048, rx_queue[tail].gth);
        rx_queue[tail].status &= ~E1000_RXD_STAT_DD;
        rx_queue[tail].status |= E1000_RXD_STAT_EOP;
        *(uint32_t *)((void*)e1000 + E1000_RDT) = tail;
        return 0;
    }
    else
    {
        return -1;
    }
}

// LAB 6: Your driver code here
int E1000_attach(struct pci_func *f)
{
    pci_func_enable(f);
    e1000 = mmio_map_region(f->reg_base[0], f->reg_size[0]);
    //uint32_t v = *(uint32_t *)((void*)e1000 + E1000_STATUS);
    /*
    tx_queue = boot_alloc(NTXDESC * sizeof(struct e1000_tx_desc));
    rx_queue = boot_alloc(NRXDESC * sizeof(struct e1000_rx_desc));
    tx_packet_buf = boot_alloc(NTXDESC * PKTSIZE);
    rx_packet_buf = boot_alloc(NRXDESC * 2048);
    */
    memset((void *) tx_queue, 0, NTXDESC * sizeof(struct e1000_tx_desc));
    memset(tx_packet_buf, 0, NTXDESC * PKTSIZE);
    int i;
    for (i=0; i<NTXDESC; i++)
    {
        tx_queue[i].buffer_addr = (uint32_t)(PADDR(tx_packet_buf + PKTSIZE * i));
        tx_queue[i].upper.fields.status |= E1000_TXD_STAT_DD;
    }

    *(uint32_t *)((void*)e1000 + E1000_TDBAL) = (uint32_t)PADDR(tx_queue);
    *(uint32_t *)((void*)e1000 + E1000_TDBAH) = 0;
    *(uint32_t *)((void*)e1000 + E1000_TDLEN) = NTXDESC * sizeof(struct e1000_tx_desc);
    *(uint32_t *)((void*)e1000 + E1000_TDH) = 0;
    *(uint32_t *)((void*)e1000 + E1000_TDT) = 0;

    *(uint32_t *)((void*)e1000 + E1000_TCTL) |= E1000_TCTL_EN;
    *(uint32_t *)((void*)e1000 + E1000_TCTL) |= E1000_TCTL_PSP;
    *(uint32_t *)((void*)e1000 + E1000_TCTL) |= E1000_TCTL_CT;
    *(uint32_t *)((void*)e1000 + E1000_TCTL) |= 0x40 << 12;

    *(uint32_t *)((void*)e1000 + E1000_TIPG) = 10;

    // Initializing receive queue
    memset((void *)rx_queue, 0, NRXDESC * sizeof(struct e1000_rx_desc));
    memset(rx_packet_buf, 0, NRXDESC * 2048);
    
    for (i=0; i<NRXDESC; i++)
    {
        rx_queue[i].buffer_addr = (uint32_t)(PADDR(rx_packet_buf + 2048*i));
    }
    uint16_t buf[3];
    get_mac_addr((void *)&buf, 1);
    cprintf("buf = %x, %x, %x\n\n", buf[0], buf[1], buf[2]);
    *(uint32_t *)((void *)e1000 + E1000_RAL) = buf[1] << 16 | buf[0];
    *(uint32_t *)((void *)e1000 + E1000_RAH) = E1000_RAH_AV | buf[2];
    *(uint32_t *)((void *)e1000 + E1000_RDBAL) = (uint32_t)PADDR(rx_queue);
    *(uint32_t *)((void *)e1000 + E1000_RDBAH) = 0;
    
    *(uint32_t *)((void *)e1000 + E1000_MTA) = 0;
  
    *(uint32_t *)((void *)e1000 + E1000_RDLEN) = NRXDESC * sizeof(struct e1000_rx_desc); 
    
    *(uint32_t *)((void*)e1000 + E1000_RDH) = 0;
    *(uint32_t *)((void*)e1000 + E1000_RDT) = NRXDESC -1;

    *(uint32_t *)((void*)e1000 + E1000_RCTL) = 0x04008002;
  
    return 0;
}

// Challenge!
void get_mac_addr(void *addr_buf, int raw)
{
    int i;
    for (i=0; i<3; i++)
    {
        
        *(uint32_t *)((void *)e1000 + E1000_EERD) |= E1000_EEPROM_RW_REG_START | i << E1000_EEPROM_RW_ADDR_SHIFT;
        while (! (*(uint32_t *)((void *)e1000 + E1000_EERD) & E1000_EEPROM_RW_REG_DONE))
        {
            ;
        }
        uint16_t eeprom = *(uint32_t *)((void *)e1000 + E1000_EERD) >> E1000_EEPROM_RW_REG_DATA;
        if (raw)
        {
            ((uint16_t *)addr_buf)[i] = eeprom;
        }
        else
        {
            ((uint8_t *)addr_buf)[i*2+1] = (eeprom & 0xff00) >> 8;
            ((uint8_t *)addr_buf)[i*2] = eeprom & 0xff;
        }
        // after we are done, clear out the register
        *(uint32_t *)((void *)e1000 + E1000_EERD) = 0;
    }
}
