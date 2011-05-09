
void interpolate(int pos, int* r, int* g, int* b)
{
	// From red to blue
	if (pos < 256/3)
	{
		pos *= 3;
		*r = 255-pos;
		*g = pos;
		*b = 0;
	}
	else if (pos < 256*2/3)
	{
		pos -= (256/3);
		pos *= 3;
		*r = 0;
		*g = 255-pos;
		*b = pos;
	}
	else
	{
		pos -= (256*2/3);
		pos *= 3;
		*r = pos;
		*g = 0;
		*b = 255-pos;
	}
}

