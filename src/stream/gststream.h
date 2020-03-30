#pragma once

#include <SDL_video.h>
#include <gst/gst.h>

gboolean pushData();
void startFeed (GstElement *source, guint size, void *data);
void stopFeed (GstElement *source, void *data);
void configureStream(SDL_Window *sdlWindow);
int startStream(void *data);