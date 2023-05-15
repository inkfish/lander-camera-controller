#ifndef __WALL_CLOCK_INTERVALOMETER_SOURCE_H__
#define __WALL_CLOCK_INTERVALOMETER_SOURCE_H__

#include <glib.h>


typedef struct {
    GSource source;
    gint64 interval;  // microseconds between triggers
} WallClockIntervalometerSource;

extern GSourceFuncs wall_clock_intervalometer_source_funcs;

void intervalometer_start(WallClockIntervalometerSource* source);

#endif
