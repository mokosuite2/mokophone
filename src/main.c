/*
 * MokoPhone
 * Entry point
 * Copyright (C) 2009-2010 Daniele Ricci <daniele.athome@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <mokosuite/utils/utils.h>
#include <mokosuite/utils/notify.h>
#include <mokosuite/utils/remote-config-service.h>
#include <mokosuite/utils/dbus.h>
#include <mokosuite/ui/gui.h>
#include <mokosuite/pim/pim.h>
#include <mokosuite/pim/contactsdb.h>
#include <freesmartphone-glib/freesmartphone-glib.h>
#include <phone-utils.h>
#include <libnotify/notify.h>

#include "globals.h"
#include "gsm.h"
#include "sound.h"
#include "phonewin.h"
#include "callwin.h"

#define MOKOPHONE_NAME               "org.mokosuite.phone"
#define MOKOPHONE_CONFIG_PATH        MOKOSUITE_DBUS_PATH "/Phone/Config"

// default log domain
int _log_dom = -1;

// configuration service
RemoteConfigService* phone_config = NULL;

int main(int argc, char* argv[])
{
    // TODO: initialize Intl
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    // initialize log
    eina_init();
    _log_dom = eina_log_domain_register(PACKAGE, EINA_COLOR_CYAN);
    eina_log_domain_level_set(PACKAGE, LOG_LEVEL);

    EINA_LOG_INFO("%s version %s", PACKAGE_NAME, VERSION);

    /* other things */
    mokosuite_utils_init();
    mokosuite_pim_init();
    mokosuite_ui_init(argc, argv);
    phone_utils_init();

    EINA_LOG_DBG("Loading data from %s", MOKOPHONE_DATADIR);
    elm_theme_extension_add(NULL, MOKOPHONE_DATADIR "/theme.edj");

    // dbus name
    DBusGConnection* session_bus = dbus_session_bus();
    if (!session_bus || !dbus_request_name(session_bus, MOKOPHONE_NAME))
        return EXIT_FAILURE;

    // settings database
    char* cfg_file = g_strdup_printf("%s/.config/mokosuite/%s.conf", g_get_home_dir(), PACKAGE);
    phone_config = remote_config_service_new(session_bus,
        MOKOPHONE_CONFIG_PATH,
        cfg_file);
    g_free(cfg_file);

    notify_init(PACKAGE_NAME);

    // TODO phone service

    /* inizializza il database dei contatti */
    char* db_path = g_strdup_printf("%s/.config/mokosuite/contacts.db", g_get_home_dir());
    contactsdb_init(db_path);
    g_free(db_path);

    // gsm init
    gsm_init();

    sound_init();

    phone_win_init();
    phone_call_win_init();

    // TEST
    phone_win_activate(SECTION_PHONE, FALSE);

    elm_run();
    elm_shutdown();

    notify_uninit();

    return EXIT_SUCCESS;
}
