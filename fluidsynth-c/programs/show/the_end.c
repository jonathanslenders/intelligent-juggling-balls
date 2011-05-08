#include <time.h> /* for clock() */

#include "../../main.h"

void the_end_activate(void * data)
{
	send_packet("RUN", 0, "pulse", "FFFFFF_000000:400");
}
