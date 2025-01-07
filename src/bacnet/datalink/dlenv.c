/**
 * @file
 * @brief Datalink environment variables used for the BACnet command line tools
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date 2009
 * @copyright SPDX-License-Identifier: MIT
 * @ingroup DataLink
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
/* BACnet Stack defines - first */
#include "bacnet/bacdef.h"
/* BACnet Stack API */
#include "bacnet/apdu.h"
#include "bacnet/datalink/datalink.h"
#include "bacnet/basic/services.h"
#include "bacnet/datalink/dlenv.h"
#include "bacnet/basic/tsm/tsm.h"
#if defined(BACDL_BIP)
#include "bacnet/datalink/bip.h"
#include "bacnet/basic/bbmd/h_bbmd.h"
#endif

#if (BACNET_PROTOCOL_REVISION >= 17)
#include "bacnet/basic/object/netport.h"
#endif
#if defined(BACDL_BSC)
#include "bacnet/basic/object/bacfile.h"
#include "bacnet/basic/object/sc_netport.h"
#include "bacnet/datalink/bsc/bvlc-sc.h"
#include "bacnet/datalink/bsc/bsc-util.h"
#include "bacnet/datalink/bsc/bsc-datalink.h"
#include "bacnet/datalink/bsc/bsc-event.h"
#endif

/* enable debugging */
static bool Datalink_Debug;

/* timer used to renew Foreign Device Registration */
#if defined(BACDL_BIP) || defined(BACDL_BIP6)
static uint16_t BBMD_Timer_Seconds;
static uint16_t BBMD_TTL_Seconds = 60000;
#endif

#if defined(BACDL_BIP)
#ifndef BBMD_ENABLED
#define BBMD_ENABLED 1
#endif
/* BBMD variables */
static BACNET_IP_ADDRESS BBMD_Address;
static bool BBMD_Address_Valid;
static uint16_t BBMD_Result = 0;
#if BBMD_ENABLED
static BACNET_IP_BROADCAST_DISTRIBUTION_TABLE_ENTRY BBMD_Table_Entry;
#endif

/* Debug toggle */

/**
 * @brief Enabled debug printing of BACnet/IPv4 DL
 */
void bip_dl_debug_enable(void)
{
    Datalink_Debug = true;
}

/**
 * @brief Disable debug printing of BACnet/IPv4 DL
 */
void bip_dl_debug_disable(void)
{
    Datalink_Debug = false;
}

/* Simple setters for BBMD registration variables. */

/**
 * @brief Sets the IPv4 address for BBMD registration.
 *
 *  If not set here or provided by Environment variables,
 *  no BBMD registration will occur.
 *
 * @param address - IPv4 address (uint32_t) of BBMD to register with,
 *  in network byte order.
 */
void dlenv_bbmd_address_set(const BACNET_IP_ADDRESS *address)
{
    bvlc_address_copy(&BBMD_Address, address);
    BBMD_Address_Valid = true;
}

/** Set the Lease Time (Time-to-Live) for BBMD registration.
 * Default if not set is 60000 (1000 minutes).
 * @param ttl_secs - The Lease Time, in seconds.
 */
void dlenv_bbmd_ttl_set(uint16_t ttl_secs)
{
    BBMD_TTL_Seconds = ttl_secs;
}

/** Get the result of the last attempt to register with the indicated BBMD.
 * If we sent a foreign registration request, then see if we've received
 * a NAK in our BVLC handler.
 *
 * @return Positive number (of bytes sent) if registration was successful,
 *         0 if no registration request was made, or
 *         -1 if registration attempt failed.
 */
int dlenv_bbmd_result(void)
{
    if ((BBMD_Result > 0) &&
        (bvlc_get_last_result() == BVLC_RESULT_REGISTER_FOREIGN_DEVICE_NAK)) {
        return -1;
    }
    /* Else, show our send: */
    return BBMD_Result;
}
#endif

#if defined(BACDL_BIP) && BBMD_ENABLED
/** Register as a Foreign Device with the designated BBMD.
 * @ingroup DataLink
 * The BBMD's address, port, and lease time must be provided by
 * internal variables or Environment variables.
 * If no address for the BBMD is provided, no BBMD registration will occur.
 *
 * The Environment Variables depend on define of BACDL_BIP:
 *     - BACNET_BBMD_PORT - 0..65534, defaults to 47808
 *     - BACNET_BBMD_TIMETOLIVE - 0..65535 seconds, defaults to 60000
 *     - BACNET_BBMD_ADDRESS - dotted IPv4 address
 * @return Positive number (of bytes sent) on success,
 *         0 if no registration request is sent, or
 *         -1 if registration fails.
 */
static int bbmd_register_as_foreign_device(void)
{
    int retval = 0;
    bool bdt_entry_valid = false;
    uint16_t bdt_entry_port = 0;
    char *pEnv = NULL;
    unsigned a[4] = { 0 };
    char bbmd_env[32] = "";
    unsigned entry_number = 0;
    long long_value = 0;
    int c;

    pEnv = getenv("BACNET_BBMD_PORT");
    if (pEnv) {
        long_value = strtol(pEnv, NULL, 0);
        if (long_value <= 0xFFFF) {
            BBMD_Address.port = (uint16_t)long_value;
        } else {
            BBMD_Address.port = 0xBAC0;
        }
    }
    pEnv = getenv("BACNET_BBMD_TIMETOLIVE");
    if (pEnv) {
        long_value = strtol(pEnv, NULL, 0);
        if (long_value <= 0xFFFF) {
            BBMD_TTL_Seconds = (uint16_t)long_value;
        } else {
            BBMD_TTL_Seconds = 0xFFFF;
        }
    }
    pEnv = getenv("BACNET_BBMD_ADDRESS");
    if (pEnv) {
        BBMD_Address_Valid = bip_get_addr_by_name(pEnv, &BBMD_Address);
    }
    if (BBMD_Address_Valid) {
        if (Datalink_Debug) {
            fprintf(
                stderr,
                "Registering with BBMD at %u.%u.%u.%u:%u for %u seconds\n",
                (unsigned)BBMD_Address.address[0],
                (unsigned)BBMD_Address.address[1],
                (unsigned)BBMD_Address.address[2],
                (unsigned)BBMD_Address.address[3], (unsigned)BBMD_Address.port,
                (unsigned)BBMD_TTL_Seconds);
        }
        retval = bvlc_register_with_bbmd(&BBMD_Address, BBMD_TTL_Seconds);
        if (retval < 0) {
            fprintf(
                stderr, "FAILED to Register with BBMD at %u.%u.%u.%u:%u\n",
                (unsigned)BBMD_Address.address[0],
                (unsigned)BBMD_Address.address[1],
                (unsigned)BBMD_Address.address[2],
                (unsigned)BBMD_Address.address[3], (unsigned)BBMD_Address.port);
        }
        BBMD_Timer_Seconds = BBMD_TTL_Seconds;
    } else {
        for (entry_number = 1; entry_number <= 128; entry_number++) {
            bdt_entry_valid = false;
            snprintf(
                bbmd_env, sizeof(bbmd_env), "BACNET_BDT_ADDR_%u", entry_number);
            pEnv = getenv(bbmd_env);
            if (pEnv) {
                bdt_entry_valid =
                    bip_get_addr_by_name(pEnv, &BBMD_Table_Entry.dest_address);
                if (entry_number == 1) {
                    if (Datalink_Debug) {
                        fprintf(
                            stderr, "BBMD 1 address overridden %s=%s!\n",
                            bbmd_env, pEnv);
                    }
                }
            } else if (entry_number == 1) {
                /* BDT 1 is self (note: can be overridden) */
                bdt_entry_valid = bip_get_addr(&BBMD_Table_Entry.dest_address);
            }
            if (bdt_entry_valid) {
                bdt_entry_port = 0xBAC0;
                snprintf(
                    bbmd_env, sizeof(bbmd_env), "BACNET_BDT_PORT_%u",
                    entry_number);
                pEnv = getenv(bbmd_env);
                if (pEnv) {
                    bdt_entry_port = strtol(pEnv, NULL, 0);
                    if (entry_number == 1) {
                        if (Datalink_Debug) {
                            fprintf(
                                stderr, "BBMD 1 port overridden %s=%s!\n",
                                bbmd_env, pEnv);
                        }
                    }
                } else if (entry_number == 1) {
                    /* BDT 1 is self (note: can be overridden) */
                    bdt_entry_port = bip_get_port();
                }
                BBMD_Table_Entry.dest_address.port = bdt_entry_port;
                /* broadcast mask */
                bvlc_broadcast_distribution_mask_from_host(
                    &BBMD_Table_Entry.broadcast_mask, 0xFFFFFFFF);
                snprintf(
                    bbmd_env, sizeof(bbmd_env), "BACNET_BDT_MASK_%u",
                    entry_number);
                pEnv = getenv(bbmd_env);
                if (pEnv) {
                    c = sscanf(
                        pEnv, "%3u.%3u.%3u.%3u", &a[0], &a[1], &a[2], &a[3]);
                    if (c == 4) {
                        bvlc_broadcast_distribution_mask_set(
                            &BBMD_Table_Entry.broadcast_mask, a[0], a[1], a[2],
                            a[3]);
                    }
                }
                bvlc_broadcast_distribution_table_entry_append(
                    bvlc_bdt_list(), &BBMD_Table_Entry);
                if (Datalink_Debug) {
                    fprintf(
                        stderr, "BBMD %4u: %u.%u.%u.%u:%u %u.%u.%u.%u\n",
                        entry_number,
                        (unsigned)BBMD_Table_Entry.dest_address.address[0],
                        (unsigned)BBMD_Table_Entry.dest_address.address[1],
                        (unsigned)BBMD_Table_Entry.dest_address.address[2],
                        (unsigned)BBMD_Table_Entry.dest_address.address[3],
                        (unsigned)BBMD_Table_Entry.dest_address.port,
                        (unsigned)BBMD_Table_Entry.broadcast_mask.address[0],
                        (unsigned)BBMD_Table_Entry.broadcast_mask.address[1],
                        (unsigned)BBMD_Table_Entry.broadcast_mask.address[2],
                        (unsigned)BBMD_Table_Entry.broadcast_mask.address[3]);
                }
            }
        }
    }
    BBMD_Result = retval;

    return retval;
}
#endif

#if defined(BACDL_BIP6) && BBMD6_ENABLED
/** Register as a Foreign Device with the designated BBMD.
 * @ingroup DataLink
 * The BBMD's address, port, and lease time must be provided by
 * internal variables or Environment variables.
 * If no address for the BBMD is provided, no BBMD registration will occur.
 *
 * The Environment Variables depend on define of BACDL_BIP:
 *     - BACNET_BBMD6_PORT - 0..65534, defaults to 47808
 *     - BACNET_BBMD6_TIMETOLIVE - 0..65535 seconds, defaults to 60000
 *     - BACNET_BBMD6_ADDRESS - IPv6 address
 * @return Positive number (of bytes sent) on success,
 *         0 if no registration request is sent, or
 *         -1 if registration fails.
 */
static int bbmd6_register_as_foreign_device(void)
{
    int retval = 0;
    bool bdt_entry_valid = false;
    uint16_t bdt_entry_port = 0;
    char *pEnv = NULL;
    unsigned a[4] = { 0 };
    char bbmd_env[32] = "";
    unsigned entry_number = 0;
    long long_value = 0;
    int c;
    BACNET_IP6_ADDRESS bip6_addr = { 0 };
    uint16_t bip6_port = 0xBAC0;

    pEnv = getenv("BACNET_BBMD6_PORT");
    if (pEnv) {
        long_value = strtol(pEnv, NULL, 0);
        if (long_value <= UINT16_MAX) {
            bip6_port = (uint16_t)long_value;
        }
    }
    pEnv = getenv("BACNET_BBMD6_TIMETOLIVE");
    if (pEnv) {
        long_value = strtol(pEnv, NULL, 0);
        if (long_value <= 60000) {
            BBMD_TTL_Seconds = (uint16_t)long_value;
        }
    }
    pEnv = getenv("BACNET_BBMD6_ADDRESS");
    if (bvlc6_address_from_ascii(pEnv, &bip6_addr)) {
        if (Datalink_Debug) {
            fprintf(
                stderr, "Registering with BBMD6 at %s for %u seconds\n", pEnv,
                (unsigned)bip6_port, (unsigned)BBMD_TTL_Seconds);
        }
        retval = bvlc6_register_with_bbmd(&bip6_addr, BBMD_TTL_Seconds);
        if (retval < 0) {
            fprintf(
                stderr, "FAILED to Register with BBMD6 at %s:%u\n", pEnv,
                (unsigned)BBMD_Address.port);
        }
        BBMD_Timer_Seconds = BBMD_TTL_Seconds;
    }
    BBMD_Result = retval;

    return retval;
}
#endif

/** Register as a Foreign Device with the designated BBMD.
 * @ingroup DataLink
 * The BBMD's address, port, and lease time must be provided by
 * internal variables or Environment variables.
 * If no address for the BBMD is provided, no BBMD registration will occur.
 *
 * @return Positive number (of bytes sent) on success,
 *         0 if no registration request is sent, or
 *         -1 if registration fails.
 */
int dlenv_register_as_foreign_device(void)
{
#if defined(BACDL_BIP) && BBMD_ENABLED
    return bbmd_register_as_foreign_device();
#elif defined(BACDL_BIP6) && BBMD6_ENABLED
    return bbmd6_register_as_foreign_device();
#else
    return 0;
#endif
}

#if (BACNET_PROTOCOL_REVISION >= 17)
#if defined(BACDL_BIP)
/**
 * Datalink network port object settings
 */
void dlenv_network_port_init(void)
{
    const uint32_t instance = 1;
    BACNET_IP_ADDRESS addr = { 0 };
    uint8_t prefix = 0;
#if BBMD_ENABLED
    uint8_t addr0, addr1, addr2, addr3;
#endif

    Network_Port_Object_Instance_Number_Set(0, instance);
    Network_Port_Name_Set(instance, "BACnet/IP Port");
    Network_Port_Type_Set(instance, PORT_TYPE_BIP);
    bip_get_addr(&addr);
    prefix = bip_get_subnet_prefix();
    if (Datalink_Debug) {
        fprintf(
            stderr, "BIP: Setting Network Port %lu address %u.%u.%u.%u:%u/%u\n",
            (unsigned long)instance, (unsigned)addr.address[0],
            (unsigned)addr.address[1], (unsigned)addr.address[2],
            (unsigned)addr.address[3], (unsigned)addr.port, (unsigned)prefix);
    }
    Network_Port_BIP_Port_Set(instance, addr.port);
    Network_Port_IP_Address_Set(
        instance, addr.address[0], addr.address[1], addr.address[2],
        addr.address[3]);
    Network_Port_IP_Subnet_Prefix_Set(instance, prefix);
    Network_Port_Link_Speed_Set(instance, 0.0);
#if BBMD_ENABLED
    Network_Port_BBMD_BD_Table_Set(instance, bvlc_bdt_list());
    Network_Port_BBMD_FD_Table_Set(instance, bvlc_fdt_list());
    /* foreign device registration */
    bvlc_address_get(&BBMD_Address, &addr0, &addr1, &addr2, &addr3);
    Network_Port_Remote_BBMD_IP_Address_Set(
        instance, addr0, addr1, addr2, addr3);
    Network_Port_Remote_BBMD_BIP_Port_Set(instance, BBMD_Address.port);
    Network_Port_Remote_BBMD_BIP_Lifetime_Set(instance, BBMD_TTL_Seconds);
#endif
    /* common NP data */
    Network_Port_Reliability_Set(instance, RELIABILITY_NO_FAULT_DETECTED);
    Network_Port_Out_Of_Service_Set(instance, false);
    Network_Port_Quality_Set(instance, PORT_QUALITY_UNKNOWN);
    Network_Port_APDU_Length_Set(instance, MAX_APDU);
    Network_Port_Network_Number_Set(instance, 0);
    /* last thing - clear pending changes - we don't want to set these
       since they are already set */
    Network_Port_Changes_Pending_Set(instance, false);
}
#elif defined(BACDL_MSTP)
/**
 * Datalink network port object settings
 */
void dlenv_network_port_init(void)
{
    uint32_t instance = 1;
    uint8_t mac[1] = { 0 };

    Network_Port_Object_Instance_Number_Set(0, instance);
    Network_Port_Name_Set(instance, "MS/TP Port");
    Network_Port_Type_Set(instance, PORT_TYPE_MSTP);
    Network_Port_MSTP_Max_Master_Set(instance, dlmstp_max_master());
    Network_Port_MSTP_Max_Info_Frames_Set(instance, dlmstp_max_info_frames());
    Network_Port_Link_Speed_Set(instance, dlmstp_baud_rate());
    mac[0] = dlmstp_mac_address();
    Network_Port_MAC_Address_Set(instance, &mac[0], 1);
    /* common NP data */
    Network_Port_Reliability_Set(instance, RELIABILITY_NO_FAULT_DETECTED);
    Network_Port_Out_Of_Service_Set(instance, false);
    Network_Port_Quality_Set(instance, PORT_QUALITY_UNKNOWN);
    Network_Port_APDU_Length_Set(instance, MAX_APDU);
    Network_Port_Network_Number_Set(instance, 0);
    /* last thing - clear pending changes - we don't want to set these
       since they are already set */
    Network_Port_Changes_Pending_Set(instance, false);
}
#elif defined(BACDL_BIP6)
/**
 * Datalink network port object settings
 */
void dlenv_network_port_init(void)
{
    uint32_t instance = 1;
    uint8_t prefix = 0;
    BACNET_ADDRESS addr = { 0 };
    BACNET_IP6_ADDRESS addr6 = { 0 };

    Network_Port_Object_Instance_Number_Set(0, instance);
    Network_Port_Name_Set(instance, "BACnet/IPv6 Port");
    Network_Port_Type_Set(instance, PORT_TYPE_BIP6);
    Network_Port_BIP6_Port_Set(instance, bip6_get_port());
    bip6_get_my_address(&addr);
    Network_Port_MAC_Address_Set(instance, &addr.mac[0], addr.mac_len);
    bip6_get_addr(&addr6);
    Network_Port_IPv6_Address_Set(instance, &addr6.address[0]);
    bip6_get_broadcast_addr(&addr6);
    Network_Port_IPv6_Multicast_Address_Set(instance, &addr6.address[0]);
    Network_Port_IPv6_Subnet_Prefix_Set(instance, prefix);

    Network_Port_Reliability_Set(instance, RELIABILITY_NO_FAULT_DETECTED);
    Network_Port_Link_Speed_Set(instance, 0.0);
    Network_Port_Out_Of_Service_Set(instance, false);
    Network_Port_Quality_Set(instance, PORT_QUALITY_UNKNOWN);
    Network_Port_APDU_Length_Set(instance, MAX_APDU);
    Network_Port_Network_Number_Set(instance, 0);
    /* last thing - clear pending changes - we don't want to set these
       since they are already set */
    Network_Port_Changes_Pending_Set(instance, false);
}
#elif defined(BACDL_BSC)
/**
 * @brief Datalink network port object settings
 * @param primary_hub_uri
 * @param failover_hub_uri
 * @param filename_ca_1_cert
 * @param filename_ca_2_cert
 * @param filename_cert
 * @param filename_key
 * @param direct_connect_port
 * @param hub_function_port
 * @param direct_connect_initiate
 * @param direct_connect_accept_urls
 */
static void bacnet_secure_connect_network_port_init(
    char *primary_hub_uri,
    char *failover_hub_uri,
    char *filename_ca_1_cert,
    char *filename_ca_2_cert,
    char *filename_cert,
    char *filename_key,
    char *direct_binding,
    char *hub_binding,
    char *direct_connect_initiate,
    char *direct_connect_accept_urls)
{
    const uint32_t instance = 1;
    BACNET_SC_UUID uuid = { 0 };
    BACNET_SC_VMAC_ADDRESS vmac = { 0 };
    long seed;
    char c;

    seed = (long)&instance;
    srand((int)seed);
    Network_Port_Object_Instance_Number_Set(0, instance);
    Network_Port_Name_Set(instance, "BACnet/BSC Port");
    Network_Port_Type_Set(instance, PORT_TYPE_BSC);

    bsc_generate_random_uuid(&uuid);
    Network_Port_SC_Local_UUID_Set(instance, (BACNET_UUID *)&uuid);
    bsc_generate_random_vmac(&vmac);
    Network_Port_MAC_Address_Set(instance, vmac.address, sizeof(vmac));

    /* common NP data */
    Network_Port_Reliability_Set(instance, RELIABILITY_NO_FAULT_DETECTED);
    Network_Port_Out_Of_Service_Set(instance, false);
    Network_Port_Quality_Set(instance, PORT_QUALITY_UNKNOWN);
    Network_Port_APDU_Length_Set(instance, MAX_APDU);
    Network_Port_Network_Number_Set(instance, 0);

    /* SC parameters */
    Network_Port_Max_BVLC_Length_Accepted_Set(instance, SC_NETPORT_BVLC_MAX);
    Network_Port_Max_NPDU_Length_Accepted_Set(instance, SC_NETPORT_NPDU_MAX);
    Network_Port_SC_Connect_Wait_Timeout_Set(
        instance, SC_NETPORT_CONNECT_TIMEOUT);
    Network_Port_SC_Heartbeat_Timeout_Set(
        instance, SC_NETPORT_HEARTBEAT_TIMEOUT);
    Network_Port_SC_Disconnect_Wait_Timeout_Set(
        instance, SC_NETPORT_DISCONNECT_TIMEOUT);
    Network_Port_SC_Maximum_Reconnect_Time_Set(
        instance, SC_NETPORT_RECONNECT_TIME);

    if (filename_ca_1_cert == NULL) {
        fprintf(stderr, "BACNET_SC_ISSUER_1_CERTIFICATE_FILE must be set\n");
        return;
    }
    bacfile_create(BSC_ISSUER_CERTIFICATE_FILE_1_INSTANCE);
    bacfile_pathname_set(
        BSC_ISSUER_CERTIFICATE_FILE_1_INSTANCE, filename_ca_1_cert);
    Network_Port_Issuer_Certificate_File_Set(
        instance, 0, BSC_ISSUER_CERTIFICATE_FILE_1_INSTANCE);

    if (filename_ca_2_cert) {
        bacfile_create(BSC_ISSUER_CERTIFICATE_FILE_2_INSTANCE);
        bacfile_pathname_set(
            BSC_ISSUER_CERTIFICATE_FILE_2_INSTANCE, filename_ca_2_cert);
        Network_Port_Issuer_Certificate_File_Set(
            instance, 1, BSC_ISSUER_CERTIFICATE_FILE_2_INSTANCE);
    }

    if (filename_cert == NULL) {
        fprintf(stderr, "BACNET_SC_OPERATIONAL_CERTIFICATE_FILE must be set\n");
        return;
    }
    bacfile_create(BSC_OPERATIONAL_CERTIFICATE_FILE_INSTANCE);
    bacfile_pathname_set(
        BSC_OPERATIONAL_CERTIFICATE_FILE_INSTANCE, filename_cert);
    Network_Port_Operational_Certificate_File_Set(
        instance, BSC_OPERATIONAL_CERTIFICATE_FILE_INSTANCE);

    if (filename_key == NULL) {
        fprintf(
            stderr,
            "BACNET_SC_OPERATIONAL_CERTIFICATE_PRIVATE_KEY_FILE must be set\n");
        return;
    }
    bacfile_create(BSC_CERTIFICATE_SIGNING_REQUEST_FILE_INSTANCE);
    bacfile_pathname_set(
        BSC_CERTIFICATE_SIGNING_REQUEST_FILE_INSTANCE, filename_key);
    Network_Port_Certificate_Key_File_Set(
        instance, BSC_CERTIFICATE_SIGNING_REQUEST_FILE_INSTANCE);

    if ((primary_hub_uri == NULL) && (failover_hub_uri == NULL) &&
        (direct_binding == NULL) && (hub_binding == NULL)) {
        fprintf(
            stderr,
            "At least must be set:\n"
            "BACNET_SC_HUB_FUNCTION_BINDING for HUB or\n"
            "BACNET_SC_PRIMARY_HUB_URI and BACNET_SC_FAILOVER_HUB_URI for node "
            "or\n"
            "BACNET_SC_DIRECT_CONNECT_BINDING for direct connect.\n");
        return;
    }

    Network_Port_SC_Primary_Hub_URI_Set(instance, primary_hub_uri);
    Network_Port_SC_Failover_Hub_URI_Set(instance, failover_hub_uri);

    Network_Port_SC_Direct_Connect_Binding_Set(instance, direct_binding);
    Network_Port_SC_Direct_Connect_Accept_Enable_Set(
        instance, direct_binding != NULL);

    c = direct_connect_initiate ? direct_connect_initiate[0] : '0';
    if ((c != '0') && (c != 'n') && (c != 'N')) {
        Network_Port_SC_Direct_Connect_Initiate_Enable_Set(instance, true);
    } else {
        Network_Port_SC_Direct_Connect_Initiate_Enable_Set(instance, false);
    }

    Network_Port_SC_Direct_Connect_Accept_URIs_Set(
        instance, direct_connect_accept_urls);

    /* HUB parameters */
    Network_Port_SC_Hub_Function_Binding_Set(instance, hub_binding);
    Network_Port_SC_Hub_Function_Enable_Set(instance, hub_binding != NULL);

    /* last thing - clear pending changes - we don't want to set these
       since they are already set */
    Network_Port_Changes_Pending_Set(instance, false);
}

static bool dlenv_hub_connection_status_check(void)
{
    uint32_t instance = Network_Port_Index_To_Instance(0);
    BACNET_SC_HUB_CONNECTION_STATUS *status;

    status = Network_Port_SC_Primary_Hub_Connection_Status(instance);
    if (status && status->State == BACNET_SC_CONNECTION_STATE_CONNECTED) {
        return true;
    }

    status = Network_Port_SC_Failover_Hub_Connection_Status(instance);
    if (status && status->State == BACNET_SC_CONNECTION_STATE_CONNECTED) {
        return true;
    }

    return false;
}

/**
 * Datalink network port object settings for BACnet/SC
 */
void dlenv_network_port_init(void)
{
    /* if a user has configured BACnet/SC port with primary hub URI,     */
    /* wait for a establishin of a connection to BACnet/SC hub at first  */
    /* to reduce possibility of packet losses.                           */
    if (Network_Port_SC_Primary_Hub_URI_char(1)) {
        while (!dlenv_hub_connection_status_check()) {
            bsc_wait(1);
            bsc_maintenance_timer(1);
        }
    }
}
#else
/**
 * Datalink network port object settings
 */
void dlenv_network_port_init(void)
{
    /* do nothing */
}
#endif
#endif

/** Datalink maintenance timer
 * @ingroup DataLink
 *
 * Call this function to renew our Foreign Device Registration
 * @param elapsed_seconds Number of seconds that have elapsed since last called.
 */
void dlenv_maintenance_timer(uint16_t elapsed_seconds)
{
#if defined(BACDL_BIP) || defined(BACDL_BIP6)
    if (BBMD_Timer_Seconds) {
        if (BBMD_Timer_Seconds <= elapsed_seconds) {
            BBMD_Timer_Seconds = 0;
        } else {
            BBMD_Timer_Seconds -= elapsed_seconds;
        }
        if (BBMD_Timer_Seconds == 0) {
            (void)dlenv_register_as_foreign_device();
            /* If that failed (negative), maybe just a network issue.
             * If nothing happened (0), may be un/misconfigured.
             * Set up to try again later in all cases. */
            BBMD_Timer_Seconds = (uint16_t)BBMD_TTL_Seconds;
        }
    }
#else
    (void)elapsed_seconds;
#endif
}

/** Initialize the DataLink configuration from Environment variables,
 * or else to defaults.
 * @ingroup DataLink
 * The items configured depend on which BACDL_ the code is built for,
 * eg, BACDL_BIP.
 *
 * For most items, checks first for an environment variable, and, if
 * found, uses that to set the item's value.  Otherwise, will set
 * to a default value.
 *
 * The Environment Variables, by BACDL_ type, are:
 * - BACDL_ALL: (the general-purpose solution)
 *   - BACNET_DATALINK to set which BACDL_ type we are using.
 * - (Any):
 *   - BACNET_APDU_TIMEOUT - set this value in milliseconds to change
 *     the APDU timeout.  APDU Timeout is how much time a client
 *     waits for a response from a BACnet device.
 *   - BACNET_APDU_RETRIES - indicate the maximum number of times that
 *     an APDU shall be retransmitted.
 *   - BACNET_IFACE - set this value to dotted IP address (Windows) of
 *     the interface (see ipconfig command on Windows) for which you
 *     want to bind.  On Linux, set this to the /dev interface
 *     (i.e. eth0, arc0).  Default is eth0 on Linux, and the default
 *     interface on Windows.  Hence, if there is only a single network
 *     interface on Windows, the applications will choose it, and this
 *     setting will not be needed.
 * - BACDL_BIP: (BACnet/IP)
 *   - BACNET_IP_PORT - UDP/IP port number (0..65534) used for BACnet/IP
 *     communications.  Default is 47808 (0xBAC0).
 *   - BACNET_BBMD_PORT - UDP/IP port number (0..65534) used for Foreign
 *       Device Registration.  Defaults to 47808 (0xBAC0).
 *   - BACNET_BBMD_TIMETOLIVE - number of seconds used in Foreign Device
 *       Registration (0..65535). Defaults to 60000 seconds.
 *   - BACNET_BBMD_ADDRESS - dotted IPv4 address of the BBMD or Foreign
 *       Device Registrar.
 *   - BACNET_BDT_ADDR_1 - dotted IPv4 address of the BBMD table entry 1..128
 *   - BACNET_BDT_PORT_1 - UDP port of the BBMD table entry 1..128 (optional)
 *   - BACNET_BDT_MASK_1 - dotted IPv4 mask of the BBMD table
 *       entry 1..128 (optional)
 *   - BACNET_IP_NAT_ADDR - dotted IPv4 address of the public facing router
 *   - BACNET_IP_BROADCAST_BIND_ADDR - dotted IPv4 address to bind broadcasts
 * - BACDL_MSTP: (BACnet MS/TP)
 *   - BACNET_MAX_INFO_FRAMES
 *   - BACNET_MAX_MASTER
 *   - BACNET_MSTP_BAUD
 *   - BACNET_MSTP_MAC
 * - BACDL_BIP6: (BACnet/IPv6)
 *   - BACNET_BIP6_PORT - UDP/IP port number (0..65534) used for BACnet/IPv6
 *     communications.  Default is 47808 (0xBAC0).
 *   - BACNET_BIP6_BROADCAST - FF05::BAC0 or FF02::BAC0 or ...
 *   - BACNET_BBMD6_PORT - UDP/IPv6 port number (0..65534) used for Foreign
 *     Device Registration.  Defaults to 47808 (0xBAC0).
 *   - BACNET_BBMD6_TIMETOLIVE - 0..65535 seconds, defaults to 60000
 *   - BACNET_BBMD6_ADDRESS - IPv6 address
 * - BACDL_BSC: (BACnet Secure Connect)
 *   - BACNET_SC_PRIMARY_HUB_URI
 *   - BACNET_SC_FAILOVER_HUB_URI
 *   - BACNET_SC_ISSUER_1_CERTIFICATE_FILE
 *   - BACNET_SC_ISSUER_2_CERTIFICATE_FILE
 *   - BACNET_SC_OPERATIONAL_CERTIFICATE_FILE
 *   - BACNET_SC_OPERATIONAL_CERTIFICATE_PRIVATE_KEY_FILE
 *   - BACNET_SC_DIRECT_CONNECT_BINDING - pair: interface name (optional) and
 *       TCP/IP port number (0..65534), like "50000" (port only) or
 *       "eth0:50000"(both)
 *   - BACNET_SC_HUB_FUNCTION_BINDING - pair: interface name (optional) and
 *       TCP/IP port number (0..65534), like "50000" (port only) or
 *       "eth0:50000"(both)
 *   - BACNET_SC_DIRECT_CONNECT_INITIATE - if true equal "1", "y", "Y",
 *       otherwise false
 *   - BACNET_SC_DIRECT_CONNECT_ACCEPT_URLS - list of direct connect accept URLs
 *       separated by a space character, e.g.
 *       "wss://192.0.0.1:40000 wss://192.0.0.2:6666"
 */
void dlenv_init(void)
{
#if defined(BACDL_BIP)
    BACNET_IP_ADDRESS addr;
#endif
#if defined(BACDL_BIP6)
    BACNET_IP6_ADDRESS addr6;
#endif
    char *pEnv = NULL;

#if defined(BACDL_MULTIPLE)
    pEnv = getenv("BACNET_DATALINK");
    if (pEnv) {
        datalink_set(pEnv);
    } else {
#if defined(BACDL_BIP)
        datalink_set("bip");
#elif defined(BACDL_BIP6)
        datalink_set("bip6");
#elif defined(BACDL_MSTP)
        datalink_set("mstp");
#elif defined(BACDL_ETHERNET)
        datalink_set("ethernet");
#elif defined(BACDL_ARCNET)
        datalink_set("arcnet");
#elif defined(BACDL_BSC)
        datalink_set("bsc");
#else
        datalink_set("none");
#endif
    }
#endif
#if defined(BACDL_BIP6)
    pEnv = getenv("BACNET_BIP6_DEBUG");
    if (pEnv) {
        bip6_debug_enable();
        bvlc6_debug_enable();
        Datalink_Debug = true;
    }
    pEnv = getenv("BACNET_BIP6_BROADCAST");
    if (pEnv) {
        bvlc6_address_set(
            &addr6, (uint16_t)strtol(pEnv, NULL, 0), 0, 0, 0, 0, 0, 0,
            BIP6_MULTICAST_GROUP_ID);
        bip6_set_broadcast_addr(&addr6);
    } else {
        bvlc6_address_set(
            &addr6, BIP6_MULTICAST_SITE_LOCAL, 0, 0, 0, 0, 0, 0,
            BIP6_MULTICAST_GROUP_ID);
        bip6_set_broadcast_addr(&addr6);
    }
    pEnv = getenv("BACNET_BIP6_PORT");
    if (pEnv) {
        bip6_set_port((uint16_t)strtol(pEnv, NULL, 0));
    } else {
        bip6_set_port(0xBAC0);
    }
#endif
#if defined(BACDL_BIP)
    pEnv = getenv("BACNET_IP_DEBUG");
    if (pEnv) {
        bip_debug_enable();
        bvlc_debug_enable();
        bip_dl_debug_enable();
    }
    pEnv = getenv("BACNET_IP_PORT");
    if (pEnv) {
        bip_set_port((uint16_t)strtol(pEnv, NULL, 0));
    } else {
        /* BIP_Port is statically initialized to 0xBAC0,
         * so if it is different, then it was programmatically altered,
         * and we shouldn't just stomp on it here.
         * Unless it is set below 1024, since:
         * "The range for well-known ports managed by the IANA is 0-1023."
         */
        if (bip_get_port() < 1024) {
            bip_set_port(0xBAC0);
        }
    }
    pEnv = getenv("BACNET_IP_BROADCAST_BIND_ADDR");
    if (pEnv) {
        bip_set_broadcast_binding(pEnv);
    }
    pEnv = getenv("BACNET_IP_NAT_ADDR");
    if (pEnv) {
        if (bip_get_addr_by_name(pEnv, &addr)) {
            addr.port = 0xBAC0;
            pEnv = getenv("BACNET_IP_NAT_PORT");
            if (pEnv) {
                addr.port = strtol(pEnv, NULL, 0);
            }
            bvlc_set_global_address_for_nat(&addr);
        }
    }
#elif defined(BACDL_MSTP)
    pEnv = getenv("BACNET_MAX_INFO_FRAMES");
    if (pEnv) {
        dlmstp_set_max_info_frames(strtol(pEnv, NULL, 0));
    } else {
        dlmstp_set_max_info_frames(1);
    }
    pEnv = getenv("BACNET_MAX_MASTER");
    if (pEnv) {
        dlmstp_set_max_master(strtol(pEnv, NULL, 0));
    } else {
        dlmstp_set_max_master(127);
    }
    pEnv = getenv("BACNET_MSTP_BAUD");
    if (pEnv) {
        dlmstp_set_baud_rate(strtol(pEnv, NULL, 0));
    } else {
        dlmstp_set_baud_rate(38400);
    }
    pEnv = getenv("BACNET_MSTP_MAC");
    if (pEnv) {
        dlmstp_set_mac_address(strtol(pEnv, NULL, 0));
    } else {
        dlmstp_set_mac_address(127);
    }
#elif defined(BACDL_BSC)
    char *primary_hub_uri;
    char *failover_hub_uri;
    char *filename_ca_1_cert;
    char *filename_ca_2_cert;
    char *filename_cert;
    char *filename_key;
    char *direct_binding;
    char *hub_binding;
    char *direct_connect_initiate;
    char *direct_connect_accept_urls;

    primary_hub_uri = getenv("BACNET_SC_PRIMARY_HUB_URI");
    failover_hub_uri = getenv("BACNET_SC_FAILOVER_HUB_URI");
    filename_ca_1_cert = getenv("BACNET_SC_ISSUER_1_CERTIFICATE_FILE");
    filename_ca_2_cert = getenv("BACNET_SC_ISSUER_2_CERTIFICATE_FILE");
    filename_cert = getenv("BACNET_SC_OPERATIONAL_CERTIFICATE_FILE");
    filename_key = getenv("BACNET_SC_OPERATIONAL_CERTIFICATE_PRIVATE_KEY_FILE");
    direct_binding = getenv("BACNET_SC_DIRECT_CONNECT_BINDING");
    hub_binding = getenv("BACNET_SC_HUB_FUNCTION_BINDING");
    direct_connect_initiate = getenv("BACNET_SC_DIRECT_CONNECT_INITIATE");
    direct_connect_accept_urls = getenv("BACNET_SC_DIRECT_CONNECT_ACCEPT_URLS");
    bacnet_secure_connect_network_port_init(
        primary_hub_uri, failover_hub_uri, filename_ca_1_cert,
        filename_ca_2_cert, filename_cert, filename_key, direct_binding,
        hub_binding, direct_connect_initiate, direct_connect_accept_urls);
    if (!bsc_cert_files_check()) {
        exit(1);
    }
#endif
    pEnv = getenv("BACNET_APDU_TIMEOUT");
    if (pEnv) {
        apdu_timeout_set((uint16_t)strtol(pEnv, NULL, 0));
    } else {
#if defined(BACDL_MSTP)
        apdu_timeout_set(60000);
#endif
    }
    pEnv = getenv("BACNET_APDU_RETRIES");
    if (pEnv) {
        apdu_retries_set((uint8_t)strtol(pEnv, NULL, 0));
    }
    /* === Initialize the Datalink Here === */
    if (!datalink_init(getenv("BACNET_IFACE"))) {
        exit(1);
    }
#if (MAX_TSM_TRANSACTIONS)
    pEnv = getenv("BACNET_INVOKE_ID");
    if (pEnv) {
        tsm_invokeID_set((uint8_t)strtol(pEnv, NULL, 0));
    }
#endif
#if (BACNET_PROTOCOL_REVISION >= 17)
    dlenv_network_port_init();
#endif
    dlenv_register_as_foreign_device();
}
