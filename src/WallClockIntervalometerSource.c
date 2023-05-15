#include <stdio.h>

#include <glib.h>

#include "WallClockIntervalometerSource.h"


static gint64 get_next_trigger_time(gint64 interval) {
    gint64 monotime = g_get_monotonic_time();  // in microseconds since ??
    gint64 walltime = g_get_real_time();  // in microseconds since epoch

    gint64 nextwalltime = ((walltime / interval) + 1) * interval;
    gint64 nextmonotime = monotime + (nextwalltime - walltime);

    return nextmonotime;
}


static gboolean intervalometer_source_dispatch(GSource* source,
                                               GSourceFunc callback,
                                               gpointer user_data)
{
    // Fire the callback
    callback(user_data);

    // Reschedule for the next tick
    g_source_set_ready_time(
        source,
        get_next_trigger_time(
            ((WallClockIntervalometerSource*)source)->interval
        )
    );
    return TRUE;
}


void intervalometer_start(WallClockIntervalometerSource* source)
{
    g_source_set_ready_time(
        (GSource*)source,
        get_next_trigger_time(source->interval)
    );
}


GSourceFuncs wall_clock_intervalometer_source_funcs = {
    NULL,
    NULL,
    intervalometer_source_dispatch,
    NULL,
};


#if 0
// Sample usage

static gboolean print_current_time(gpointer user_data) {
    gint64 walltime = g_get_real_time();  // in microseconds since epoch
    GDateTime* dt = g_date_time_new_from_unix_local(walltime / G_TIME_SPAN_SECOND);
    gchar* dtstr = g_date_time_format_iso8601(dt);

    printf("%s %06ld\n", dtstr, walltime % G_TIME_SPAN_SECOND);
    g_free(dtstr);
    g_date_time_unref(dt);

    return TRUE;
}

int main(void) {
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);

    WallClockIntervalometerSource* source =
        (WallClockIntervalometerSource*)g_source_new(
            &wall_clock_intervalometer_source_funcs,
            sizeof(WallClockIntervalometerSource)
        );
    
    source->interval = 2 * G_TIME_SPAN_SECOND;

    g_source_set_callback(
        (GSource*)source,
        G_SOURCE_FUNC(print_current_time),
        NULL,  // user data
        NULL  // destroy notify callback
    );
    
    g_source_attach((GSource*)source, g_main_loop_get_context(loop));

    intervalometer_start(source);

    g_source_unref((GSource*)source);

    g_main_loop_run(loop);
    return 0;
}
#endif
