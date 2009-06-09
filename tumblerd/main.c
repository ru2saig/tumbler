/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of 
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write to the Free 
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>

#include <dbus/dbus-glib.h>

#include <tumbler/tumbler.h>

#include <tumblerd/tumbler-cache-service.h>
#include <tumblerd/tumbler-manager.h>
#include <tumblerd/tumbler-registry.h>
#include <tumblerd/tumbler-service.h>



int
main (int    argc,
      char **argv)
{
  TumblerProviderFactory *provider_factory;
  DBusGConnection        *connection;
  TumblerRegistry        *registry;
  TumblerManager         *manager;
  TumblerService         *service;
  TumblerCacheService    *cache_service;
  GMainLoop              *main_loop;
  GError                 *error = NULL;
  GList                  *providers;
  GList                  *thumbnailers;
  GList                  *lp;
  GList                  *tp;

  /* set the program name */
  g_set_prgname (G_LOG_DOMAIN);

  /* set the application name. Translators: Don't translate "Tumbler". */
  g_set_application_name (_("Tumbler Thumbnailing Service"));

  /* initialize the GLib type system */
  g_type_init ();

  /* initialize threading system */
  if (!g_thread_supported ())
    g_thread_init (NULL);

  /* try to connect to the D-Bus session bus */
  connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  /* print an error and exit if the connection could not be established */
  if (connection == NULL)
    {
      g_warning (_("Failed to connect to the D-Bus session bus: %s"), error->message);
      g_error_free (error);

      return EXIT_FAILURE;
    }

  /* create the thumbnail cache service */
  cache_service = tumbler_cache_service_new (connection);

  /* try to start the service and exit if that fails */
  if (!tumbler_cache_service_start (cache_service, &error))
    {
      g_warning (_("Failed to start the thumbnail cache service: %s"), error->message);
      g_error_free (error);

      g_object_unref (cache_service);

      dbus_g_connection_unref (connection);

      return EXIT_FAILURE;
    }

  /* create the thumbnailer registry */
  registry = tumbler_registry_new ();

  /* register the built-in pixbuf thumbnailer */
  provider_factory = tumbler_provider_factory_get_default ();
  providers = tumbler_provider_factory_get_providers (provider_factory,
                                                      TUMBLER_TYPE_THUMBNAILER_PROVIDER);
  g_object_unref (provider_factory);

  for (lp = providers; lp != NULL; lp = lp->next)
    {
      thumbnailers = tumbler_thumbnailer_provider_get_thumbnailers (lp->data);

      for (tp = thumbnailers; tp != NULL; tp = tp->next)
        {
          tumbler_registry_add (registry, tp->data);
          g_object_unref (tp->data);
        }

      g_list_free (thumbnailers);
    }

  g_list_foreach (providers, (GFunc) g_object_unref, NULL);
  g_list_free (providers);

  /* try to load specialized thumbnailers and exit if that fails */
  if (!tumbler_registry_load (registry, &error))
    {
      g_warning (_("Failed to load specialized thumbnailers into the registry: %s"),
                 error->message);
      g_error_free (error);

      g_object_unref (registry);
      g_object_unref (cache_service);

      dbus_g_connection_unref (connection);

      return EXIT_FAILURE;
    }

  /* create the thumbnailer manager service */
  manager = tumbler_manager_new (connection, registry);

  /* try to start the service and exit if that fails */
  if (!tumbler_manager_start (manager, &error))
    {
      g_warning (_("Failed to start the thumbnailer manager: %s"), error->message);
      g_error_free (error);

      g_object_unref (manager);
      g_object_unref (registry);
      g_object_unref (cache_service);

      dbus_g_connection_unref (connection);

      return EXIT_FAILURE;
    }

  /* create the generic thumbnailer service */
  service = tumbler_service_new (connection, registry);

  /* try to start the service and exit if that fails */
  if (!tumbler_service_start (service, &error))
    {
      g_warning (_("Failed to start the thumbnailer service: %s"), error->message);
      g_error_free (error);

      g_object_unref (service);
      g_object_unref (manager);
      g_object_unref (registry);
      g_object_unref (cache_service);

      dbus_g_connection_unref (connection);

      return EXIT_FAILURE;
    }

  /* create a new main loop and run it */
  main_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (main_loop);

  /* shut our services down and release all objects */
  g_object_unref (service);
  g_object_unref (manager);
  g_object_unref (registry);
  g_object_unref (cache_service);

  /* disconnect from the D-Bus session bus */
  dbus_g_connection_unref (connection);

  /* we're done, all fine */
  return EXIT_SUCCESS;
}
