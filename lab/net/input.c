#include "ns.h"
#include "inc/lib.h"

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.

    int value;
    
    while(1)
    {
        //nsipcbuf.pkt.jp_len = 0;
        if ((value = sys_page_alloc(0, (void *)&nsipcbuf, PTE_P|PTE_U|PTE_W)) < 0)
        {
            panic("input: can't page alloc with error %e", value);
        }
        if ((value = sys_packet_recv((void *)nsipcbuf.pkt.jp_data, (uint16_t *)(&(nsipcbuf.pkt.jp_len)))) == 0)
        {
            cprintf("received! sending to network server packet %s\n\n", nsipcbuf.pkt.jp_data); 
            ipc_send(ns_envid, NSREQ_INPUT, (void *)&nsipcbuf, PTE_P|PTE_U|PTE_W);
        }
        else
        {
            sys_yield();
        }
    }
}
