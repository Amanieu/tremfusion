#include <math.h>

float rint( float v ) {
	if( v >= 0.5f ) return ceilf( v );
	else return floorf( v );
}
