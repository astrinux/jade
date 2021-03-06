/*
 * pjsip_handler.c
 *
 *  Created on: Dec 13, 2017
 *      Author: pchero
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <jansson.h>
#include <string.h>

#include "common.h"
#include "slog.h"
#include "utils.h"
#include "resource_handler.h"
#include "http_handler.h"
#include "ami_handler.h"
#include "ami_action_handler.h"
#include "conf_handler.h"
#include "publication_handler.h"

#include "pjsip_handler.h"

#define DEF_PJSIP_CONFNAME  "pjsip.conf"

#define DEF_PJSIP_CONFNAME_AOR            "jade.pjsip.aor.conf"
#define DEF_PJSIP_CONFNAME_AUTH           "jade.pjsip.auth.conf"
#define DEF_PJSIP_CONFNAME_CONTACT        "jade.pjsip.contact.conf"
#define DEF_PJSIP_CONFNAME_ENDPOINT       "jade.pjsip.endpoint.conf"
#define DEF_PJSIP_CONFNAME_IDENTIFY       "jade.pjsip.identify.conf"
#define DEF_PJSIP_CONFNAME_REGISTRATION   "jade.pjsip.registration.conf"
#define DEF_PJSIP_CONFNAME_TRANSPORT      "jade.pjsip.transports.conf"

#define DEF_DB_TABLE_PJSIP_AOR            "pjsip_aor"
#define DEF_DB_TABLE_PJSIP_AUTH           "pjsip_auth"
#define DEF_DB_TABLE_PJSIP_CONTACT        "pjsip_contact"
#define DEF_DB_TABLE_PJSIP_ENDPOINT       "pjsip_endpoint"
#define DEF_DB_TABLE_PJSIP_INDENTIFY      "pjsip_identify"
#define DEF_DB_TABLE_PJSIP_REGISTRATION   "pjsip_registration"
#define DEF_DB_TABLE_PJSIP_TRANSPORT      "pjsip_transport"
#define DEF_DB_TABLE_PJSIP_REGISTRATION_INBOUND         "pjsip_registration_inbound"
#define DEF_DB_TABLE_PJSIP_REGISTRATION_OUTBOUND        "pjsip_registration_outbound"

#define DEF_TRANSPORT_NAME_UDP   "jade-transport-udp"
#define DEF_TRANSPORT_NAME_TCP   "jade-transport-tcp"
#define DEF_TRANSPORT_NAME_WSS   "jade-transport-wss"


enum EN_OBJ_TYPES {
  EN_TYPE_AOR            = 1,
  EN_TYPE_AUTH,
  EN_TYPE_CONTACT,
  EN_TYPE_ENDPOINT,
  EN_TYPE_IDENTIFY       = 5,
  EN_TYPE_REGISTRATION,
  EN_TYPE_TRANSPORT
};

extern app* g_app;

static struct st_callback* g_callback_module;

static struct st_callback* g_callback_db_registration_outbound;
static struct st_callback* g_callback_cfg_endpoint;

static bool init_callbacks(void);
static bool term_callbacks(void);

static bool init_databases(void);
static bool init_pjsip_database_aor(void);
static bool init_pjsip_database_auth(void);
static bool init_pjsip_database_contact(void);
static bool init_pjsip_database_endpoint(void);
static bool init_pjsip_database_registration_inbound(void);
static bool init_pjsip_database_registration_outbound(void);

static bool init_configs(void);
static bool init_config_default_info(void);

static bool init_resources(void);

static bool term_pjsip_databases(void);

static bool create_config_aor_with_default_setting(const char* name);
static bool create_config_auth_with_default_setting(const char* name);
static bool create_config_endpoint_with_default_setting(const char* name, const char* context);

// db handlers
static bool db_create_aor_info(const json_t* j_data);
static bool db_update_aor_info(const json_t* j_data);
static bool db_delete_aor_info(const char* key);

static bool db_create_endpoint_info(const json_t* j_data);
static bool db_update_endpoint_info(const json_t* j_data);
static bool db_delete_endpoint_info(const char* key);

static bool db_create_auth_info(const json_t* j_data);
static bool db_update_auth_info(const json_t* j_data);
static bool db_delete_auth_info(const char* key);

static bool db_create_contact_info(const json_t* j_data);
static bool db_update_contact_info(const json_t* j_data);
static bool db_delete_contact_info(const char* key);

static bool db_create_registration_inbound_info(const json_t* j_data);
static bool db_update_registration_inbound_info(const json_t* j_data);
static bool db_delete_registration_inbound_info(const char* key);

static bool db_create_registration_outbound_info(const json_t* j_data);
static bool db_update_registration_outbound_info(const json_t* j_data);
static bool db_delete_registration_outbound_info(const char* key);

// cfg handlers
static bool cfg_create_aor_info(const json_t* j_data);
static bool cfg_update_aor_info(const json_t* j_data);
static bool cfg_delete_aor_info(const char* name);

static bool cfg_create_auth_info(const json_t* j_data);
static bool cfg_update_auth_info(const json_t* j_data);
static bool cfg_delete_auth_info(const char* name);

static bool cfg_create_contact_info(const json_t* j_data);
static bool cfg_update_contact_info(const json_t* j_data);
static bool cfg_delete_contact_info(const char* name);

static bool cfg_create_endpoint_info(const json_t* j_data);
static bool cfg_update_endpoint_info(const json_t* j_data);
static bool cfg_delete_endpoint_info(const char* name);

static bool cfg_create_identify_info(const json_t* j_data);
static bool cfg_update_identify_info(const json_t* j_data);
static bool cfg_delete_identify_info(const char* name);

static bool cfg_create_registration_info(const json_t* j_data);
static bool cfg_update_registration_info(const json_t* j_data);
static bool cfg_delete_registration_info(const char* name);

static bool cfg_create_transport_info(const json_t* j_data);
static bool cfg_update_transport_info(const json_t* j_data);
static bool cfg_delete_transport_info(const char* name);

static void execute_callbacks_module(enum EN_RESOURCE_UPDATE_TYPES type, const json_t* j_data);
static void execute_callbacks_db_registration_outbound(enum EN_RESOURCE_UPDATE_TYPES type, const json_t* j_data);
static void execute_callbacks_cfg_endpoint(enum EN_RESOURCE_UPDATE_TYPES type, const json_t* j_data);


bool pjsip_init_handler(void)
{
  int ret;

  slog(LOG_DEBUG, "Fired init_pjsip_handler.");

  // init config
  ret = init_configs();
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate pjsip config.");
    return false;
  }

  // init database
  ret = init_databases();
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate pjsip database.");
    return false;
  }

  ret = init_callbacks();
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate callbacks.");
    return false;
  }

  // init resources
  ret = init_resources();
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate resources.");
    return false;
  }

  return true;
}

bool pjsip_term_handler(void)
{
  int ret;
  char* timestamp;
  json_t* j_tmp;

  // execute pjsip unload callback
  timestamp = utils_get_utc_timestamp();
  j_tmp = json_pack("{s:s}", "timestamp", timestamp);
  sfree(timestamp);

  execute_callbacks_module(EN_RESOURCE_UNLOAD, j_tmp);
  json_decref(j_tmp);

  ret = term_pjsip_databases();
  if(ret == false) {
    slog(LOG_ERR, "Could not clear pjsip info.");
    return false;
  }

  ret = term_callbacks();
  if(ret == false) {
    slog(LOG_ERR, "Could not terminate callbacks.");
    return false;
  }

  return true;
}

bool pjsip_reload_handler(void)
{
  int ret;
  json_t* j_tmp;
  char* timestamp;

  // execute pjsip reload callback
  timestamp = utils_get_utc_timestamp();
  j_tmp = json_pack("{s:s}", "timestamp", timestamp);
  sfree(timestamp);

  execute_callbacks_module(EN_RESOURCE_RELOAD, j_tmp);
  json_decref(j_tmp);

  // init config
  ret = init_configs();
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate pjsip config.");
    return false;
  }

  // init database
  ret = init_databases();
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate pjsip database.");
    return false;
  }

  // init resources
  ret = init_resources();
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate resources.");
    return false;
  }

  return true;
}

/**
 * Initiate pjsip config file.
 * @return
 */
static bool init_configs(void)
{
  int ret;

  // aor
  ret = conf_add_external_config_file(DEF_PJSIP_CONFNAME, DEF_PJSIP_CONFNAME_AOR);
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate aor config.");
    return false;
  }

  // auth
  ret = conf_add_external_config_file(DEF_PJSIP_CONFNAME, DEF_PJSIP_CONFNAME_AUTH);
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate auth config.");
    return false;
  }

  // contact
  ret = conf_add_external_config_file(DEF_PJSIP_CONFNAME, DEF_PJSIP_CONFNAME_CONTACT);
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate contact config.");
    return false;
  }

  // endpoint
  ret = conf_add_external_config_file(DEF_PJSIP_CONFNAME, DEF_PJSIP_CONFNAME_ENDPOINT);
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate endpoint config.");
    return false;
  }

  // identify
  ret = conf_add_external_config_file(DEF_PJSIP_CONFNAME, DEF_PJSIP_CONFNAME_IDENTIFY);
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate identify config.");
    return false;
  }

  // registration
  ret = conf_add_external_config_file(DEF_PJSIP_CONFNAME, DEF_PJSIP_CONFNAME_REGISTRATION);
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate registration config.");
    return false;
  }

  // transport
  ret = conf_add_external_config_file(DEF_PJSIP_CONFNAME, DEF_PJSIP_CONFNAME_TRANSPORT);
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate transport config.");
    return false;
  }

  // initiate default info
  ret = init_config_default_info();
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate default config info.");
    return false;
  }

  return true;
}

static bool init_config_default_info(void)
{
  int ret;
  json_t* j_tmp;
  json_t* j_data;

  // create default transport udp
  j_tmp = pjsip_cfg_get_transport_info(DEF_TRANSPORT_NAME_UDP);
  if(j_tmp != NULL) {
    json_decref(j_tmp);
  }
  else {
    j_data = json_pack("{"
        "s:s, "
        "s:{s:s, s:s}"
        "}",
        "name", DEF_TRANSPORT_NAME_UDP,
        "data",
          "protocol",   "udp",
          "bind",       "0.0.0.0"
        );
    ret = cfg_create_transport_info(j_data);
    json_decref(j_data);
    if(ret == false) {
      slog(LOG_ERR, "Could not create default config info.");
      return false;
    }
  }

  // create default transport tcp
  j_tmp = pjsip_cfg_get_transport_info(DEF_TRANSPORT_NAME_TCP);
  if(j_tmp != NULL) {
    json_decref(j_tmp);
  }
  else  {
    j_data = json_pack("{"
        "s:s, "
        "s:{s:s, s:s}"
        "}",
        "name", DEF_TRANSPORT_NAME_TCP,
        "data",
          "protocol",   "tcp",
          "bind",       "0.0.0.0"
        );
    ret = cfg_create_transport_info(j_data);
    json_decref(j_data);
    if(ret == false) {
      slog(LOG_ERR, "Could not create default config info.");
      return false;
    }
  }

  // create default transport wss
  j_tmp = pjsip_cfg_get_transport_info(DEF_TRANSPORT_NAME_WSS);
  if(j_tmp != NULL) {
    json_decref(j_tmp);
  }
  else {
    j_data = json_pack("{"
        "s:s, "
        "s:{s:s, s:s}"
        "}",
        "name", DEF_TRANSPORT_NAME_WSS,
        "data",
          "protocol",   "wss",
          "bind",       "0.0.0.0"
        );
    ret = cfg_create_transport_info(j_data);
    json_decref(j_data);
    if(ret == false) {
      slog(LOG_ERR, "Could not create default config info.");
      return false;
    }
  }

  return true;
}

static bool init_resources(void)
{
  int ret;
  json_t* j_tmp;

  // send PJSIPShowEndpoints
  j_tmp = json_pack("{s:s}",
      "Action", "PJSIPShowEndpoints"
      );
  ret = ami_send_cmd(j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_ERR, "Could not send ami action. action[%s]", "PJSIPShowEndpoints");
    return false;
  }

  // send PJSIPShowRegistrationsOutbound
  j_tmp = json_pack("{s:s}",
      "Action", "PJSIPShowRegistrationsOutbound"
      );
  ret = ami_send_cmd(j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_ERR, "Could not send ami action. action[%s]", "PJSIPShowRegistrationsOutbound");
    return false;
  }

  // send PJSIPShowRegistrationsInbound
  j_tmp = json_pack("{s:s}",
      "Action", "PJSIPShowRegistrationsInbound"
      );
  ret = ami_send_cmd(j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_ERR, "Could not send ami action. action[%s]", "PJSIPShowRegistrationsInbound");
    return false;
  }

  return true;
}

static bool init_callbacks(void)
{
  // registration_outbound
  g_callback_module = utils_create_callback();
  g_callback_db_registration_outbound = utils_create_callback();
  g_callback_cfg_endpoint = utils_create_callback();

  return true;
}

static bool term_callbacks(void)
{
  utils_terminate_callback(g_callback_module);
  utils_terminate_callback(g_callback_db_registration_outbound);
  utils_terminate_callback(g_callback_cfg_endpoint);

  return true;
}

/**
 * Initiate pjsip database.
 * @return
 */
static bool init_databases(void)
{
  int ret;

  // aor
  ret = init_pjsip_database_aor();
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate aor database.");
    return false;
  }

  // auth
  ret = init_pjsip_database_auth();
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate auth database.");
    return false;
  }

  // contact
  ret = init_pjsip_database_contact();
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate contact database.");
    return false;
  }

  // endpoint
  ret = init_pjsip_database_endpoint();
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate endpoint database.");
    return false;
  }

  // registration_inbound
  ret = init_pjsip_database_registration_inbound();
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate registration_inbound database.");
    return false;
  }

  // registration_outbound
  ret = init_pjsip_database_registration_outbound();
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate registration_outbound database.");
    return false;
  }

  return true;
}

/**
 * Initiate aor database.
 * @return
 */
static bool init_pjsip_database_aor(void)
{
  int ret;
  const char* drop_table;
  const char* create_table;

  drop_table = "drop table if exists " DEF_DB_TABLE_PJSIP_AOR;

  create_table =
    "create table " DEF_DB_TABLE_PJSIP_AOR " ("

    // basic info
    "   object_type         varchar(255),"
    "   object_name         varchar(255),"
    "   endpoint_name       varchar(255),"

    // expiration
    "   minimum_expiration  int,"
    "   default_expiration  int,"
    "   maximum_expiration  int,"

    // qualify
    "   qualify_timeout     int,"
    "   qualify_frequency   int,"

    // contact
    "   contacts                    string,"
    "   max_contacts                int,"
    "   total_contacts              int,"
    "   contacts_registered         int,"
    "   remove_existing             varchar(255),"

    // etc
    "   mailboxes                   varchar(1023),"
    "   support_path                varchar(255),"
    "   voicemail_extension         varchar(1023),"
    "   authenticate_qualify        varchar(255),"
    "   outbound_proxy              varchar(1023),"

    // timestamp. UTC."
    "   tm_update         datetime(6),"   // update time."

    "   primary  key(object_name)"

    ");";

  // execute
  resource_exec_mem_sql(drop_table);
  ret = resource_exec_mem_sql(create_table);
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate database. database[%s]", DEF_DB_TABLE_PJSIP_AOR);
    return false;
  }

  return true;
}

/**
 * Initiate auth database.
 * @return
 */
static bool init_pjsip_database_auth(void)
{
  int ret;
  const char* drop_table;
  const char* create_table;

  drop_table = "drop table if exists " DEF_DB_TABLE_PJSIP_AUTH;

  create_table =
    "create table " DEF_DB_TABLE_PJSIP_AUTH " ("

    // basic info
    "   object_type         varchar(255),"
    "   object_name         varchar(1023),"
    "   username            varchar(1023),"
    "   endpoint_name       varchar(1023),"

    // credential
    "   auth_type           varchar(255),"
    "   password            varchar(1023),"
    "   md5_cred            varchar(1023),"

    // etc
    "   realm               varchar(255),"
    "   nonce_lifetime      int,"

    // timestamp. UTC."
    "   tm_update         datetime(6),"   // update time."

    "   primary key(object_name)"

    ");";

  // execute
  resource_exec_mem_sql(drop_table);
  ret = resource_exec_mem_sql(create_table);
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate database. database[%s]", DEF_DB_TABLE_PJSIP_AUTH);
    return false;
  }

  return true;
}

/**
 * Initiate contact database.
 * @return
 */
static bool init_pjsip_database_contact(void)
{
  int ret;
  const char* drop_table;
  const char* create_table;

  drop_table = "drop table if exists " DEF_DB_TABLE_PJSIP_CONTACT;

  create_table =
    "create table " DEF_DB_TABLE_PJSIP_CONTACT " ("

    "   uri             varchar(1023),"
    "   id              varchar(1023),"
    "   aor             varchar(1023),"
    "   endpoint_name   varchar(1023),"

    "   status  varchar(255),"

    "   round_trip_usec varchar(255),"
    "   user_agent      varchar(4095),"
    "   reg_expire      int,"
    "   via_address     varchar(1023),"
    "   call_id         varchar(1023),"

    "   authentication_qualify  int,"
    "   outbound_proxy          varchar(1023),"
    "   path                    varchar(1023),"

    "   qualify_frequency   int,"
    "   qualify_timout      int,"

    // timestamp. UTC."
    "   tm_update         datetime(6),"   // update time."

    "   primary key(uri)"

    ");";

  // execute
  resource_exec_mem_sql(drop_table);
  ret = resource_exec_mem_sql(create_table);
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate database. database[%s]", DEF_DB_TABLE_PJSIP_CONTACT);
    return false;
  }

  return true;
}

/**
 * Initiate endpoint database.
 * @return
 */
static bool init_pjsip_database_endpoint(void)
{
  int ret;
  const char* drop_table;
  const char* create_table;

  drop_table = "drop table if exists " DEF_DB_TABLE_PJSIP_ENDPOINT;

  create_table =
    "create table " DEF_DB_TABLE_PJSIP_ENDPOINT " ("

    // basic info
    "   object_type     varchar(255),"
    "   object_name     varchar(1023),"
    "   auth            varchar(1023),"
    "   aors            varchar(1023),"

    // context
    "   context             varchar(255),"
    "   message_context     varchar(1023),"
    "   subscribe_context   varchar(1023),"

    // mailbox
    "   mailboxes               varchar(1023),"
    "   voicemail_extension     varchar(1023),"
    "   incoming_mwi_mailbox    varchar(1023),"

    // allow and disallow
    "   disallow            varchar(255),"
    "   allow               varchar(1023),"
    "   allow_subscribe     varchar(1023),"
    "   allow_overlap       varchar(1023),"
    "   allow_transfer      varchar(1023),"

    "   webrtc          varchar(255),"

    "   dtmf_mode       varchar(1023),"

    // rtp
    "   rtp_engine            varchar(1023),"
    "   rtp_ipv6              varchar(1023),"
    "   rtp_symmetric         varchar(1023),"
    "   rtp_keepalive         int,"
    "   rtp_timeout           int,"
    "   rtp_timeout_hold      int,"
    "   asymmetric_rtp_codec  varchar(255),"

    "   rtcp_mux        varchar(255),"

    "   ice_support     varchar(1023),"
    "   use_ptime       varchar(1023),"

    "   force_rport         varchar(1023),"
    "   rewrite_contact     varchar(1023),"
    "   transport           varchar(1023),"
    "   moh_suggest         varchar(1023),"

    "   rel_100                 varchar(1023),"

    // timers
    "   timers                  varchar(1023),"
    "   timers_min_se           varchar(1023),"
    "   timers_sess_expires     int,"

    // outbound
    "   outbound_proxy          varchar(1023),"
    "   outbound_auth           varchar(1023),"

    "   identify_by             varchar(1023),"

    "   redirect_method         varchar(1023),"

    // direct media
    "   direct_media                    varchar(1023),"
    "   direct_media_method             varchar(1023),"
    "   direct_media_glare_mitigation   varchar(1023),"
    "   disable_direct_media_on_nat     varchar(1023),"

    "   connected_line_method           varchar(1023),"

    // caller id
    "   caller_id               varchar(1023),"
    "   caller_id_privacy       varchar(1023),"
    "   caller_id_tag           varchar(1023),"

    // trust id
    "   trust_id_inbound        varchar(1023),"
    "   trust_id_outbound       varchar(1023),"

    "   send_pai                varchar(1023),"

    "   rpid_immediate          varchar(255),"
    "   send_rpid               varchar(1023),"
    "   send_diversion          varchar(1023),"

    "   aggregate_mwi           varchar(1023),"

    // media
    "   media_address           varchar(1023),"
    "   media_encryption                varchar(1023),"
    "   media_encryption_optimistic     varchar(1023),"
    "   media_use_received_transport    varchar(1023),"

    "   use_avpf                        varchar(1023),"
    "   force_avp                       varchar(1023),"

    "   one_touch_recording             varchar(1023),"

    // prgoress
    "   inband_progress         varchar(1023),"
    "   refer_blind_progress    varchar(255),"

    // group
    "   call_group              varchar(1023),"
    "   pickup_group            varchar(1023),"
    "   named_call_group        varchar(1023),"
    "   named_pickup_group      varchar(1023),"

    "   device_state            varchar(1023),"
    "   device_state_busy_at    int,"

    // t38 fax
    "   fax_detect              varchar(1023),"
    "   fax_detect_time         int,"
    "   t38_udptl               varchar(1023),"
    "   t38_udptl_ec            varchar(1023),"
    "   t38_udptl_maxdatagram   int,"
    "   t38_udptl_nat           varchar(1023),"
    "   t38_udptl_ipv6          varchar(1023),"

    "   tone_zone               varchar(1023),"
    "   language                varchar(1023),"

    // record
    "   record_on_feature       varchar(1023),"
    "   record_off_feature      varchar(1023),"

    "   user_eq_phone       varchar(1023),"
    "   moh_passthrough     varchar(1023),"

    // sdp
    "   sdp_owner       varchar(1023),"
    "   sdp_session     varchar(1023),"

    // tos
    "   tos_audio       int,"
    "   tos_video       int,"

    // cos
    "   cos_audio       int,"
    "   cos_video       int,"

    "   sub_min_expiry      varchar(1023),"
    "   from_user           varchar(1023),"
    "   from_domain         varchar(1023),"

    // mwi
    "   mwi_from_user                       varchar(1023),"
    "   mwi_subscribe_replaces_unsolicited  varchar(255),"

    // dtls
    "   dtls_verify         varchar(1023),"
    "   dtls_rekey          int,"
    "   dtls_cert_file      varchar(1023),"
    "   dtls_private_key    varchar(1023),"
    "   dtls_cipher         varchar(1023),"
    "   dtls_ca_file        varchar(1023),"
    "   dtls_ca_path        varchar(1023),"
    "   dtls_setup          varchar(1023),"
    "   dtls_fingerprint    varchar(1023),"

    "   srtp_tag32      varchar(1023),"

    "   set_var                 varchar(1023),"

    "   account_code            varchar(1023),"
    "   preferred_codec_only    varchar(1023),"

    "   active_channels     varchar(1023),"

    // max streams
    "   max_audio_streams           int,"
    "   max_video_streams           int,"

    // acl
    "   acl                         varchar(1023),"
    "   contact_acl                 varchar(1023),"

    // etc
    "   g_726_non_standard          varchar(255),"
    "   notify_early_inuse_ringing  varchar(255),"
    "   bind_rtp_to_media_address   varchar(255),"
    "   bundle                      varchar(255),"


    // timestamp. UTC."
    "   tm_update         datetime(6),"   // update time."

    "   primary key(object_name)"

    ");";

  // execute
  resource_exec_mem_sql(drop_table);
  ret = resource_exec_mem_sql(create_table);
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate database. database[%s]", DEF_DB_TABLE_PJSIP_ENDPOINT);
    return false;
  }

  return true;
}

/**
 * Initiate registration_inbound database.
 * @return
 */
static bool init_pjsip_database_registration_inbound(void)
{
  int ret;
  const char* drop_table;
  const char* create_table;

  drop_table = "drop table if exists " DEF_DB_TABLE_PJSIP_REGISTRATION_INBOUND;

  create_table =
    "create table " DEF_DB_TABLE_PJSIP_REGISTRATION_INBOUND " ("

    "   object_type         varchar(255),"
    "   object_name         varchar(1023),"

    "   minimum_expiration      int,"
    "   maximum_expiration      int,"
    "   default_expiration      int,"

    "   authenticate_qualify      varchar(255),"
    "   qualify_timeout           real,"
    "   qualify_frequency         int,"

    "   mailboxes               varchar(255),"
    "   support_path            varchar(255),"
    "   voicemail_extension     varchar(255),"

    "   max_contacts              int,"
    "   contact                   varchar(255),"
    "   contacts                  varchar(255),"

    "   remove_existing           varchar(255),"
    "   outbound_proxy            varchar(255),"

    // timestamp. UTC."
    "   tm_update         datetime(6),"   // update time."

    "   primary key(object_name)"

    ");";

  // execute
  resource_exec_mem_sql(drop_table);
  ret = resource_exec_mem_sql(create_table);
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate database. database[%s]", DEF_DB_TABLE_PJSIP_REGISTRATION_INBOUND);
    return false;
  }

  return true;
}

/**
 * Initiate registration_outbound database.
 * @return
 */
static bool init_pjsip_database_registration_outbound(void)
{
  int ret;
  const char* drop_table;
  const char* create_table;

  drop_table = "drop table if exists " DEF_DB_TABLE_PJSIP_REGISTRATION_OUTBOUND;

  create_table =
    "create table " DEF_DB_TABLE_PJSIP_REGISTRATION_OUTBOUND " ("

    "   object_type         varchar(255),"
    "   object_name         varchar(1023),"

    "   status        varchar(255),"

    "   server_uri    varchar(255),"
    "   client_uri    varchar(255),"

    "   max_retries                 int,"
    "   retry_interval              int,"
    "   fatal_retry_interval        int,"
    "   forbidden_retry_interval    int,"

    "   auth_rejection_permanent    varchar(255),"
    "   support_path                varchar(255),"
    "   expiration                  int,"

    "   line          varchar(255),"
    "   transport     varchar(255),"
    "   contact_user  varchar(255),"
    "   endpoint      varchar(255),"

    "   outbound_auth     varchar(255),"
    "   outbound_proxy    varchar(255),"

    "   next_reg    int,"

    // timestamp. UTC."
    "   tm_update         datetime(6),"   // update time."

    "   primary key(object_name)"

    ");";

  // execute
  resource_exec_mem_sql(drop_table);
  ret = resource_exec_mem_sql(create_table);
  if(ret == false) {
    slog(LOG_ERR, "Could not initiate database. database[%s]", DEF_DB_TABLE_PJSIP_REGISTRATION_INBOUND);
    return false;
  }

  return true;
}

/**
 * Get corresponding pjsip_endpoint info.
 * @param name
 * @return
 */
json_t* pjsip_get_endpoint_info(const char* name)
{
  json_t* j_res;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return NULL;
  }
  slog(LOG_DEBUG, "Fired get_pjsip_endpoint_info. object_name[%s]", name);

  j_res = resource_get_mem_detail_item_key_string(DEF_DB_TABLE_PJSIP_ENDPOINT, "object_name", name);

  return j_res;
}

/**
 * Get all list of pjsip_endpoint info.
 * @param name
 * @return
 */
json_t* pjsip_get_endpoints_all(void)
{
  json_t* j_res;

  slog(LOG_DEBUG, "Fired get_pjsip_endpoints_all.");

  j_res = resource_get_mem_items(DEF_DB_TABLE_PJSIP_ENDPOINT, "*");

  return j_res;
}

/**
 * Get all list of pjsip_aor info.
 * @param name
 * @return
 */
json_t* pjsip_get_aors_all(void)
{
  json_t* j_res;

  slog(LOG_DEBUG, "Fired pjsip_aor.");

  j_res = resource_get_mem_items(DEF_DB_TABLE_PJSIP_AOR, "*");

  return j_res;
}

/**
 * Get detail info of given pjsip_aor key.
 * @param name
 * @return
 */
json_t* pjsip_get_aor_info(const char* key)
{
  json_t* j_res;

  if(key == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return NULL;
  }
  slog(LOG_DEBUG, "Fired get_pjsip_aors_info.");

  j_res = resource_get_mem_detail_item_key_string(DEF_DB_TABLE_PJSIP_AOR, "object_name", key);

  return j_res;
}

/**
 * Get all list of pjsip_auth info.
 * @param name
 * @return
 */
json_t* pjsip_get_auths_all(void)
{
  json_t* j_res;

  slog(LOG_DEBUG, "Fired get_pjsip_auths_all.");

  j_res = resource_get_mem_items(DEF_DB_TABLE_PJSIP_AUTH, "*");

  return j_res;
}

/**
 * Get detail info of given pjsip_auth key.
 * @param name
 * @return
 */
json_t* pjsip_get_auth_info(const char* key)
{
  json_t* j_res;

  if(key == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return NULL;
  }
  slog(LOG_DEBUG, "Fired get_pjsip_auth_info.");

  j_res = resource_get_mem_detail_item_key_string(DEF_DB_TABLE_PJSIP_AUTH, "object_name", key);

  return j_res;
}

/**
 * Get all list of pjsip_contact info.
 * @param name
 * @return
 */
json_t* pjsip_get_contacts_all(void)
{
  json_t* j_res;

  slog(LOG_DEBUG, "Fired get_pjsip_contacts_all.");

  j_res = resource_get_mem_items(DEF_DB_TABLE_PJSIP_CONTACT, "*");

  return j_res;
}

/**
 * Get detail info of given pjsip_contact key.
 * @param name
 * @return
 */
json_t* pjsip_get_contact_info(const char* key)
{
  json_t* j_res;

  if(key == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return NULL;
  }
  slog(LOG_DEBUG, "Fired get_pjsip_contact_info.");

  j_res = resource_get_mem_detail_item_key_string(DEF_DB_TABLE_PJSIP_CONTACT, "uri", key);

  return j_res;
}

static bool db_create_endpoint_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired db_create_endpoint_info.");

  // insert queue info
  ret = resource_insert_mem_item(DEF_DB_TABLE_PJSIP_ENDPOINT, j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not insert pjsip endpoint.");
    return false;
  }

  return true;
}

static bool db_update_endpoint_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired db_update_endpoint_info.");

  ret = resource_update_mem_item(DEF_DB_TABLE_PJSIP_ENDPOINT, "object_name", j_data);
  if(ret == false) {
    slog(LOG_WARNING, "Could not update pjsip endpoint info.");
    return false;
  }

  return true;
}

static bool db_delete_endpoint_info(const char* key)
{
  int ret;

  if(key == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired db_delete_endpoint_info. key[%s]", key);

  // delete info
  ret = resource_delete_mem_items_string(DEF_DB_TABLE_PJSIP_ENDPOINT, "object_name", key);
  if(ret == false) {
    slog(LOG_WARNING, "Could not delete pjsip_endpoint info. key[%s]", key);
    return false;
  }

  return true;
}

/**
 * Create pjsip endpoint info.
 * @param j_data
 * @return
 */
bool pjsip_create_endpoint_info(const json_t* j_data)
{
  int ret;
  const char* tmp_const;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired create_pjsip_endpoint_info.");

  // create info
  ret = db_create_endpoint_info(j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not insert pjsip endpoint.");
    return false;
  }

  // publish
  // get info
  tmp_const = json_string_value(json_object_get(j_data, "object_name"));
  j_tmp = pjsip_get_endpoint_info(tmp_const);
  if(j_tmp == NULL) {
    slog(LOG_ERR, "Could not get pjsip_endpoint info. id[%s]", tmp_const);
    return false;
  }

  // publish event
  ret = publication_publish_event_pjsip_endpoint(DEF_PUB_TYPE_CREATE, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_ERR, "Could not publish event.");
    return false;
  }

  return true;
}

/**
 * Update pjsip endpoint info.
 * @param j_data
 * @return
 */
bool pjsip_update_endpoint_info(const json_t* j_data)
{
  int ret;
  const char* tmp_const;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired update_pjsip_endpoint_info.");

  ret = db_update_endpoint_info(j_data);
  if(ret == false) {
    slog(LOG_WARNING, "Could not update pjsip endpoint info.");
    return false;
  }

  // publish
  // get info
  tmp_const = json_string_value(json_object_get(j_data, "object_name"));
  j_tmp = pjsip_get_endpoint_info(tmp_const);
  if(j_tmp == NULL) {
    slog(LOG_ERR, "Could not get pjsip_endpoint info. id[%s]", tmp_const);
    return false;
  }

  // publish event
  ret = publication_publish_event_pjsip_endpoint(DEF_PUB_TYPE_UPDATE, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_ERR, "Could not publish event.");
    return false;
  }

  return true;
}

/**
 * delete pjsip endpoint info.
 * @return
 */
bool pjsip_delete_endpoint_info(const char* key)
{
  int ret;
  json_t* j_tmp;

  if(key == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired delete_pjsip_endpoint_info. key[%s]", key);

  // get info
  j_tmp = pjsip_get_endpoint_info(key);
  if(j_tmp == NULL) {
    slog(LOG_ERR, "Could not get pjsip_endpoint info. id[%s]", key);
    return false;
  }

  // delete info
  ret = db_delete_endpoint_info(key);
  if(ret == false) {
    slog(LOG_WARNING, "Could not delete pjsip_endpoint info. key[%s]", key);
    json_decref(j_tmp);
    return false;
  }

  // publish
  // publish event
  ret = publication_publish_event_pjsip_endpoint(DEF_PUB_TYPE_DELETE, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_ERR, "Could not publish event.");
    return false;
  }

  return true;
}

static bool db_create_auth_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired db_create_auth_info.");

  // insert queue info
  ret = resource_insert_mem_item(DEF_DB_TABLE_PJSIP_AUTH, j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not insert pjsip auth.");
    return false;
  }

  return true;
}

static bool db_update_auth_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired db_update_auth_info.");

  // update
  ret = resource_update_mem_item(DEF_DB_TABLE_PJSIP_AUTH, "object_name", j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not update pjsip_auth info.");
    return false;
  }

  return true;
}

static bool db_delete_auth_info(const char* key)
{
  int ret;

  if(key == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired db_delete_auth_info. key[%s]", key);

  ret = resource_delete_mem_items_string(DEF_DB_TABLE_PJSIP_AUTH, "object_name", key);
  if(ret == false) {
    slog(LOG_WARNING, "Could not delete pjsip_auth info. key[%s]", key);
    return false;
  }

  return true;
}


/**
 * Create pjsip auth info.
 * @param j_data
 * @return
 */
bool pjsip_create_auth_info(const json_t* j_data)
{
  int ret;
  const char* tmp_const;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired create_pjsip_auth_info.");

  // insert queue info
  ret = db_create_auth_info(j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not insert pjsip auth.");
    return false;
  }

  // publish
  // get info
  tmp_const = json_string_value(json_object_get(j_data, "object_name"));
  j_tmp = pjsip_get_auth_info(tmp_const);
  if(j_tmp == NULL) {
    slog(LOG_ERR, "Could not get pjsip_auth info. object_name[%s]", tmp_const);
    return false;
  }

  // publish event
  ret = publication_publish_event_pjsip_auth(DEF_PUB_TYPE_CREATE, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_ERR, "Could not publish event.");
    return false;
  }

  return true;
}

/**
 * Update pjsip auth info.
 * @param j_data
 * @return
 */
bool pjsip_update_auth_info(const json_t* j_data)
{
  int ret;
  json_t* j_tmp;
  const char* tmp_const;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired update_pjsip_auth_info.");

  // update
  ret = db_update_auth_info(j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not update pjsip_auth info.");
    return false;
  }

  // publish
  // get info
  tmp_const = json_string_value(json_object_get(j_data, "object_name"));
  j_tmp = pjsip_get_auth_info(tmp_const);
  if(j_tmp == NULL) {
    slog(LOG_ERR, "Could not get pjsip_auth info. object_name[%s]", tmp_const);
    return false;
  }

  // publish event
  ret = publication_publish_event_pjsip_auth(DEF_PUB_TYPE_UPDATE, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_ERR, "Could not publish event.");
    return false;
  }

  return true;
}

/**
 * delete pjsip auth info.
 * @return
 */
bool pjsip_delete_auth_info(const char* key)
{
  int ret;
  json_t* j_tmp;

  if(key == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired delete_pjsip_auth_info. key[%s]", key);

  // get info
  j_tmp = pjsip_get_auth_info(key);
  if(j_tmp == NULL) {
    slog(LOG_ERR, "Could not get pjsip_auth info. object_name[%s]", key);
    return false;
  }

  // delete
  ret = db_delete_auth_info(key);
  if(ret == false) {
    slog(LOG_WARNING, "Could not delete pjsip_auth info. key[%s]", key);
    json_decref(j_tmp);
    return false;
  }

  // publish
  // publish event
  ret = publication_publish_event_pjsip_auth(DEF_PUB_TYPE_DELETE, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_ERR, "Could not publish event.");
    return false;
  }

  return true;
}

static bool db_create_aor_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired db_create_aor_info.");

  // insert info
  ret = resource_insert_mem_item(DEF_DB_TABLE_PJSIP_AOR, j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not insert pjsip aor.");
    return false;
  }

  return true;
}

static bool db_update_aor_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired db_update_aor_info.");

  // update
  ret = resource_update_mem_item(DEF_DB_TABLE_PJSIP_AOR, "object_name", j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not update pjsip_aor info.");
    return false;
  }

  return true;
}

static bool db_delete_aor_info(const char* key)
{
  int ret;

  if(key == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }

  ret = resource_delete_mem_items_string(DEF_DB_TABLE_PJSIP_AOR, "object_name", key);
  if(ret == false) {
    slog(LOG_WARNING, "Could not delete pjsip_aor info. key[%s]", key);
    return false;
  }

  return true;
}

/**
 * Create pjsip aor info.
 * @param j_data
 * @return
 */
bool pjsip_create_aor_info(const json_t* j_data)
{
  int ret;
  const char* tmp_const;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired create_pjsip_aor_info.");

  // create
  ret = db_create_aor_info(j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not insert pjsip aor.");
    return false;
  }

  // publish
  // get info
  tmp_const = json_string_value(json_object_get(j_data, "object_name"));
  j_tmp = pjsip_get_aor_info(tmp_const);
  if(j_tmp == NULL) {
    slog(LOG_ERR, "Could not get pjsip_aor info. object_name[%s]", tmp_const);
    return false;
  }

  // publish event
  ret = publication_publish_event_pjsip_aor(DEF_PUB_TYPE_CREATE, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_ERR, "Could not publish event.");
    return false;
  }

  return true;
}

/**
 * Update pjsip aor info.
 * @param j_data
 * @return
 */
bool pjsip_update_aor_info(const json_t* j_data)
{
  int ret;
  const char* tmp_const;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired update_pjsip_aor_info.");

  // update
  ret = db_update_aor_info(j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not update pjsip_aor info.");
    return false;
  }

  // publish
  // get info
  tmp_const = json_string_value(json_object_get(j_data, "object_name"));
  j_tmp = pjsip_get_aor_info(tmp_const);
  if(j_tmp == NULL) {
    slog(LOG_ERR, "Could not get pjsip_aor info. object_name[%s]", tmp_const);
    return false;
  }

  // publish event
  ret = publication_publish_event_pjsip_aor(DEF_PUB_TYPE_UPDATE, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_ERR, "Could not publish event.");
    return false;
  }

  return true;
}

/**
 * delete pjsip aor info.
 * @return
 */
bool pjsip_delete_aor_info(const char* key)
{
  int ret;
  json_t* j_tmp;

  if(key == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired delete_pjsip_aor_info. key[%s]", key);

  // publish
  // get info
  j_tmp = pjsip_get_aor_info(key);
  if(j_tmp == NULL) {
    slog(LOG_ERR, "Could not get pjsip_aor info. object_name[%s]", key);
    return false;
  }

  // delete
  ret = db_delete_aor_info(key);
  if(ret == false) {
    slog(LOG_WARNING, "Could not delete pjsip_aor info. key[%s]", key);
    return false;
  }

  // publish event
  ret = publication_publish_event_pjsip_aor(DEF_PUB_TYPE_DELETE, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_ERR, "Could not publish event.");
    return false;
  }

  return true;
}

static bool db_create_registration_inbound_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired db_create_registration_inbound_info.");

  // insert info
  ret = resource_insert_mem_item(DEF_DB_TABLE_PJSIP_REGISTRATION_INBOUND, j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not insert pjsip registration_inbound.");
    return false;
  }

  return true;
}

static bool db_update_registration_inbound_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired db_update_registration_inbound_info.");

  // update
  ret = resource_update_mem_item(DEF_DB_TABLE_PJSIP_REGISTRATION_INBOUND, "object_name", j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not update pjsip_registration_inbound info.");
    return false;
  }

  return true;
}

static bool db_delete_registration_inbound_info(const char* key)
{
  int ret;

  if(key == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired db_delete_registration_inbound_info. key[%s]", key);

  ret = resource_delete_mem_items_string(DEF_DB_TABLE_PJSIP_REGISTRATION_INBOUND, "object_name", key);
  if(ret == false) {
    slog(LOG_WARNING, "Could not delete pjsip_registration_inbound info. key[%s]", key);
    return false;
  }

  return true;
}

/**
 * Create pjsip registration_inbound info.
 * @param j_data
 * @return
 */
bool pjsip_create_registration_inbound_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired pjsip_create_registration_inbound_info.");

  // create
  ret = db_create_registration_inbound_info(j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not insert pjsip registration_inbound.");
    return false;
  }

  return true;
}

/**
 * Update pjsip registration_inbound info.
 * @param j_data
 * @return
 */
bool pjsip_update_registration_inbound_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired pjsip_update_registration_inbound_info.");

  // update
  ret = db_update_registration_inbound_info(j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not update pjsip_contact info.");
    return false;
  }

  return true;
}

/**
 * delete pjsip registration_inbound info.
 * @return
 */
bool pjsip_delete_registration_inbound_info(const char* key)
{
  int ret;

  if(key == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired pjsip_delete_registration_inbound_info. key[%s]", key);

  // delete
  ret = db_delete_registration_inbound_info(key);
  if(ret == false) {
    slog(LOG_WARNING, "Could not delete pjsip_registration_inbound info. key[%s]", key);
    return false;
  }

  return true;
}

static bool db_create_registration_outbound_info(const json_t* j_data)
{
  int ret;
  json_t* j_tmp;
  const char* key;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired db_create_registration_outbound_info.");

  // insert info
  ret = resource_insert_mem_item(DEF_DB_TABLE_PJSIP_REGISTRATION_OUTBOUND, j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not insert pjsip registration_outbound.");
    return false;
  }

  // get key
  key = json_string_value(json_object_get(j_data, "object_name"));
  if(key == NULL) {
    slog(LOG_NOTICE, "Could not get object_name info.");
    return false;
  }

  // get created info
  j_tmp = pjsip_get_registration_outbound_info(key);
  if(j_tmp == NULL) {
    slog(LOG_NOTICE, "Could not get created registration outbound info.");
    return false;
  }

  // execute registered callbacks
  execute_callbacks_db_registration_outbound(EN_RESOURCE_CREATE, j_tmp);
  json_decref(j_tmp);

  return true;
}

static bool db_update_registration_outbound_info(const json_t* j_data)
{
  int ret;
  const char* key;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired db_update_registration_outbound_info.");

  // update
  ret = resource_update_mem_item(DEF_DB_TABLE_PJSIP_REGISTRATION_OUTBOUND, "object_name", j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not update pjsip_registration_outbound info.");
    return false;
  }

  // get key
  key = json_string_value(json_object_get(j_data, "object_name"));
  if(key == NULL) {
    slog(LOG_NOTICE, "Could not get object_name info.");
    return false;
  }

  // get updated info
  j_tmp = pjsip_get_registration_outbound_info(key);
  if(j_tmp == NULL) {
    slog(LOG_NOTICE, "Could not get created registration outbound info.");
    return false;
  }

  // execute registered callbacks
  execute_callbacks_db_registration_outbound(EN_RESOURCE_UPDATE, j_tmp);
  json_decref(j_tmp);

  return true;
}

static bool db_delete_registration_outbound_info(const char* key)
{
  int ret;
  json_t* j_tmp;

  if(key == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired db_delete_registration_outbound_info. key[%s]", key);

  // get delete info
  j_tmp = pjsip_get_registration_outbound_info(key);
  if(j_tmp == NULL) {
    slog(LOG_NOTICE, "No delete info.");
    return false;
  }

  ret = resource_delete_mem_items_string(DEF_DB_TABLE_PJSIP_REGISTRATION_OUTBOUND, "object_name", key);
  if(ret == false) {
    slog(LOG_WARNING, "Could not delete pjsip_registration_outbound info. key[%s]", key);
    json_decref(j_tmp);
    return false;
  }

  // execute registered callbacks
  execute_callbacks_db_registration_outbound(EN_RESOURCE_DELETE, j_tmp);
  json_decref(j_tmp);

  return true;
}

json_t* pjsip_get_registration_outbounds_all(void)
{
  json_t* j_res;

  j_res = resource_get_mem_items(DEF_DB_TABLE_PJSIP_REGISTRATION_OUTBOUND, "*");
  if(j_res == NULL) {
    return NULL;
  }

  return j_res;
}

/**
 * Return the given registraion_outbound info.
 * @param key
 * @return
 */
json_t* pjsip_get_registration_outbound_info(const char* key)
{
  json_t* j_res;

  if(key == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return NULL;
  }

  j_res = resource_get_mem_detail_item_key_string(DEF_DB_TABLE_PJSIP_REGISTRATION_OUTBOUND, "object_name", key);
  if(j_res == NULL) {
    return NULL;
  }

  return j_res;
}

/**
 * Create pjsip registration_outbound info.
 * @param j_data
 * @return
 */
bool pjsip_create_registration_outbound_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired pjsip_create_registration_outbound_info.");

  // create
  ret = db_create_registration_outbound_info(j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not insert pjsip registration_outbound.");
    return false;
  }

  return true;
}

/**
 * Update pjsip registration_outbound info.
 * @param j_data
 * @return
 */
bool pjsip_update_registration_outbound_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired pjsip_update_registration_outbound_info.");

  // update
  ret = db_update_registration_outbound_info(j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not update pjsip registration_outbound info.");
    return false;
  }

  return true;
}

/**
 * delete pjsip registration_outbound info.
 * @return
 */
bool pjsip_delete_registration_outbound_info(const char* key)
{
  int ret;

  if(key == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired pjsip_delete_registration_outbound_info. key[%s]", key);

  // delete
  ret = db_delete_registration_outbound_info(key);
  if(ret == false) {
    slog(LOG_WARNING, "Could not delete pjsip_registration_outbound info. key[%s]", key);
    return false;
  }

  return true;
}

static bool db_create_contact_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired db_create_contact_info.");

  // insert info
  ret = resource_insert_mem_item(DEF_DB_TABLE_PJSIP_CONTACT, j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not insert pjsip contact.");
    return false;
  }

  return true;
}

static bool db_update_contact_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired db_update_contact_info.");

  // update
  ret = resource_update_mem_item(DEF_DB_TABLE_PJSIP_CONTACT, "uri", j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not update pjsip_contact info.");
    return false;
  }

  return true;
}

static bool db_delete_contact_info(const char* key)
{
  int ret;

  if(key == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired db_delete_contact_info. key[%s]", key);

  ret = resource_delete_mem_items_string(DEF_DB_TABLE_PJSIP_CONTACT, "uri", key);
  if(ret == false) {
    slog(LOG_WARNING, "Could not delete pjsip_contact info. key[%s]", key);
    return false;
  }

  return true;
}

/**
 * Create pjsip contact info.
 * @param j_data
 * @return
 */
bool pjsip_create_contact_info(const json_t* j_data)
{
  int ret;
  const char* tmp_const;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired create_pjsip_contact_info.");

  // insert info
  ret = db_create_contact_info(j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not insert pjsip contact.");
    return false;
  }

  // publish
  // get info
  tmp_const = json_string_value(json_object_get(j_data, "uri"));
  j_tmp = pjsip_get_contact_info(tmp_const);
  if(j_tmp == NULL) {
    slog(LOG_ERR, "Could not get pjsip_contact info. uri[%s]", tmp_const);
    return false;
  }

  // publish event
  ret = publication_publish_event_pjsip_contact(DEF_PUB_TYPE_CREATE, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_ERR, "Could not publish event.");
    return false;
  }

  return true;
}

/**
 * Update pjsip contact info.
 * @param j_data
 * @return
 */
bool pjsip_update_contact_info(const json_t* j_data)
{
  int ret;
  const char* tmp_const;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired update_pjsip_contact_info.");

  // update
  ret = db_update_contact_info(j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not update pjsip_contact info.");
    return false;
  }

  // publish
  // get info
  tmp_const = json_string_value(json_object_get(j_data, "uri"));
  j_tmp = pjsip_get_contact_info(tmp_const);
  if(j_tmp == NULL) {
    slog(LOG_ERR, "Could not get pjsip_contact info. uri[%s]", tmp_const);
    return false;
  }

  // publish event
  ret = publication_publish_event_pjsip_contact(DEF_PUB_TYPE_UPDATE, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_ERR, "Could not publish event.");
    return false;
  }

  return true;
}

/**
 * delete pjsip contact info.
 * @return
 */
bool pjsip_delete_contact_info(const char* key)
{
  int ret;
  json_t* j_tmp;

  if(key == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired delete_pjsip_contact_info. key[%s]", key);

  // get info
  j_tmp = pjsip_get_contact_info(key);
  if(j_tmp == NULL) {
    slog(LOG_ERR, "Could not get pjsip_contact info. uri[%s]", key);
    return false;
  }

  ret = db_delete_contact_info(key);
  if(ret == false) {
    slog(LOG_WARNING, "Could not delete pjsip_contact info. key[%s]", key);
    json_decref(j_tmp);
    return false;
  }

  // publish
  // publish event
  ret = publication_publish_event_pjsip_contact(DEF_PUB_TYPE_DELETE, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_ERR, "Could not publish event.");
    return false;
  }

  return true;
}

/**
 * Clear all pjsip resources.
 * @return
 */
static bool term_pjsip_databases(void)
{
  int ret;

  ret = resource_clear_mem_table(DEF_DB_TABLE_PJSIP_AOR);
  if(ret == false) {
    slog(LOG_ERR, "Could not clear pjsip_aor");
    return false;
  }

  ret = resource_clear_mem_table(DEF_DB_TABLE_PJSIP_AUTH);
  if(ret == false) {
    slog(LOG_ERR, "Could not clear pjsip_auth");
    return false;
  }

  ret = resource_clear_mem_table(DEF_DB_TABLE_PJSIP_CONTACT);
  if(ret == false) {
    slog(LOG_ERR, "Could not clear pjsip_contact");
    return false;
  }

  ret = resource_clear_mem_table(DEF_DB_TABLE_PJSIP_ENDPOINT);
  if(ret == false) {
    slog(LOG_ERR, "Could not clear pjsip_endpoint");
    return false;
  }



  return true;

}
/**
 * Delete pjsip aor info from the pjsip-aor config file
 * @param j_data
 * @return
 */
bool pjsip_cfg_delete_aor_info(const char* name)
{
  int ret;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired delete_config_pjsip_aor.");

  // delete
  ret = cfg_delete_aor_info(name);
  if(ret == false) {
    slog(LOG_ERR, "Could not delete pjsip aor.");
    return false;
  }

  return true;
}

/**
 * Delete pjsip auth info from the pjsip-auth config file
 * @param j_data
 * @return
 */
bool pjsip_cfg_delete_auth_info(const char* name)
{
  int ret;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired delete_config_pjsip_auth.");

  // delete
  ret = cfg_delete_auth_info(name);
  if(ret == false) {
    slog(LOG_ERR, "Could not delete pjsip auth.");
    return false;
  }

  return true;
}

bool pjsip_cfg_create_contact_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }

  // update
  ret = cfg_create_contact_info(j_data);
  if(ret == false) {
    return false;
  }

  return true;
}


bool pjsip_cfg_update_contact_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }

  // update
  ret = cfg_update_contact_info(j_data);
  if(ret == false) {
    return false;
  }

  return true;
}

/**
 * Delete pjsip contact info from the pjsip-contact config file
 * @param j_data
 * @return
 */
bool pjsip_cfg_delete_contact_info(const char* name)
{
  int ret;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired delete_config_pjsip_contact.");

  // delete
  ret = cfg_delete_contact_info(name);
  if(ret == false) {
    slog(LOG_ERR, "Could not delete pjsip contact.");
    return false;
  }

  return true;
}

json_t* pjsip_cfg_get_endpoint_info(const char* name)
{
  json_t* j_res;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return NULL;
  }

  j_res = conf_get_ast_section(DEF_PJSIP_CONFNAME_ENDPOINT, name);
  if(j_res == NULL) {
    return NULL;
  }

  return j_res;
}

/**
 * Delete pjsip endpoint info from the pjsip-endpoint config file
 * @param j_data
 * @return
 */
bool pjsip_cfg_delete_endpoint_info(const char* name)
{
  int ret;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired delete_config_pjsip_endpoint.");

  // delete
  ret = cfg_delete_endpoint_info(name);
  if(ret == false) {
    slog(LOG_ERR, "Could not delete pjsip endpoint.");
    return false;
  }

  return true;
}

/**
 * Delete pjsip identify info from the pjsip-indentify config file
 * @param j_data
 * @return
 */
bool pjsip_cfg_delete_identify_info(const char* name)
{
  int ret;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired delete_config_pjsip_identify.");

  // delete
  ret = cfg_delete_identify_info(name);
  if(ret == false) {
    slog(LOG_ERR, "Could not delete pjsip identify.");
    return false;
  }

  return true;
}

json_t* pjsip_cfg_get_registration_info(const char* name)
{
  json_t* j_res;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return NULL;
  }

  j_res = conf_get_ast_section(DEF_PJSIP_CONFNAME_REGISTRATION, name);
  if(j_res == NULL) {
    return NULL;
  }

  return j_res;
}

/**
 * Create pjsip registration info from the pjsip-registration config file
 * @param j_data
 * @return
 */
bool pjsip_cfg_create_registration_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired pjsip_cfg_create_registration_info.");

  // update
  ret = cfg_create_registration_info(j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not delete pjsip registration.");
    return false;
  }

  return true;
}

/**
 * Update pjsip registration info from the pjsip-registration config file
 * @param j_data
 * @return
 */
bool pjsip_cfg_update_registration_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired pjsip_cfg_update_registration_info.");

  // update
  ret = cfg_update_registration_info(j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not delete pjsip registration.");
    return false;
  }

  return true;
}

/**
 * Delete pjsip registration info from the pjsip-registration config file
 * @param j_data
 * @return
 */
bool pjsip_cfg_delete_registration_info(const char* name)
{
  int ret;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired delete_config_pjsip_registration.");

  // delete
  ret = cfg_delete_registration_info(name);
  if(ret == false) {
    slog(LOG_ERR, "Could not delete pjsip registration.");
    return false;
  }

  return true;
}

/**
 * Create pjsip transport info to the pjsip-transport config file
 * @param j_data
 * @return
 */
bool pjsip_cfg_create_transport_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired pjsip_cfg_create_transport_info.");

  ret = cfg_create_transport_info(j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not create pjsip transport.");
    return false;
  }

  return true;
}

/**
 * Update pjsip transport info to the pjsip-transport config file
 * @param j_data
 * @return
 */
bool pjsip_cfg_update_transport_info(const json_t* j_data)
{
  int ret;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired pjsip_cfg_update_transport_info.");

  ret = cfg_update_transport_info(j_data);
  if(ret == false) {
    slog(LOG_ERR, "Could not update pjsip transport.");
    return false;
  }

  return true;
}

/**
 * Delete pjsip transport info from the pjsip-transport config file
 * @param j_data
 * @return
 */
bool pjsip_cfg_delete_transport_info(const char* name)
{
  int ret;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired delete_config_pjsip_transport.");

  ret = cfg_delete_transport_info(name);
  if(ret == false) {
    slog(LOG_ERR, "Could not delete pjsip transport.");
    return false;
  }

  return true;
}

static bool cfg_create_aor_info(const json_t* j_data)
{
  int ret;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_create_aor_info.");

  j_tmp = json_deep_copy(j_data);
  json_object_set_new(json_object_get(j_tmp, "data"), "type", json_string("aor"));

  ret = conf_create_ast_section(DEF_PJSIP_CONFNAME_AOR, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not create config for pjsip aor.");
    return false;
  }

  return true;
}

static bool cfg_update_aor_info(const json_t* j_data)
{
  int ret;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_update_aor_info.");

  j_tmp = json_deep_copy(j_data);
  json_object_set_new(json_object_get(j_tmp, "data"), "type", json_string("aor"));

  ret = conf_update_ast_section(DEF_PJSIP_CONFNAME_AOR, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not update config for pjsip aor.");
    return false;
  }

  return true;
}

static bool cfg_delete_aor_info(const char* name)
{
  int ret;

  if((name == NULL)) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_delete_aor_info. name[%s]", name);

  ret = conf_delete_ast_section(DEF_PJSIP_CONFNAME_AOR, name);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not update config for pjsip aor.");
    return false;
  }

  return true;
}

static bool cfg_create_auth_info(const json_t* j_data)
{
  int ret;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_create_auth_info.");

  j_tmp = json_deep_copy(j_data);
  json_object_set_new(json_object_get(j_tmp, "data"), "type", json_string("auth"));

  ret = conf_create_ast_section(DEF_PJSIP_CONFNAME_AUTH, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not create config for pjsip auth.");
    return false;
  }

  return true;
}

static bool cfg_update_auth_info(const json_t* j_data)
{
  int ret;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_update_auth_info.");

  j_tmp = json_deep_copy(j_data);
  json_object_set_new(json_object_get(j_tmp, "data"), "type", json_string("auth"));

  ret = conf_update_ast_section(DEF_PJSIP_CONFNAME_AUTH, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not update config for pjsip auth.");
    return false;
  }

  return true;
}

static bool cfg_delete_auth_info(const char* name)
{
  int ret;

  if((name == NULL)) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_delete_auth_info. name[%s]", name);

  ret = conf_delete_ast_section(DEF_PJSIP_CONFNAME_AUTH, name);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not update config for pjsip auth.");
    return false;
  }

  return true;
}

static bool cfg_create_contact_info(const json_t* j_data)
{
  int ret;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_create_contact_info.");

  j_tmp = json_deep_copy(j_data);
  json_object_set_new(json_object_get(j_tmp, "data"), "type", json_string("contact"));

  ret = conf_create_ast_section(DEF_PJSIP_CONFNAME_CONTACT, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not create config for pjsip contact.");
    return false;
  }

  return true;
}

static bool cfg_update_contact_info(const json_t* j_data)
{
  int ret;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_update_contact_info.");

  j_tmp = json_deep_copy(j_data);
  json_object_set_new(json_object_get(j_tmp, "data"), "type", json_string("contact"));

  ret = conf_update_ast_section(DEF_PJSIP_CONFNAME_CONTACT, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not update config for pjsip contact.");
    return false;
  }

  return true;
}

static bool cfg_delete_contact_info(const char* name)
{
  int ret;

  if((name == NULL)) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_delete_contact_info. name[%s]", name);

  ret = conf_delete_ast_section(DEF_PJSIP_CONFNAME_CONTACT, name);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not update config for pjsip contact.");
    return false;
  }

  return true;
}

static bool cfg_create_endpoint_info(const json_t* j_data)
{
  int ret;
  json_t* j_tmp;
  const char* name;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_create_endpoint_info.");

  j_tmp = json_deep_copy(j_data);
  json_object_set_new(json_object_get(j_tmp, "data"), "type", json_string("endpoint"));

  ret = conf_create_ast_section(DEF_PJSIP_CONFNAME_ENDPOINT, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not create config for pjsip endpoint.");
    return false;
  }

  name = json_string_value(json_object_get(j_data, "name"));
  if(name == NULL) {
    slog(LOG_NOTICE, "Could not get name info.");
    return false;
  }

  // get created data
  j_tmp = pjsip_cfg_get_endpoint_info(name);
  if(j_tmp == NULL) {
    slog(LOG_NOTICE, "Could not get created cfg endpoint info.");
    return false;
  }

  // execute
  execute_callbacks_cfg_endpoint(EN_RESOURCE_CREATE, j_tmp);
  json_decref(j_tmp);

  return true;
}

static bool cfg_update_endpoint_info(const json_t* j_data)
{
  int ret;
  const char* name;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_update_endpoint_info.");

  j_tmp = json_deep_copy(j_data);
  json_object_set_new(json_object_get(j_tmp, "data"), "type", json_string("endpoint"));

  ret = conf_update_ast_section(DEF_PJSIP_CONFNAME_ENDPOINT, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not update config for pjsip endpoint.");
    return false;
  }

  name = json_string_value(json_object_get(j_data, "name"));
  if(name == NULL) {
    slog(LOG_NOTICE, "Could not get name info.");
    return false;
  }

  // get updated data
  j_tmp = pjsip_cfg_get_endpoint_info(name);
  if(j_tmp == NULL) {
    slog(LOG_NOTICE, "Could not get updated cfg endpoint info.");
    return false;
  }

  execute_callbacks_cfg_endpoint(EN_RESOURCE_UPDATE, j_tmp);
  json_decref(j_tmp);

  return true;
}

static bool cfg_delete_endpoint_info(const char* name)
{
  int ret;
  json_t* j_tmp;
  json_t* j_cb_data;

  if((name == NULL)) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_delete_endpoint_info. name[%s]", name);

  // get delete info
  j_tmp = pjsip_cfg_get_endpoint_info_data(name);
  if(j_tmp == NULL) {
    slog(LOG_NOTICE, "Could not get delete cfg endpoint info.");
    return false;
  }

  ret = conf_delete_ast_section(DEF_PJSIP_CONFNAME_ENDPOINT, name);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not update config for pjsip endpoint.");
    json_decref(j_tmp);
    return false;
  }

  j_cb_data = json_pack("{s:s, s:o}",
      "name", name,
      "data", j_tmp
      );

  execute_callbacks_cfg_endpoint(EN_RESOURCE_DELETE, j_cb_data);
  json_decref(j_cb_data);

  return true;
}

static bool cfg_create_identify_info(const json_t* j_data)
{
  int ret;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_create_identify_info.");

  j_tmp = json_deep_copy(j_data);
  json_object_set_new(json_object_get(j_tmp, "data"), "type", json_string("identify"));

  ret = conf_create_ast_section(DEF_PJSIP_CONFNAME_IDENTIFY, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not create config for pjsip identify.");
    return false;
  }

  return true;
}

static bool cfg_update_identify_info(const json_t* j_data)
{
  int ret;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_update_identify_info.");

  j_tmp = json_deep_copy(j_data);
  json_object_set_new(json_object_get(j_tmp, "data"), "type", json_string("identify"));

  ret = conf_update_ast_section(DEF_PJSIP_CONFNAME_IDENTIFY, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not update config for pjsip identify.");
    return false;
  }

  return true;
}

static bool cfg_delete_identify_info(const char* name)
{
  int ret;

  if((name == NULL)) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_delete_identify_info. name[%s]", name);

  ret = conf_delete_ast_section(DEF_PJSIP_CONFNAME_IDENTIFY, name);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not update config for pjsip identify.");
    return false;
  }

  return true;
}

static bool cfg_create_registration_info(const json_t* j_data)
{
  int ret;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_create_registration_info.");

  j_tmp = json_deep_copy(j_data);
  json_object_set_new(json_object_get(j_tmp, "data"), "type", json_string("registration"));

  ret = conf_create_ast_section(DEF_PJSIP_CONFNAME_REGISTRATION, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not create config for pjsip registration.");
    return false;
  }

  return true;
}

static bool cfg_update_registration_info(const json_t* j_data)
{
  int ret;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_update_registration_info.");

  j_tmp = json_deep_copy(j_data);

  json_object_set_new(json_object_get(j_tmp, "data"), "type", json_string("registration"));

  ret = conf_update_ast_section(DEF_PJSIP_CONFNAME_REGISTRATION, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not update config for pjsip registration.");
    return false;
  }

  return true;
}


static bool cfg_delete_registration_info(const char* name)
{
  int ret;

  if((name == NULL)) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_delete_registration_info. name[%s]", name);

  ret = conf_delete_ast_section(DEF_PJSIP_CONFNAME_REGISTRATION, name);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not update config for pjsip registration.");
    return false;
  }

  return true;
}

static bool cfg_create_transport_info(const json_t* j_data)
{
  int ret;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_create_transport_info.");

  j_tmp = json_deep_copy(j_data);
  json_object_set_new(json_object_get(j_tmp, "data"), "type", json_string("transport"));

  ret = conf_create_ast_section(DEF_PJSIP_CONFNAME_TRANSPORT, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not create config for pjsip transport.");
    return false;
  }

  return true;
}

static bool cfg_update_transport_info(const json_t* j_data)
{
  int ret;
  json_t* j_tmp;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_update_transport_info.");

  j_tmp = json_deep_copy(j_data);
  json_object_set_new(json_object_get(j_tmp, "data"), "type", json_string("transport"));

  ret = conf_update_ast_section(DEF_PJSIP_CONFNAME_TRANSPORT, j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not update config for pjsip transport.");
    return false;
  }

  return true;
}

static bool cfg_delete_transport_info(const char* name)
{
  int ret;

  if((name == NULL)) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired cfg_delete_registration_info. name[%s]", name);

  ret = conf_delete_ast_section(DEF_PJSIP_CONFNAME_TRANSPORT, name);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not update config for pjsip transport.");
    return false;
  }

  return true;
}

/**
 * Create aor config with default setting.
 * @param name
 * @return
 */
static bool create_config_aor_with_default_setting(const char* name)
{
  int ret;
  json_t* j_tmp;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }

  // create default data
  j_tmp = json_pack("{s:s, s:{s:s, s:s}}",
      "name",   name,
      "data",
        "max_contacts",     "10",
        "remove_existing",  "yes"
      );

  // create default aor info
  ret = cfg_create_aor_info(j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    return false;
  }

  return true;
}

static bool create_config_auth_with_default_setting(const char* name)
{
  int ret;
  char* password;
  json_t* j_tmp;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired create_config_auth_with_default_setting. name[%s]", name);

  // create default data
  password = utils_gen_uuid();
  j_tmp = json_pack("{s:s, s:{s:s, s:s, s:s}}",
      "name",   name,
      "data",
        "auth_type",    "userpass",
        "username",     name,
        "password",     password
      );
  sfree(password);

  // create default auth info
  ret = cfg_create_auth_info(j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    return false;
  }

  return true;
}

static bool create_config_endpoint_with_default_setting(const char* name, const char* context)
{
  int ret;
  json_t* j_tmp;
  const char* cert_file;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired create_config_endpoint_with_default_setting. name[%s], context[%s]", name, context);

  cert_file = json_string_value(json_object_get(json_object_get(g_app->j_conf, "pjsip"), "dtls_cert_file"));

  j_tmp = json_pack("{"
      "s:s, "
      "s: {"
        "s:s, s:s, "
        "s:s, "
        "s:s, s:s, "

        "s:s, s:s, s:s, "
        "s:s, s:s, "
        "s:s, "
        "s:s, s:s"
        "}"
      "}",

      "name",   name,
      "data",
        "aors",         name,
        "auth",         name,

        "use_avpf",     "yes",

        "media_encryption",               "dtls",
        "media_use_received_transport",   "yes",


        "dtls_cert_file",   cert_file,
        "dtls_verify",      "fingerprint",
        "dtls_setup",       "actpass",

        "ice_support",    "yes",
        "rtcp_mux",       "yes",

        "context",        context,

        "allow",          "opus",
        "allow",          "ulaw"
      );

  // create default info
  ret = cfg_create_endpoint_info(j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    return false;
  }

  return true;
}

/**
 * Create pjsip account(aor, auth, endpoint) with default settings.
 * @param target
 * @param context
 * @return
 */
bool pjsip_create_account_with_default_setting(const char* target, const char* context)
{
  int ret;

  if((target == NULL) || (context == NULL)) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }

  // create aor
  ret = create_config_aor_with_default_setting(target);
  if(ret == false) {
    return false;
  }

  // create auth
  ret = create_config_auth_with_default_setting(target);
  if(ret == false) {
    return false;
  }

  // create endpoint
  ret = create_config_endpoint_with_default_setting(target, context);
  if(ret == false) {
    return false;
  }

  return ret;
}

/**
 * Delete pjsip account(aor, auth, endpoint).
 * @param target_name
 * @return
 */
bool pjsip_delete_account(const char* account)
{
  int ret;

  if(account == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired pjsip_delete_target_all. account[%s]", account);

  // delete aor
  ret = pjsip_cfg_delete_aor_info(account);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not delete aor info.");
    return false;
  }

  // delete auth
  ret = pjsip_cfg_delete_auth_info(account);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not delete auth info.");
    return false;
  }

  // delete endpoint
  ret = pjsip_cfg_delete_endpoint_info(account);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not delete endpoint info.");
    return false;
  }

  return true;
}

bool pjsip_is_exist_endpoint(const char* target)
{
  json_t* j_tmp;

  if(target == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }

  // check db
  j_tmp = pjsip_get_endpoint_info(target);
  if(j_tmp != NULL) {
    json_decref(j_tmp);
    return true;
  }
  json_decref(j_tmp);

  // check config
  j_tmp = conf_get_ast_current_config_info(DEF_PJSIP_CONFNAME_ENDPOINT);
  if(json_object_get(j_tmp, target) != NULL) {
    json_decref(j_tmp);
    return true;
  }
  json_decref(j_tmp);

  return false;
}

/**
 * Reload asterisk pjsip module
 * @return
 */
bool pjsip_reload_config(void)
{
  int ret;

  ret = ami_action_moduleload("res_pjsip", "reload");
  if(ret == false) {
    return false;
  }

  return true;
}

json_t* pjsip_cfg_get_aor_info_data(const char* name)
{
  json_t* j_res;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return NULL;
  }

  j_res = conf_get_ast_section_data(DEF_PJSIP_CONFNAME_AOR, name);
  if(j_res == NULL) {
    return NULL;
  }

  return j_res;
}

json_t* pjsip_cfg_get_auth_info_data(const char* name)
{
  json_t* j_res;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return NULL;
  }

  j_res = conf_get_ast_section_data(DEF_PJSIP_CONFNAME_AUTH, name);
  if(j_res == NULL) {
    return NULL;
  }

  return j_res;
}

json_t* pjsip_cfg_get_contact_info_data(const char* name)
{
  json_t* j_res;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return NULL;
  }

  j_res = conf_get_ast_section_data(DEF_PJSIP_CONFNAME_CONTACT, name);
  if(j_res == NULL) {
    return NULL;
  }

  return j_res;
}

json_t* pjsip_cfg_get_endpoint_info_data(const char* name)
{
  json_t* j_res;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return NULL;
  }

  j_res = conf_get_ast_section_data(DEF_PJSIP_CONFNAME_ENDPOINT, name);
  if(j_res == NULL) {
    return NULL;
  }

  return j_res;
}

json_t* pjsip_cfg_get_identify_info_data(const char* name)
{
  json_t* j_res;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return NULL;
  }

  j_res = conf_get_ast_section_data(DEF_PJSIP_CONFNAME_IDENTIFY, name);
  if(j_res == NULL) {
    return NULL;
  }

  return j_res;
}

json_t* pjsip_cfg_get_registrations_all(void)
{
  json_t* j_res;

  j_res = conf_get_ast_sections_all(DEF_PJSIP_CONFNAME_REGISTRATION);
  if(j_res == NULL) {
    return NULL;
  }

  return j_res;
}

json_t* pjsip_cfg_get_registration_info_data(const char* name)
{
  json_t* j_res;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return NULL;
  }

  j_res = conf_get_ast_section_data(DEF_PJSIP_CONFNAME_REGISTRATION, name);
  if(j_res == NULL) {
    return NULL;
  }

  return j_res;
}

json_t* pjsip_cfg_get_transport_info(const char* name)
{
  json_t* j_res;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return NULL;
  }

  j_res = conf_get_ast_section(DEF_PJSIP_CONFNAME_TRANSPORT, name);
  if(j_res == NULL) {
    return NULL;
  }

  return j_res;
}

bool pjsip_cfg_create_registration_with_default_info(const char* name, const char* server_uri, const char* client_uri)
{
  int ret;
  json_t* j_tmp;

  if((name == NULL) || (server_uri == NULL) || (client_uri == NULL)) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }

  j_tmp = json_pack("{"
      "s:s, "
      "s:{"
        "s:s, s:s, s:s, "
        "s:s, "
        "s:s, s:s, s:s "
        "}"
      "}",

      "name",   name,
      "data",

        "outbound_auth",  name,
        "server_uri",   server_uri,
        "client_uri",   client_uri,

        "retry_interval", "60",

        "line",       "yes",
        "endpoint",   name,
        "transport",  DEF_TRANSPORT_NAME_UDP
      );

  ret = cfg_create_registration_info(j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not create registration with default info.");
    return false;
  }

  return true;
}

bool pjsip_cfg_create_auth_info(const char* name, const char* username, const char* password)
{
  int ret;
  json_t* j_tmp;

  if((name == NULL) || (username == NULL) || (password == NULL)) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }

  // create auth
  j_tmp = json_pack("{s:s, s:{s:s, s:s, s:s}}",
      "name",       name,
      "data",
        "auth_type",    "userpass",
        "username",     username,
        "password",     password
      );

  ret = cfg_create_auth_info(j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not create auth info.");
    return false;
  }

  return true;
}

bool pjsip_cfg_update_auth_info(const char* name, const json_t* j_data)
{
  int ret;
  json_t* j_tmp;

  if((name == NULL) || (j_data == NULL)) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired pjsip_cfg_update_auth_info. name[%s]", name);

  j_tmp = json_pack("{s:s, s:O}",
      "name",   name,
      "data",   j_data
      );

  ret = cfg_update_auth_info(j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not update auth info.");
    return false;
  }

  return true;
}

bool pjsip_cfg_create_aor_info(const char* name, const char* contact)
{
  int ret;
  json_t* j_tmp;

  if((name == NULL) || (contact == NULL)) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }

  j_tmp = json_pack("{s:s, s:{s:s}}",
      "name",   name,
      "data",
        "contact", contact
      );

  ret = cfg_create_aor_info(j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not create aor info.");
    return false;
  }

  return true;
}

bool pjsip_cfg_update_aor_info_data(const char* name, const json_t* j_data)
{
  int ret;
  json_t* j_tmp;

  if((name == NULL) || (j_data == NULL)) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired pjsip_cfg_update_aor_info. name[%s]", name);

  j_tmp = json_pack("{s:s, s:O}",
      "name",   name,
      "data",   j_data
      );

  ret = cfg_update_aor_info(j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not update aor info.");
    return false;
  }

  return true;
}

bool pjsip_cfg_create_endpoint_with_default_info(const char* name, const char* context)
{
  int ret;
  json_t* j_tmp;

  if((name == NULL) || (context == NULL)) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }

  j_tmp = json_pack("{s:s, s:{s:s, s:s, s:s, s:s}}",
      "name",           name,
      "data",
        "outbound_auth",  name,
        "aors",           name,
        "context",        context,
        "allow",          "ulaw"
      );

  ret = cfg_create_endpoint_info(j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not create endpoint info.");
    return false;
  }

  return true;
}

bool pjsip_cfg_update_endpoint_info_data(const char* name, const json_t* j_data)
{
  int ret;
  json_t* j_tmp;

  if((name == NULL) || (j_data == NULL)) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired pjsip_cfg_update_endpoint_info. name[%s]", name);

  j_tmp = json_pack("{s:s, s:O}",
      "name",   name,
      "data",   j_data
      );

  ret = cfg_update_endpoint_info(j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not update endpoint info.");
    return false;
  }

  return true;
}

bool pjsip_cfg_create_identify_info(const char* name, const char* match)
{
  int ret;
  json_t* j_tmp;

  if((name == NULL) || (match == NULL)) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }

  j_tmp = json_pack("{"
      "s:s, "
      "s:{s:s, s:s}"
      "}",

      "name",     name,
      "data",
        "endpoint",     name,
        "match",        match
      );

  ret = cfg_create_identify_info(j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not create identify info.");
    return false;
  }

  return true;
}

bool pjsip_cfg_update_identify_info_data(const char* name, const json_t* j_data)
{
  int ret;
  json_t* j_tmp;

  if((name == NULL) || (j_data == NULL)) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired pjsip_cfg_update_identify_info_data. name[%s]", name);

  j_tmp = json_pack("{s:s, s:O}",
      "name",   name,
      "data",   j_data
      );

  ret = cfg_update_identify_info(j_tmp);
  json_decref(j_tmp);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not update identify info.");
    return false;
  }

  return true;
}

/**
 * Register the callback for pjsip reload
 */
bool pjsip_register_callback_module(bool (*func)(enum EN_RESOURCE_UPDATE_TYPES, const json_t*))
{
  int ret;

  if(func == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired pjsip_register_callback_reload.");

  ret = utils_register_callback(g_callback_module, func);
  if(ret == false) {
    slog(LOG_ERR, "Could not register callback for registration_outbound.");
    return false;
  }

  return true;
}

/**
 * Execute the registered callbacks for this module
 * @param j_data
 */
static void execute_callbacks_module(enum EN_RESOURCE_UPDATE_TYPES type, const json_t* j_data)
{
  slog(LOG_DEBUG, "Fired execute_callbacks_reload.");

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }

  utils_execute_callbacks(g_callback_module, type, j_data);

  return;
}


/**
 * Register callback for registration_outbound
 */
bool pjsip_register_callback_db_registration_outbound(bool (*func)(enum EN_RESOURCE_UPDATE_TYPES, const json_t*))
{
  int ret;

  if(func == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired pjsip_register_callback_registration_outbound.");

  ret = utils_register_callback(g_callback_db_registration_outbound, func);
  if(ret == false) {
    slog(LOG_ERR, "Could not register callback for registration_outbound.");
    return false;
  }

  return true;
}

/**
 * Execute the registered callbacks for registration_outbound
 * @param j_data
 */
static void execute_callbacks_db_registration_outbound(enum EN_RESOURCE_UPDATE_TYPES type, const json_t* j_data)
{
  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_DEBUG, "Fired execute_callbacks_registration_outbound.");

  utils_execute_callbacks(g_callback_db_registration_outbound, type, j_data);

  return;
}

/**
 * Register callback for config endpoint
 */
bool pjsip_register_callback_cfg_endpoint(bool (*func)(enum EN_RESOURCE_UPDATE_TYPES, const json_t*))
{
  int ret;

  if(func == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }
  slog(LOG_DEBUG, "Fired pjsip_register_callback_cfg_endpoint.");

  ret = utils_register_callback(g_callback_cfg_endpoint, func);
  if(ret == false) {
    slog(LOG_ERR, "Could not register callback for registration_outbound.");
    return false;
  }

  return true;
}

/**
 * Execute the registered callbacks for config endpoint
 * @param j_data
 */
static void execute_callbacks_cfg_endpoint(enum EN_RESOURCE_UPDATE_TYPES type, const json_t* j_data)
{
  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return;
  }
  slog(LOG_DEBUG, "Fired execute_callbacks_cfg_endpoint.");

  utils_execute_callbacks(g_callback_cfg_endpoint, type, j_data);

  return;
}

json_t* pjsip_get_configurations_all(void)
{
  json_t* j_res;
  json_t* j_tmp;

  j_res = json_array();

  // get current config
  j_tmp = conf_get_ast_current_config_info_text(DEF_PJSIP_CONFNAME);
  if(j_tmp == NULL) {
    slog(LOG_NOTICE, "Could not get current config info.");
    json_decref(j_res);
    return NULL;
  }
  json_array_append_new(j_res, j_tmp);

  // get backup configs
  j_tmp = conf_get_ast_backup_configs_text_all(DEF_PJSIP_CONFNAME);
  if(j_tmp == NULL) {
    slog(LOG_NOTICE, "Could not get backup configs info.");
    json_decref(j_res);
    return NULL;
  }
  json_array_extend(j_res, j_tmp);
  json_decref(j_tmp);

  return j_res;
}

json_t* pjsip_get_configuration_info(const char* name)
{
  int ret;
  json_t* j_res;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return NULL;
  }

  ret = strcmp(name, DEF_PJSIP_CONFNAME);
  if(ret == 0) {
    j_res = conf_get_ast_current_config_info_text(DEF_PJSIP_CONFNAME);
  }
  else {
    j_res = conf_get_ast_backup_config_info_text(name);
  }

  if(j_res == NULL) {
    return NULL;
  }

  return j_res;
}

bool pjsip_update_configuration_info(const json_t* j_data)
{
  int ret;
  const char* name;
  const char* data;

  if(j_data == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }

  name = json_string_value(json_object_get(j_data, "name"));
  data = json_string_value(json_object_get(j_data, "data"));
  if((name == NULL) || (data == NULL)) {
    slog(LOG_NOTICE, "Could not get name or data info.");
    return false;
  }

  ret = strcmp(name, DEF_PJSIP_CONFNAME);
  if(ret != 0) {
    slog(LOG_NOTICE, "The only current configuration can update.");
    return false;
  }

  ret = conf_update_ast_current_config_info_text_data(DEF_PJSIP_CONFNAME, data);
  if(ret == false) {
    return false;
  }

  return true;
}

bool pjsip_delete_configuration_info(const char* name)
{
  int ret;

  if(name == NULL) {
    slog(LOG_WARNING, "Wrong input parameter.");
    return false;
  }

  ret = strcmp(name, DEF_PJSIP_CONFNAME);
  if(ret == 0) {
    slog(LOG_NOTICE, "The current configuration is not deletable.");
    return false;
  }

  ret = conf_remove_ast_backup_config_info_valid(name, DEF_PJSIP_CONFNAME);
  if(ret == false) {
    slog(LOG_NOTICE, "Could not delete backup config info.");
    return false;
  }

  return true;
}

