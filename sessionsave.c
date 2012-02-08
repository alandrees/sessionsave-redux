/* SessionSave by Matt Perry
 * Works for purple 2.0.0
 * Plugin to save your sessions and restore them next time you open purple.
 */

#include "internal.h"

#include "account.h"
#include "conversation.h"
#include "core.h"
#include "debug.h"
#include "log.h"
#include "signals.h"
#include "prefs.h"
#include "util.h"
#include "version.h"

#include "plugin.h"

#include "gtkconv.h"
#include "gtkblist.h"
#include "gtkplugin.h"

#include "gtk/gtk.h"

#define SESSIONSAVE_PLUGIN_ID "core-sessionsave"
#define SESSIONSAVE_VERSION VERSION ".1"

typedef struct {
	struct {
		char* name;
		char* protocol;
	} account;  /* which account has the convo open */

	PurpleConversationType type;

	char* name;  /* buddy/chat/whatever name of the convo */

	char* html;  /* html content of the conv window */

	gboolean restored;  /* not saved - indicates this conversation was already restored this session, so don't re-restore */
      
        int x; /* x coord of window to restore */
        
        int y; /* y coord of the window to restore */

        int w; /* width of the window to restore */
   
        int h; /* height of the window to restore */

        
} SavedConversation;

GList* saved_conversations = NULL;
PurplePlugin* gplugin = NULL;
static gboolean restoring_session = FALSE;

/*
 * Utility functions
 */
static PurpleConversationType conversation_char_to_type(char ch)
{
	switch (ch) {
	case 'i': return PURPLE_CONV_TYPE_IM;
	case 'c': return PURPLE_CONV_TYPE_CHAT;
	case 'u': return PURPLE_CONV_TYPE_UNKNOWN;
	default:
		purple_debug_warning("sessionsave", "unknown conversation type char.\n");
		return PURPLE_CONV_TYPE_UNKNOWN;
	}
}

static char conversation_type_to_char(PurpleConversationType type)
{
	switch (type) {
	case PURPLE_CONV_TYPE_IM: return 'i';
	case PURPLE_CONV_TYPE_CHAT: return 'c';
	default: return 'u';
	}
}

/*
 * API for manipulating a SavedConversation
 */

static SavedConversation* saved_conv_new(PurpleConversation* conv)
{
	PurpleAccount* account = purple_conversation_get_account(conv);
	SavedConversation* sconv;
	int x, y, w, h;
	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(conv != NULL, NULL);

	sconv = g_new(SavedConversation, 1);
	sconv->account.name = g_strdup(purple_account_get_username(account));
	sconv->account.protocol = g_strdup(purple_account_get_protocol_id(account));
	sconv->type = purple_conversation_get_type(conv);
	sconv->name = g_strdup(purple_conversation_get_name(conv));
	gtk_window_get_position (GTK_WINDOW(PIDGIN_CONVERSATION(conv)->win->window),&x, &y);
	gtk_window_get_size (GTK_WINDOW(PIDGIN_CONVERSATION(conv)->win->window), &w, &h);
	sconv->x = x;
	sconv->y = y;
	sconv->w = w;
	sconv->h = h;
	sconv->html = g_strdup("");
	sconv->restored = TRUE;  /* This conversation already exists, don't restore it this session. */

	return sconv;
}

static SavedConversation* saved_conv_new_with_data(const char* account_name, const char* account_protocol,
						   int type_id, const char* name, int x, int y, int w, int h, const char* html)
{
	SavedConversation* sconv;

	sconv = g_new(SavedConversation, 1);
	sconv->account.name = g_strdup(account_name);
	sconv->account.protocol = g_strdup(account_protocol);
	sconv->type = conversation_char_to_type(type_id);
	sconv->name = g_strdup(name);
	sconv->x = x;
	sconv->y = y;
	sconv->w = w;
	sconv->h = h;
	sconv->html = g_strdup(html);
	sconv->restored = FALSE;

	return sconv;
}

static void saved_conv_delete(SavedConversation* sconv)
{
	g_return_if_fail(sconv != NULL);

	g_free(sconv->html);
	g_free(sconv->account.name);
	g_free(sconv->account.protocol);
	g_free(sconv->name);
	g_free(sconv);
}

static gint saved_convo_cmp(gconstpointer p1, gconstpointer p2)
{
	SavedConversation* sconv1 = (SavedConversation*)p1;
	SavedConversation* sconv2 = (SavedConversation*)p2;
	gint cmp;

	if (!sconv1) return -1;
	if (!sconv2) return 1;

	if ((cmp = g_strcasecmp(sconv1->account.name, sconv2->account.name)) != 0)
		return cmp;
	if ((cmp = g_strcasecmp(sconv1->account.protocol, sconv2->account.protocol)) != 0)
		return cmp;
	if ((cmp = (sconv2->type - sconv1->type)) != 0)
		return cmp;
	if ((cmp = g_strcasecmp(sconv1->name, sconv2->name)) != 0)
		return cmp;

	/* they match */
	return 0;
}

static PurpleAccount* saved_conv_get_account(SavedConversation* sconv)
{
	return purple_accounts_find(sconv->account.name, sconv->account.protocol);
}

static void saved_conv_restore(SavedConversation* sconv)
{
	PurpleAccount* account = saved_conv_get_account(sconv);
	PurpleConversation* conv;
		
	if (sconv->restored)
		return;

	purple_debug_info("sessionsave", "restoring: %s, %s\n", sconv->account.name, sconv->name);
	g_return_if_fail(account != NULL);

	if (purple_account_get_connection(account) == NULL)
		return;
	
	//todo: setup to disable for non-pidgin instances, aka finch
	pidgin_conversations_init();
	conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, sconv->name);

	purple_conversation_present(conv);

	gtk_window_stick(GTK_WINDOW(PIDGIN_CONVERSATION(conv)->win->window));
	gtk_window_move(GTK_WINDOW(PIDGIN_CONVERSATION(conv)->win->window),sconv->x,sconv->y);
	gtk_window_resize(GTK_WINDOW(PIDGIN_CONVERSATION(conv)->win->window), sconv->w, sconv->h);
	
	
	//buddylist = pidgin_blist_get_default_gtk_blist();
	//if(buddylist != NULL){
	//       gtk_window_stick(GTK_WINDOW(buddylist->window->window));
	//}else{
	//  purple_debug_error(SESSIONSAVE_PLUGIN_ID, "not a window");
	//}
	

	//setup for configuration window
	/*pidgin_conv_new(conv);
	if (conv && sconv->html[0]) {
		purple_conversation_write(conv, NULL, sconv->html, PURPLE_MESSAGE_RAW | PURPLE_MESSAGE_NO_LOG, time(NULL));
	}*/

	sconv->restored = TRUE;
}

/*
 * API for manipulating the list of SavedConversation's
 */

static void saved_convs_add(SavedConversation* sconv)
{
	saved_conversations = g_list_append(saved_conversations, sconv);
}

static void saved_convs_remove(SavedConversation* sconv)
{
	saved_conversations = g_list_remove(saved_conversations, sconv);
}

static void saved_convs_delete_all()
{
	g_list_foreach(saved_conversations, (GFunc)saved_conv_delete, NULL);
	g_list_free(saved_conversations);
	saved_conversations = NULL;
}

static SavedConversation* saved_convs_find_sc(SavedConversation* sconv)
{
	GList* lp = g_list_find_custom(saved_conversations, sconv, (GCompareFunc)saved_convo_cmp);
	return lp ? (SavedConversation*)lp->data : NULL;
}

static SavedConversation* saved_convs_find(PurpleConversation* conv)
{
	PurpleAccount* account = purple_conversation_get_account(conv);

	SavedConversation tmp;
	tmp.account.name = (char*)purple_account_get_username(account);
	tmp.account.protocol = (char*)purple_account_get_protocol_id(account);
	tmp.type = purple_conversation_get_type(conv);
	tmp.name = (char*)purple_conversation_get_name(conv);

	return saved_convs_find_sc(&tmp);
}

static SavedConversation* saved_convs_find_or_create(PurpleConversation* conv)
{
	SavedConversation* sconv;

	g_return_val_if_fail(conv != NULL, NULL);

	sconv = saved_convs_find(conv);
	if (sconv == NULL) {
		sconv = saved_conv_new(conv);
		saved_convs_add(sconv);
	}

	return sconv;
}


/* this function updates the geometry of all windows on exit*/
static void update_geometry_on_exit(PurpleConversation* conv)
{
        
        int x, y, w, h;
        SavedConversation* sconv = saved_convs_find(conv);
	gtk_window_get_position (GTK_WINDOW(PIDGIN_CONVERSATION(conv)->win->window),&x, &y);
        gtk_window_get_size (GTK_WINDOW(PIDGIN_CONVERSATION(conv)->win->window), &w, &h);
        sconv->x = x;
        sconv->y = y;
        sconv->w = w;
        sconv->h = h;

}

static void saved_convs_save()
{
  GList* str_list = NULL, *lp;

	purple_debug_info("sessionsave", "saving\n");

	for (lp = saved_conversations; lp; lp = g_list_next(lp)) {
	    SavedConversation* sconv = (SavedConversation*)lp->data;
	    char* str = g_strdup_printf("{%s} {%s} {%c} {%s} {%d} {%d} {%d} {%d}",
									sconv->account.name,
									sconv->account.protocol,
									conversation_type_to_char(sconv->type),
									sconv->name,
					                                sconv->x,
					                                sconv->y,
					                                sconv->w,
					                                sconv->h);
		str_list = g_list_append(str_list, str);
	}

	purple_prefs_set_string_list("/plugins/core/sessionsave/conversations", str_list);

	g_list_foreach(str_list, (GFunc)g_free, NULL);
	g_list_free(str_list);
}

static void saved_convs_load()
{
	GList* str_list = NULL, *lp;

	purple_debug_info("sessionsave", "loading\n");

	saved_conversations = NULL;
	str_list = purple_prefs_get_string_list("/plugins/core/sessionsave/conversations");

	for (lp = str_list; lp; lp = g_list_next(lp)) {
		char* str = (char*)lp->data;
		char account_name[BUF_LEN];
		char account_protocol[BUF_LEN];
		char type_id;
		char name[BUF_LEN];
		char* html = g_new(char, strlen(str));  // known to be big enough for that field
		SavedConversation* sconv;
		int x, y, w, h;

		// Note: scanf's %[...] only accepts nonempty strings.  The last field (html) can be
		// empty, in which case scanf will only set the first 4 fields.  Ensure that html is
		// null-terminated.
		*html = 0;
		if (sscanf(str, "{%[^}]} {%[^}]} {%c} {%[^}]} {%d} {%d} {%d} {%d} {%[^}]}", account_name, account_protocol, &type_id, name, &x, &y, &w, &h, html) < 8) {
			purple_debug_warning("sessionsave", "prefs syntax error: %s", str);
			g_free(html);
			continue;
		}

		sconv = saved_conv_new_with_data(account_name, account_protocol, type_id, name, x, y, w, h, html);

		g_free(html);

		if (saved_convs_find_sc(sconv) != NULL)
			saved_conv_delete(sconv);
		else
			saved_convs_add(sconv);
	}

	g_list_foreach(str_list, (GFunc)g_free, NULL);
	g_list_free(str_list);
}

static void saved_convs_restore()
{
	restoring_session = TRUE;
	g_list_foreach(saved_conversations, (GFunc)saved_conv_restore, NULL);
	restoring_session = FALSE;
}

/*
 * Purple Callbacks
 */

/*static void conv_add_message(PurpleConversation* conv, const char* from, const char* message, PurpleMessageFlags type)
{
   SavedConversation* sconv = saved_convs_find_or_create(conv);
	char* msg_fixed;
	char* html = NULL, *date = NULL;
	g_return_if_fail(sconv);

	if (restoring_session)
		return;

	msg_fixed = g_strdup(message);

	// get date
	{
		time_t when = time(NULL);
		struct tm tm;
		tm = *(localtime(&when));
		date = g_strdup(purple_time_format(&tm));
	}

	// full html message from log.c (too bad the code isn't shared)
	if (type & PURPLE_MESSAGE_SYSTEM)
		html = g_strdup_printf("<font size=\"2\">(%s)</font><b> %s</b><br/>", date, msg_fixed);
	else if (type & PURPLE_MESSAGE_ERROR)
		html = g_strdup_printf("<font color=\"#FF0000\"><font size=\"2\">(%s)</font><b> %s</b></font><br/>", date, msg_fixed);
	else if (type & PURPLE_MESSAGE_WHISPER)
		html = g_strdup_printf("<font color=\"#6C2585\"><font size=\"2\">(%s)</font><b> %s:</b></font> %s<br/>", date, from, msg_fixed);
	else if (type & PURPLE_MESSAGE_AUTO_RESP) {
		if (type & PURPLE_MESSAGE_SEND)
			html = g_strdup_printf(_("<font color=\"#16569E\"><font size=\"2\">(%s)</font> <b>%s &lt;AUTO-REPLY&gt;:</b></font> %s<br/>"), date, from, msg_fixed);
		else if (type & PURPLE_MESSAGE_RECV)
			html = g_strdup_printf(_("<font color=\"#A82F2F\"><font size=\"2\">(%s)</font> <b>%s &lt;AUTO-REPLY&gt;:</b></font> %s<br/>"), date, from, msg_fixed);
	} else if (type & PURPLE_MESSAGE_RECV) {
		if(purple_message_meify(msg_fixed, -1))
			html = g_strdup_printf("<font color=\"#062585\"><font size=\"2\">(%s)</font> <b>***%s</b></font> %s<br/>", date, from, msg_fixed);
		else
			html = g_strdup_printf("<font color=\"#A82F2F\"><font size=\"2\">(%s)</font> <b>%s:</b></font> %s<br/>", date, from, msg_fixed);
	} else if (type & PURPLE_MESSAGE_SEND) {
		if(purple_message_meify(msg_fixed, -1))
			html = g_strdup_printf("<font color=\"#062585\"><font size=\"2\">(%s)</font> <b>***%s</b></font> %s<br/>", date, from, msg_fixed);
		else
			html = g_strdup_printf("<font color=\"#16569E\"><font size=\"2\">(%s)</font> <b>%s:</b></font> %s<br/>", date, from, msg_fixed);
	} else if (type & PURPLE_MESSAGE_NO_LOG) {
		// Do nothing.  These aren't that important anyway.
	} else {
		purple_debug_error("log", "Unhandled message type.");
		html = g_strdup_printf("<font size=\"2\">(%s)</font><b> %s:</b></font> %s<br/>", date, from, msg_fixed);
	}

	if (html) {
		char* tmp = sconv->html;
		sconv->html = g_strconcat(sconv->html, html, NULL);
		g_free(tmp);
		g_free(html);
	}

	g_free(date);
	g_free(msg_fixed);

	saved_convs_save();

}*/

/*static void on_wrote_im_msg(PurpleAccount *account, const char *from, const char *message,
							PurpleConversation *conv, PurpleMessageFlags flags, void *data)
{
	if (flags & PURPLE_MESSAGE_SEND)
		from = purple_account_get_username(account);
	conv_add_message(conv, from, message, flags);
	}*/

/*#if 0
static void on_sent_im_msg(PurpleAccount *account, const char *receiver, const char *message)
{
	PurpleConversation* conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, receiver, account);
	conv_add_message(conv, message);
}

static void on_sent_chat_msg(PurpleAccount *account, const char *message, int id)
{
	PurpleConversation* conv = purple_find_chat(purple_account_get_connection(account), id);
	conv_add_message(conv, message);
}
#endif*/

static void on_conversation_created(PurpleConversation *conv)
{
	saved_convs_find_or_create(conv);
	saved_convs_save();
}

static void on_deleting_conversation(PurpleConversation *conv)
{
	SavedConversation* sconv = saved_convs_find(conv);
	if (sconv != NULL)
		saved_convs_remove(sconv);

	saved_convs_save();
}

static void on_signed_on(PurpleConnection *gc, void *data)
{
	saved_convs_restore();
}

static void on_quitting(void *data)
{
	void *conv_handle = purple_conversations_get_handle();

	purple_signal_disconnect(conv_handle, "conversation-created", gplugin, PURPLE_CALLBACK(on_conversation_created));
	purple_signal_disconnect(conv_handle, "chat-joined", gplugin, PURPLE_CALLBACK(on_conversation_created));
	purple_signal_disconnect(conv_handle, "deleting-conversation", gplugin, PURPLE_CALLBACK(on_deleting_conversation));
	g_list_foreach(purple_get_conversations(), (GFunc)update_geometry_on_exit, NULL);
	saved_convs_save();
	gplugin = NULL;
}

/*
 * Purple plugin initialization
 */

static void on_blist_create(PurpleBuddyList *blist)
{
        PidginBuddyList *pblist = pidgin_blist_get_default_gtk_blist();
	gtk_window_stick(GTK_WINDOW(pblist->window->window));
	

	purple_debug_fatal("Sweet","Fail");
	return;
}

static gboolean plugin_load(PurplePlugin *plugin)
{
	void *core_handle = purple_get_core();
	void *conv_handle = purple_conversations_get_handle();
	void *conn_handle = purple_connections_get_handle();

	gplugin = plugin;

	saved_convs_load();
//    purple_signal_connect(conv_handle, "wrote-im-msg", gplugin, PURPLE_CALLBACK(on_wrote_im_msg), NULL);
//    purple_signal_connect(conv_handle, "wrote-chat-msg", gplugin, PURPLE_CALLBACK(on_wrote_im_msg), NULL);
//    purple_signal_connect(conv_handle, "sent-im-msg", gplugin, PURPLE_CALLBACK(on_sent_im_msg), NULL);
//    purple_signal_connect(conv_handle, "sent-chat-msg", gplugin, PURPLE_CALLBACK(on_sent_chat_msg), NULL);
	purple_signal_connect(conv_handle, "conversation-created", plugin, PURPLE_CALLBACK(on_conversation_created), NULL);
	purple_signal_connect(conv_handle, "chat-joined", plugin, PURPLE_CALLBACK(on_conversation_created), NULL);
	purple_signal_connect(conv_handle, "deleting-conversation", plugin, PURPLE_CALLBACK(on_deleting_conversation), NULL);
	
	purple_signal_connect(conn_handle, "signed-on", plugin, PURPLE_CALLBACK(on_signed_on), NULL);

	purple_signal_connect(conn_handle, "gtkblist-unhiding",plugin, PURPLE_CALLBACK(on_blist_create), NULL);

	purple_signal_connect(core_handle, "quitting", plugin, PURPLE_CALLBACK(on_quitting), NULL);

	g_list_foreach(purple_get_conversations(), (GFunc)on_conversation_created, NULL);

	return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin)
{
	gplugin = NULL;

	saved_convs_delete_all();
	return TRUE;
}

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,                           /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                          /**< priority       */

	SESSIONSAVE_PLUGIN_ID,                            /**< id             */
	N_("SessionSave"),                                /**< name           */
	SESSIONSAVE_VERSION,                              /**< version        */
	                                                  /**  summary        */
	N_("Saves your sessions and restores them next time you start purple.  Modified to restore windows to their previous positions."),
	                                                  /**  description    */
	N_("Saves your sessions and restores them next time you start purple. Modified to restore windows to their previous positions."),
	                                                  /**< author         */
	"Matt Perry feat. Alan Drees" ,
	"http://somewhere.fscked.org/sessionsave/",       /**< homepage       */

	plugin_load,                                      /**< load           */
	plugin_unload,                                    /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL,                                             /**< extra_info     */
	NULL,
	NULL
};

static void init_plugin(PurplePlugin *plugin)
{
	purple_prefs_add_none("/plugins/core/sessionsave");
	purple_prefs_add_string_list("/plugins/core/sessionsave/conversations", NULL);
}

PURPLE_INIT_PLUGIN(notify, init_plugin, info)
