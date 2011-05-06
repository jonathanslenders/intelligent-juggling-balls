#include "../main.h"


/* *** Colors on movement *** */

void light_up_on_movement_activate(void*data)
{
	send_packet("RUN", 0, "fixed", "440000");
}
void light_up_on_movement_packet_received(struct juggle_packet_t* packet)
{
	if (strcmp(packet->action, "MOVING") == 0)
	{
		send_packet("RUN", packet->ball, "fade", "ffeedd:50");

	}
	else if (strcmp(packet->action, "ON_TABLE") == 0)
	{
		send_packet("RUN", packet->ball, "fade", "440000:200");
	}
}
