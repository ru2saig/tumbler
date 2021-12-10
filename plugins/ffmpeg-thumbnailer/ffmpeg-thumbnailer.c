/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2010 Lionel Le Folgoc <mrpouit@ubuntu.com>
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

#include <math.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libffmpegthumbnailer/videothumbnailerc.h>

#include <tumbler/tumbler.h>

#include <ffmpeg-thumbnailer/ffmpeg-thumbnailer.h>


static void ffmpeg_thumbnailer_finalize (GObject                    *object);
static void ffmpeg_thumbnailer_create   (TumblerAbstractThumbnailer *thumbnailer,
                                         GCancellable               *cancellable,
                                         TumblerFileInfo            *info);



struct _FfmpegThumbnailerClass
{
  TumblerAbstractThumbnailerClass __parent__;
};

struct _FfmpegThumbnailer
{
  TumblerAbstractThumbnailer __parent__;

  video_thumbnailer *video;
};



G_DEFINE_DYNAMIC_TYPE (FfmpegThumbnailer,
                       ffmpeg_thumbnailer,
                       TUMBLER_TYPE_ABSTRACT_THUMBNAILER);



void
ffmpeg_thumbnailer_register (TumblerProviderPlugin *plugin)
{
  ffmpeg_thumbnailer_register_type (G_TYPE_MODULE (plugin));
}



static void
ffmpeg_thumbnailer_class_init (FfmpegThumbnailerClass *klass)
{
  TumblerAbstractThumbnailerClass *abstractthumbnailer_class;
  GObjectClass                    *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = ffmpeg_thumbnailer_finalize;

  abstractthumbnailer_class = TUMBLER_ABSTRACT_THUMBNAILER_CLASS (klass);
  abstractthumbnailer_class->create = ffmpeg_thumbnailer_create;
}



static void
ffmpeg_thumbnailer_class_finalize (FfmpegThumbnailerClass *klass)
{
}



static void
ffmpeg_thumbnailer_init (FfmpegThumbnailer *thumbnailer)
{
  /* initialize libffmpegthumbnailer with default parameters */
  thumbnailer->video = video_thumbnailer_create ();
  thumbnailer->video->seek_percentage = 15;
  thumbnailer->video->overlay_film_strip = 1;
  thumbnailer->video->thumbnail_image_type = Png;
}



static void
ffmpeg_thumbnailer_finalize (GObject *object)
{
  FfmpegThumbnailer *thumbnailer = FFMPEG_THUMBNAILER (object);

  /* release the libffmpegthumbnailer video object */
  video_thumbnailer_destroy (thumbnailer->video);

  (*G_OBJECT_CLASS (ffmpeg_thumbnailer_parent_class)->finalize) (object);
}



static GdkPixbuf *
generate_pixbuf (GdkPixbuf *source,
                 gint       dest_width,
                 gint       dest_height)
{
  gdouble hratio;
  gdouble wratio;
  gint    source_width;
  gint    source_height;

  /* determine the source pixbuf dimensions */
  source_width  = gdk_pixbuf_get_width (source);
  source_height = gdk_pixbuf_get_height (source);

  /* return the same pixbuf if no scaling is required */
  if (source_width <= dest_width && source_height <= dest_height)
    return g_object_ref (source);

  /* determine which axis needs to be scaled down more */
  wratio = (gdouble) source_width / (gdouble) dest_width;
  hratio = (gdouble) source_height / (gdouble) dest_height;

  /* adjust the other axis */
  if (hratio > wratio)
    dest_width = rint (source_width / hratio);
  else
    dest_height = rint (source_height / wratio);

  /* scale the pixbuf down to the desired size */
  return gdk_pixbuf_scale_simple (source, MAX (dest_width, 1), MAX (dest_height, 1),
                                  GDK_INTERP_BILINEAR);
}



static void
ffmpeg_thumbnailer_create (TumblerAbstractThumbnailer *thumbnailer,
                           GCancellable               *cancellable,
                           TumblerFileInfo            *info)
{
  image_data             *v_data;
  GInputStream           *v_stream;
  GdkPixbuf              *v_pixbuf;
  FfmpegThumbnailer      *ffmpeg_thumbnailer = FFMPEG_THUMBNAILER (thumbnailer);
  TumblerThumbnailFlavor *flavor;
  TumblerThumbnail       *thumbnail;
  TumblerImageData        data;
  GdkPixbuf              *pixbuf;
  GFile                  *file;
  GError                 *error = NULL;
  gint                    dest_width;
  gint                    dest_height;
  gchar                  *path;
  const gchar            *uri;

  g_return_if_fail (IS_FFMPEG_THUMBNAILER (thumbnailer));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (TUMBLER_IS_FILE_INFO (info));

  /* do nothing if cancelled */
  if (g_cancellable_is_cancelled (cancellable))
    return;

  /* Check if is a sparse video file */
  if (tumbler_util_guess_is_sparse (info))
  {
    g_debug ("Video file '%s' is probably sparse, skipping\n",
             tumbler_file_info_get_uri (info));
    return;
  }

  /* fetch required info */
  thumbnail = tumbler_file_info_get_thumbnail (info);
  g_assert (thumbnail != NULL);
  flavor = tumbler_thumbnail_get_flavor (thumbnail);
  tumbler_thumbnail_flavor_get_size (flavor, &dest_width, &dest_height);
  g_object_unref (flavor);

  /* prepare the video thumbnailer */
  ffmpeg_thumbnailer->video->thumbnail_size = MAX (dest_width, dest_height);
  v_data = video_thumbnailer_create_image_data ();

  uri = tumbler_file_info_get_uri (info);

  /* get the local absolute path to the source file */
  file = g_file_new_for_uri (uri);
  path = g_file_get_path (file);

  if (path == NULL)
    {
      /* there was an error, emit error signal */
      g_set_error (&error, TUMBLER_ERROR, TUMBLER_ERROR_INVALID_FORMAT,
                   _("Thumbnail could not be inferred from file contents"));
      g_signal_emit_by_name (thumbnailer, "error", uri, error->code, error->message);
                             g_error_free (error);

      /* clean up */
      g_object_unref (file);
      g_object_unref (thumbnail);
      video_thumbnailer_destroy_image_data (v_data);
      return;
    }
  g_object_unref (file);

  /* try to generate a thumbnail */
  if (video_thumbnailer_generate_thumbnail_to_buffer (ffmpeg_thumbnailer->video, path, v_data) != 0)
    {
      /* there was an error, emit error signal */
      g_set_error (&error, TUMBLER_ERROR, TUMBLER_ERROR_INVALID_FORMAT,
                   _("Thumbnail could not be inferred from file contents"));
      g_signal_emit_by_name (thumbnailer, "error", uri, error->code, error->message);
      g_error_free (error);

      /* clean up */
      g_free (path);
      g_object_unref (thumbnail);
      video_thumbnailer_destroy_image_data (v_data);
      return;
    }
    g_free (path);

  v_stream = g_memory_input_stream_new_from_data (v_data->image_data_ptr,
                                                  v_data->image_data_size, NULL);

  if (v_stream == NULL)
    {
      /* there was an error, emit error signal */
      g_set_error (&error, TUMBLER_ERROR, TUMBLER_ERROR_INVALID_FORMAT,
                   _("Thumbnail could not be inferred from file contents"));
      g_signal_emit_by_name (thumbnailer, "error", uri, error->code, error->message);
      g_error_free (error);

      /* clean up */
      g_object_unref (thumbnail);
      video_thumbnailer_destroy_image_data (v_data);
      return;
    }

  v_pixbuf = gdk_pixbuf_new_from_stream (v_stream, cancellable, &error);

  g_object_unref (v_stream);
  video_thumbnailer_destroy_image_data (v_data);

  if (v_pixbuf == NULL)
    {
      /* emit an error signal */
      g_signal_emit_by_name (thumbnailer, "error", uri, error->code, error->message);
      g_error_free (error);

      /* clean up */
      g_object_unref (thumbnail);

      return;
    }

  /* generate a valid thumbnail */
  pixbuf = generate_pixbuf (v_pixbuf, dest_width, dest_height);

  g_assert (pixbuf != NULL);

  /* compose the image data */
  data.data = gdk_pixbuf_get_pixels (pixbuf);
  data.has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
  data.bits_per_sample = gdk_pixbuf_get_bits_per_sample (pixbuf);
  data.width = gdk_pixbuf_get_width (pixbuf);
  data.height = gdk_pixbuf_get_height (pixbuf);
  data.rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  data.colorspace = (TumblerColorspace) gdk_pixbuf_get_colorspace (pixbuf);

  /* save the thumbnail */
  tumbler_thumbnail_save_image_data (thumbnail, &data,
                                     tumbler_file_info_get_mtime (info),
                                     NULL, &error);

  /* check if there was an error */
  if (error != NULL)
    {
      /* emit an error signal */
      g_signal_emit_by_name (thumbnailer, "error", uri, error->code, error->message);
      g_error_free (error);
    }
  else
    {
      /* otherwise, the thumbnail is now ready */
      g_signal_emit_by_name (thumbnailer, "ready", uri);
    }

  /* clean up */
  g_object_unref (thumbnail);
  g_object_unref (pixbuf);
  g_object_unref (v_pixbuf);
}
