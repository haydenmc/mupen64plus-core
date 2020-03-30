#include "gststream.h"

#include <SDL_opengl.h>
#include <SDL_syswm.h>
#include <gst/gl/gl.h>
#include <gst/gl/x11/gstgldisplay_x11.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glext.h>

GstElement *pipeline;
GstElement *sink;
GstElement *appSource;
GstGLFramebuffer *gstFb;
GstCaps *gstCaps;
guint eventSourceId = 0;
GMainLoop *mainLoop;

// Push OpenGL data to pipeline
gboolean pushData()
{
    printf("Pushing data...\n");
    GstGLContext *gstGlContext = gst_gl_context_get_current();
    printf("GL Context: %p...\n", gstGlContext);
    GstBuffer *gstBuffer;
    GstGLMemory *gstGlMemory;
    GstFlowReturn ret;
    gst_gl_memory_init_once();
    GstVideoInfo *gstVideoInfo = gst_video_info_new();

    /* Create a new empty buffer */
    printf("Creating buffer...\n");
    gstBuffer = gst_buffer_new ();

    /* Get video info from caps */
    //printf ("GST Caps: %p\n", gstCaps);
    printf("Getting caps...\n");
    gst_video_info_from_caps(gstVideoInfo, gstCaps);

    /* Allocate gl memory */
    printf("Getting allocator...\n");
    GstGLMemoryAllocator *gstGlMemoryAlloc = gst_gl_memory_allocator_get_default(gstGlContext);
    printf("Allocator: %p...\n", gstGlMemoryAlloc);
    printf("Getting allocator params...\n");
    GstGLAllocationParams *gstGlAllocationParams = (GstGLAllocationParams *) gst_gl_video_allocation_params_new (gstGlContext, NULL, gstVideoInfo, 0, NULL, GST_GL_TEXTURE_TARGET_2D, GST_GL_RGBA);
    printf("Allocator params: %p...\n", gstGlAllocationParams);
    printf("Allocating...\n");
    gstGlMemory = (GstGLMemory *)gst_gl_base_memory_alloc ((GstGLBaseMemoryAllocator *)gstGlMemoryAlloc, gstGlAllocationParams);

    // Attach FBO to texture memory and draw
    printf("Attaching framebuffer...\n");
    //gst_gl_framebuffer_attach (gstFb, GL_COLOR_ATTACHMENT0, (GstGLBaseMemory *) gstGlMemory);
    // glBlitNamedFramebuffer(
    //     (GLuint)0,
    //     (GLuint)gst_gl_framebuffer_get_id(gstFb),
    //     (GLint)0,
    //     (GLint)0,
    //     (GLint)1280,
    //     (GLint)720,
    //     (GLint)0,
    //     (GLint)0,
    //     (GLint)1280,
    //     (GLint)720,
    //     GL_COLOR_BUFFER_BIT,
    //     GL_LINEAR);

    printf("Inserting memory into buffer...\n");
    gst_buffer_insert_memory (gstBuffer, -1, (GstMemory *)gstGlMemory);

    /* Push the buffer into the appsrc */
    //printf ("Appsrc: %p\n", appSource);
    //printf ("Buffer: %p\n", gstBuffer);
    printf("Emitting push signal...\n");
    g_signal_emit_by_name (appSource, "push-buffer", gstBuffer, &ret);

    /* Free the buffer now that we are done with it */
    printf("Freeing buffer...\n");
    gst_buffer_unref (gstBuffer);

    if (ret != GST_FLOW_OK) {
        /* We got some error */
        g_printerr ("Couldn't push buffer.\n");
        printf ("error value: %d\n", ret);
        return FALSE;
    }

    return TRUE;
}

/* This signal callback triggers when appsrc needs data. Here, we add an idle handler
 * to the mainloop to start pushing data into the appsrc */
void startFeed (GstElement *source, guint size, void *data)
{
    if (eventSourceId == 0)
    {
        g_print ("Start feeding\n");
        eventSourceId = g_idle_add ((GSourceFunc) pushData, NULL);
    }
}

/* This callback triggers when appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop */
void stopFeed (GstElement *source, void *data)
{
    if (eventSourceId != 0)
    {
        g_print ("Stop feeding\n");
        g_source_remove (eventSourceId);
        eventSourceId = 0;
    }
}

/* This function is called when an error message is posted on the bus */
void errorCallback (GstBus *bus, GstMessage *msg, void *data) {
    GError *err;
    gchar *debug_info;

    /* Print error details on the screen */
    gst_message_parse_error (msg, &err, &debug_info);
    g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
    g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
    g_clear_error (&err);
    g_free (debug_info);

    g_main_loop_quit (mainLoop);
}

// This runs on the main opengl thread and is used to set up everything we need
// for streaming
void configureStream(SDL_Window *sdlWindow)
{
    SDL_SysWMinfo info;
    Display *sdl_display = NULL;
    GLXContext gl_context = NULL;
    GstGLDisplay *sdl_gl_display = NULL;

    /* Initialize GStreamer */
    gst_init (NULL, NULL);

    SDL_VERSION (&info.version);
    SDL_GetWindowWMInfo (sdlWindow, &info);
    sdl_display = info.info.x11.display;
    printf ("SDL Display: %p\n", sdl_display);
    gl_context = glXGetCurrentContext ();
    printf ("GL Context: %p\n", gl_context);
    sdl_gl_display =
        (GstGLDisplay *) gst_gl_display_x11_new_with_display (sdl_display);
    printf ("GL Display: %p\n", sdl_gl_display);
    GstContext *x11context = gst_context_new (GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
    gst_context_set_gl_display (x11context, sdl_gl_display);
    
    GstGLContext* gstGlContext = gst_gl_context_new(sdl_gl_display);
    gst_gl_context_create(gstGlContext, 0, NULL);
    printf ("GST GL Context: %p\n", gstGlContext);
    // Activate OpenGL context on this thread
    printf("Activating gst opengl context...\n");
    gst_gl_context_activate(gstGlContext, TRUE);

    //GstContext *ctxcontext = gst_context_new ("gst.gl.app_context", TRUE);
    //gst_structure_set (gst_context_writable_structure (ctxcontext), "context", GST_TYPE_GL_CONTEXT, gstGlContext, NULL);

    printf ("Creating caps...\n");
    gstCaps = gst_caps_from_string("video/x-raw(memory:GLMemory), "
        "format = (string) RGBA, "
        "width = 1280, "
        "height = 720, "
        "framerate = 60/1,"
        "texture-target = (string) 2D");
    printf ("GST Caps: %p\n", gstCaps);

    /* Create the empty pipeline */
    printf ("Creating pipeline...\n");
    pipeline = gst_pipeline_new ("test-pipeline");
    gst_element_set_context (GST_ELEMENT (pipeline), x11context);
    //gst_element_set_context (GST_ELEMENT (pipeline), ctxcontext);

    /* Create the elements */
    printf ("Creating pipeline elements...\n");
    appSource = gst_element_factory_make ("appsrc", "source");
    sink = gst_element_factory_make ("glimagesink", "sink");

    /* Configure appsrc element */
    printf ("Configuring appsrc...\n");
    g_object_set(appSource, "caps", gstCaps, NULL);
    g_signal_connect (appSource, "need-data", G_CALLBACK (startFeed), NULL);
    g_signal_connect (appSource, "enough-data", G_CALLBACK (stopFeed), NULL);

    if (!pipeline || !appSource || !sink) {
        g_printerr ("Not all elements are available.\n");
        return;
    }

    /* add elements */
    printf ("Adding elements to pipeline...\n");
    gst_bin_add_many (GST_BIN (pipeline), appSource, sink, NULL);

    /* link elements */
    printf ("Linking elements...\n");
    if (gst_element_link (appSource, sink) != TRUE) {
        g_printerr ("Elements could not be linked.\n");
        gst_object_unref (pipeline);
        return;
    }

    /* Pause pipeline */
    printf ("Pausing pipeline...\n");
    gst_element_set_state (pipeline, GST_STATE_PAUSED);
}

int startStream(void *data)
{
    SDL_Window *sdlWindow = (SDL_Window*)data;
    configureStream(sdlWindow);
    GstBus *bus;
    GstStateChangeReturn ret;

    printf("Starting stream...\n");

    // Set up GstGLFramebuffer
    printf("Setting up gst framebuffer object...\n");
    // gstFb = gst_gl_framebuffer_new_with_default_depth (gstGlContext, 1280, 720);

    /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
    bus = gst_element_get_bus (pipeline);
    gst_bus_add_signal_watch (bus);
    g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)errorCallback, NULL);
    gst_object_unref (bus);

    /* Start playing */
    printf ("Playing pipeline...\n");
    ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr ("Unable to set the pipeline to the playing state.\n");
        gst_object_unref (pipeline);
        return -1;
    }

    g_print ("WEEEEEEEEEEEEEEEE\n");

    mainLoop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (mainLoop);

    /* Free resources */
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);
    return 0;
}