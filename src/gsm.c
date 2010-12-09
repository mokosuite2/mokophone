#include <glib.h>
#include <string.h>
#include <freesmartphone-glib/freesmartphone-glib.h>
#include <freesmartphone-glib/ousaged/usage.h>
#include <freesmartphone-glib/ogsmd/device.h>
#include <freesmartphone-glib/ogsmd/sim.h>
#include <freesmartphone-glib/ogsmd/call.h>
#include <freesmartphone-glib/ogsmd/network.h>
#include <mokosuite/utils/utils.h>
#include <mokosuite/utils/remote-config-service.h>

#include "globals.h"
#include "gsm.h"
#include "simauthwin.h"
//#include "callwin.h"
//#include "phonewin.h"


static SimAuthWin *sim_auth = NULL;

static gboolean gsm_available = FALSE;
static gboolean gsm_ready = FALSE;
static gboolean sim_ready = FALSE;
static gboolean network_registered = FALSE;
static gboolean offline_mode = FALSE;

static void online_offline(void);
static gboolean request_gsm(gpointer data);
static gboolean get_auth_status(gpointer data);
static gboolean list_resources(gpointer data);
static gboolean register_network(gpointer data);

// eh...
static void network_status(gpointer data, GHashTable *status);
static void _get_network_status_callback(GError * error, GHashTable * status, gpointer data);

static void sim_auth_show(SIMAuthStatus type);
static void sim_auth_hide(void);
static void sim_auth_error(void);

static void _request_resource_callback(GError * error, gpointer userdata)
{
    // nessun errore -- tocca a ResourceChanged
    if (error == NULL) return;

    if (FREESMARTPHONE_GLIB_IS_USAGE_ERROR(error, USAGE_ERROR_USER_EXISTS)) {
        g_message("GSM has already been requested.");
        return;
    }

    g_debug("request resource error, try again in 1s (%s, %d)", error->message, error->code);
    g_timeout_add(1000, request_gsm, NULL);
}

static void _list_resources_callback(GError *error, char **resources, gpointer userdata)
{
    /* if we successfully got a list of resources...
     * check if GSM is within them and request it if
     * so, otherwise wait for ResourceAvailable signal */
    if (error) {
        g_message("error: (%d) %s", error->code, error->message);
        g_timeout_add(1000, list_resources, NULL);
        return;
    }

    if (resources) {
        int i = 0;
        while (resources[i] != NULL) {
            g_debug("Resource %s available", resources[i]);
            if (!strcmp(resources[i], "GSM")) {
                gsm_available = TRUE;
                break;
            }
            i++;
        }

        if (gsm_available)
            request_gsm(NULL);
    }
}

static void _register_to_network_callback(GError * error, gpointer userdata)
{
    if (error == NULL) {
        g_debug("Successfully registered to the network");
        // non settare ora -- network_registered = TRUE;
        ogsmd_network_get_status(_get_network_status_callback, NULL);
    }
    else {
        g_debug("Registering to network failed: %s %s %d", error->message, g_quark_to_string(error->domain), error->code);
        if (!offline_mode && gsm_ready)
            g_timeout_add(10 * 1000, register_network, NULL);   // TODO: parametrizza il timeout
    }
}

static void _get_network_status_callback(GError * error, GHashTable * status, gpointer data)
{
    if (!error)
        network_status(NULL, status);

    else
        _register_to_network_callback(error, data);
}

/* GSM.SIM.GetAuthStatus */
static void _sim_get_auth_status_callback(GError *error, int status, gpointer data)
{
    g_debug("sim_auth_status_callback(%s,status=%d)", error ? "ERROR" : "OK", status);

    /* if no SIM is present inform the user about it and
     * stop retrying to authenticate the SIM */
    if (FREESMARTPHONE_GLIB_IS_GSM_ERROR(error, GSM_ERROR_SIM_NOT_PRESENT)) {
        g_message("SIM card not present.");
        // TODO
        return;
    }

    /* altri errori o stato sconosciuto, riprova tra 5s */
    if (error || status == SIM_AUTH_STATUS_UNKNOWN) {
        g_debug("SIM auth status error: %s", error->message);
        g_timeout_add(5000, get_auth_status, NULL);
        return;
    }

    if (status != SIM_AUTH_STATUS_READY) {
        g_debug("SIM authentication started");
        sim_auth_show(SIM_AUTH_STATUS_PIN_REQUIRED);
        return;
    }

    g_debug("SIM authenticated");
    sim_auth_hide();

    // registra sulla rete
    register_network(NULL);
}

static void _sim_send_auth_callback(GError *error, gpointer data)
{
    if (error != NULL) {
        g_debug("SendAuthCode error");

        sim_auth_error();
    }
}

static gboolean request_gsm(gpointer data)
{
    if (gsm_available) {
        g_debug("Request GSM resource");
        ousaged_usage_request_resource("GSM", _request_resource_callback, NULL);
    }

    return FALSE;
}

static void _offline_callback(GError *error, gpointer userdata)
{
    g_debug("going offline");
    // TODO controlla errori set functionality
    network_registered = FALSE;
}

static void _online_callback(GError *error, gpointer userdata)
{
    g_debug("going online");
    // TODO controlla errori set functionality
    if (error) {
        g_warning("SetFunctionality error: (%d) %s", error->code, error->message);
    }

    get_auth_status(NULL);
}

static void online_offline(void)
{
    g_debug("online_offline: offline_mode = %d", offline_mode);

    if (offline_mode)
        ogsmd_device_set_functionality("airplane", FALSE, "", _offline_callback, NULL);

    else
        ogsmd_device_set_functionality("full", TRUE, "", _online_callback, NULL);
}

static gboolean get_auth_status(gpointer data)
{
    ogsmd_sim_get_auth_status(_sim_get_auth_status_callback, NULL);
    return FALSE;
}

static gboolean register_network(gpointer data)
{
    ogsmd_network_register(_register_to_network_callback, NULL);
    return FALSE;
}

static gboolean list_resources(gpointer data)
{
    ousaged_usage_list_resources(_list_resources_callback, NULL);
    return FALSE;
}

static void _pin_ok(void *simauth, const char *code) {
    SimAuthWin *s = SIMAUTH_WIN(simauth);

    switch (s->type) {
        case SIM_AUTH_STATUS_PIN_REQUIRED:
            moko_popup_status_activate(s->status, _("Checking..."));
            ogsmd_sim_send_auth_code(code, _sim_send_auth_callback, NULL);

            break;

        // TODO: altri case
        default:
            break;
    }
}

static void sim_auth_show(SIMAuthStatus type)
{
    if (sim_auth == NULL) {
        sim_auth = sim_auth_win_new(type);
        sim_auth->code_callback = _pin_ok;  // TODO: altri casi
    }

    mokowin_activate(MOKO_WIN(sim_auth));
}

static void sim_auth_error(void)
{
    if (sim_auth != NULL) {
        sim_auth_win_auth_error(sim_auth);
        mokoinwin_hide(MOKO_INWIN(sim_auth->status));
    }
}

static void sim_auth_hide(void)
{
    if (sim_auth != NULL) {
        sim_auth_win_destroy(sim_auth);
        sim_auth = NULL;
    }
}

static void offline_mode_changed(GObject *settings, const char* section, const char* key, GValue* value)
{
    if (!strcmp(section, "phone") && !strcmp(key, "offline_mode")) {
        offline_mode = g_value_get_boolean(value);
        online_offline();
    }
}

/* -- SIGNAL HANDLERS -- */

/* Usage.ResourceChanged signal */
static void resource_changed (gpointer data, const char *name, const gboolean state, GHashTable *attributes)
{
    g_debug("Resource changed: %s (%d, current gsm_ready = %d)", name, state, gsm_ready);
    if (!strcmp(name, "GSM")) {

        if (gsm_ready ^ state) {
            gsm_ready = state;

            if (gsm_ready) {
                g_debug("GSM resource enabled, powering on modem");
                online_offline();
            }

        } else if (!gsm_ready) {
                // gsm disattivato, gestisci il recupero della risorsa
                g_debug("GSM resource disabled, requesting");
                request_gsm(NULL);
        }
    }
}

/* Usage.ResourceAvailable signal */
static void resource_available(gpointer data, const char *name, gboolean available)
{
    g_debug("resource %s is %s", name, available ? "now available" : "gone");
    if (!strcmp(name, "GSM")) {
        gsm_available = available;
        if (gsm_available)
            request_gsm(NULL);
    }
}

/* GSM.SimAuthStatus */
static void sim_auth_status(gpointer data, const int status)
{
    if (status == SIM_AUTH_STATUS_READY) {
        sim_auth_hide();
        register_network(NULL);
    }
}

/* GSM.CallStatus */
static void call_status(gpointer data, int id, int status, GHashTable* properties)
{
    // :)
    phone_call_win_call_status(id, status, properties);
}

/* GSM.IncomingUssd */
static void incoming_ussd(gpointer data, int mode, const char* message)
{
    phone_win_ussd_reply(mode, message);
}

/* GSM.NetworkStatus */
static void network_status(gpointer data, GHashTable *status)
{
    if (!status) {
        g_debug("got no status from NetworkStatus?!");
        return;
    }

    GValue *tmp = g_hash_table_lookup(status, "registration");
    if (tmp) {

        const char *registration = g_value_get_string(tmp);
        g_debug("network_status (registration=%s)", registration);

        if (!strcmp(registration, "home")) {
            network_registered = TRUE;
        }

        else {
            network_registered = FALSE;
            if (!offline_mode && gsm_ready) {
                g_message("scheduling registration to network");
                g_timeout_add(10 * 1000, register_network, NULL);   // TODO: parametrizza il timeout
            }
        }

        /*
        if (!strcmp(registration, "unregistered")) {
            g_message("scheduling registration to network");
            g_timeout_add(10 * 1000,    // FIXME parametrizzabile
                    register_network, NULL);
        }
        */
    }
    else {
        g_debug("got NetworkStatus without registration?!?");
    }
}

/* GSM.DeviceStatus */
static void gsm_device_status(gpointer data, const int status)
{
    // TODO
    // TODO per sim ready aaaaaahhhh!!!! :-o
}

/* -- funzioni pubbliche -- */

bool gsm_is_ready(void)
{
    return (gsm_ready && sim_ready);
}

bool gsm_can_call(void)
{
    return (gsm_ready && network_registered);
}

void gsm_online(void)
{
    offline_mode = FALSE;
    online_offline();
}

void gsm_offline(void)
{
    offline_mode = TRUE;
    online_offline();
}

void gsm_init(RemoteConfigService *config)
{
    ousaged_usage_resource_changed_connect(resource_changed, NULL);
    ousaged_usage_resource_available_connect(resource_available, NULL);
    ogsmd_device_device_status_connect(gsm_device_status, NULL);
    ogsmd_sim_auth_status_connect(sim_auth_status, NULL);
    ogsmd_call_call_status_connect(call_status, NULL);
    ogsmd_network_incoming_ussd_connect(incoming_ussd, NULL);
    ogsmd_network_status_connect(network_status, NULL);

    ousaged_usage_dbus_connect();

    if (ousagedUsageBus == NULL) {
        g_error("Cannot connect to ousaged. Exiting");
        return;
    }

    ogsmd_device_dbus_connect();

    if (ogsmdDeviceBus == NULL) {
        g_error("Cannot connect to ogsmd (device). Exiting");
        return;
    }

    ogsmd_sim_dbus_connect();

    if (ogsmdSimBus == NULL) {
        g_error("Cannot connect to ogsmd (sim). Exiting");
        return;
    }

    ogsmd_call_dbus_connect();

    if (ogsmdCallBus == NULL) {
        g_error("Cannot connect to ogsmd (call). Exiting");
        return;
    }

    // offline mode
    remote_config_service_get_bool(config, "phone", "offline_mode", &offline_mode);

    // offline mode listener
    g_signal_connect(G_OBJECT(config), "Changed", G_CALLBACK(offline_mode_changed), NULL);

    // ping fsogsmd
    ogsmd_device_get_device_status(NULL, NULL);

    // let's go!
    list_resources(NULL);
}
