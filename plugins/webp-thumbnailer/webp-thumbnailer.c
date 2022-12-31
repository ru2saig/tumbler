/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
 
#include <sys/types.h>
#include <fcntl.h>
#include <memory.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>

#include <tumbler/tumbler.h>

#include <webp/decode.h>                /* from the libwebp library */
#include <gdk-pixbuf/gdk-pixbuf.h>      /* for pixbuf handling */

#include "webp-thumbnailer.h"


static void webp_thumbnailer_create (TumblerAbstractThumbnailer *thumbnailer,
                                    GCancellable               *cancellable,
                                    TumblerFileInfo            *info);



struct _WebpThumbnailerClass
{
  TumblerAbstractThumbnailerClass __parent__;
};

struct _WebpThumbnailer
{
  TumblerAbstractThumbnailer __parent__;
};



G_DEFINE_DYNAMIC_TYPE (WebpThumbnailer,
                       webp_thumbnailer,
                       TUMBLER_TYPE_ABSTRACT_THUMBNAILER);



void
webp_thumbnailer_register (TumblerProviderPlugin *plugin)
{
  webp_thumbnailer_register_type (G_TYPE_MODULE (plugin));
}



static void
webp_thumbnailer_class_init (WebpThumbnailerClass *klass)
{
  TumblerAbstractThumbnailerClass *abstractthumbnailer_class;

  abstractthumbnailer_class = TUMBLER_ABSTRACT_THUMBNAILER_CLASS (klass);
  abstractthumbnailer_class->create = webp_thumbnailer_create;
}



static void
webp_thumbnailer_class_finalize (WebpThumbnailerClass *klass)
{
}



static void
webp_thumbnailer_init (WebpThumbnailer *thumbnailer)
{
}



/* create the thumbnail - based on: 
    https://github.com/aruiz/webp-pixbuf-loader/blob/master/io-webp.c */
    
static void
destroy_data (guchar *pixels, gpointer data)
{
        g_free (pixels);
}
    
    /* Shared library entry point */
static GdkPixbuf *
gdk_pixbuf__webp_image_load (FILE *f, GError **error)
{
        GdkPixbuf * volatile pixbuf = NULL;
        guint32 data_size;
        guint8 *out;
        gint w, h, ok;
        gpointer data;
        WebPBitstreamFeatures features;
        gboolean use_alpha = TRUE;

        /* Get data size */
        fseek (f, 0, SEEK_END);
        data_size = ftell(f);
        fseek (f, 0, SEEK_SET);

        /* Get data */
        data = g_malloc (data_size);
        ok = (fread (data, data_size, 1, f) == 1);
        if (!ok) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_FAILED,
                             "Failed to read file");
                return FALSE;
        }

        /* Take the safe route and only disable the alpha channel when
           we're sure that there is not any. */
        if (WebPGetFeatures (data, data_size, &features) == VP8_STATUS_OK
            && features.has_alpha == FALSE) {
                use_alpha = FALSE;
        }

        if (use_alpha) {
                out = WebPDecodeRGBA (data, data_size, &w, &h);
        } else {
                out = WebPDecodeRGB (data, data_size, &w, &h);
        }
        g_free (data);

        if (!out) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_FAILED,
                             "Cannot create WebP decoder.");
                return FALSE;
        }

        pixbuf = gdk_pixbuf_new_from_data ((const guchar *)out,
                                           GDK_COLORSPACE_RGB,
                                           use_alpha,
                                           8,
                                           w, h,
                                           w * (use_alpha ? 4 : 3),
                                           destroy_data,
                                           NULL);

        if (!pixbuf) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_FAILED,
                             "Failed to decode image");
                return FALSE;
        }
        return pixbuf;
}


static GdkPixbuf*
scale_pixbuf (GdkPixbuf *source,
              gint       dest_width,
              gint       dest_height)
{
  gdouble wratio;
  gdouble hratio;
  gint    source_width;
  gint    source_height;

  /* determine source pixbuf dimensions */
  source_width  = gdk_pixbuf_get_width  (source);
  source_height = gdk_pixbuf_get_height (source);

  /* don't do anything if there is no need to resize */
  if (source_width <= dest_width && source_height <= dest_height)
    return g_object_ref (source);

  /* determine which axis needs to be scaled down more */
  wratio = (gdouble) source_width  / (gdouble) dest_width;
  hratio = (gdouble) source_height / (gdouble) dest_height;

  /* adjust the other axis */
  if (hratio > wratio)
    dest_width = rint (source_width / hratio);
  else
    dest_height = rint (source_height / wratio);

  /* scale the pixbuf down to the desired size */
  return gdk_pixbuf_scale_simple (source, MAX (dest_width, 1),
                                  MAX (dest_height, 1),
                                  GDK_INTERP_BILINEAR);
}


static void
webp_thumbnailer_create (TumblerAbstractThumbnailer *thumbnailer,
                        GCancellable               *cancellable,
                        TumblerFileInfo            *info)
{
  TumblerThumbnailFlavor *flavor;
  TumblerImageData        data;
  TumblerThumbnail       *thumbnail;
  const gchar            *uri;
  gchar                  *path;
  GdkPixbuf              *pixbuf = NULL;
  GError                 *error = NULL;
  GFile                  *file;
  gint                    height;
  gint                    width;
  GdkPixbuf              *scaled;
  FILE                   *fp;

  g_return_if_fail (IS_WEBP_THUMBNAILER (thumbnailer));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (TUMBLER_IS_FILE_INFO (info));

  /* do nothing if cancelled */
  if (g_cancellable_is_cancelled (cancellable))
    return;

  uri = tumbler_file_info_get_uri (info);
  file = g_file_new_for_uri (uri);
  
  thumbnail = tumbler_file_info_get_thumbnail (info);
  g_assert (thumbnail != NULL);

  flavor = tumbler_thumbnail_get_flavor (thumbnail);
  g_assert (flavor != NULL);
  
  tumbler_thumbnail_flavor_get_size (flavor, &width, &height);
  
  /* load the webp file into a pixbuf */
  path = g_file_get_path (file);
  if (path != NULL && g_path_is_absolute (path))
    {
        /* create FILE * */
        fp = fopen(path, "r");
        if (fp)
        {
            pixbuf = gdk_pixbuf__webp_image_load(fp, &error);
            if (pixbuf == NULL)
            {
              g_set_error_literal (&error, TUMBLER_ERROR, TUMBLER_ERROR_NO_CONTENT,
                                   _("Thumbnail could not be inferred from file contents"));
            }
        fclose(fp);
        }
    }
  else
    {
      g_set_error_literal (&error, TUMBLER_ERROR, TUMBLER_ERROR_UNSUPPORTED,
                           _("Only local files are supported"));
    }

  g_free (path);
  g_object_unref (file);

  if (pixbuf != NULL)
    {
      scaled = scale_pixbuf (pixbuf, width, height);
      g_object_unref (pixbuf);
      pixbuf = scaled;

      data.data = gdk_pixbuf_get_pixels (pixbuf);
      data.has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
      data.bits_per_sample = gdk_pixbuf_get_bits_per_sample (pixbuf);
      data.width = gdk_pixbuf_get_width (pixbuf);
      data.height = gdk_pixbuf_get_height (pixbuf);
      data.rowstride = gdk_pixbuf_get_rowstride (pixbuf);
      data.colorspace = (TumblerColorspace) gdk_pixbuf_get_colorspace (pixbuf);

      tumbler_thumbnail_save_image_data (thumbnail, &data,
                                         tumbler_file_info_get_mtime (info),
                                         NULL, &error);
    }

  if (error != NULL)
    {
      g_signal_emit_by_name (thumbnailer, "error", uri, error->code, error->message);
      g_error_free (error);
    }
  else
    {
      g_signal_emit_by_name (thumbnailer, "ready", uri);
      g_object_unref (pixbuf);
    }
  g_object_unref (flavor);
  g_object_unref (thumbnail);
  
}
