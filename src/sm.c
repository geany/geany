/*
 *      sm.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2009 Eugene Arshinov <earshinov(at)gmail(dot)com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $Id$
 */

/*
 * Changes by Dimitar Zhekov <dimitar(dot)zhekov(at)gmail(dot)com>
 */

/*
 * @file sm.c
 * Provides X session management protocol (XSMP) support using libSM library.
 *
 * In order to support XSMP, we have to support Inter-Client Exchange
 * Protocol (ICE). This file takes care of the latter too.
 *
 * Typical usage:
 * @c sm_init() is called when Geany is starting.
 * @c sm_discard() is called on normal (non-sm) quit.
 * @c sm_finalize() is called when Geany is quitting.
 *
 * According to libSM documentation, client should retain the same ID after
 * it is restarted. The main module (@c main.c) maintains "--libsm-client-id"
 * command-line option and passes the specified value (if any) to @c sm_init().
 */

/*
 * As libSM is not available on Windows,
 * it is safe enough to use POSIX-specific things here.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_LIBSM

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <X11/SM/SMlib.h>
#include <X11/ICE/ICElib.h>

#include "geany.h"
#include "main.h"	/* for cl_options, main_status */
#include "ui_utils.h"	/* for main_widgets */
#include "document.h"	/* for unsaved documents */
#include "utils.h"	/* for option reverse parsing */
#include "keyfile.h"	/* for configuration save */


static void ice_connection_watch(IceConn icecon, IcePointer client_data, Bool opening,
	IcePointer *watch_data);
static gboolean ice_iochannel_watch(GIOChannel *source, GIOCondition condition, gpointer data);

static SmcConn sm_connect(const char *libsm_client_id);
static void sm_set_props(const char *argv0);
static void sm_prevent_interaction(gboolean prevent);
static void sm_finish_save(SmcConn smcon);

static void sm_save_yourself_callback(SmcConn smcon, SmPointer client_data,
	int save_type, Bool shutdown, int interact_style, Bool fast);
static void sm_interact_callback(SmcConn smcon, SmPointer client_data);
static void sm_save_complete_callback(SmcConn smcon, SmPointer client_data);
static void sm_shutdown_cancelled_callback(SmcConn smcon, SmPointer client_data);
static void sm_die_callback(SmcConn smcon, SmPointer client_data);


/* LibSM connection object initialized in @c sm_init() and used in @c sm_finalize(). */
static SmcConn smc_conn = 0;

/* LibSM new client id. Initialized by @c sm_init() and used in many places. */
static char *new_client_id = NULL;

/* The save_type and shutdown values received by the last save yourself callback. */
static int sm_last_save_type;
static Bool sm_last_shutdown;
/* Whether the last save yourself request was completed and signalled as done. */
static gboolean sm_last_save_done;

/* To check for improper Gnome/Xfce xsmp implementations */
char *sm_vendor = NULL;

#endif


/*
 * @name Exported functions
 * @{
 */

/*
 * Initialize XSMP support.
 *
 * @param argv0             Value of @c argv[0] used to define Geany's restart command.
 * @param libsm_client_id   Client-ID specified with "--libsm-client-id" command line
 *                          option or @c NULL if the option was not passed.
 *
 * This function connects to the session manager using @c sm_connect(). If
 * everything is successful, it stores libSM connection object and the new session id
 * in the global variables @c smcon and @c new_client_id, and calls @c sm_set_props().
 *
 * This function should be called on Geany startup, before the main window is realized.
 *
 * When Geany is compiled without XSMP support, this function is a no-op.
 */
void sm_init(const char *argv0, const char *libsm_client_id)
{
	#ifdef HAVE_LIBSM
	/* This function should be called once */
	g_assert(!smc_conn);
	if (smc_conn)
		return;

	smc_conn = sm_connect(libsm_client_id);
	if (smc_conn)
	{
		sm_vendor = SmcVendor(smc_conn);
		gdk_set_sm_client_id(new_client_id);
		sm_set_props(argv0);
	}
	#endif
}


/*
 * Remove the saved session file.
 *
 * Should be called on normal (non-sm) Geany quit, since GSM does not execute
 * the discard command. Ignored in other managers to preserve permanent sessions.
 *
 * When Geany is compiled without XSMP support, this function is a no-op.
 */
void sm_discard(void)
{
	#ifdef HAVE_LIBSM
	if (smc_conn && g_str_has_prefix(sm_vendor, "Gnome"))
	{
		gchar *configfile = configuration_name(new_client_id);
		g_unlink(configfile);
		g_free(configfile);
	}
	#endif
}


/*
 * Perform cleanup.
 *
 * Call this function when XSMP support is no longer needed. In fact it is
 * called when Geany is quitting.
 *
 * When Geany is compiled without XSMP support, this function is a no-op.
 */
void sm_finalize(void)
{
	#ifdef HAVE_LIBSM
	if (smc_conn)
	{
		free(sm_vendor);
		free(new_client_id);
		SmcCloseConnection(smc_conn, 0, NULL);
		smc_conn = 0;
	}
	#endif
}


/* @} */

#ifdef HAVE_LIBSM

/*
 * @name ICE support
 * @{
 */

/*
 * ICE connection watcher used to attach a GIOChannel to each ICE connection
 * so that this connection can be handled in GTK main loop.
 *
 * @param icecon        ICE connection.
 * @param client_data   Client data specified in @c IceAddConnectionWatch() function call.
 *                      Currently it is not used.
 * @param opening       Whether @c icecon is opening or closing.
 * @param watch_data    A piece of data that can be set when @c icecon is opening and
 *                      read when it is closing. We store GIOChannel watcher ID here.
 *
 * We attach @c ice_iochannel_watch GIOChannel watcher to every created GIOChannel
 * in order to handle messages. This is how session manager communicates to Geany.
 *
 * @see sm_connect()
 */
static void ice_connection_watch(IceConn icecon, IcePointer client_data,
		Bool opening, IcePointer *watch_data)
{
	guint input_id;

	if (opening)
	{
		GIOChannel *channel = g_io_channel_unix_new(IceConnectionNumber(icecon));

		input_id = g_io_add_watch(channel, G_IO_IN | G_IO_HUP | G_IO_ERR,
			ice_iochannel_watch, icecon);
		g_io_channel_unref(channel);
		*watch_data = (IcePointer) GUINT_TO_POINTER(input_id);
	}
	else
	{
		input_id = GPOINTER_TO_UINT((gpointer) *watch_data);
		g_source_remove(input_id);
	}
}


/*
 * A watcher attached to a GIOChannel corresponding to an ICE connection.
 *
 * @param source   A GIOChannel corresponding to an ICE connection.
 * @param condition
 * @param data     Client data specified in @c g_io_add_watch() function call.
 *                 An ICE connection object (of type @c IceConn) is stored here.
 * @return         Return FALSE to remove the GIOChannel.
 *
 * This function calls @c IceProcessMessages causing an appropriate libSM
 * callback to be invoked.
 *
 * @see ice_connection_watch()
 */
static gboolean ice_iochannel_watch(GIOChannel *source, GIOCondition condition, gpointer data)
{
	IceConn icecon = (IceConn) data;
	IceProcessMessages(icecon, NULL, NULL);
	return TRUE;
}


/*
 * @}
 * @name libSM support implementation
 * @{
 */

/*
 * Connect to the session manager.
 *
 * @param libsm_client_id      LibSM client ID saved from the previous session,
 *                             or @c NULL if there was no previous session.
 * @return libSM connection object or @c 0 if connection to the session manager fails.
 */
static SmcConn sm_connect(const char *libsm_client_id)
{
	static const SmcCallbacks callbacks = {
		{ sm_save_yourself_callback, NULL },
		{ sm_die_callback, NULL },
		{ sm_save_complete_callback, NULL },
		{ sm_shutdown_cancelled_callback, NULL } };

	gchar err[256] = "";
	SmcConn smcon;

	if (!g_getenv("SESSION_MANAGER"))
		return 0;

	IceAddConnectionWatch(ice_connection_watch, NULL);

	smcon = SmcOpenConnection(NULL, NULL,
		SmProtoMajor, SmProtoMinor,
		SmcSaveYourselfProcMask |
		SmcDieProcMask |
		SmcSaveCompleteProcMask |
		SmcShutdownCancelledProcMask,
		(SmcCallbacks *) &callbacks,
		(char *) libsm_client_id, &new_client_id,
		sizeof err, err);

	if (!smcon)
	{
		g_warning("While connecting to session manager:\n%s.", err);
		IceRemoveConnectionWatch(ice_connection_watch, NULL);
		return 0;
	}

	return smcon;
}


/*
 * Set the libSM properties.
 *
 * @param argv0             Value of @c argv[0].
 */
static void sm_set_props(const char *argv0)
{
	char *executable_path;
	gchar *curdir = g_get_current_dir();
	const gchar *username = g_get_user_name();
	char *client_id_arg = g_strconcat("--libsm-client-id=", new_client_id, NULL);
	gchar *configfile = configuration_name(new_client_id);

	SmPropValue curdir_val = { strlen(curdir), curdir};
	SmPropValue userid_val, program_val, client_id_val;
	SmPropValue discard_val[3] = { { 2, "rm" }, { 2, "-f" },
		{ strlen(configfile), (char *) configfile } };

	SmProp program_prop = { SmProgram, SmARRAY8, 1, &program_val };
	SmProp userid_prop = { SmUserID, SmARRAY8, 1, &userid_val };
	SmProp curdir_prop = { SmCurrentDirectory, SmARRAY8, 1, &curdir_val };
	SmProp restart_prop, clone_prop;
	SmProp discard_prop = { SmDiscardCommand, SmLISTofARRAY8, 3, discard_val };

	SmProp *props[4] = { &program_prop, &curdir_prop, &restart_prop, &discard_prop };

	int i, arg_max, argc;
	SmPropValue *arg_val;

	/* -- end of declarations -- */

	/* GSM does not work with relative executable names, try to obtain absolute. */
	if (g_path_is_absolute(argv0) || (executable_path = tm_get_real_path(argv0)) == NULL)
		executable_path = g_strdup(argv0);

	program_val.length = strlen(executable_path);
	program_val.value = executable_path;
	client_id_val.length = strlen(client_id_arg);
	client_id_val.value = client_id_arg;

	for (i = 0, arg_max = 2; optentries[i].long_name; i++)
		if (optentries_aux[i].persist_upon_restart)
			arg_max++;

	arg_val = g_new(SmPropValue, arg_max);
	arg_val[0] = program_val;
	arg_val[1] = client_id_val;

	for (i = 0, argc = 2; optentries[i].long_name; i++)
	{
		if (optentries_aux[i].persist_upon_restart)
		{
			char *option = utils_option_entry_reverse_parse(&optentries[i]);

			if (option)
			{
				arg_val[argc].length = strlen(option);
				arg_val[argc].value = option;
				argc++;
			}
		}
	}

	restart_prop.name = SmRestartCommand;
	restart_prop.type = SmLISTofARRAY8;
	restart_prop.num_vals = argc;
	restart_prop.vals = arg_val;

	SmcSetProperties(smc_conn, 4, props);

	arg_val[1] = program_val;
	clone_prop.name = SmCloneCommand;
	clone_prop.type = SmLISTofARRAY8;
	clone_prop.num_vals = argc - 1;
	clone_prop.vals = arg_val + 1;
	props[0] = &clone_prop;
	SmcSetProperties(smc_conn, 1, props);

	if (username)
	{
		userid_val.length = strlen(username);
		userid_val.value = (char *) username;
		props[0] = &userid_prop;
		SmcSetProperties(smc_conn, 1, props);
	}

	g_free(executable_path);
	g_free(curdir);
	g_free(client_id_arg);
	g_free(configfile);
	for (i = 2; i < argc; i++)
		g_free(arg_val[i].value);
	g_free(arg_val);
}


/*
 * @}
 * @name Utility functions used by the libSM callbacks.
 * @{
 */

/*
 * Prevent/allow user interaction with Geany if shutting down.
 *
 * @param prevent  TRUE to prevent interaction, FALSE to allow.
 *
 * The new 6.9/7.0 XSMP specification is not very clear whether interaction should
 * be prevented if not shutting down. But the rationale is to prevent anything the
 * user does after the save being lost, which can only happen if shutting down.
 */
static void sm_prevent_interaction(gboolean prevent)
{
	if (sm_last_shutdown)
	{
		/* Disabling the main window takes time, nobody does it,
		   and that is window/session manager's job anyway. */
		/*gtk_widget_set_sensitive(main_widgets.window, prevent == FALSE);*/
		main_status.prevent_interaction = prevent;
	}
}


/*
 * Finish an interactive or non-interactive save yourself.
 *
 * @param smcon            LibSM connection object.
 */
static void sm_finish_save(SmcConn smcon)
{
	if (sm_last_save_type == SmSaveLocal || sm_last_save_type == SmSaveBoth)
		configuration_save(new_client_id);

	/* SaveYourselfDone() may take some time, while the assignment
	   is a single instruction. So it's safer in that order. */
	sm_last_save_done = TRUE;
	SmcSaveYourselfDone(smcon, TRUE);
	sm_prevent_interaction(TRUE);
}


/*
 * @}
 * @name libSM callbacks
 * @{
 */

/*
 * "Save Yourself" callback.
 *
 * @param smcon            LibSM connection object.
 * @param client_data      Client data specified when the callback was registered.
 *                         Currently it is not used.
 * @param save_type        Specifies the type of information that should be saved.
 * @param shutdown         Specifies if a shutdown is taking place.
 * @param interact_style   The type of interaction allowed with the user.
 * @param fast             If True, the client should save its state as quickly as possible.
 *
 * See libSM documentation for more details.
 *
 * If there are any unsaved documents and interaction with the user is allowed,
 * we make an "Interact Request", so that the session manager sends us an "Interact"
 * message, and return.
 *
 * Otherwise, we save the configuration and are ready to handle a "Save Complete",
 * "Shutdown Cancelled" or "Die" message.
 *
 * @see sm_interact_callback()
 * @see sm_save_complete_callback()
 * @see sm_shutdown_cancelled_callback()
 * @see sm_die_callback()
 */
static void sm_save_yourself_callback(SmcConn smcon, SmPointer client_data,
		int save_type, Bool shutdown, int interact_style, Bool fast)
{
	/* Main window not realized means some startup dialog loop,
	   so the configuration/interface are not initialized yet. */
	if (main_status.main_window_realized)
	{
		sm_last_save_type = save_type;
		sm_last_shutdown = shutdown;
		sm_last_save_done = FALSE;

		if (!(save_type == SmSaveGlobal || save_type == SmSaveBoth) &&
			interact_style == SmInteractStyleAny && document_any_unsaved() &&
			!g_str_has_prefix(sm_vendor, "xfce") &&
			SmcInteractRequest(smcon, SmDialogNormal, sm_interact_callback, NULL))
		{
			return; /* sm_interact_callback() takes over */
		}

		sm_finish_save(smcon);
	}
}


/*
 * "Interact" callback.
 *
 * @param smcon         LibSM connection object.
 * @param client_data   Client data specified when the callback was registered.
 *                      Currently it is not used.
 *
 * See libSM documentation for more details.
 *
 * The session manager sends us an "Interact" message after we make an
 * "Interact Request" in @c sm_save_yourself_callback(). Here we are allowed to
 * interact with the user, so we ask whether to the save changed documents (the
 * user also can cancel the shutdown), and save the configuration. After that we
 * are ready to handle a "Save Complete", "Shutdown Cancelled" or "Die" message.
 *
 * @see sm_shutdown_cancelled_callback()
 * @see sm_die_callback()
 */
static void sm_interact_callback(SmcConn smcon, SmPointer client_data)
{
	gboolean unsaved = !document_prompt_for_unsaved();
	SmcInteractDone(smcon, sm_last_shutdown && unsaved);
	sm_finish_save(smcon);
}


/*
 * "Save Complete" callback.
 *
 * @param smcon         LibSM connection object.
 * @param client_data   Client data specified when the callback was registered.
 *                      Currently it is not used.
 *
 * See libSM documentation for more details.
 *
 * Unfreeze and continue normally.
 */
static void sm_save_complete_callback(SmcConn smcon, SmPointer client_data)
{
	sm_prevent_interaction(FALSE);
}


/*
 * "Shutdown Cancelled" callback.
 *
 * @param smcon         LibSM connection object.
 * @param client_data   Client data specified when the callback was registered.
 *                      Currently it is not used.
 *
 * See libSM documentation for more details.
 *
 * Signal save yourself if needed and unfreeze.
 */
static void sm_shutdown_cancelled_callback(SmcConn smcon, SmPointer client_data)
{
	if (!sm_last_save_done)
		SmcSaveYourselfDone(smcon, FALSE);
	sm_prevent_interaction(FALSE);
}


/*
 * "Die" callback.
 *
 * @param smcon         LibSM connection object.
 * @param client_data   Client data specified when the callback was registered.
 *                      Currently it is not used.
 *
 * See libSM documentation for more details.
 *
 * The session manager asks us to quit Geany and we do it. When quitting, the
 * main module (@c main.c) will call @c sm_finalize() where we will close the
 * connection to the session manager.
 */
static void sm_die_callback(SmcConn smcon, SmPointer client_data)
{
	main_status.quitting = TRUE;
	main_quit();
}


/* @} */

#endif
