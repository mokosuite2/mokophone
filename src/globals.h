/*
 * MokoPhone
 * Globals definitions
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

#ifndef __GLOBALS_H
#define __GLOBALS_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <Elementary.h>
#include <libintl.h>
#include <stdio.h>
#include <stdlib.h>
#include <mokosuite/utils/remote-config-service.h>

// default log domain
#undef EINA_LOG_DOMAIN_DEFAULT
#define EINA_LOG_DOMAIN_DEFAULT _log_dom
extern int _log_dom;

#ifdef DEBUG
#define LOG_LEVEL   EINA_LOG_LEVEL_DBG
#else
#define LOG_LEVEL   EINA_LOG_LEVEL_INFO
#endif

// this should work...
#define _(string)   gettext(string)


#define MOKOPHONE_SYSCONFDIR     SYSCONFDIR "/mokosuite"
#define MOKOPHONE_DATADIR        DATADIR "/mokosuite/messages"

extern RemoteConfigService* phone_config;


#endif  /* __GLOBALS_H */
