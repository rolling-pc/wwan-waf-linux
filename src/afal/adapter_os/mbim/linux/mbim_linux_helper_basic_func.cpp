#include <cstdio>
#include "glib.h"
#include <gio/gio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <semaphore.h>
#include "libmbim-glib.h"
#include "mbim-fibocom.h"
#include "log.hpp"
#include "common.hpp"
#include "libmbim_common_struct.h"
#include "mbim_linux_helper_adapter.h"

using namespace afal::log;
using namespace afal::error;

#define PORT_MIN_LEN                     8 // eg: cdc-wdm0
#define MSG_MIN_LEN                      2 // eg: AT
#define GREP_MBIM_PORT_CMD_LEN           30
#define RDONLY                           "r"
#define WRONLY                           "w"

static sem_t           force_sync_sem;
GCancellable    *g_cancellable                = nullptr;
MbimDevice      *g_mbimdevice                 = nullptr;
gboolean        g_sim_inserted_flag           = FALSE;
extern bool        g_mbim_device_init_flag;

static int query_local_mccmnc(GAsyncReadyCallback func_pointer, gpointer userdata);
static int query_slot_info_status(guint32 slot_index, GAsyncReadyCallback func_pointer, gpointer userdata);
static int query_slot_mapping_status(GAsyncReadyCallback func_pointer, gpointer userdata);
static int query_sys_caps(GAsyncReadyCallback func_pointer, int timeout, gpointer userdata);
static int query_connect_state(GAsyncReadyCallback func_pointer, gpointer userdata);
static int query_register_state(GAsyncReadyCallback func_pointer, int timeout, gpointer userdata);
static int query_signal_state(GAsyncReadyCallback func_pointer, gpointer userdata);

/* -------------------Begin Notification related functions------------------- */
static void
_basic_connect_notification_query_local_mccmnc_ready (MbimDevice   *device,
                                                      GAsyncResult *res,
                                                      gpointer userdata)
{
    g_autoptr(GError)                   error          =  nullptr;
    g_autoptr(MbimMessage)              response       =  nullptr;
    gint                                ret            =  ERR;
    MbimProvider                        *out_provider  =  nullptr;
    _SimData                            *pointer       =  (_SimData *)userdata;

    LOG_DEBUG("enter %s!\n", __func__);

    response = mbim_device_command_finish (device, res, &error);

    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        goto TAIL;
    }

    if (!mbim_message_home_provider_response_parse (
            response,
            &out_provider,
            &error)) {
        LOG_ERROR("error: couldn't parse response message: %s\n", error->message);
        goto TAIL;
    }

    strncpy(pointer->local_mccmnc, (const char *)(out_provider->provider_id), 6);

    if (pointer->imsi)
    {
        LOG_DEBUG("imsi: %s\n", pointer->imsi);
    }

    if (pointer->iccid)
    {
        LOG_DEBUG("iccid: %s\n", pointer->iccid);
    }

    if (pointer->local_mccmnc)
    {
        LOG_DEBUG("local_mccmnc: %s\n", pointer->local_mccmnc);
    }

TAIL:
    if (out_provider)
        mbim_provider_free(out_provider);

    GThread  *sub_thread = g_thread_new ("notify_func", (GThreadFunc)linux_trigger_sim_notify, pointer);
    return;
}

static void
_basic_connect_notification_query_signal_state_ready(MbimDevice *device,
                                                     GAsyncResult *res,
                                                     gpointer user_data)
{
    g_autoptr(GError)    error          = nullptr;
    _NetworkData         *pointer       = (_NetworkData *)user_data;
    guint32              rssi           = OK;
    MbimMessage          *response      = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);

    response = mbim_device_command_finish(device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        goto TAIL;
    }

    if (!mbim_message_signal_state_response_parse (response,
                                                   &rssi,
                                                   nullptr,
                                                   nullptr,
                                                   nullptr,
                                                   nullptr,
                                                   &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        goto TAIL;
    }

    pointer->rssi = (int)rssi;
    LOG_DEBUG("rssi:%d\n", pointer->rssi);

TAIL:
    GThread  *sub_thread = g_thread_new ("notify_func", (GThreadFunc)linux_trigger_network_notify, pointer);
    return;
}

static void
_basic_connect_notification_query_connect_state_ready(MbimDevice *device,
                                                      GAsyncResult *res,
                                                      gpointer userdata)
{
    g_autoptr(MbimMessage)   response          = NULL;
    g_autoptr(GError)        error             = NULL;
    MbimActivationState      activation_state  = MBIM_ACTIVATION_STATE_UNKNOWN;
    guint32                  nw_error          = OK;
    _NetworkData             *pointer          = (_NetworkData *)userdata;
    int                      ret               = ERR;

    LOG_DEBUG("enter %s!\n", __func__);

    response = mbim_device_command_finish (device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        goto TAIL;
    }

    if (!mbim_message_connect_response_parse (
            response,
            NULL,  // session_id
            &activation_state,
            NULL,  // voice_call_state
            NULL,  // ip_type
            NULL,  // context_type
            &nw_error,
            &error)) {
        LOG_ERROR("error: couldn't parse response message: %s\n", error->message);
        goto TAIL;
    }

    if (nw_error != OK) {
        LOG_ERROR("Network error:%d\n", nw_error);
        goto TAIL;
    }

    pointer->ConnectState = activation_state;
    LOG_DEBUG("Get Connect State: %d\n", pointer->ConnectState);

    // send signal state query request.
    ret = query_signal_state((GAsyncReadyCallback)_basic_connect_notification_query_signal_state_ready, userdata);
    if (ret != OK) {
        LOG_ERROR("executed Internal message failed!\n");
        goto TAIL;
    }
    return;

TAIL:
    GThread  *sub_thread = g_thread_new ("notify_func", (GThreadFunc)linux_trigger_network_notify, pointer);
    return;
}


static void
_basic_connect_notification_query_register_state_ready(MbimDevice *device,
                                                       GAsyncResult *res,
                                                       gpointer userdata)
{
    g_autoptr(GError)    error          = nullptr;
    _NetworkData         *pointer       = (_NetworkData *)userdata;
    MbimNwError          nw_error;
    MbimRegisterState    register_state = MBIM_REGISTER_STATE_UNKNOWN;
    g_autofree gchar     *provider_id   = NULL;
    MbimMessage          *response      = nullptr;
    int                  ret            = ERR;
    GThread              *sub_thread    = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);

    response = mbim_device_command_finish(device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        goto TAIL;
    }

    if (!mbim_message_register_state_response_parse (response,
                                                     &nw_error,
                                                     &register_state,
                                                     nullptr,
                                                     nullptr,
                                                     nullptr,
                                                     &provider_id,
                                                     nullptr,
                                                     nullptr,
                                                     nullptr,
                                                     &error)) {
        LOG_ERROR("error: couldn't parse response message: %s\n", error->message);
        goto TAIL;
    }

    if (nw_error != (MbimNwError)OK) {
        LOG_DEBUG("Network error:%d\n", nw_error);
        goto TAIL;
    }

    pointer->RegisterState = register_state;
    LOG_DEBUG("RegisterState: %d\n", pointer->RegisterState);
    if (provider_id && strlen(provider_id) > 1) {
        strncpy(pointer->RoamMccmnc, provider_id, 7);
        LOG_DEBUG("roam mccmnc: %s\n", pointer->RoamMccmnc);
    } else {
        strncpy(pointer->RoamMccmnc, "NULL", 7);
        LOG_DEBUG("roam mccmnc: %s\n", pointer->RoamMccmnc);
    }

    switch (register_state) {
        case MBIM_REGISTER_STATE_HOME:
        case MBIM_REGISTER_STATE_ROAMING:
        case MBIM_REGISTER_STATE_PARTNER:
            // send signal state query request.
            // ret = query_signal_state((GAsyncReadyCallback)query_signal_state_ready, userdata);
            ret = query_connect_state((GAsyncReadyCallback)_basic_connect_notification_query_connect_state_ready, userdata);
            if (ret != OK) {
                LOG_ERROR("executed Internal message failed!\n");
                goto TAIL;
            }
            break;
        default:
            LOG_DEBUG("Invalid register state: %d, refuse to check connect and signal!\n", register_state);
            pointer->rssi = 99;
    }

    return;

TAIL:
    sub_thread = g_thread_new ("notify_func", (GThreadFunc)linux_trigger_network_notify, pointer);
    return;
}

static void
_basic_connect_extension_notification_query_slot_info_status_ready(MbimDevice *device,
                                                                   GAsyncResult *res,
                                                                   gpointer userdata)
{
    g_autoptr(GError)         error          = nullptr;
    _SlotData                 *pointer       = (_SlotData *)userdata;
    MbimMessage               *response      = nullptr;
    int                       ret            = ERR;
    guint32                   slot_index     = 0;
    MbimUiccSlotState         slot_state     = MBIM_UICC_SLOT_STATE_UNKNOWN;

    LOG_DEBUG("enter %s!\n", __func__);

    response = mbim_device_command_finish(device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        goto TAIL;
    }

    if (!mbim_message_ms_basic_connect_extensions_slot_info_status_response_parse (
            response,
            &slot_index,
            &slot_state,
            &error))
    {
        LOG_ERROR("error: couldn't parse response message: %s\n",
                  error->message);
        goto TAIL;
    }

    pointer->slot_state[slot_index] = slot_state;
    LOG_DEBUG("slot %d state: %d\n", slot_index, slot_state);

    slot_index++;
    if (slot_index < pointer->ValidSlotNum)
    {
        ret = query_slot_info_status(
                slot_index, (GAsyncReadyCallback)_basic_connect_extension_notification_query_slot_info_status_ready, userdata);
        if (ret != OK)
        {
            LOG_ERROR("executed Internal message failed!\n");
        }
    }
    return;
TAIL:
    GThread  *sub_thread = g_thread_new ("notify_func", (GThreadFunc)linux_trigger_slot_notify, pointer);
    return;
}

static void
_basic_connect_extension_notification_query_slot_mapping_status_ready(MbimDevice *device,
                                                                      GAsyncResult *res,
                                                                      gpointer userdata)
{
    g_autoptr(GError)         error          = nullptr;
    _SlotData                 *pointer       = (_SlotData *)userdata;
    MbimMessage               *response      = nullptr;
    int                       ret            = ERR;
    guint32                   out_map_count  = 0;
    g_autofree MbimSlotArray  *out_slot_map  = NULL;
    int                       max_executor   = 0;

    LOG_DEBUG("enter %s!\n", __func__);

    response = mbim_device_command_finish(device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        goto TAIL;
    }

    if (!mbim_message_ms_basic_connect_extensions_device_slot_mappings_response_parse
            (response,
             &out_map_count,
             &out_slot_map,
             &error)) {
        LOG_ERROR("error: couldn't parse response message: %s\n", error->message);
        goto TAIL;
    }

    max_executor = pointer->ValidExecutorNum;
    if (out_map_count != max_executor) {
        LOG_DEBUG("Not match with sys-caps! Actual %d executors are activated!\n", out_map_count);
        goto TAIL;
    }

    pointer->CurrentSlotIndex = out_slot_map[0]->slot;
    LOG_DEBUG("found work slot: %d\n", pointer->CurrentSlotIndex);

    ret = query_slot_info_status(0, (GAsyncReadyCallback)_basic_connect_extension_notification_query_slot_info_status_ready, userdata);
    if (ret != OK) {
        LOG_ERROR("executed Internal message failed!\n");
        goto TAIL;
    }
    return;
TAIL:
    GThread  *sub_thread = g_thread_new ("notify_func", (GThreadFunc)linux_trigger_slot_notify, pointer);
    return;
}

static void
_basic_connect_extension_notification_query_sys_caps_ready (MbimDevice   *device,
                                                            GAsyncResult  *res,
                                                            gpointer      userdata)
{
    g_autoptr(MbimMessage) response         = NULL;
    g_autoptr(GError)      error            = NULL;
    guint32                number_executors = 0;
    guint32                number_slots     = 0;
    _SlotData              *pointer         = (_SlotData *)userdata;
    int                    ret              = ERR;

    LOG_DEBUG("enter %s\n", __func__);

    response = mbim_device_command_finish(device, res, &error);
    if (!response || !mbim_message_response_get_result(
            response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error))
    {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        goto TAIL;
    }

    if (!mbim_message_ms_basic_connect_extensions_sys_caps_response_parse(
            response, &number_executors, &number_slots, nullptr, nullptr,
            &error))
    {
        LOG_ERROR("error: couldn't parse response messages: %s\n",
                  error->message);
        goto TAIL;
    }

    if (number_slots > 2) {
        LOG_DEBUG("number_slots: %d, convert it to 2!\n", number_slots);
        number_slots = 2;
    }

    if (number_executors < 1 || number_slots < 1) {
        LOG_DEBUG("Invalid data! slot: %d, executor: %d!\n", number_slots, number_executors);
        goto TAIL;
    }
    else if (number_executors > 1 && number_slots > 1)
    {
        LOG_DEBUG("don't support DSDA yet!\n");
        goto TAIL;
    }

    LOG_DEBUG("DSSA: %d slots with 1 activated!\n", number_slots);

    pointer->ValidSlotNum     = number_slots;
    pointer->ValidExecutorNum = number_executors;

    ret = query_slot_mapping_status((GAsyncReadyCallback)_basic_connect_extension_notification_query_slot_mapping_status_ready, userdata);
    if (ret != OK) {
        LOG_ERROR("executed Internal message failed!\n");
        goto TAIL;
    }

    return;
TAIL:
    GThread  *sub_thread = g_thread_new ("notify_func", (GThreadFunc)linux_trigger_slot_notify, pointer);
    return;
}

void
basic_connect_notification_subscriber_ready_status (MbimDevice           *device,
                                                    MbimMessage          *notification)
{
    MbimSubscriberReadyState ready_state       =  MBIM_SUBSCRIBER_READY_STATE_FAILURE;
    g_autoptr(GError)        error             =  NULL;
    g_autofree gchar         *subscriber_id    =  nullptr;
    g_autofree gchar         *sim_iccid        =  nullptr;
    _SimData                 *pointer          =  nullptr;
    int                      ret               =  ERR;
    GThread                  *sub_thread       = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);

    if (mbim_device_check_ms_mbimex_version (device, 3, 0)) {
        if (!mbim_message_ms_basic_connect_v3_subscriber_ready_status_notification_parse (
                notification,
                &ready_state,
                NULL, // flags
                &subscriber_id, // subscriber id
                &sim_iccid, // sim_iccid
                NULL, // ready_info
                NULL, // telephone_numbers_count
                NULL, // telephone number
                &error)) {
            LOG_ERROR("Failed processing MBIMEx v3.0 subscriber ready status notification: %s", error->message);
            goto TAIL;
        }
        LOG_DEBUG("processed MBIMEx v3.0 subscriber ready status notification\n");
    } else {
        if (!mbim_message_subscriber_ready_status_notification_parse (
                notification,
                &ready_state,
                &subscriber_id, // subscriber id
                &sim_iccid, // sim_iccid
                NULL, // ready_info
                NULL, // telephone_numbers_count
                NULL, // telephone number
                &error)) {
            LOG_ERROR("Failed processing subscriber ready status notification: %s", error->message);
            goto TAIL;
        }
        LOG_DEBUG("processed subscriber ready status notification");
    }

    LOG_DEBUG("ready state:%d\n", ready_state);
    pointer = (_SimData *)malloc(sizeof(_SimData));
    if (!pointer) {
        LOG_ERROR("malloc failed!\n");
        goto TAIL;
    }
    memset(pointer, 0, sizeof(_SimData));

    switch (ready_state) {
        case MBIM_SUBSCRIBER_READY_STATE_INITIALIZED: {
            if (g_sim_inserted_flag)
            {
                LOG_DEBUG("SIM card was inserted before!\n");
                goto TAIL;
            }
            g_sim_inserted_flag = TRUE;

            // HOME PROVIDER not support indication, so here have to manually query. Only when "initialized" notification is received at first time, here will trigger a manually query.
            pointer->ready_state = ready_state;
            if (subscriber_id)
            {
                strncpy(pointer->imsi, (const char*)subscriber_id, 15);
                LOG_DEBUG("imsi: %s\n", pointer->imsi);
            }

            if (sim_iccid)
            {
                strncpy(pointer->iccid, (const char*)sim_iccid, 31);
                LOG_DEBUG("iccid: %s\n", pointer->iccid);
            }

            // send home provider query request.
            ret = query_local_mccmnc((GAsyncReadyCallback)_basic_connect_notification_query_local_mccmnc_ready, pointer);
            if (ret != OK)
            {
                LOG_ERROR("executed Internal message failed!\n");
                goto TAIL;
            }
            return;
        }
        default:
            pointer->ready_state = ready_state;
            if (!g_sim_inserted_flag) {
                LOG_DEBUG("SIM card not inserted at all, abort to send signal!\n");
                goto TAIL;
            }
            g_sim_inserted_flag = FALSE;
            strncpy(pointer->imsi, "NULL", 16);
            strncpy(pointer->iccid, "NULL", 32);
            strncpy(pointer->local_mccmnc, "NULL", 7);
            sub_thread = g_thread_new ("notify_func", (GThreadFunc)linux_trigger_sim_notify, pointer);
            return;
    }
TAIL:
    if (pointer) {
        memset(pointer, 0, sizeof(_SimData));
        free(pointer);
    }
    pointer = nullptr;
    return;
    // this func is the first func about notification, if this func error, don't need to send notify.
}

void
basic_connect_notification_radio_state (MbimDevice           *device,
                                        MbimMessage          *notification)
{
    MbimRadioSwitchState hw_radio_state    = MBIM_RADIO_SWITCH_STATE_OFF;
    MbimRadioSwitchState sw_radio_state    = MBIM_RADIO_SWITCH_STATE_OFF;
    g_autoptr(GError)    error             = NULL;
    _RadioData           *pointer          = nullptr;
    int                  ret               = ERR;
    GThread              *sub_thread       = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);

    mbim_message_radio_state_notification_parse(notification, &hw_radio_state, &sw_radio_state, &error);
    if (error) {
        LOG_ERROR("Failed to parse radio state response: %s\n", error->message);
        goto TAIL;
    } else {
        LOG_DEBUG("Hardware Radio state: %s", (hw_radio_state == MBIM_RADIO_SWITCH_STATE_ON ? "ON" : "OFF"));
        LOG_DEBUG("Software Radio state: %s", (sw_radio_state == MBIM_RADIO_SWITCH_STATE_ON ? "ON" : "OFF"));

        pointer = (_RadioData *)malloc(sizeof(_RadioData));
        if (!pointer) {
            LOG_ERROR("malloc failed!\n");
            goto TAIL;
        }
        memset(pointer, 0, sizeof(_RadioData));

        pointer->hw_state = hw_radio_state;
        pointer->sw_state = sw_radio_state;
    }

    sub_thread = g_thread_new ("notify_func", (GThreadFunc)linux_trigger_radio_notify, pointer);
    return;
TAIL:
    if (pointer) {
        memset(pointer, 0, sizeof(_RadioData));
        free(pointer);
    }
    pointer = nullptr;
    // don't free the pointer by default because thread will use it.
    return;
}

void
basic_connect_extension_notification_slot_info_status (MbimDevice           *device,
                                                       MbimMessage          *notification)
{
    _SlotData                 *pointer          = nullptr;
    int                       ret               = ERR;
    guint32                   slot_index        = 0;
    MbimUiccSlotState         slot_state        = MBIM_UICC_SLOT_STATE_UNKNOWN;
    GThread                   *sub_thread       = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);

    pointer = (_SlotData *)malloc(sizeof(_SlotData));
    if (!pointer) {
        LOG_ERROR("malloc failed!\n");
        goto TAIL;
    }
    memset(pointer, 0, sizeof(_SlotData));

    // consider there won't be possibility that multi-slots change at same time, so here ignore which slot change and just do a whole check logic.
    ret = query_sys_caps((GAsyncReadyCallback)_basic_connect_extension_notification_query_sys_caps_ready, 10, pointer);
    if (ret != OK) {
        LOG_ERROR("executed failed!\n");
        goto TAIL;
    }

    sub_thread = g_thread_new ("notify_func", (GThreadFunc)linux_trigger_slot_notify, pointer);
    return;
TAIL:
    if (pointer) {
        memset(pointer, 0, sizeof(_SlotData));
        free(pointer);
    }
    pointer = nullptr;
    // don't free the pointer by default because thread will use it.
    return;
}

void
basic_connect_notification_register_state (MbimDevice           *device,
                                           MbimMessage          *notification)
{
    _NetworkData              *pointer          = nullptr;
    int                       ret               = ERR;
    GThread                   *sub_thread       = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);

    pointer = (_NetworkData *)malloc(sizeof(_NetworkData));
    if (!pointer) {
        LOG_ERROR("malloc failed!\n");
        goto TAIL;
    }
    memset(pointer, 0, sizeof(_NetworkData));

    // don't concern about the dependent state about register state, connect and signal state, just do a whole check logic on all 3 notifications.
    ret = query_register_state((GAsyncReadyCallback)_basic_connect_notification_query_register_state_ready, 10, pointer);
    if (ret != OK) {
        LOG_ERROR("executed failed!\n");
        goto TAIL;
    }

    // don't free the pointer by default because thread will use it.
    return;

TAIL:
    if (pointer) {
        memset(pointer, 0, sizeof(_NetworkData));
        free(pointer);
    }
    pointer = nullptr;
    return;
}

void
basic_connect_notification_signal_state (MbimDevice           *device,
                                         MbimMessage          *notification)
{
    LOG_DEBUG("enter %s!\n", __func__);
    basic_connect_notification_register_state(device, notification);
}

void
basic_connect_notification_connection_state (MbimDevice           *device,
                                             MbimMessage          *notification)
{
    LOG_DEBUG("enter %s!\n", __func__);
    basic_connect_notification_register_state(device, notification);
}

static void
bc_ind_parse(MbimDevice   *Device,
             MbimMessage   *notification)
{
    guint        cid         = 0;

    cid = mbim_message_indicate_status_get_cid (notification);
    switch (cid) {
        case MBIM_CID_BASIC_CONNECT_REGISTER_STATE:
            basic_connect_notification_register_state(Device, notification);
            break;
        case MBIM_CID_BASIC_CONNECT_SUBSCRIBER_READY_STATUS:
            basic_connect_notification_subscriber_ready_status(Device, notification);
            break;
        case MBIM_CID_BASIC_CONNECT_RADIO_STATE:
            basic_connect_notification_radio_state(Device, notification);
            break;
        case MBIM_CID_BASIC_CONNECT_CONNECT:
            basic_connect_notification_connection_state(Device, notification);
            break;
        case MBIM_CID_BASIC_CONNECT_SIGNAL_STATE:
            basic_connect_notification_signal_state(Device, notification);
            break;
        default:
            LOG_DEBUG("Unsupported cid: %d\n", cid);
    }
    return;
}

static void
bcext_ind_parse(MbimDevice   *Device,
                MbimMessage   *notification)
{
    guint        cid         = ERR;

    cid = mbim_message_indicate_status_get_cid (notification);
    switch (cid) {
        case MBIM_CID_MS_BASIC_CONNECT_EXTENSIONS_SLOT_INFO_STATUS:
            LOG_DEBUG("Noted SLOT_INFO_STATUS cid for debug!\n");
            basic_connect_extension_notification_slot_info_status (Device, notification);
            break;
        default:
            LOG_DEBUG("Unsupported cid: %d\n", cid);
    }

    return;
}

// if one notification reached, we should execute below steps:
// 1. parse the current notification and get more necessary information.
// 2. pack all data to a temp C data struct.
// 3. repack C struct to a QT-C++ struct and trigger all notification.
static gint
mbim_common_notification_cb (MbimDevice   *Device,
                             MbimMessage   *notification,
                             gpointer      userdata)
{
    guint        cid         = 0;
    MbimService  service     = (MbimService)0;

    LOG_DEBUG("enter %s!\n", __func__);

    service = mbim_message_indicate_status_get_service (notification);
    cid     = mbim_message_indicate_status_get_cid (notification);

    switch (service) {
        case MBIM_SERVICE_BASIC_CONNECT:
            bc_ind_parse(Device, notification);
            break;
        case MBIM_SERVICE_MS_BASIC_CONNECT_EXTENSIONS:
            bcext_ind_parse(Device, notification);
            break;
        default:
            LOG_DEBUG("Unsupported mbim indication! service: %d, cid: %d\n", service, cid);
    }

    // here can't return 0 cause it will block ModemManager or other APP's indication func!
    return ERR;
}
/* -------------------End Notification related functions------------------- */

/* -------------------End mbim port init related functions------------------- */

/* -------------------Begin mbim message related functions------------------- */
static void
query_local_mccmnc_ready (MbimDevice   *device,
                                       GAsyncResult *res,
                                       gpointer userdata)
{
    g_autoptr(GError)                   error          =  nullptr;
    g_autoptr(MbimMessage)              response       =  nullptr;
    gint                                ret            =  ERR;
    MbimProvider                        *out_provider  =  nullptr;
    _SimData                            *pointer       =  (_SimData *)userdata;

    LOG_DEBUG("enter %s!\n", __func__);

    response = mbim_device_command_finish (device, res, &error);

    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        sem_post(&force_sync_sem);
        return;
    }

    if (!mbim_message_home_provider_response_parse (
            response,
            &out_provider,
            &error)) {
        LOG_ERROR("error: couldn't parse response message: %s\n", error->message);
        sem_post(&force_sync_sem);
        return;
    }

    strncpy(pointer->local_mccmnc, (const char *)(out_provider->provider_id), 6);

    if (pointer->imsi)
    {
        LOG_DEBUG("imsi: %s\n", pointer->imsi);
    }

    if (pointer->iccid)
    {
        LOG_DEBUG("iccid: %s\n", pointer->iccid);
    }

    if (pointer->local_mccmnc)
    {
        LOG_DEBUG("local_mccmnc: %s\n", pointer->local_mccmnc);
    }

    if (out_provider)
        mbim_provider_free(out_provider);

    sem_post(&force_sync_sem);
    return;
}

static int
query_local_mccmnc(GAsyncReadyCallback func_pointer, gpointer userdata)
{
    g_autoptr(MbimMessage)   request  =  nullptr;

    LOG_DEBUG("enter %s!\n", __func__);

    request = mbim_message_home_provider_query_new (nullptr);
    // main thread deal with callback, sub thread will exit without any deal!
    mbim_device_command (g_mbimdevice,
                        request,
                        5,
                        g_cancellable,
                        func_pointer,
                        userdata);

    return OK;
}

static void
query_subscriber_ready_status_ready (void *device,
                                     GAsyncResult *res,
                                     gpointer userdata)
{
    g_autoptr(GError)                   error          =  nullptr;
    g_autoptr(MbimMessage)              response       =  nullptr;
    gint                                ret            =  ERR;
    MbimSubscriberReadyState            ready_state    =  MBIM_SUBSCRIBER_READY_STATE_FAILURE;
    g_autofree gchar                    *subscriber_id =  nullptr;
    g_autofree gchar                    *sim_iccid     =  nullptr;
    _SimData                            *pointer       =  (_SimData *)userdata;

    LOG_DEBUG("enter %s!\n", __func__);

    LOG_DEBUG("pointer address: %p\n", pointer);
    response = mbim_device_command_finish ((MbimDevice *)device, res, &error);

    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        pointer->ready_state = ready_state;
        sem_post(&force_sync_sem);
        return;
    }

    if (!mbim_message_subscriber_ready_status_response_parse (
            response,
            &ready_state,
            &subscriber_id, // subscriber_id
            &sim_iccid, // sim_iccid
            nullptr, // ready_info
            nullptr, // telephone_numbers_count
            nullptr, // telephone number
            &error)) {
        LOG_ERROR("error: couldn't parse response message: %s\n", error->message);
        ready_state = MBIM_SUBSCRIBER_READY_STATE_FAILURE;
        sem_post(&force_sync_sem);
        return;
    }

    switch (ready_state) {
        case MBIM_SUBSCRIBER_READY_STATE_INITIALIZED:
            pointer->ready_state = ready_state;
            LOG_DEBUG("ready_state: %d\n", pointer->ready_state);
            if (subscriber_id)
            {
                strncpy(pointer->imsi, (const char*)subscriber_id, 15);
                LOG_DEBUG("imsi: %s\n", pointer->imsi);
            }

            if (sim_iccid)
            {
                strncpy(pointer->iccid, (const char*)sim_iccid, 31);
                LOG_DEBUG("iccid: %s\n", pointer->iccid);
            }
            // send home provider query request.
            ret = query_local_mccmnc((GAsyncReadyCallback)query_local_mccmnc_ready, userdata);
            if (ret != OK) {
                LOG_ERROR("executed Internal message failed!\n");
                sem_post(&force_sync_sem);
                return;
            }

            if (g_sim_inserted_flag) {
                return;
            }
            LOG_DEBUG("SIM card already inserted without notification!\n");
            g_sim_inserted_flag = TRUE;
            break;
        case MBIM_SUBSCRIBER_READY_STATE_NO_ESIM_PROFILE:
        case MBIM_SUBSCRIBER_READY_STATE_DEVICE_LOCKED:
        case MBIM_SUBSCRIBER_READY_STATE_NOT_INITIALIZED:
        case MBIM_SUBSCRIBER_READY_STATE_FAILURE:
        case MBIM_SUBSCRIBER_READY_STATE_SIM_NOT_INSERTED:
            // only send message back to caller.
            pointer->ready_state = ready_state;
            sem_post(&force_sync_sem);

            if (!g_sim_inserted_flag) {
                return;
            }
            LOG_DEBUG("SIM card already removed without notification!\n");
            g_sim_inserted_flag = FALSE;
            break;
        default:
            LOG_DEBUG("Unsupported SIM card ready state: %d!\n", ready_state);
    }

    // here won't free the sem because we need to use it on sub thread.
    return;
}

static gint
query_subscriber_ready_status(GAsyncReadyCallback func_pointer, int timeout, gpointer userdata)
{
    g_autoptr(MbimMessage)   request = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);
    request = mbim_message_subscriber_ready_status_query_new (nullptr);
    // main thread deal with callback, sub thread will exit without any deal!
    mbim_device_command (g_mbimdevice,
                        request,
                        timeout,
                        g_cancellable,
                        func_pointer,
                        userdata);
    return OK;
}

static void
query_signal_state_ready(MbimDevice *device,
                         GAsyncResult *res,
                         gpointer user_data)
{
    g_autoptr(GError)    error          = nullptr;
    _NetworkData         *pointer       = (_NetworkData *)user_data;
    guint32              rssi           = OK;
    MbimMessage          *response      = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);

    response = mbim_device_command_finish(device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        sem_post(&force_sync_sem);
        return;
    }

    if (!mbim_message_signal_state_response_parse (response,
                                                  &rssi,
                                                  nullptr,
                                                  nullptr,
                                                  nullptr,
                                                  nullptr,
                                                  &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        sem_post(&force_sync_sem);
        return;
    }
    pointer->rssi = (int)rssi;
    LOG_DEBUG("rssi:%d\n", pointer->rssi);

    sem_post(&force_sync_sem);
    return;
}

static gint
query_signal_state(GAsyncReadyCallback func_pointer, gpointer userdata)
{
    g_autoptr(MbimMessage)   request = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);
    request = mbim_message_signal_state_query_new(nullptr);
    // main thread deal with callback, sub thread will exit without any deal!
    mbim_device_command (g_mbimdevice,
                        request,
                        10,
                        g_cancellable,
                        func_pointer,
                        userdata);
    return OK;
}

static void
query_connect_state_ready(MbimDevice *device,
                          GAsyncResult *res,
                          gpointer userdata)
{
    g_autoptr(MbimMessage)   response          = NULL;
    g_autoptr(GError)        error             = NULL;
    MbimActivationState      activation_state  = MBIM_ACTIVATION_STATE_UNKNOWN;
    guint32                  nw_error          = OK;
    _NetworkData             *pointer          = (_NetworkData *)userdata;

    LOG_DEBUG("enter %s\n", __func__);

    response = mbim_device_command_finish (device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        sem_post(&force_sync_sem);
        return;
    }

    if (!mbim_message_connect_response_parse (
            response,
            NULL,  // session_id
            &activation_state,
            NULL,  // voice_call_state
            NULL,  // ip_type
            NULL,  // context_type
            &nw_error,
            &error)) {
        LOG_ERROR("error: couldn't parse response message: %s\n", error->message);
        sem_post(&force_sync_sem);
        return;
    }

    if (nw_error != OK) {
        LOG_DEBUG("Network error:%d\n", nw_error);
        sem_post(&force_sync_sem);
        return;
    }

    pointer->ConnectState = activation_state;
    LOG_DEBUG("Get Connect State: %d\n", pointer->ConnectState);

    sem_post(&force_sync_sem);
    return;
}

static gint
query_connect_state(GAsyncReadyCallback func_pointer, gpointer userdata)
{
    g_autoptr(MbimMessage)   request           = nullptr;
    g_autoptr(GError)        error             = NULL;

    LOG_DEBUG("enter %s!\n", __func__);
    // there might be possible that session_id is not 0.
    // but there isn't any suggestion on discuss meeting about any mbim message's details, so just keep it as 0.
    request = mbim_message_connect_query_new (0,  //session_id
                                              MBIM_ACTIVATION_STATE_UNKNOWN,
                                              MBIM_VOICE_CALL_STATE_NONE,
                                              MBIM_CONTEXT_IP_TYPE_DEFAULT,
                                              mbim_uuid_from_context_type (MBIM_CONTEXT_TYPE_INTERNET),
                                              0,
                                              &error);
    if (!request) {
        LOG_ERROR("error: couldn't create request: %s\n", error->message);
        return ERR;
    }

    mbim_device_command (g_mbimdevice,
                         request,
                         10,
                         g_cancellable,
                         func_pointer,
                         userdata);

    return OK;
}

static void
query_register_state_ready(MbimDevice *device,
                     GAsyncResult *res,
                     gpointer userdata)
{
    g_autoptr(GError)    error          = nullptr;
    _NetworkData         *pointer       = (_NetworkData *)userdata;
    MbimNwError          nw_error       = MBIM_NW_ERROR_NONE;
    MbimRegisterState    register_state = MBIM_REGISTER_STATE_UNKNOWN;
    g_autofree gchar     *provider_id   = NULL;
    MbimMessage          *response      = nullptr;
    int                  ret            = ERR;

    LOG_DEBUG("enter %s!\n", __func__);

    response = mbim_device_command_finish(device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        sem_post(&force_sync_sem);
        return;
    }

    if (!mbim_message_register_state_response_parse (response,
                                                    &nw_error,
                                                    &register_state,
                                                    nullptr,
                                                    nullptr,
                                                    nullptr,
                                                    &provider_id,
                                                    nullptr,
                                                    nullptr,
                                                    nullptr,
                                                    &error)) {
        LOG_ERROR("error: couldn't parse response message: %s\n", error->message);
        sem_post(&force_sync_sem);
        return;
    }

    if (nw_error != (MbimNwError)OK) {
        LOG_DEBUG("Network error:%d\n", nw_error);
        sem_post(&force_sync_sem);
        return;
    }

    pointer->RegisterState = register_state;
    LOG_DEBUG("RegisterState: %d\n", pointer->RegisterState);
    if (provider_id && strlen(provider_id) > 1) {
        strncpy(pointer->RoamMccmnc, provider_id, 7);
    } else {
        strncpy(pointer->RoamMccmnc, "NULL", 7);
    }
    LOG_DEBUG("roam mccmnc: %s\n", pointer->RoamMccmnc);

    switch (register_state) {
        case MBIM_REGISTER_STATE_HOME:
        case MBIM_REGISTER_STATE_ROAMING:
        case MBIM_REGISTER_STATE_PARTNER:
            // send signal state query request.
            ret = query_signal_state((GAsyncReadyCallback)query_signal_state_ready, userdata);
            if (ret != OK) {
                LOG_ERROR("executed Internal message failed!\n");
                sem_post(&force_sync_sem);
                return;
            }
            break;
        default:
            LOG_DEBUG("Invalid register state: %d, refuse to check signal!\n", register_state);
            pointer->rssi = 99;
            sem_post(&force_sync_sem);
    }

    // here won't post semaphore cause internal func will post it.
    return;
}

static gint
query_register_state(GAsyncReadyCallback func_pointer, int timeout, gpointer userdata)
{
    g_autoptr(MbimMessage)   request = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);

    request = mbim_message_register_state_query_new(nullptr);
    // main thread deal with callback, sub thread will exit without any deal!
    mbim_device_command (g_mbimdevice,
                        request,
                        timeout,
                        g_cancellable,
                        func_pointer,
                        userdata);
    return OK;
}

static void
query_radio_state_ready(MbimDevice *device,
                        GAsyncResult *res,
                        gpointer user_data)
{
    g_autoptr(GError)    error          = nullptr;
    MbimRadioSwitchState hw_radio_state = MBIM_RADIO_SWITCH_STATE_OFF;
    MbimRadioSwitchState sw_radio_state = MBIM_RADIO_SWITCH_STATE_OFF;
    _RadioData           *pointer       = (_RadioData *)user_data;
    MbimMessage          *response      = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);

    response = mbim_device_command_finish(device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        sem_post(&force_sync_sem);
        return;
    }

    // 解析Radio State响应
    mbim_message_radio_state_response_parse(response, &hw_radio_state, &sw_radio_state, &error);
    if (error) {
        LOG_ERROR("Failed to parse radio state response: %s\n", error->message);
    } else {
        LOG_DEBUG("Hardware Radio state: %s", (hw_radio_state == MBIM_RADIO_SWITCH_STATE_ON ? "ON" : "OFF"));
        LOG_DEBUG("Software Radio state: %s", (sw_radio_state == MBIM_RADIO_SWITCH_STATE_ON ? "ON" : "OFF"));
        pointer->hw_state = hw_radio_state;
        pointer->sw_state = sw_radio_state;
    }

    sem_post(&force_sync_sem);
    return;
}

static gint
query_radio_state(GAsyncReadyCallback func_pointer, int timeout, gpointer userdata)
{
    g_autoptr(MbimMessage)   request = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);

    request = mbim_message_radio_state_query_new(nullptr);
    // main thread deal with callback, sub thread will exit without any deal!
    mbim_device_command (g_mbimdevice,
                        request,
                        timeout,
                        g_cancellable,
                        func_pointer,
                        userdata);
    return OK;
}

static void
query_slot_info_status_ready(MbimDevice *device,
                                GAsyncResult *res,
                                gpointer userdata)
{
    g_autoptr(GError)         error          = nullptr;
    _SlotData                 *pointer       = (_SlotData *)userdata;
    MbimMessage               *response      = nullptr;
    int                       ret            = ERR;
    guint32                   slot_index     = 0;
    MbimUiccSlotState         slot_state     = MBIM_UICC_SLOT_STATE_UNKNOWN;

    LOG_DEBUG("enter %s!\n", __func__);

    response = mbim_device_command_finish(device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        sem_post(&force_sync_sem);
        return;
    }

    if (!mbim_message_ms_basic_connect_extensions_slot_info_status_response_parse (
            response,
            &slot_index,
            &slot_state,
            &error))
    {
        LOG_ERROR("error: couldn't parse response message: %s\n",
                  error->message);
        sem_post(&force_sync_sem);
        return;
    }

    pointer->slot_state[slot_index] = slot_state;
    LOG_DEBUG("slot %d state: %d\n", slot_index, slot_state);

    slot_index++;
    if (slot_index < pointer->ValidSlotNum)
    {
        ret = query_slot_info_status(
            slot_index, (GAsyncReadyCallback)query_slot_info_status_ready, userdata);
        if (ret != OK)
        {
            LOG_ERROR("executed Internal message failed!\n");
            sem_post(&force_sync_sem);
        }
        return;
    }

    sem_post(&force_sync_sem);
    return;
}

static gint
query_slot_info_status(guint32 slot_index, GAsyncReadyCallback func_pointer, gpointer userdata)
{
    g_autoptr(MbimMessage)   request = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);
    request = mbim_message_ms_basic_connect_extensions_slot_info_status_query_new (slot_index, NULL);    // main thread deal with callback, sub thread will exit without any deal!
    // main thread deal with callback, sub thread will exit without any deal!
    mbim_device_command (g_mbimdevice,
                        request,
                        5,
                        g_cancellable,
                        func_pointer,
                        userdata);
    return OK;
}

static void
query_slot_mapping_status_ready(MbimDevice *device,
                             GAsyncResult *res,
                             gpointer userdata)
{
    GError                    *error         = nullptr;
    _SlotData                 *pointer       = (_SlotData *)userdata;
    MbimMessage               *response      = nullptr;
    int                       ret            = ERR;
    guint32                   out_map_count  = 0;
    g_autofree MbimSlotArray  *out_slot_map  = NULL;
    int                       max_executor   = 0;

    LOG_DEBUG("enter %s!\n", __func__);

    response = mbim_device_command_finish(device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        if (error) g_error_free(error);
        sem_post(&force_sync_sem);
        return;
    }

    if (!mbim_message_ms_basic_connect_extensions_device_slot_mappings_response_parse
        (response,
         &out_map_count,
         &out_slot_map,
         &error)) {
        LOG_ERROR("error: couldn't parse response message: %s\n", error->message);
        if (error) g_error_free(error);
        sem_post(&force_sync_sem);
        return;
    }

    max_executor = pointer->ValidExecutorNum;
    if (out_map_count != max_executor) {
        LOG_DEBUG("Not match with sys-caps! Actual %d executors are activated!\n", out_map_count);
        if (error) g_error_free(error);
        sem_post(&force_sync_sem);
        return;
    }

    pointer->CurrentSlotIndex = out_slot_map[0]->slot;
    LOG_DEBUG("found work slot: %d\n", pointer->CurrentSlotIndex);

    ret = query_slot_info_status(0, (GAsyncReadyCallback)query_slot_info_status_ready, userdata);
    if (ret != OK) {
        LOG_ERROR("executed Internal message failed!\n");
        sem_post(&force_sync_sem);
        return;
    }
    // here won't post semaphore cause internal func will post it.
    return;
}

static int
query_slot_mapping_status(GAsyncReadyCallback func_pointer, gpointer userdata)
{
    g_autoptr(MbimMessage) request = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);
    request = mbim_message_ms_basic_connect_extensions_device_slot_mappings_query_new(nullptr);
    mbim_device_command (g_mbimdevice,
                        request,
                        5,
                        g_cancellable,
                        func_pointer,
                        userdata);
    return OK;
}

static void
query_sys_caps_ready (MbimDevice   *device,
                     GAsyncResult  *res,
                     gpointer      userdata)
{
    g_autoptr(MbimMessage) response         = NULL;
    g_autoptr(GError)      error            = NULL;
    guint32                number_executors = 0;
    guint32                number_slots     = 0;
    _SlotData              *pointer         = (_SlotData *)userdata;
    int                    ret              = ERR;

    LOG_DEBUG("enter %s\n", __func__);

    response = mbim_device_command_finish(device, res, &error);
    if (!response || !mbim_message_response_get_result(
                         response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error))
    {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        sem_post(&force_sync_sem);
        return;
    }

    if (!mbim_message_ms_basic_connect_extensions_sys_caps_response_parse(
            response, &number_executors, &number_slots, nullptr, nullptr,
            &error))
    {
        LOG_ERROR("error: couldn't parse response messages: %s\n",
                  error->message);
        sem_post(&force_sync_sem);
        return;
    }

    if (number_slots > 2) {
        LOG_DEBUG("number_slots: %d, convert it to 2!\n", number_slots);
        number_slots = 2;
    }

    if (number_executors < 1 || number_slots < 1) {
        LOG_DEBUG("Invalid data! slot: %d, executor: %d!\n", number_slots, number_executors);
        sem_post(&force_sync_sem);
        return;
    }
    else if (number_executors > 1 && number_slots > 1)
    {
        LOG_DEBUG("don't support DSDA yet!\n");
        sem_post(&force_sync_sem);
        return;
    }

    LOG_DEBUG("DSSA: %d slots with 1 activated!\n", number_slots);

    pointer->ValidSlotNum     = number_slots;
    pointer->ValidExecutorNum = number_executors;

    ret = query_slot_mapping_status((GAsyncReadyCallback)query_slot_mapping_status_ready, userdata);
    if (ret != OK) {
        LOG_ERROR("executed Internal message failed!\n");
        sem_post(&force_sync_sem);
        return;
    }

    // here won't post semaphore cause internal func will post it.
    return;
}

static int
query_sys_caps(GAsyncReadyCallback func_pointer, int timeout, gpointer userdata)
{
    g_autoptr(MbimMessage)   request = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);
    request = mbim_message_ms_basic_connect_extensions_device_slot_mappings_query_new(nullptr);
    // main thread deal with callback, sub thread will exit without any deal!
    mbim_device_command (g_mbimdevice,
                        request,
                        timeout,
                        g_cancellable,
                        func_pointer,
                        userdata);
    return OK;
}

static void
query_trace_log_state_ready(MbimDevice *device,
                            GAsyncResult *res,
                            gpointer userdata)
{
    g_autoptr(GError)    error          = nullptr;
    int                  *pointer       = (int *)userdata;
    MbimMessage          *response      = nullptr;
    guint32              trace_result   = OK;

    LOG_DEBUG("enter %s!\n", __func__);

    response = mbim_device_command_finish (device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        *pointer = ERR;
        goto TAIL;
    }

    if (!mbim_message_intel_tools_trace_config_response_parse (
            response,
            nullptr,
            &trace_result,
            &error)) {
        LOG_ERROR("error: couldn't parse response messages: %s\n", error->message);
        *pointer = ERR;
        goto TAIL;
    }

    *pointer = trace_result;
TAIL:
    sem_post(&force_sync_sem);
    return;
}

static int
query_trace_log_state(int InputData, GAsyncReadyCallback func_pointer, int timeout, gpointer userdata)
{
    g_autoptr(MbimMessage)   request = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);

    request = mbim_message_intel_tools_trace_config_query_new ((MbimTraceCommand)InputData, NULL);

    // main thread deal with callback, sub thread will exit without any deal!
    mbim_device_command (g_mbimdevice,
                        request,
                        timeout,
                        g_cancellable,
                        func_pointer,
                        userdata);
    return OK;
}

static int
set_trace_log_state(int trace_command, int trace_value, GAsyncReadyCallback func_pointer, int timeout, gpointer userdata)
{
    g_autoptr(MbimMessage)   request = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);
    if (trace_command < 0 || trace_value < 0 || !func_pointer || timeout <= 0 || !userdata) {
        LOG_ERROR("Invalid input param!\n");
        return ERR;
    }

    request = mbim_message_intel_tools_trace_config_set_new ((MbimTraceCommand)trace_command, trace_value, NULL);

    mbim_device_command (g_mbimdevice,
                         request,
                         timeout,
                         g_cancellable,
                         func_pointer,
                         userdata);
    return OK;
}

static void
no_resp_set_message_ready (MbimDevice   *device,
                           GAsyncResult *res,
                           gpointer     userdata)
{
    g_autoptr(GError)      error     = nullptr;
    g_autoptr(MbimMessage) response  = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);
    response = mbim_device_command_finish (device, res, &error);
    return;
}

static void
set_message_ready (MbimDevice   *device,
                   GAsyncResult *res,
                   gpointer     userdata)
{
    g_autoptr(GError)      error     = nullptr;
    guint32                ret_size  = 0;
    const guint8           *ret_str  = nullptr;
    g_autoptr(MbimMessage) response  = nullptr;
    int                    ret       = ERR;

    LOG_DEBUG("enter %s!\n", __func__);
    response = mbim_device_command_finish (device, res, &error);

    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        goto ERR;
    }

    if (!mbim_message_fibocom_at_command_response_parse (
            response,
            &ret_size,
            &ret_str,
            &error)) {
        LOG_ERROR("error: couldn't parse response message: %s\n", error->message);
        sem_post(&force_sync_sem);
        return;
    }
    ret = OK;

ERR:
    LOG_DEBUG("resp size:%d, data:%s\n", ret_size, ret_str);
    if (ret != OK && strstr((char *)userdata, "sys_reboot") != nullptr) {
        strncpy((char *)userdata, "OK", sizeof("OK"));
    } else if (ret != OK) {
        strncpy((char *)userdata, "ERROR", sizeof("ERROR"));
    }
    else {
        strncpy((char *) userdata, (const char *) ret_str, ret_size);
    }
    sem_post(&force_sync_sem);
    return;
}

static int
set_at_over_mbim_message(void                   *message,
                         guint32                len,
                         guint32                timeout,
                         GAsyncReadyCallback    callback,
                         gpointer               userdata)
{
    g_autoptr(MbimMessage)   request                              =  nullptr;
    guint8                   *req_str                             =  nullptr;
    guint32                  req_size                             =  0;
    guint32                  malloc_size                          =  0;
    gint                     ret                                  =  ERR;

    LOG_DEBUG("enter %s!\n", __func__);

    if (!message || !len || !timeout || !callback || !userdata) {
        LOG_ERROR("nullptr pointer!\n");
        sem_post(&force_sync_sem);
        return ERR;
    }

    req_size = strlen((const char *)message);

    malloc_size = (req_size > len ? req_size : len);
    malloc_size = malloc_size + 3;  // add more 1 bytes to store the '\0'

    req_str = (guint8 *)malloc(malloc_size);
    if (!req_str) {
        LOG_ERROR("malloc space failed!\n");
        sem_post(&force_sync_sem);
        return ERR;
    }
    memset(req_str, 0, malloc_size);

    strcpy((char *)req_str, (const char*)message);
    strcat((char *)req_str, "\r\n");

    LOG_DEBUG("original data:%s\n", (char *)message);
    request = mbim_message_fibocom_at_command_set_new (malloc_size, (const guint8 *)req_str, nullptr);

    // main thread deal with callback, sub thread will exit without any deal!
    mbim_device_command (g_mbimdevice,
                        request,
                        timeout,
                        g_cancellable,
                        callback,
                        userdata);

    if (req_str) {
        free(req_str);
    }
    req_str = nullptr;
    return OK;
}

static void
set_slot_mapping_message_ready(MbimDevice *device,
                               GAsyncResult *res,
                               gpointer userdata)
{
    g_autoptr(GError)         error          = nullptr;
    _SlotData                 *pointer       = (_SlotData *)userdata;
    MbimMessage               *response      = nullptr;
    int                       ret            = ERR;
    guint32                   out_map_count  = 0;
    g_autofree MbimSlotArray  *out_slot_map  = NULL;
    int                       max_executor   = 0;

    LOG_DEBUG("enter %s!\n", __func__);

    pointer->CurrentSlotIndex = ERR;
    response = mbim_device_command_finish(device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        goto TAIL;
    }

    if (!mbim_message_ms_basic_connect_extensions_device_slot_mappings_response_parse
            (response,
             &out_map_count,
             &out_slot_map,
             &error)) {
        LOG_ERROR("error: couldn't parse response message: %s\n", error->message);
        goto TAIL;
    }

    pointer->CurrentSlotIndex = out_slot_map[0]->slot;
    LOG_DEBUG("found work slot: %d\n", pointer->CurrentSlotIndex);

TAIL:
    sem_post(&force_sync_sem);
    return;
}

static int
set_slot_mapping_message(GAsyncReadyCallback func_pointer, int timeout, gpointer userdata)
{
    LOG_DEBUG("enter %s!\n", __func__);

    g_autoptr(MbimMessage)   request        =  nullptr;
    _SlotData                *InputData     =  (_SlotData *)userdata;
    gint64                   input_slot     =  ERR;
    MbimSlot                 *slot_index    =  NULL;
    g_autoptr(GPtrArray)     slot_array     =  NULL;

    input_slot = InputData->CurrentSlotIndex;
    if (input_slot < 0 || input_slot > 1) {
        LOG_ERROR("Invalid slot: %ld!\n", input_slot);
        sem_post(&force_sync_sem);
        return ERR;
    }

    slot_index       = g_new (MbimSlot, 1);
    slot_index->slot = (guint32) input_slot;
    slot_array       = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(slot_array, slot_index);

    request = mbim_message_ms_basic_connect_extensions_device_slot_mappings_set_new(slot_array->len, (const MbimSlot **)slot_array->pdata, NULL);
    // main thread deal with callback, sub thread will exit without any deal!
    mbim_device_command (g_mbimdevice,
                         request,
                         timeout,
                         g_cancellable,
                         func_pointer,
                         userdata);

    // here don't post semaphore cause callback will post it.
    return OK;
}

static int
set_radio_state_message(GAsyncReadyCallback func_pointer, int timeout, gpointer userdata)
{
    LOG_DEBUG("enter %s!\n", __func__);

    MbimRadioSwitchState     radio_state     =  MBIM_RADIO_SWITCH_STATE_OFF;
    g_autoptr(MbimMessage)   request         =  nullptr;
    _RadioData               *pointer        =  (_RadioData *)userdata;

    radio_state = (MbimRadioSwitchState)pointer->sw_state;

    request = mbim_message_radio_state_set_new (radio_state, NULL);
    mbim_device_command (g_mbimdevice,
                         request,
                         timeout,
                         g_cancellable,
                         func_pointer,
                         userdata);

    // here don't post semaphore cause callback will post it.
    return OK;
}

static void
set_trace_log_message_ready(MbimDevice *device,
                            GAsyncResult *res,
                            gpointer userdata)
{
    g_autoptr(MbimMessage)  response       = NULL;
    g_autoptr(GError)       error          = NULL;
    MbimTraceCommand        trace_command  = MBIM_TRACE_COMMAND_MODE;
    guint32                 trace_result   = OK;
    int                     *pointer       = (int *)userdata;

    LOG_DEBUG("enter %s!\n", __func__);

    response = mbim_device_command_finish (device, res, &error);
    if (!response || !mbim_message_response_get_result (response, MBIM_MESSAGE_TYPE_COMMAND_DONE, &error)) {
        LOG_ERROR("error: operation failed: %s\n", error->message);
        *pointer = ERR;
        goto TAIL;
    }

    // set ready don't contain data.
    *pointer = OK;
TAIL:
    sem_post(&force_sync_sem);
    return;
}

static int
set_trace_log_message(int input_type, int input_value, GAsyncReadyCallback func_pointer, int timeout, gpointer userdata)
{
    LOG_DEBUG("enter %s!\n", __func__);

    g_autoptr(MbimMessage)   request         =  nullptr;
    MbimTraceCommand         trace_command   =  MBIM_TRACE_COMMAND_MODE;
    guint32                  trace_value     =  0;

    trace_command = (MbimTraceCommand)input_type;
    trace_value   = input_value;

    request = mbim_message_intel_tools_trace_config_set_new (trace_command, trace_value, NULL);
    mbim_device_command (g_mbimdevice,
                         request,
                         timeout,
                         g_cancellable,
                         func_pointer,
                         userdata);
    return OK;
}
/* -------------------End mbim message related functions------------------- */

static void
mbim_device_open_ready (MbimDevice   *dev,
                        GAsyncResult *res,
                        gpointer     userdata)
{
    g_autoptr(GError) error = nullptr;

    LOG_DEBUG("enter %s!\n", __func__);

    if (!mbim_device_open_finish (dev, res, &error)) {
        LOG_ERROR("error: couldn't open the MbimDevice: %s\n",
                  error->message);
        sem_post(&force_sync_sem);
        return;
    }

    LOG_DEBUG("MBIM Device at '%s' ready\n", mbim_device_get_path_display (dev));

    g_mbim_device_init_flag = TRUE;

    g_signal_connect (dev,
                      MBIM_DEVICE_SIGNAL_INDICATE_STATUS,
                      G_CALLBACK (mbim_common_notification_cb),
                      userdata);

    sem_post(&force_sync_sem);
    return;
}

static void
mbim_device_new_ready (GObject      *unused,
                       GAsyncResult *res,
                       gpointer     userdata)
{
    g_autoptr(GError) error = nullptr;
    MbimDeviceOpenFlags open_flags = MBIM_DEVICE_OPEN_FLAGS_NONE;

    LOG_DEBUG("enter %s!\n", __func__);

    g_mbimdevice = mbim_device_new_finish (res, &error);
    if (!g_mbimdevice) {
        LOG_ERROR("error: couldn't create MbimDevice: %s\n",
                  error->message);
        sem_post(&force_sync_sem);
        return;
    }

    open_flags = MBIM_DEVICE_OPEN_FLAGS_PROXY;

    // Open the device
    mbim_device_open_full (g_mbimdevice,
                           open_flags,
                           30,
                           g_cancellable,
                           (GAsyncReadyCallback) mbim_device_open_ready,
                           nullptr);
    return;
}

static gint
mbim_port_init (char *mbimport) {
    g_autoptr(GError)         error                                = nullptr;
    g_autoptr(GFile)          file                                 = nullptr;
    gint                      ret                                  = ERR;

    LOG_DEBUG("enter %s!\n", __func__);

    if (!mbimport || strlen(mbimport) < 1) {
        LOG_ERROR("NULL pointer!\n");
        return ERR;
    }

    file = g_file_new_for_path(mbimport);
    if (!file)
    {
        LOG_ERROR("ERROR: GFile new failed!\n");
        // if not root user to run, here will report privillages error.
        return ERR;
    }

    free(mbimport);
    mbimport = NULL;

    g_cancellable = g_cancellable_new();
    if (!g_cancellable)
    {
        LOG_ERROR("ERROR: g_cancellable new failed!\n");
        return ERR;
    }

    sem_init(&force_sync_sem, 0, 0);
    // any error about device new will cause cb won't work.
    mbim_device_new (file, g_cancellable, (GAsyncReadyCallback)mbim_device_new_ready, nullptr);

    sem_wait(&force_sync_sem);
    sem_destroy(&force_sync_sem);

    if (!g_mbim_device_init_flag || !g_mbimdevice) {
        LOG_DEBUG("mbim device init failed!\n");
        return ERR;
    }

    LOG_DEBUG("mbim device init ok.\n");
    return OK;
}

static gint
check_mbim_port_exist(char *mbimport)
{
    gchar  command[GREP_MBIM_PORT_CMD_LEN + 9]  = {0};
    gchar  commandrsp[GREP_MBIM_PORT_CMD_LEN]   = {0};
    FILE   *fp                                  = nullptr;
    gint   ret                                  = ERR;

    LOG_DEBUG("enter %s!\n", __func__);
    sprintf(command, "find /dev -name %s*", mbimport);

    // execute command.
    fp = popen(command, RDONLY);
    if (fp == nullptr) {
        LOG_ERROR("execute command failed!\n");
        return ERR;
    }

    // get command's resp.
    while(fgets(commandrsp, GREP_MBIM_PORT_CMD_LEN, fp) != nullptr);

    // check command's execute result.
    ret = pclose(fp);
    if (ret != OK || strlen(commandrsp) < 1) {
        LOG_ERROR("error: can't find mbimport!\n");
        return ERR;
    }
    return OK;
}

/* -------------------Begin external functions------------------- */
int
HelperLinuxMbimDeviceOpen(char *portname) {

    LOG_DEBUG("enter %s!\n", __func__);
    int ret = ERR;
    char *mbimport = NULL;

    if (portname == nullptr || strlen(portname) < PORT_MIN_LEN) {
        LOG_ERROR("illegal input param!\n");
        return ERR;
    }

    if (!strstr(portname, "dev")) {
        ret = check_mbim_port_exist(portname);
        if (ret != OK) {
            LOG_ERROR("error: mbim port not existed! refused to do mbim port init!\n");
            return ERR;
        }
    }

    if (g_mbim_device_init_flag != TRUE || g_mbimdevice == nullptr) {
        mbimport = (char *)malloc(64);
        if (!mbimport) {
            LOG_ERROR("malloc failed!\n");
            return ERR;
        }
        memset(mbimport, 0, 64);
        // size 改成宏。
        if (!strstr(portname, "dev")) {
            snprintf(mbimport, 64, "/dev/%s", portname);
        }
        else {
            snprintf(mbimport, 64, "%s", portname);
        }

        ret = mbim_port_init(mbimport);
//        GThread *pointer1 = g_thread_new("init thread", (GThreadFunc)mbim_thread_func, nullptr);
//        GThread *pointer2 = g_thread_new("init port", (GThreadFunc), mbimport);
    }
    else {
        LOG_DEBUG("Existed mbim thread!\n");
        ret = OK;
    }
    return ret;
}

int
HelperLinuxMbimDeviceClose() {
    LOG_DEBUG("enter %s!\n", __func__);

    g_mbim_device_init_flag = FALSE;
    g_sim_inserted_flag     = FALSE;

    if (g_cancellable) {
        g_object_unref(g_cancellable);
        g_cancellable = nullptr;
    }

    if (g_mbimdevice) {
        g_object_unref(g_mbimdevice);
        g_mbimdevice = nullptr;
    }

    return OK;
}

int
HelperAfalMbimLinuxSimQuerySync(_SimData *OutputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);
    int ret = ERR;

    if (timeout <= 0) {
        LOG_ERROR("illegal input param!\n");
        return ret;
    }

    if (g_mbim_device_init_flag != TRUE) {
        LOG_ERROR("MBIM not init!\n");
        return ERR;
    }

    sem_init(&force_sync_sem, 0, 0);
    ret = query_subscriber_ready_status((GAsyncReadyCallback)query_subscriber_ready_status_ready, timeout, OutputData);
    if (ret != OK) {
        LOG_ERROR("executed failed!\n");
        sem_destroy(&force_sync_sem);
        return ERR;
    }

    sem_wait(&force_sync_sem);
    LOG_DEBUG("%s finished!\n", __func__);

    sem_destroy(&force_sync_sem);
    return OK;
}

int
HelperAfalMbimLinuxNetworkQuerySync(_NetworkData *OutputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);
    int ret = ERR;

    if (timeout <= 0) {
        LOG_ERROR("illegal input param!\n");
        return ret;
    }

    if (g_mbim_device_init_flag != TRUE) {
        LOG_ERROR("MBIM not init!\n");
        return ERR;
    }

    sem_init(&force_sync_sem, 0, 0);
    ret = query_register_state((GAsyncReadyCallback)query_register_state_ready, timeout, OutputData);
    if (ret != OK) {
        LOG_ERROR("executed failed!\n");
        sem_destroy(&force_sync_sem);
        return ERR;
    }

    sem_wait(&force_sync_sem);
    LOG_DEBUG("%s finished!\n", __func__);

    sem_destroy(&force_sync_sem);
    return OK;
}

int
HelperAfalMbimLinuxRadioQuerySync(_RadioData *OutputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);
    int ret = ERR;

    if (timeout <= 0) {
        LOG_ERROR("illegal input param!\n");
        return ret;
    }

    if (g_mbim_device_init_flag != TRUE) {
        LOG_ERROR("MBIM not init!\n");
        return ERR;
    }

    sem_init(&force_sync_sem, 0, 0);
    ret = query_radio_state((GAsyncReadyCallback)query_radio_state_ready, timeout, OutputData);
    if (ret != OK) {
        LOG_ERROR("executed failed!\n");
        return ERR;
    }
    sem_wait(&force_sync_sem);
    LOG_DEBUG("%s finished!\n", __func__);

    sem_destroy(&force_sync_sem);
    return OK;
}

int
HelperAfalMbimLinuxSlotQuerySync(_SlotData *OutputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);
    int ret = ERR;

    if (timeout <= 0) {
        LOG_ERROR("illegal input param!\n");
        return ret;
    }

    if (g_mbim_device_init_flag != TRUE) {
        LOG_ERROR("MBIM not init!\n");
        return ERR;
    }

    sem_init(&force_sync_sem, 0, 0);
    ret = query_sys_caps((GAsyncReadyCallback)query_sys_caps_ready, timeout, OutputData);
    if (ret != OK) {
        LOG_ERROR("executed failed!\n");
        sem_destroy(&force_sync_sem);
        return ERR;
    }

    sem_wait(&force_sync_sem);
    LOG_DEBUG("%s finished!\n", __func__);

    sem_destroy(&force_sync_sem);
    return OK;
}

int
HelperAfalMbimLinuxTraceLogQuerySync(char *InputData, int *OutputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);
    int ret     = ERR;
    int request = ERR;

    if (timeout <= 0) {
        LOG_ERROR("illegal input param!\n");
        return ret;
    }

    if (g_mbim_device_init_flag != TRUE) {
        LOG_ERROR("MBIM not init!\n");
        return ERR;
    }

    if (!InputData || strlen(InputData) < 4) {
        LOG_ERROR("Invalid input param!\n");
        return ERR;
    }

    if (strstr(InputData, "mode") != NULL) {
        request = 0;
    }
    else if (strstr(InputData, "level") != NULL) {
        request = 1;
    }
    else {
        LOG_ERROR("unsupported input param: %s\n", InputData);
        return ERR;
    }

    sem_init(&force_sync_sem, 0, 0);
    ret = query_trace_log_state(request, (GAsyncReadyCallback)query_trace_log_state_ready, timeout, OutputData);
    if (ret != OK) {
        LOG_ERROR("executed failed!\n");
        sem_destroy(&force_sync_sem);
        return ERR;
    }

    sem_wait(&force_sync_sem);
    LOG_DEBUG("%s finished!\n", __func__);

    sem_destroy(&force_sync_sem);
    return OK;
}

int
HelperAfalMbimLinuxAtOverMbimSetSync(char *input_str, char *output_str, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);
    int ret = ERR;

    if (input_str == nullptr || strlen(input_str) < MSG_MIN_LEN || timeout <= 0) {
        LOG_ERROR("illegal input param!\n");
        return ret;
    }

    if (output_str == nullptr) {
        LOG_ERROR("illegal output param!\n");
        return ret;
    }

    if (g_mbim_device_init_flag != TRUE) {
        LOG_ERROR("MBIM not init!\n");
        return ret;
    }
    
    if (strstr(input_str, "sys_reboot") != nullptr || strstr(input_str, "sysrq-trigger") != nullptr ||
        strcasestr(input_str, "forcedump") != nullptr) {
        // Destructive syscmd: modem may not respond; avoid blocking MBIM on reply.
        LOG_ERROR("no-response syscmd at command!\n");
        set_at_over_mbim_message(input_str, strlen(input_str), timeout, (GAsyncReadyCallback) no_resp_set_message_ready, output_str);
        strncpy(output_str, "OK", sizeof("OK"));
        ret = OK;
    }
    else {
        sem_init(&force_sync_sem, 0, 0);
        ret = set_at_over_mbim_message(input_str, strlen(input_str), timeout, (GAsyncReadyCallback) set_message_ready, output_str);
        if (ret != OK) {
            LOG_ERROR("executed failed!\n");
            sem_destroy(&force_sync_sem);
            return ret;
        }

        sem_wait(&force_sync_sem);
        LOG_DEBUG("%s finished!\n", __func__);

        sem_destroy(&force_sync_sem);
    }
    return ret;
}

int
HelperAfalMbimLinuxRadioSetSync(_RadioData *InputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);
    int ret = ERR;

    if (timeout <= 0) {
        LOG_ERROR("illegal input param!\n");
        return ret;
    }

    if (g_mbim_device_init_flag != TRUE) {
        LOG_ERROR("MBIM not init!\n");
        return ERR;
    }

    sem_init(&force_sync_sem, 0, 0);
    ret = ERR;
    ret = set_radio_state_message((GAsyncReadyCallback)query_radio_state_ready, timeout, InputData);
    if (ret != OK) {
        LOG_ERROR("executed failed!\n");
        sem_destroy(&force_sync_sem);
        return ERR;
    }

    sem_wait(&force_sync_sem);
    LOG_DEBUG("%s finished!\n", __func__);

    sem_destroy(&force_sync_sem);
    return OK;
}

int
HelperAfalMbimLinuxSlotSetSync(_SlotData *InputData, int timeout) {
    LOG_DEBUG("enter %s!\n", __func__);
    int ret = ERR;

    if (timeout <= 0) {
        LOG_ERROR("illegal input param!\n");
        return ret;
    }

    if (g_mbim_device_init_flag != TRUE) {
        LOG_ERROR("MBIM not init!\n");
        return ERR;
    }

    sem_init(&force_sync_sem, 0, 0);
    ret = set_slot_mapping_message((GAsyncReadyCallback)set_slot_mapping_message_ready, timeout, InputData);
    if (ret != OK) {
        LOG_ERROR("executed failed!\n");
        sem_destroy(&force_sync_sem);
        return ERR;
    }

    sem_wait(&force_sync_sem);
    LOG_DEBUG("%s finished!\n", __func__);

    sem_destroy(&force_sync_sem);
    return OK;
}

int
HelperAfalMbimLinuxTraceLogSetSync(char *InputStr, int *InputData, int timeout) {
    int ret     = ERR;
    int request = ERR;
    int OutputData = ERR;

    LOG_DEBUG("enter %s!\n", __func__);

    if (timeout <= 0 || !InputStr || strlen(InputStr) < 1) {
        LOG_ERROR("illegal input param!\n");
        return ret;
    }

    if (g_mbim_device_init_flag != TRUE) {
        LOG_ERROR("MBIM not init!\n");
        return ERR;
    }

    if (strstr(InputStr, "mode") != NULL) {
        request = 0;
    }
    else if (strstr(InputStr, "level") != NULL) {
        request = 1;
    }
    else {
        LOG_ERROR("unsupported input param: %s\n", InputStr);
        return ERR;
    }

    sem_init(&force_sync_sem, 0, 0);
    ret = set_trace_log_message(request, *InputData, (GAsyncReadyCallback)set_trace_log_message_ready, timeout, &OutputData);
    if (ret != OK) {
        LOG_ERROR("executed failed!\n");
        sem_destroy(&force_sync_sem);
        return ERR;
    }

    sem_wait(&force_sync_sem);
    if (OutputData != OK) {
        LOG_ERROR("executed failed!\n");
        sem_destroy(&force_sync_sem);
        return ERR;
    }

    ret = query_trace_log_state(request, (GAsyncReadyCallback)query_trace_log_state_ready, timeout, &OutputData);
    if (ret != OK) {
        LOG_ERROR("executed failed!\n");
        sem_destroy(&force_sync_sem);
        return ERR;
    }

    sem_wait(&force_sync_sem);
    LOG_DEBUG("OutputData: %d", OutputData);
    *InputData = OutputData;

    LOG_DEBUG("%s finished!\n", __func__);
    sem_destroy(&force_sync_sem);
    return OK;
}

/* -------------------End external functions------------------- */