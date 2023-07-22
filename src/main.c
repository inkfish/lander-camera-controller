#include <stdio.h>
#include <time.h>

#include <arv.h>

#include <glib.h>
#include <glib-unix.h>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include "WallClockIntervalometerSource.h"


#define CAMERA_NAME "Lucid Vision Labs-PHX051S-C-222701823"
#define INTERVAL 10  /* seconds */

#define STRINGIFY(x) #x
#define FRAMERATE_STR(interval) "1/" STRINGIFY(interval)

static char *get_iso8601_timestamp(void) {
    time_t rawtime;
    time(&rawtime);

    struct tm *timeinfo = gmtime(&rawtime);

    char buffer[18];  // future Y10K problem
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H%M%S", timeinfo);
    return g_strdup(buffer);
}


static GstElement *create_pipeline()
{
    GstElement *pipeline = gst_pipeline_new("pipeline");

    GstElement *source = gst_element_factory_make("aravissrc", "source");
    GstElement *filter = gst_element_factory_make("capsfilter", "filter");
    GstElement *bayer2rgb = gst_element_factory_make("bayer2rgb", "bayer2rgb");
    GstElement *convert = gst_element_factory_make("videoconvert", "convert");
    GstElement *encoder = gst_element_factory_make("avenc_ffv1", "encoder");
    GstElement *muxer = gst_element_factory_make("matroskamux", "muxer");
    GstElement *sink = gst_element_factory_make("splitmuxsink", "sink");

    if(!(pipeline && source && filter && bayer2rgb && convert && encoder && muxer && sink)) {
        g_printerr("Failed to create pipeline elements\n");
        if (pipeline)
            gst_object_unref(pipeline);
        return NULL;
    }

    // Set the camera we are looking for
    g_object_set(
        source,
        "camera-name", CAMERA_NAME,
        NULL
    );

    // Set caps on the filter element so that we interpret the stream's raw data
    // correctly.
    GstCaps *caps = gst_caps_from_string(
        "video/x-bayer,"
        "format=rggb,"
        "width=2448,height=2048,"
        "framerate=" FRAMERATE_STR(INTERVAL)
    );
    g_object_set(filter, "caps", caps, NULL);
    gst_caps_unref(caps);

    // Configure the output file name and splitting options
    // TODO: Make this user-configurable
    char *timestamp = get_iso8601_timestamp();
    char *location = g_strdup_printf("/data/video_%s_%%05d.mkv", timestamp);
    g_object_set(
        sink,
        "location", location,
        "max-size-bytes", (gint64)(1024*1024*1024),  // 1 gigabyte
        // TODO: Make max file size time configurable
        "max-size-time", (gint64)3600000000000LL,  // 1 hour in nanoseconds
        "muxer", muxer,
        NULL
    );
    g_free(location);
    g_free(timestamp);

    // Link the elements together into the pipeline
    gst_bin_add_many(GST_BIN(pipeline), source, filter, bayer2rgb, convert, encoder, sink, NULL);
    if(gst_element_link_many(source, filter, bayer2rgb, convert, encoder, sink, NULL) != TRUE) {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return NULL;
    }

    return pipeline;
}


static ArvCamera *get_camera_from_pipeline(GstElement *pipeline)
{
    ArvCamera *camera = NULL;

    // Get the "camera" prop of the "source" element in the pipeline
    GstElement *source = gst_bin_get_by_name(GST_BIN(pipeline), "source");
    g_object_get(G_OBJECT(source), "camera", &camera, NULL);
    gst_object_unref(source);

    if (!ARV_IS_CAMERA(camera)) {
        g_printerr("Could not get Aravis camera object from pipeline\n");
        exit(1);
    }

    g_object_ref(camera);
    return camera;
}


static void configure_camera(ArvCamera *camera)
{
    GError *error = NULL;

    // The camera must be in continuous capture mode for triggers to work
    arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_CONTINUOUS, &error);
    if (error) {
        g_printerr("Failed to set acquisition mode: %s\n", error->message);
        exit(1);
    }

    // This not only configures the TriggerSource, but also enables TriggerMode
    arv_camera_set_trigger(camera, "Software", &error);
    if (error) {
        g_printerr("Failed to set trigger: %s\n", error->message);
        exit(1);
    }

#if 0
    // For testing, in case we want to disable the trigger
    arv_device_set_string_feature_value(
        arv_camera_get_device(camera),
        "TriggerMode", "Off",
        NULL
    );
#endif
}

static void print_trigger_time(void) {
    gint64 walltime = g_get_real_time();  // in microseconds since epoch
    GDateTime* dt = g_date_time_new_from_unix_local(walltime / G_TIME_SPAN_SECOND);
    gchar* dtstr = g_date_time_format_iso8601(dt);

    g_print("Triggering at %s %06ld\n", dtstr, walltime % G_TIME_SPAN_SECOND);
    g_free(dtstr);
    g_date_time_unref(dt);
}

static gboolean grab_frame_cb(gpointer user_data) {
    ArvCamera *camera = (ArvCamera *)user_data;
    print_trigger_time();
    arv_camera_software_trigger(camera, NULL);
    return TRUE;  // keeps the source
}


static gboolean on_unix_signal(gpointer user_data)
{
    g_print("Signal received, stopping main loop...\n");
    GMainLoop *loop = (GMainLoop *)user_data;
    g_main_loop_quit(loop);
    return TRUE;
}


int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);

    // Create the pipeline
    GstElement *pipeline = create_pipeline();

    // Set the pipeline to the PAUSED state. This is an asynchronous state
    // change so we also have to wait for it to complete.
    gst_element_set_state(pipeline, GST_STATE_PAUSED);
    gst_element_get_state(pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);

    // Now the PAUSED state is reached, GstAravis has connected to the camera
    // and now we can configure it.
    ArvCamera *camera = get_camera_from_pipeline(pipeline);
    configure_camera(camera);

    // Create the main loop
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    // Register UNIX signal handlers
    g_unix_signal_add(SIGINT, G_SOURCE_FUNC(on_unix_signal), loop);
    g_unix_signal_add(SIGTERM, G_SOURCE_FUNC(on_unix_signal), loop);

    // Create the intervalometer
    WallClockIntervalometerSource* intervalometer =
        (WallClockIntervalometerSource*)g_source_new(
            &wall_clock_intervalometer_source_funcs,
            sizeof(WallClockIntervalometerSource)
        );

    // TODO: Make interval configurable
    intervalometer->interval = INTERVAL * G_TIME_SPAN_SECOND;

    g_object_ref(camera);
    g_source_set_callback(
        (GSource*)intervalometer,
        G_SOURCE_FUNC(grab_frame_cb),
        camera,  // user data
        g_object_unref  // destroy notify callback
    );

    g_source_attach((GSource*)intervalometer, g_main_loop_get_context(loop));
    intervalometer_start(intervalometer);
    g_source_unref((GSource*)intervalometer);

    // Start playing the pipeline and run the main loop
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_main_loop_run(loop);

    // When we reach here, the main loop has exited
    g_print("The main loop has exited\n");
    g_main_loop_unref(loop);

    // Send an end-of-stream event to the pipeline. This allows elements
    // (e.g., the muxer) to gracefully finish.
    //
    // We then wait synchronously for the bus to report that the end-of-stream
    // message (success) or an error.
    gst_element_send_event(pipeline, gst_event_new_eos());
    GstMessage *bus_msg = gst_bus_timed_pop_filtered(
        GST_ELEMENT_BUS(pipeline),
        GST_CLOCK_TIME_NONE,
        GST_MESSAGE_EOS | GST_MESSAGE_ERROR
    );

    if (GST_MESSAGE_TYPE(bus_msg) == GST_MESSAGE_ERROR) {
        GError *error = NULL;
        gst_message_parse_error(bus_msg, &error, NULL);
        g_printerr("Failed to end stream, got error: %s\n", error->message);
        g_error_free(error);
    }

    gst_message_unref(bus_msg);

    // Set the state to NULL to release pipeline resources. This is a
    // synchronous state change.
    gst_element_set_state(pipeline, GST_STATE_NULL);

    g_object_unref(camera);
    gst_object_unref(pipeline);
    gst_deinit();

    // Hey! AddressSanitizer reports a memory leak from glib_init, what gives?
    // Just ignore it: https://stackoverflow.com/a/72936690

    return 0;
}
