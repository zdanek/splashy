/************************************************************************
 *                      splashy_video.c                                 *
 *                                                                      *
 *  2005-05-28 03:01 EDT                                                *
 *  Copyright  2005  Otavio Salvador <otavio@debian.org>                *
 *                   Luis Mondesi    <lemsx1@gmail.com>                 *
 ************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>             /* abs() */

#include <stdio.h>
#include <string.h>
#include <time.h>               /* nanosleep() */
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <glib.h>
/*
 * 64bit arch fix bug#302647 in alioth and
 * bug#354856 on bugs.debian.org/splashy (DBTS)
 */
#include <directfb.h>

#include "common_macros.h"
#include "splashycnf.h"
#include "splashy.h"

typedef struct splashy_videomode_s
{
        gint xres;
        gint yres;
        gint overx;
        gint overy;
        gint bpp;
} splashy_videomode_t;

typedef struct
{
        IDirectFBSurface *offscreen;    /** off-screen storage */
        IDirectFBSurface *surface;      /** Subsurface to draw text on */
        DFBRectangle area;
} splashy_box_t;

typedef struct
{
        IDirectFB *dfb;                         /** our directfb object */
        IDirectFBImageProvider *provider;       /** our image class */
        IDirectFBWindow *primary_window;        /** our main window. where opacity takes place */
        IDirectFBSurface *primary_window_surface; /** interface to window's surface */

        IDirectFBDisplayLayer *primary_layer;   /** interface to our layer */
        DFBDisplayLayerConfig primary_layer_config;  /** configuration for our primary_layer */
        IDirectFBSurface *primary_surface;      /** our main background */
        IDirectFBInputDevice *keyboard;         /** our main input dev */
        IDirectFBEventBuffer *ev_buffer;        /** our events listener */
        splashy_videomode_t *mode;              /** video mode supported by surface */
        splashy_box_t *textbox;             /** text box parameters */
        DFBRectangle progressbar;               /** our progressbar */
        IDirectFBFont *font;                    /** The font we use */
        DFBFontDescription fontdesc;
        __u8 opacity;                           /** current opacity level */
} splashy_video_t;

static splashy_video_t video;

static const char *_current_background = NULL;
static guint last_text_y_position = 0;
static guint _last_progress = 0;
static guint _last_w = 0;
static guint _last_x = 0;

static gboolean _draw_progressbar_forward = TRUE;
static gboolean _show_progressbar = FALSE;
static gboolean _show_textbox_area = FALSE;     /* flag to toggle showing *
                                                 * textbox area. @see
                                                 * splashy_function::keyevent_loop() 
                                                 */
#define NRMODES 4
static const char *valid_modes[NRMODES] =
        { "boot", "shutdown", "resume", "suspend" };
static enum
{ BOOT, SHUTDOWN, RESUME, SUSPEND } _current_mode;      /* boot, halt,
                                                         * resume, suspend */
/*
 * prototype for local helpers
 */


/*
 * helper functions 
 */

/*
 * be very carefull when calling this function 
 * as it's not thread safe
 */
void
splashy_reset_progressbar_counters ()
{
        _last_progress = 0;
        _last_w = 0;
        _last_x = 0;
}

void
splashy_set_progressbar_forward (gboolean go_forward)
{
        _draw_progressbar_forward = go_forward;
}

void
splashy_set_progressbar_visible (gboolean visible)
{
        _show_progressbar = visible;
}

void
splashy_set_textbox_area_visible (gboolean visible)
{
        _show_textbox_area = visible;
        /*
         * users expect to see something happen as soon
         * as F2 is pressed. Let's give'm a show for their
         * money!
         */
        if (visible)
        {
                video.textbox->offscreen->Blit (video.textbox->surface,
                                                video.textbox->offscreen,
                                                NULL, 0, 0);
        }
        else
        {
                _clear_offscreen ();
        }
}

gboolean
splashy_get_textbox_area_visible (void)
{
        return _show_textbox_area;
}

static gint
fb_preinit (struct fb_var_screeninfo *fb_vinfo)
{
        static gint fb_preinit_done = 0;
        static gint fb_err = -1;
        gchar *fb_dev_name = NULL;      /* such as /dev/fb0 */

        gint fb_dev_fd;         /* handle for fb_dev_name */

        if (fb_preinit_done)
                return fb_err;
        fb_preinit_done = 1;

        if (!fb_dev_name && !(fb_dev_name = getenv ("FRAMEBUFFER")))
                fb_dev_name = g_strdup ("/dev/fb0");

        DEBUG_PRINT ("Using device %s\n", fb_dev_name);

        if ((fb_dev_fd = open (fb_dev_name, O_RDWR)) == -1)
        {
                ERROR_PRINT (_("libsplashy: Cannot open %s"), fb_dev_name);
                ERROR_PRINT ("%s\n", strerror (errno));
                goto err_out;
        }
        if (ioctl (fb_dev_fd, FBIOGET_VSCREENINFO, fb_vinfo))
        {
                ERROR_PRINT (_("libsplashy: [fbdev2] Can't get VSCREENINFO: %s\n"),
                             strerror (errno));
                goto err_out;
        }

        fb_err = 0;
        return 0;
      err_out:
        if (fb_dev_fd >= 0)
                close (fb_dev_fd);
        fb_dev_fd = -1;
        fb_err = -1;
        return -1;
}


/*
 * Public members 
 */
void
splashy_allow_vt_switching ()
{
        DEBUG_PRINT ("%s", __FUNCTION__);
        /*
         * Please read functions.c about why NOT to set vt-switching here.
         * 2005-04-28 20:42 EDT - Luis Mondesi <lemsx1@gmail.com> 
         */
        /*
         * DirectFBSetOption( "vt-switching", NULL); 
         */
        /*
         * DirectFBSetOption ("force-windowed", NULL); 
         */
        DEBUG_PRINT ("%s", __FUNCTION__);
}

void
_get_divider (gint * divider_width, gint * divider_height)
{
        /*
         * Source image width and height 
         */
        gint source_width, source_height;

        /*
         * If a source image width and height is specified, assume
         * pixel-precise placement of progress bar rather than percentage. 
         */
        source_width =
                splashy_get_config_int ("/splashy/background/dimension/width",
                                        10);
        source_height =
                splashy_get_config_int
                ("/splashy/background/dimension/height", 10);
        if (source_width > 0 && source_height > 0)
        {
                /*
                 * The value is based on specified units 
                 */
                *divider_width = source_width;
                *divider_height = source_height;
        }
        else
        {
                /*
                 * The value is based on percentage 
                 */
                *divider_width = *divider_height = 100;
        }

        DEBUG_PRINT ("Divider is (width x height) = %d x %d\n",
                     *divider_width, *divider_height);
}

/**
 * gets the real (resolution related) dimension for our primary_window_surface
 * (which should match our primary_surface since we are in fullscreen mode)
 * On dual-head systems the value of height is sometimes 2 times heigher than
 * what's reported by xres from /dev/fb0 (@see fb_preinit())
 * fb_preinit() must be called before this function
 */
void
_get_screen_size (gint * width, gint * height)
{
        /*
         * Let's not rely on this since it reports the wrong size 
         * (on dual-head video cards):
         */
#ifdef DEBUG
        gint screen_width, screen_height;
        video.primary_surface->GetSize
                (video.primary_surface, &screen_width, &screen_height);

        DEBUG_PRINT
                ("Primary surface actual size (width x height): %d x %d\n",
                 screen_width, screen_height);
#endif

        /*
         * trust the values set by fb_preinit() from calling
         * ioctl (fb_dev_fd, FBIOGET_VSCREENINFO, fb_vinfo)
         */
        *width = video.mode->xres;
        *height = video.mode->yres;

        DEBUG_PRINT ("Screen size (width x height): %d x %d",
                     *width, *height);
}

gint
draw_progressbar ()
{
        gint screen_width, screen_height;
        gint divider_w, divider_h;
        guint rectangle_red, rectangle_green, rectangle_blue, rectangle_alpha;
        /*
         * /splashy/progressbar/background/color 
         */
        guint fondo_red, fondo_green, fondo_blue, fondo_alpha;

        DFBRectangle *progressbar = &video.progressbar;

        const gchar *draw_progress =
                splashy_get_config_string
                ("/splashy/progressbar/border/enable");
        gboolean draw_progress_border = (draw_progress
                                         &&
                                         g_ascii_strncasecmp (draw_progress,
                                                              "yes",
                                                              3) ==
                                         0) ? TRUE : FALSE;

        if (!_show_progressbar)
                return 0;

        DEBUG_PRINT ("Printing progress border: %d", draw_progress_border);

        _get_divider (&divider_w, &divider_h);
        _get_screen_size (&screen_width, &screen_height);

        progressbar->x = screen_width *
                splashy_get_config_int ("/splashy/progressbar/dimension/x",
                                        10) / divider_w;
        progressbar->y =
                screen_height *
                splashy_get_config_int ("/splashy/progressbar/dimension/y",
                                        10) / divider_h;
        progressbar->w =
                screen_width *
                splashy_get_config_int
                ("/splashy/progressbar/dimension/width", 10) / divider_w;
        progressbar->h =
                screen_height *
                splashy_get_config_int
                ("/splashy/progressbar/dimension/height", 10) / divider_h;

        if (progressbar->x < 0 ||
            progressbar->y < 0 || progressbar->w < 0 || progressbar->h < 0)
        {
                g_printerr
                        ("ERROR: progress bar coordinates (x,y,width,height) had negative values. Please check that your configuration file exists and it's readable\n");
                return 1;
        }

        if (draw_progress_border == TRUE)
        {
                /*
                 * Check progress bar border colours. Assume we want a border 
                 * if they are all properly specified. 
                 */
                rectangle_red =
                        splashy_get_config_int
                        ("/splashy/progressbar/border/color/red", 10);
                rectangle_green =
                        splashy_get_config_int
                        ("/splashy/progressbar/border/color/green", 10);
                rectangle_blue =
                        splashy_get_config_int
                        ("/splashy/progressbar/border/color/blue", 10);
                rectangle_alpha =
                        splashy_get_config_int
                        ("/splashy/progressbar/border/color/alpha", 10);
                if (!
                    (rectangle_red > 255 || rectangle_green > 255
                     || rectangle_blue > 255 || rectangle_alpha > 255))
                {
                        DEBUG_PRINT
                                ("Border draw: x, y, w, h = %d, %d, %d, %d",
                                 progressbar->x, progressbar->y,
                                 progressbar->w, progressbar->h);

                        DEBUG_PRINT ("Border color: rgba = %d, %d, %d, %d",
                                     rectangle_red, rectangle_green,
                                     rectangle_blue, rectangle_alpha);

                        video.primary_surface->SetColor (video.
                                                         primary_surface,
                                                         rectangle_red,
                                                         rectangle_green,
                                                         rectangle_blue,
                                                         rectangle_alpha);

                        /*
                         * Draw border so that it is one pixel around the bar 
                         */
                        video.primary_surface->DrawRectangle (video.
                                                              primary_surface,
                                                              progressbar->
                                                              x - 1,
                                                              progressbar->
                                                              y - 1,
                                                              progressbar->
                                                              w + 2,
                                                              progressbar->
                                                              h + 2);
                }
                else if (rectangle_red < 0 || rectangle_green < 0 ||
                         rectangle_blue < 0 || rectangle_alpha < 0)
                {
                        ERROR_PRINT
                                (_("Progress bar border color not properly specified"));
                }
                else
                {
                        DEBUG_PRINT ("No progress bar border specified");
                }
        }
        rectangle_red =
                splashy_get_config_int ("/splashy/progressbar/color/red", 10);
        rectangle_green =
                splashy_get_config_int ("/splashy/progressbar/color/green",
                                        10);
        rectangle_blue =
                splashy_get_config_int ("/splashy/progressbar/color/blue",
                                        10);
        rectangle_alpha =
                splashy_get_config_int ("/splashy/progressbar/color/alpha",
                                        10);

        /*
         * set the filling color of the progressbar to be 
         * used by splashy_update_progress()
         */
        DEBUG_PRINT ("Draw rgba = %d, %d, %d, %d",
                     rectangle_red, rectangle_green, rectangle_blue,
                     rectangle_alpha);

        if (rectangle_red > 255 ||
            rectangle_green > 255 ||
            rectangle_blue > 255 || rectangle_alpha > 255)
        {
                ERROR_PRINT
                        (_("Red, green, blue or alpha tags in the configuration file contain wrong values"));
                return 3;
        }

        /*
         * TODO make this configurable from theme.xml
         * /splashy/progressbar/background/color/red...
         * for now picking gray
         */
        fondo_red =
                splashy_get_config_int
                ("/splashy/progressbar/background/color/red", 10);
        fondo_green =
                splashy_get_config_int
                ("/splashy/progressbar/background/color/green", 10);
        fondo_blue =
                splashy_get_config_int
                ("/splashy/progressbar/background/color/blue", 10);
        fondo_alpha =
                splashy_get_config_int
                ("/splashy/progressbar/background/color/alpha", 10);
        if (_draw_progressbar_forward == TRUE)
        {
                /*
                 * fill the rectangle so that it's easier to do the reverse progress trick
                 * @see splashy_update_progress()
                 */
                video.primary_surface->SetColor (video.primary_surface,
                                                 fondo_red,
                                                 fondo_green,
                                                 fondo_blue, fondo_alpha);
        }
        else
        {
                video.primary_surface->SetColor (video.primary_surface,
                                                 rectangle_red,
                                                 rectangle_green,
                                                 rectangle_blue,
                                                 rectangle_alpha);
        }
        video.primary_surface->FillRectangle (video.primary_surface,
                                              progressbar->x,
                                              progressbar->y,
                                              progressbar->w, progressbar->h);
        /*
         * now invert the colors set 
         */
        if (_draw_progressbar_forward != TRUE)
        {
                video.primary_surface->SetColor (video.primary_surface,
                                                 fondo_red,
                                                 fondo_green,
                                                 fondo_blue, fondo_alpha);
        }
        else
        {
                video.primary_surface->SetColor (video.primary_surface,
                                                 rectangle_red,
                                                 rectangle_green,
                                                 rectangle_blue,
                                                 rectangle_alpha);
        }

        /*
         * finally, reset our counters to their proper values
         * for forward and backward progress
         */
        _last_x = progressbar->w;       /* we go from back to front */
        _last_w = 0;            /* we start at progressbar->x really. 0 is
                                 * fine */

        /*
         * IDirectFBSurface *_layer_surface;
         * 
         * DFBCHECK (video.primary_layer-> GetSurface (video.primary_layer,
         * &_layer_surface));
         * 
         * _layer_surface->Blit(_layer_surface, video.primary_surface, NULL,
         * 0, 0);
         */

        return 0;
}

/*
 * TODO 
 * if we get a perc that's less that the previus number
 * update the progressbar in reverse
 */
gint
splashy_update_progressbar (gint perc)
{
        /*
         * how many nanoseconds we sleep while updating the progress bar 
         */
        struct timespec _progress_sleep;
        guint i = 0, _w = 0;

        DFBRectangle *progressbar = &video.progressbar;

        DEBUG_PRINT ("splashy_update_progressbar(%d) called", perc);

        if (!_show_progressbar)
                return 0;
        /*
         * sanity check 
         */
        if (perc < 0)
                return 1;
        if (&video == NULL)
                return 1;
        if (perc > 100)
                return 0;
        DEBUG_PRINT ("splashy_update_progressbar(%d) sanity checks passed",
                     perc);
        DEBUG_PRINT ("Progress x, y, w, h = %d, %d, %d, %d",
                     progressbar->x, progressbar->y,
                     progressbar->w, progressbar->h);

        _progress_sleep.tv_sec = 0;
        _progress_sleep.tv_nsec = 100;

        _w = progressbar->w * perc / 100;
        if (_w <= 0)
        {
                DEBUG_PRINT ("Skipped progress width %d", _w);
                return 1;
        }

        if (_draw_progressbar_forward != TRUE)
        {
                if (perc == 0)
                        _last_x = progressbar->w;

                guint _x = progressbar->x + (progressbar->w - _w);

                /*
                 * draw (smooth) progress backward 
                 */
                for (i = _last_x; i > _x; i--)
                {
                        video.primary_surface->FillRectangle (video.
                                                              primary_surface,
                                                              i,
                                                              progressbar->y,
                                                              progressbar->
                                                              w - (i -
                                                                   progressbar->
                                                                   x),
                                                              progressbar->h);
                        _last_progress = i;
                        nanosleep (&_progress_sleep, NULL);
                }
                _last_x = _x;
        }
        else
        {
                /*
                 * draw (smooth) progress forward 
                 */
                for (i = _last_w; i <= _w; i++)
                {
                        video.primary_surface->FillRectangle (video.
                                                              primary_surface,
                                                              progressbar->x,
                                                              progressbar->y,
                                                              i,
                                                              progressbar->h);
                        _last_progress = i;
                        nanosleep (&_progress_sleep, NULL);
                }
                _last_w = _w;
        }
        _last_progress = perc;
        return 0;
}

/**
 * updates the progress bar without smoothing it out
 */
gint
splashy_update_progressbar_quick (gint perc)
{
        guint _w = 0;

        DFBRectangle *progressbar = &video.progressbar;

        DEBUG_PRINT ("%s(%d) called", __FUNCTION__, perc);
        if (!_show_progressbar)
                return 0;

        /*
         * sanity check 
         */
        if (perc < 0)
                return 1;
        if (&video == NULL)
                return 1;
        if (perc > 100)
                return 0;
        DEBUG_PRINT
                ("splashy_update_progressbar_quick(%d) sanity checks passed",
                 perc);
        DEBUG_PRINT ("Progress x, y, w, h = %d, %d, %d, %d",
                     progressbar->x, progressbar->y, progressbar->w,
                     progressbar->h);

        _w = progressbar->w * perc / 100;
        if (_w <= 0)
        {
                DEBUG_PRINT ("Skipped progress width %d", _w);
                return 1;
        }

        DEBUG_PRINT ("Progress width %d", _w);

        if (_draw_progressbar_forward != TRUE)
        {
                /*
                 * guint _x = progressbar->x + (progressbar->w - _w); 
                 */
                video.primary_surface->FillRectangle (video.
                                                      primary_surface,
                                                      _w,
                                                      progressbar->y,
                                                      progressbar->w - (_w -
                                                                        progressbar->
                                                                        x),
                                                      progressbar->h);
                _last_x = progressbar->x + (progressbar->w - _w);
        }
        else
        {
                video.primary_surface->FillRectangle (video.
                                                      primary_surface,
                                                      progressbar->x,
                                                      progressbar->y,
                                                      _w, progressbar->h);
                _last_w = _w;
        }

        _last_progress = perc;
        return 0;
}

void
video_set_mode ()
{
        DFBResult ret;
        /*
         * used to disable inputs from mouse and reduce overhead (about 15%
         * of CPU usage)
         */
        /*
         * FIXME video.dfb->SetCooperativeLevel (video.dfb,
         * DFSCL_FULLSCREEN); 
         */
        video.primary_layer->SetCooperativeLevel (video.primary_layer,
                                                  DLSCL_EXCLUSIVE);
        video.primary_layer->GetConfiguration (video.primary_layer,
                                               &video.primary_layer_config);

        /*
         * http://directfb.org/docs/DirectFB_Reference/types.html#DFBSurfacePixelFormat 
         */

        /*
         * DLCONF_WIDTH|DLCONF_HEIGHT|DLCONF_PIXELFORMAT|DLCONF_SURFACE_CAPS 
         */
        video.primary_layer_config.width = video.mode->xres;
        video.primary_layer_config.height = video.mode->yres;
        /*
         * FIXME video.primary_layer_config.pixelformat = DSPF_ARGB; 
         */
        /*
         * DLOP_ALPHACHANNEL|DLOP_OPACITY 
         */

        ret = video.primary_layer->SetConfiguration (video.primary_layer,
                                                     &video.
                                                     primary_layer_config);
        if (ret)
                DEBUG_PRINT
                        ("Error while configuring our primary layer for fullscreen mode");

        /*
         * for vesafb this doesn't help since the resolution is set 
         * on boot:
         */
        video.dfb->SetVideoMode (video.dfb, video.mode->xres,
                                 video.mode->yres, video.mode->bpp);


        DEBUG_PRINT ("Set resolution to %d x %d",
                     video.mode->xres, video.mode->yres);
}

gint
get_time_in_ms (void)
{
        struct timeval tp;
        struct timezone tz;

        if (gettimeofday (&tp, &tz) == 0)
                return (tp.tv_sec * 1000) + (tp.tv_usec / 1000);
        return 0;               /* should never be reached */
}

/*
 * Fade the opacity from its current value to the specified.
 * helper for fade_in/out()
 */
static void
_window_fade (__u8 opacity, long ms_duration)
{
        long t1, t2;
        int diff;
        /*
         * IDirectFBDisplayLayer *layer = win->primary_layer;
         */
        IDirectFBWindow *window = video.primary_window;

        /*
         * Retrieve its current opacity. 
         */
        window->GetOpacity (window, &video.opacity);

        /*
         * TODO how do we know the current opacity level 
         */
        /*
         * Retrieve its current opacity. 
         */
        /*
         * layer->GetOpacity (layer, &win->opacity);
         */

        /*
         * Calculate the opacity difference. 
         */
        diff = opacity - video.opacity;
        /*
         * fprintf(stderr,"diff %d\n",diff); 
         */
        /*
         * Initialize both time stamps. 
         */
        t1 = t2 = get_time_in_ms ();
        /*
         * fprintf(stderr,"t1 %lu\n",t2); 
         */

        do
        {
                /*
                 * Calculate an intermediate opacity. 
                 */
                __u8 op =
                        video.opacity + ((t2 - t1 + 1) * diff / ms_duration);
                /*
                 * fprintf(stderr,"op %d\n",op); 
                 */
                /*
                 * Set the intermediate opacity. 
                 */
                window->SetOpacity (window, op);
                /*
                 * layer->SetOpacity (layer, op); 
                 */

                /*
                 * Update the time stamp. 
                 */
                t2 = get_time_in_ms ();
                /*
                 * fprintf(stderr,"t2 %lu\n",t2); 
                 */
        }
        while (t2 - t1 < ms_duration);
}

/**
 * A simple function to create the effect of "fading in" for the
 * primary surface
 * @see splashy_child_boot() on splashy_functions.c
 * @param video main video holding our primary window
 */
void
fade_in ()
{
        IDirectFBWindow *window = video.primary_window;
        /*
         * start from almost black 
         */
        window->SetOpacity (window, 1);

        _window_fade (255, 1000);

        /*
         * Set the exact opacity. 
         */
        window->SetOpacity (window, 255);
        video.opacity = 255;
}

/**
 * A simple function to create the effect of "fading out" for the
 * primary surface
 * @see splashy_child_exit() on splashy_functions.c
 * @param video main video holding our primary window
 */
void
fade_out ()
{
        IDirectFBWindow *window = video.primary_window;
        /*
         * start from fully opaque 
         */
        window->SetOpacity (window, 255);

        _window_fade (0, 1000);

        /*
         * Set the exact opacity. 
         */
        window->SetOpacity (window, 0);
        video.opacity = 0;
}

/*
 * Initialize the font 
 */
int
init_font ()
{
        const gchar *fontface;
        gint temp;
        gint screen_width, screen_height;
        gint divider_width, divider_height;

        _get_screen_size (&screen_width, &screen_height);
        _get_divider (&divider_width, &divider_height);

        video.fontdesc.flags = DFDESC_HEIGHT;
        fontface =
                splashy_get_config_string ("/splashy/textbox/text/font/file");
        temp = splashy_get_config_int ("/splashy/textbox/text/font/height",
                                       10);

        video.fontdesc.height = temp * screen_height / divider_height;
        video.dfb->CreateFont (video.dfb, fontface,
                               &video.fontdesc, &video.font);
        if (video.font == NULL)
                video.dfb->CreateFont (video.dfb, NULL, NULL, &video.font);

        if (video.font == NULL)
                return -1;

        return 0;
}

/**
 * A helper function to clear up the offscreen surface
 */

void
_clear_offscreen (void)
{
        gint red, green, blue, alpha;
        if (video.textbox == NULL)
                return;
        /*
         * TODO later we will need to add textbox border support here: const
         * Get the tinting colour 
         */
        red = splashy_get_config_int ("/splashy/textbox/color/red", 10);
        green = splashy_get_config_int ("/splashy/textbox/color/green", 10);
        blue = splashy_get_config_int ("/splashy/textbox/color/blue", 10);
        alpha = splashy_get_config_int ("/splashy/textbox/color/alpha", 10);
        if (red < 0 || red > 255)
                red = 128;
        if (green < 0 || green > 255)
                green = 128;
        if (blue < 0 || blue > 255)
                blue = 128;
        if (alpha < 0 || alpha > 255)
                alpha = 255;

        /*
         * copy the "clean" surface from our primary surface to offscreen 
         */
        splashy_change_splash (_current_background);

        /*
         * now we need to flip these surfaces so that the background tint
         * gets displayed correctly 
         */
        video.textbox->offscreen->Blit (video.textbox->offscreen,
                                        video.primary_surface,
                                        &video.textbox->area, 0, 0);

        video.textbox->offscreen->Blit (video.primary_surface,
                                        video.textbox->offscreen,
                                        &video.textbox->area, 0, 0);

        /*
         * Tint the box in the off-screen surface 
         */
        video.textbox->offscreen->SetDrawingFlags (video.textbox->offscreen,
                                                   DSDRAW_BLEND);
        video.textbox->offscreen->SetColor (video.textbox->offscreen, red,
                                            green, blue, alpha);
        video.textbox->offscreen->FillRectangle (video.textbox->offscreen,
                                                 0, 0,
                                                 video.textbox->area.w,
                                                 video.textbox->area.h);
}


/**
* Initialize and draw an alpha-transparent text box.
*
* @return
*/
void
start_text_area ()
{
        gint divider_w, divider_h;
        gint screen_width, screen_height;
        gint red, green, blue;  /* alpha; */
        gint temp;
        DFBSurfaceDescription desc;


        /*
         * Check if there will be a text area. 
         */
        /*
         * TODO autoverboseonerror implies /splashy/textbox/enable="yes" 
         */
        const gchar *enable =
                splashy_get_config_string ("/splashy/textbox/enable");

        if (g_ascii_strncasecmp (enable, "yes", 3) != 0)
                return;

        _get_screen_size (&screen_width, &screen_height);
        _get_divider (&divider_w, &divider_h);

        video.textbox = g_new0 (splashy_box_t, 1);

        /*
         * Set some defaults so that there are no divide by zeroes if the
         * checks below fail 
         */
        video.textbox->area.x = video.textbox->area.w = divider_w / 4;
        video.textbox->area.y = video.textbox->area.h = divider_h / 4;

        /*
         * Read in the text box dimensions 
         */
        temp = splashy_get_config_int ("/splashy/textbox/dimension/x", 10);
        if (temp >= 0 && temp < divider_w)
                video.textbox->area.x = temp * screen_width / divider_w;

        temp = splashy_get_config_int ("/splashy/textbox/dimension/y", 10);
        if (temp >= 0 && temp < divider_h)
                video.textbox->area.y = temp * screen_height / divider_h;

        temp = splashy_get_config_int ("/splashy/textbox/dimension/width",
                                       10);
        if (temp > 0 && temp <= divider_w)
                video.textbox->area.w = temp * screen_width / divider_w;

        temp = splashy_get_config_int ("/splashy/textbox/dimension/height",
                                       10);
        if (temp > 0 && temp <= divider_h)
                video.textbox->area.h = temp * screen_height / divider_h;

        desc.width = video.textbox->area.w;
        desc.height = video.textbox->area.h;
        desc.caps = DSCAPS_SYSTEMONLY;  // Only in system
        // memory
        desc.flags = DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT;

        /*
         * Grab the textbox area to a seperate surface 
         */
        video.dfb->CreateSurface (video.dfb, &desc,
                                  &video.textbox->offscreen);
        video.textbox->offscreen->Blit (video.textbox->offscreen,
                                        video.primary_surface,
                                        &video.textbox->area, 0, 0);

        /*
         * clear the offscreen surface and set the drawing flags
         */
        _clear_offscreen ();

        /*
         * Get the text colour (no alpha) 
         */
        red = splashy_get_config_int ("/splashy/textbox/text/color/red", 10);
        green = splashy_get_config_int ("/splashy/textbox/text/color/green",
                                        10);
        blue = splashy_get_config_int ("/splashy/textbox/text/color/blue",
                                       10);
        if (red < 0 || red > 255)
                red = 0;        /* if missing, make font color black */
        if (green < 0 || green > 255)
                green = 0;
        if (blue < 0 || blue > 255)
                blue = 0;

        /*
         * Establish the subsurface to which we print 
         */
        video.primary_surface->GetSubSurface (video.primary_surface,
                                              &video.textbox->area,
                                              &video.textbox->surface);
        video.textbox->surface->SetColor (video.textbox->surface, red,
                                          green, blue, 255);

        video.textbox->surface->SetFont (video.textbox->surface, video.font);
}

int
create_event_buffer ()
{
        DEBUG_PRINT ("%s", __FUNCTION__);
        /*
         * setup our main input: keyboard 
         */
        /*
         * TODO would we leak if we don't check if video.keyboard is not NULL 
         * ? We should make sure splashy can't be launched indent: Standard input:1067: Warning:Extra )
         twice from this
         * same thread 
         */
        if (video.dfb->GetInputDevice (video.dfb, DIDID_KEYBOARD,
                                       &video.keyboard) != DFB_OK)
                return -1;

        /*
         * create event buffer for the keyboard: to listen for events 
         */
        if (video.keyboard->CreateEventBuffer (video.keyboard,
                                               &video.ev_buffer) != DFB_OK)
                return -2;

        return 0;
}

int
splashy_start_splash ()
{
        DFBSurfaceDescription desc;
        DFBWindowDescription win_desc;
        struct fb_var_screeninfo fb_vinfo;

        /*
         * initializing Directfb
         */
        /*
         * - no-debug           := suppresses debug messages
         * - quiet              := suppresses all messages (but debug)
         * - graphics-vt        := puts directfb vt in graphics mode
         * - no-cursor          := disallow showing a cursor
         */

        if (DirectFBInit (NULL, NULL) != DFB_OK)
                return -1;

        DirectFBSetOption ("quiet", NULL);
        DirectFBSetOption ("no-debug", NULL);
        /*
         * graphics-vt avoids kernel text displaying on top
         * of our code
         * 2007-12-21 01:20 EST Luis Mondesi
         * - disabled this for now (remove 'no-' to reenable)
         */
        DirectFBSetOption ("no-graphics-vt", NULL);
        DirectFBSetOption ("no-cursor", NULL);
        /*
         * we want to avoid keyboard problems when running from initramfs
         * - we tell directfb to use vt 8 instead of 2
         */
        DirectFBSetOption ("vt-num", 8);
        /*
         * Now we tell directfb to NOT use vt's at all!
         * That way we should not have problems of loosing our input device
         * (keyboard)
         * - Luis Mondesi
         */
        DirectFBSetOption ("no-vt", NULL);

        /*
         * 2007-12-21 01:17 EST Luis Mondesi
         * - re-enabling radeon
         * DirectFBSetOption ("disable-module", "radeon");
         */
        /* 
         * 2007-12-21 01:14 EST Luis Mondesi
         * - re-enabling linux_input
         *
         * DirectFBSetOption ("disable-module", "linux_input");
         */

        /*
         * TODO doesn't solve anything: DirectFBSetOption ("dont-catch",
         * "15"); 
         */
        /*
         * TODO too dangerous DirectFBSetOption ("block-all-signals", NULL); 
         */


        if (DirectFBCreate (&video.dfb) != DFB_OK)
        {
                ERROR_PRINT (_("libsplashy: Framebuffer is not configured properly please see %s"), "http://tinyurl.com/339h67");
                return -2;
        }

        video.mode = g_new0 (splashy_videomode_t, 1);
        /*
         * set our expectation to a very big number 
         */
        fb_preinit (&fb_vinfo);
        video.mode->yres = fb_vinfo.yres;
        video.mode->xres = fb_vinfo.xres;

        if (fb_vinfo.bits_per_pixel > 24)
        {
                video.mode->bpp = 32;
        }
        else if (fb_vinfo.bits_per_pixel > 16)
        {
                video.mode->bpp = 24;
        }
        else if (fb_vinfo.bits_per_pixel > 8)
        {
                video.mode->bpp = 16;
        }
        else
        {
                video.mode->bpp = 8;
        }

        DEBUG_PRINT ("Setting min Width (x) resolution to %d\n",
                     video.mode->xres);
        DEBUG_PRINT ("Setting min Height (y) resolution to %d\n",
                     video.mode->yres);
        DEBUG_PRINT ("Setting min bits_per_pixel to %d (was %d)\n",
                     video.mode->bpp, fb_vinfo.bits_per_pixel);

        if (video.dfb->CreateImageProvider (video.dfb,
                                            _current_background,
                                            &video.provider) != DFB_OK)
        {
                video.dfb->Release (video.dfb);
                return -3;
        }
        if (video.provider->GetSurfaceDescription (video.provider,
                                                   &desc) != DFB_OK)
        {
                video.dfb->Release (video.dfb);
                return -4;
        }
        /*
         * flags to set the default surface as main surface
         */
        desc.flags = DSDESC_CAPS;
        desc.caps = DSCAPS_PRIMARY;

        /*
         * store our primary layer as this will be use for setting the opacity
         * levels later
         */
        if (video.dfb->GetDisplayLayer (video.dfb,
                                        DLID_PRIMARY,
                                        &video.primary_layer) != DFB_OK)
        {
                video.dfb->Release (video.dfb);
                return -5;
        }


        /*
         * we need to call this after primary_layer was set. @see video_set_mode()
         */
        video_set_mode ();

        /*
         * get our primary_surface, this will hold the progressbar, textbox
         * and others
         */
        if (video.primary_layer->GetSurface (video.primary_layer,
                                             &video.primary_surface)
            != DFB_OK)
        {
                video.dfb->Release (video.dfb);
                return -6;
        }

        win_desc.flags = (DWDESC_POSX | DWDESC_POSY |
                          DWDESC_WIDTH | DWDESC_HEIGHT);
        win_desc.posx = 0;
        win_desc.posy = 0;
        win_desc.width = video.mode->xres;
        win_desc.height = video.mode->yres;
        /*
         * TODO do we really need this? win_desc.caps = DWCAPS_ALPHACHANNEL; 
         */

        if (video.primary_layer->CreateWindow (video.primary_layer,
                                               &win_desc,
                                               &video.
                                               primary_window) != DFB_OK)
        {
                video.dfb->Release (video.dfb);
                video.primary_surface->Release (video.primary_surface);
                return -7;
        }

        DEBUG_PRINT
                ("Window was created with (x_start,y_start,x_end,y_end) 0,0,%d,%d dimensions\n",
                 win_desc.width, win_desc.height);

        if (video.primary_window->GetSurface (video.primary_window,
                                              &video.primary_window_surface)
            != DFB_OK)
        {
                video.dfb->Release (video.dfb);
                video.primary_surface->Release (video.primary_surface);
                video.primary_window->Release (video.primary_window);
                return -8;
        }

        if (g_ascii_strncasecmp
            (splashy_get_config_string ("/splashy/fadein"), "yes", 3) == 0)
                video.primary_window->SetOpacity (video.primary_window, 0x0);

        /*
         * allow surface to have alpha channels 
         * fade in effect won't work with alpha channels in window
         * because we use Opacity levels. See windows capability DWCAPS_ALPHACHANNEL
         * above
         */
        /*
         * DFBCHECK (video.primary_surface->SetBlittingFlags
         * (video.primary_surface, DSBLIT_BLEND_ALPHACHANNEL));
         */

        /*
         * it writes on the framebuffer the background image
         */
        video.provider->RenderTo (video.provider,
                                  video.primary_window_surface, NULL);
        video.provider->RenderTo (video.provider, video.primary_surface,
                                  NULL);

        if (create_event_buffer () < 0)
        {
                video.dfb->Release (video.dfb);
                video.primary_surface->Release (video.primary_surface);
                video.primary_window->Release (video.primary_window);
                return -9;
        }
        /*
         * fade in effect 
         */

        if (g_ascii_strncasecmp
            (splashy_get_config_string ("/splashy/fadein"), "yes", 3) == 0)
                fade_in ();


        draw_progressbar ();

        if (init_font () < 0)
        {
                splashy_stop_splash ();
                return -10;
        }

        start_text_area ();

        return 0;
}

void
splashy_stop_splash ()
{
        /*
         * do an expectacular exit routine 
         */
        const gchar *fadeout = splashy_get_config_string ("/splashy/fadeout");
        if (fadeout)
        {
                if (g_ascii_strncasecmp (fadeout, "yes", 3) == 0)
                {
                        fade_out ();
                }
        }

        /*
         * free up memory 
         */
        /*
         * Also release textarea ? 
         */
        /*
         * TODO Do we need to check if keyboard and ev_buffer were init'd ?
         */

        DEBUG_PRINT ("Releasing %s\n", "ev_buffer");
        video.ev_buffer->Release (video.ev_buffer);     /* input buffer */
        DEBUG_PRINT ("Releasing %s\n", "keyboard");
        video.keyboard->Release (video.keyboard);       /* keyevents gone */
        DEBUG_PRINT ("Releasing %s\n", "primary_surface");
        video.primary_surface->Release (video.primary_surface); /* pix holder 
                                                                 */
        DEBUG_PRINT ("Releasing %s\n", "primary_window");
        video.primary_window->Release (video.primary_window);
        DEBUG_PRINT ("Releasing %s\n", "dfb");
        video.dfb->Release (video.dfb); /* kill it ! */
        DEBUG_PRINT ("done Releasing %s\n", "dfb");
}


void
splashy_wait_for_event ()
{
        DEBUG_PRINT ("%s", __FUNCTION__);
        video.ev_buffer->WaitForEvent (video.ev_buffer);
}

void
splashy_wake_up ()
{
        DEBUG_PRINT ("%s", __FUNCTION__);
        video.ev_buffer->WakeUp (video.ev_buffer);
}

/*
 * Get key event from the event buffer.
 *
 * It will return -1 if there is no valid key event, else it
 * will return the unicode value (which is a superset of ascii)
 */
int
splashy_get_key_event ()
{
        DEBUG_PRINT ("%s", __FUNCTION__);
        DFBInputEvent DFBevent;

        while (video.ev_buffer->HasEvent (video.ev_buffer) == DFB_OK
               && video.ev_buffer->GetEvent (video.ev_buffer,
                                             DFB_EVENT (&DFBevent)) == DFB_OK)
        {
                if (DFBevent.type != DIET_KEYPRESS)
                        continue;

                return DFBevent.key_symbol;
        }

        return -1;
}

void
splashy_change_splash (const gchar * newimage)
{
        video.dfb->CreateImageProvider (video.dfb, newimage, &video.provider);
        video.provider->RenderTo (video.provider, video.primary_surface,
                                  NULL);
        /*
         * restore progressbar 
         */
        if (_show_progressbar == TRUE)
        {
                draw_progressbar ();
                if (_last_progress >= 0)
                {
                        DEBUG_PRINT ("Restoring progressbar to %d ticks\n",
                                     _last_progress);
                        splashy_update_progressbar_quick (_last_progress);
                }
        }
        _current_background = newimage;
}

void
splashy_reset_splash ()
{
        splashy_change_splash (_current_background);
}

/**
 * Prints a line of text in the textbox. Supports multi-lines split by new-lines (\n)
 * This displays regardless of whether the user wants
 * to see the message or not. Use this function to send
 * important status messages (one line at a time) to the user.
 *
 * @return
 */
void
splashy_printline (const char *string)
{
        char *sp = NULL, *tok, *str;
        DFBRectangle rect;
        int x, y, ls;

        if (video.textbox == NULL)
                return;

        if (last_text_y_position > 0)
        {
                _clear_offscreen ();
                last_text_y_position = 0;
        }

        /*
         * Copy the textbox background from off-screen surface 
         */
        video.textbox->offscreen->Blit (video.textbox->surface,
                                        video.textbox->offscreen, NULL, 0, 0);


        video.font->GetStringExtents (video.font, "_", -1, NULL, &rect);
        video.font->GetHeight (video.font, &ls);
        x = rect.w;
        y = ls / 2;

        for (str = strdup (string);
             (tok = strtok_r (str, "\n", &sp)) != NULL; str = NULL)
        {
                /*
                 * Draw string to the clipped surface 
                 */
                video.textbox->surface->DrawString (video.textbox->surface,
                                                    tok, -1, x, (y +=
                                                                 ls),
                                                    DSTF_BOTTOM);
        }

        free (str);
}

/**
 * Prints a line of text in the textbox and scroll down
 * Note that this function respects _show_textbox_area while
 * splashy_printline() does not. The reason is that this
 * function will show the textbox only when F2 is pressed.
 * splashy_printline() is assumed to be used for displaying
 * status regardless of whether the user actually wanted to see this
 * or not.
 *
 * @return
 */
void
splashy_printline_s (const char *string)
{
        char *str;
        DFBRectangle rect;
        int x, y = 0, ls;

        if (video.textbox == NULL)
                return;

        if (last_text_y_position >=
            (video.textbox->area.h - abs (video.fontdesc.height / 2)))
        {
                _clear_offscreen ();
                last_text_y_position = 0;
        }

        video.font->GetStringExtents (video.font, "_", -1, NULL, &rect);
        video.font->GetHeight (video.font, &ls);
        x = rect.w;
        y = (ls / 2 + last_text_y_position) + ls;

        str = strdup (string);
        /*
         * Draw string to the clipped surface 
         */
        if (_show_textbox_area)
        {
                /*
                 * Copy the textbox background from off-screen surface 
                 */
                video.textbox->offscreen->Blit (video.textbox->surface,
                                                video.textbox->offscreen,
                                                NULL, 0, 0);
                video.textbox->surface->DrawString (video.textbox->surface,
                                                    str, -1, x, y,
                                                    DSTF_BOTTOM);

                /*
                 * save our textbox for the next scroll 
                 */
                video.textbox->offscreen->Blit (video.textbox->offscreen,
                                                video.textbox->surface,
                                                NULL, 0, 0);
        }
        else
        {
                /*
                 * draw the string offscreen anyway 
                 */
                video.textbox->offscreen->DrawString (video.textbox->
                                                      offscreen, str, -1, x,
                                                      y, DSTF_BOTTOM);
        }
        last_text_y_position = y;
        free (str);
}

int
splashy_getchar ()
{
        DFBInputEvent event;
        int res = -1;

        while (res < 0)
        {
                video.ev_buffer->WaitForEvent (video.ev_buffer);

                while (video.ev_buffer->
                       GetEvent (video.ev_buffer,
                                 DFB_EVENT (&event)) == DFB_OK)
                {
                        if (event.type != DIET_KEYPRESS)
                                continue;
                        if (!DFB_KEY_IS_ASCII (event.key_symbol))
                                continue;

                        res = event.key_symbol;
                }
        }

        return res;
}

void
draw_input_bar (char *string, int len, int pos, splashy_box_t * input)
{

        int dir = DSTF_LEFT, loc = 5, cursor;
        DFBRectangle rect;
        video.font->GetStringExtents (video.font, string, pos, NULL, &rect);
        cursor = rect.w;

        /*
         * Clear input bar 
         */
        input->surface->Blit (input->surface, input->offscreen, NULL, 0, 0);

        /*
         * If string is longer than box, scroll to left 
         */
        if (cursor > input->area.w - 2 * loc)
        {
                video.font->GetStringExtents (video.font, "_", 1, NULL,
                                              &rect);
                dir = DSTF_RIGHT;
                loc = input->area.w - loc - rect.w;
                cursor = loc + rect.w;
        }
        /*
         * Draw text 
         */
        input->surface->DrawString (input->surface, string, -1,
                                    loc, input->area.h / 2, dir);
        /*
         * Draw cursor 
         */
        input->surface->DrawString (input->surface, "_", -1,
                                    cursor, input->area.h / 2 + 2, dir);

}

/*
 * Asks user to fill the buffer with a string with max_length-1 char's The
 * buffer will always end with a \0. 
 */
int
_gets_from_input_bar (char *buf, int max_length, splashy_box_t * input,
                      int pass)
{
        DFBInputEvent event;
        int enter_pressed = 0, len = 0, pos = 0, i;
        char *tmp = (char *) malloc (max_length * sizeof (char));

        if (max_length < 1)
                return 0;


        while (!enter_pressed)
        {
                video.ev_buffer->WaitForEvent (video.ev_buffer);

                while (video.ev_buffer->
                       GetEvent (video.ev_buffer,
                                 DFB_EVENT (&event)) == DFB_OK)
                {
                        if (event.type != DIET_KEYPRESS)
                                continue;

                        switch (event.key_id)
                        {
                        case DIKI_ENTER:
                                enter_pressed = 1;
                                break;
                        case DIKI_RIGHT:
                                pos = (++pos > len ? len : pos);
                                break;
                        case DIKI_LEFT:
                                pos = (--pos < 0 ? 0 : pos);
                                break;
                        case DIKI_DELETE:
                                if (len == 0 || pos == len)
                                        break;
                                len--;
                                // For the corner cases pos=0 or len
                                // nrchars == 0
                                strncpy (tmp, buf, pos);
                                strncpy (tmp + pos, buf + pos + 1, len - pos);
                                strncpy (buf, tmp, len);
                                pos = (pos > len - 1 ? len - 1 : pos);
                                break;
                        case DIKI_BACKSPACE:
                                if (pos == 0)
                                        break;
                                len--;
                                pos--;
                                strncpy (tmp, buf, pos);
                                strncpy (tmp + pos, buf + pos + 1, len - pos);
                                strncpy (buf, tmp, len);
                                break;
                        case DIKI_HOME:
                                pos = 0;
                                break;
                        case DIKI_END:
                                pos = len;
                                break;
                                // case DIKI_SPACE:
                        default:
                                // if( DFB_KEY_TYPE(event.key_symbol) 
                                // !=
                                // DIKT_UNICODE ) continue;
                                if (!DFB_KEY_IS_ASCII (event.key_symbol))
                                        continue;

                                if (len + 1 >= max_length)
                                {
                                        /*
                                         * We're full, blink 
                                         */
                                        break;
                                }

                                buf[len++] = event.key_symbol;
                                pos = (++pos > len ? len : pos);
                                break;
                        }
                }

                buf[len] = '\0';
                tmp[len] = '\0';
                if (pass)
                        for (i = 0; i < len; i++)
                                tmp[i] = '*';
                else
                        strncpy (tmp, buf, len);

                draw_input_bar (tmp, len, pos, input);

        }                       /* while ! enter */

        free (tmp);
        return len;
}

int
_get_string (char *buf, int len, const char *prompt, int pass)
{
        gint divider_w, divider_h;
        gint screen_width, screen_height;
        gint red, green, blue, alpha;

        splashy_box_t box;
        splashy_box_t input;
        DFBSurfaceDescription desc;

        int font_height, r_len;
        video.font->GetHeight (video.font, &font_height);

        _get_screen_size (&screen_width, &screen_height);
        _get_divider (&divider_w, &divider_h);

        /*
         * Set up the size of the box and input line and title box 
         */
        box.area.x = 0.2 * screen_width;
        box.area.w = 0.6 * screen_width;
        box.area.y = 0.35 * screen_height;
        box.area.h = 0.3 * screen_height;

        input.area.x = box.area.x + 2 * font_height;
        input.area.w = box.area.w - 4 * font_height;
        input.area.h = 2 * font_height;
        input.area.y = box.area.y + box.area.h / 2;

        /*
         * Make a copy of the surface below, so we can put it back
         */
        desc.width = box.area.w;
        desc.height = box.area.h;
        desc.caps = DSCAPS_SYSTEMONLY;  // Only in system
        desc.flags = DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT;

        video.dfb->CreateSurface (video.dfb, &desc, &box.offscreen);

        box.offscreen->Blit (box.offscreen,
                             video.primary_surface, &box.area, 0, 0);

        /*
         * Create a pointer to the SubSurface where the box will be 
         */
        video.primary_surface->GetSubSurface (video.primary_surface,
                                              &box.area, &box.surface);
        box.surface->SetDrawingFlags (box.surface, DSDRAW_BLEND);

        /*
         * Fill it with a color (using alpha) 
         *
         * TODO make these colors theme-able
         */
        red = alpha = 255;
        green = blue = 0;
        box.surface->SetColor (box.surface, red, green, blue, alpha);
        box.surface->FillRectangle (box.surface, 0, 0, box.area.w,
                                    box.area.h);

        /*
         * Draw the title box, and the title 
         */
        box.surface->SetColor (box.surface, 0, 0, 0, alpha);
        box.surface->DrawRectangle (box.surface, 0, 0, box.area.w,
                                    box.area.h);
        box.surface->SetFont (box.surface, video.font);
        box.surface->DrawString (box.surface, prompt, -1, box.area.w / 2,
                                 font_height, DSTF_CENTER | DSTF_TOP);

        /*
         * Create a pointer to the SubSurface where the input line is Give
         * it some color. 
         */
        video.primary_surface->GetSubSurface (video.primary_surface,
                                              &input.area, &input.surface);
        input.surface->SetColor (input.surface, 255, 255, 255, 255);
        input.surface->FillRectangle (input.surface, 0, 0, input.area.w,
                                      input.area.h);
        alpha = 255;
        green = blue = red = 0;
        input.surface->SetColor (input.surface, red, green, blue, alpha);
        input.surface->SetFont (input.surface, video.font);

        /*
         * Create a copy of the empty input box 
         */
        desc.width = input.area.w;
        desc.height = input.area.h;
        video.dfb->CreateSurface (video.dfb, &desc, &input.offscreen);
        input.offscreen->Blit (input.offscreen, video.primary_surface,
                               &input.area, 0, 0);

        r_len = _gets_from_input_bar (buf, len, &input, pass);

        video.primary_surface->Blit (video.primary_surface, box.offscreen,
                                     NULL, box.area.x, box.area.y);

        input.surface->Release (input.surface);
        input.offscreen->Release (input.offscreen);
        box.surface->Release (box.surface);
        box.offscreen->Release (box.offscreen);

        return r_len;
}

int
splashy_get_string (char *buf, int len, const char *prompt)
{
        return _get_string (buf, len, prompt, 0);
}

int
splashy_get_password (char *buf, int len, const char *prompt)
{
        return _get_string (buf, len, prompt, 1);
}

/*
 * Initialize the library 
 *  - Sets up config
 *  - Checks it the config is sane
 *  @param const char * file    Path to splashy configfile. If NULL, standard is taken
 *  @param const char * mode    Mode of operation: boot, shutdown, resume, suspend
 *  @return int     Returns <0 for error 0 for succes
 */
int
splashy_init (const char *file, const char *mode)
{
        DEBUG_PRINT ("Entering splashy_init(...,%s)", mode);
        GString *xpath = g_string_new ("");
        const char *cnf_item;
        int i;

        if (!splashy_init_config ((file != NULL ? file : SPL_CONFIG_FILE)))
        {
                ERROR_PRINT (_("libsplashy: No config file found at %s"), file);
                return -1;
        }

        _current_mode = -1;
        for (i = 0; i < NRMODES; i++)
        {
                if (strncmp (valid_modes[i], mode, strlen (valid_modes[i])) ==
                    0)
                {
                        _current_mode = i;
                        DEBUG_PRINT ("We're in mode %s", mode);
                }
        }

        if (_current_mode == -1)
        {
                ERROR_PRINT (_("libsplashy: %s is not a legal mode"), mode);
                return -2;
        }

        g_string_assign (xpath, "/splashy/progressbar/direction/");
        g_string_append (xpath, mode);
        DEBUG_PRINT ("Getting %s", xpath->str);
        if ((cnf_item = splashy_get_config_string (xpath->str)))
        {
                if (strncmp ("forward", cnf_item, 7) == 0)
                        splashy_set_progressbar_forward (TRUE);
                else if (strncmp ("backward", cnf_item, 8) == 0)
                        splashy_set_progressbar_forward (FALSE);
        }

        g_string_assign (xpath, "/splashy/progressbar/visibility/");
        g_string_append (xpath, mode);
        DEBUG_PRINT ("Getting %s", xpath->str);
        if ((cnf_item = splashy_get_config_string (xpath->str)))
        {
                if (strncmp ("y", cnf_item, 1) == 0)
                        splashy_set_progressbar_visible (TRUE);
                else if (strncmp ("n", cnf_item, 1) == 0)
                        splashy_set_progressbar_visible (FALSE);
        }

        g_string_assign (xpath, "/splashy/background/errorimg");
        DEBUG_PRINT ("Getting %s", xpath->str);
        cnf_item = splashy_image_path (xpath->str);
        if (!g_file_test (cnf_item, G_FILE_TEST_IS_REGULAR))
        {
                ERROR_PRINT (_("libsplashy: Could not find error image at %s."),
                             cnf_item);
                return -3;
        }

        g_string_assign (xpath, "/splashy/background/");
        g_string_append (xpath, mode);
        DEBUG_PRINT ("Getting %s", xpath->str);
        cnf_item = splashy_image_path (xpath->str);
        if (!g_file_test (cnf_item, G_FILE_TEST_IS_REGULAR))
        {
                ERROR_PRINT
                        (_("libsplashy: Could not find background image at %s."),
                         cnf_item);
                return -4;
        }

        _current_background = cnf_item;

        g_string_free (xpath, TRUE);

        return TRUE;
}
