/*****************************************************************************
**
**  Name:           bluetooth_interface.c
**
**  Description:    Bluetooth Manager application
**
**  Copyright (c) 2010-2014, Broadcom Corp., All Rights Reserved.
**  Broadcom Bluetooth Core. Proprietary and confidential.
**
*****************************************************************************/
#ifndef BUILD_LIB

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>

#include "gki.h"
#include "uipc.h"

#include "bsa_api.h"

#include "app_xml_utils.h"

#include "app_disc.h"
#include "app_utils.h"
#include "app_dm.h"

#include "app_services.h"
#include "app_mgt.h"

#include "app_manager.h"
#include "app_avk.h"
#include "bluetooth_interface.h"

#define MAX_PATH_LEN  256

/*
 * Extern variables
 */
extern tAPP_MGR_CB app_mgr_cb;
static tBSA_SEC_IO_CAP g_sp_caps = 0;
extern tAPP_XML_CONFIG         app_xml_config;
char bta_conf_path[MAX_PATH_LEN];

/*static variables */
static BD_ADDR     dev_connected;        /* BdAddr of connected device */
static void *p_sbt = NULL;

//tAvkCallback
static void app_avk_callback(tBSA_AVK_EVT event, tBSA_AVK_MSG *p_data);
static void app_hs_callback(tBSA_HS_EVT event, tBSA_HS_MSG *p_data);

static void app_avk_callback(tBSA_AVK_EVT event, tBSA_AVK_MSG *p_data)
{
	  switch(event)
	  {
	  	  case BSA_AVK_OPEN_EVT:
	  	  {
	  	      printf("avk connected!\n");
	  	      bt_event_transact(p_sbt, APP_AVK_CONNECTED_EVT);
	  	      bdcpy(app_sec_db_addr, p_data->open.bd_addr);   
	  	      break;	
	  	  }
	  	  case BSA_AVK_CLOSE_EVT:
	  	  {
	  	      printf("avk disconnected!\n");
            bt_event_transact(p_sbt, APP_AVK_DISCONNECTED_EVT);
	  	      break;	
	  	  }
	      case BSA_AVK_START_EVT:
	      {
	      	  if(p_data->start.streaming == TRUE)
	      	  {
                printf("BT is playing music!\n");
                bt_event_transact(p_sbt, APP_AVK_START_EVT);
	      	  }
	      	  break;
	      }
	      case BSA_AVK_STOP_EVT:
	      {
	      	  printf("BT is stop music!\n");
	      	  bt_event_transact(p_sbt, APP_AVK_STOP_EVT);
	      	  break;
	      }
	      
	      default:
	          ;      		
	  }
}

static void app_hs_callback(tBSA_HS_EVT event, tBSA_HS_MSG *p_data)
{
    switch(event)
    {
        case BSA_HS_CONN_EVT:
        {
            printf("hs connected!\n");
            bt_event_transact(p_sbt, APP_HS_CONNECTED_EVT);	
            break;	
        }
        case BSA_HS_CLOSE_EVT:
        {
        	  printf("hs disconnected!\n");
        	  bt_event_transact(p_sbt, APP_AVK_DISCONNECTED_EVT);
        	  break;
        }	
        case BSA_HS_AUDIO_OPEN_EVT:
        {
            printf("hs audio open!\n");
            break;	
        } 			
    }   	
}

/*******************************************************************************
 **
 ** Function         app_mgr_mgt_callback
 **
 ** Description      This callback function is called in case of server
 **                  disconnection (e.g. server crashes)
 **
 ** Parameters
 **
 ** Returns          void
 **
 *******************************************************************************/
static BOOLEAN app_mgr_mgt_callback(tBSA_MGT_EVT event, tBSA_MGT_MSG *p_data)
{
    switch(event)
    {
    case BSA_MGT_STATUS_EVT:
        APP_DEBUG0("BSA_MGT_STATUS_EVT");
        if (p_data->status.enable)
        {
            APP_DEBUG0("Bluetooth restarted => re-initialize the application");
            app_mgr_config();
        }
        break;

    case BSA_MGT_DISCONNECT_EVT:
        APP_DEBUG1("BSA_MGT_DISCONNECT_EVT reason:%d", p_data->disconnect.reason);
        exit(-1);
        break;

    default:
        break;
    }
    return FALSE;
}

int bluetooth_start(void *p, char *p_conf)
{
	  int mode;

    p_sbt = p;
   
    if(p_conf && p_conf[0] != '\0')
    {
    	  printf("Bt conf file %s\n", p_conf); 
        strncpy(bta_conf_path, p_conf, MAX_PATH_LEN-1);    	
    }
     	  
	  /* Init App manager */
    app_mgt_init();
    if (app_mgt_open(NULL, app_mgr_mgt_callback) < 0)
    {
        APP_ERROR0("Unable to connect to server");
        return -1;
    }

    /* Init XML state machine */
    app_xml_init();

    if (app_mgr_config())
    {
        APP_ERROR0("Couldn't configure successfully, exiting");
        return -1;
    }
    g_sp_caps = app_xml_config.io_cap;

    /* Display FW versions */
    app_mgr_read_version();

    /* Get the current Stack mode */
    mode = app_dm_get_dual_stack_mode();
    if (mode < 0)
    {
        APP_ERROR0("app_dm_get_dual_stack_mode failed");
        return -1;
    }
    else
    {
        /* Save the current DualStack mode */
        app_mgr_cb.dual_stack_mode = mode;
        APP_INFO1("Current DualStack mode:%s", app_mgr_get_dual_stack_mode_desc());
    }
    
    /* Init avk Application */
    app_avk_init(app_avk_callback);
    //auto register 
    app_avk_register();
    
    /* Init Headset Application */
    app_hs_init();
    /* Start Headset service*/
    app_hs_start(app_hs_callback);
    
    return 0;
}

void s_set_bt_name(const char *name)
{
    app_mgr_set_bd_name(name);	
}

void s_avk_play()
{
    tAPP_AVK_CONNECTION *connection = NULL;
	  
	  connection = app_avk_find_connection_by_bd_addr(dev_connected);
	  if(connection)
	  {
        app_avk_play_start(connection->rc_handle);
    }
    else
    {
    	  printf("Connection is NULL when playing\n");
    }	
}

void s_avk_pause()
{
	  tAPP_AVK_CONNECTION *connection = NULL;
	  
	  connection = app_avk_find_connection_by_bd_addr(dev_connected);
	  if(connection)
	  {
        app_avk_play_pause(connection->rc_handle);
    }
    else
    {
    	  printf("Connection is NULL when pausing\n");
    }
}

void s_avk_play_previous()
{
	  tAPP_AVK_CONNECTION *connection = NULL;
	  
	  connection = app_avk_find_connection_by_bd_addr(dev_connected);
	  if(connection)
	  {
        app_avk_play_previous_track(connection->rc_handle);
    }
    else
    {
    	  printf("Connection is NULL when playing pre\n");
    }
}

void s_avk_play_next()
{
    tAPP_AVK_CONNECTION *connection = NULL;
	  
	  connection = app_avk_find_connection_by_bd_addr(dev_connected);
	  if(connection)
	  {
        app_avk_play_next_track(connection->rc_handle);
    }
    else
    {
    	  printf("Connection is NULL when playing next\n");
    }	
}

void s_hs_pick_up()
{
    app_hs_answer_call();	
}

void s_hs_hung_up()
{
    app_hs_hangup();    	
}

void bluetooth_stop()
{
	  /* Stop Headset service*/
    app_hs_stop();

    app_avk_deregister();
    /* Terminate the avk profile */
    app_avk_end();

    app_mgt_close();
}

#else
int bluetooth_start(void *p, char *p_conf)
{
    return 0;
}

void s_set_bt_name(const char *name)
{
    ;	
}

void s_avk_play()
{
	;
}

void s_avk_pause()
{
	;
}

void s_avk_play_previous()
{
	;
}

void s_avk_play_next()
{
	;
}

void s_hs_pick_up()
{
	;
}

void s_hs_hung_up()
{
	;
}

#endif
