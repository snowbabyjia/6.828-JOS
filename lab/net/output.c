#include "ns.h"
#include "inc/lib.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet from the network server
	//	- send the packet to the device driver
    int value;
    int perm;
    while(1)
    {
        if ((value = ipc_recv(&ns_envid, (void *)&nsipcbuf, &perm))<0)
        {
            panic("output: ipc_recv has error %e", value);
        }
        assert(value == NSREQ_OUTPUT);
        if ((value = sys_packet_send((void *)(nsipcbuf.pkt.jp_data), nsipcbuf.pkt.jp_len)) < 0)
        {
            cprintf("output: packet_send failed with error %d\n", value);
        }
    }
}
