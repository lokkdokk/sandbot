//
// HPB bot - botman's High Ping Bastard bot
//
// (http://planethalflife.com/botman/)
//
// dll.cpp
//

#include "extdll.h"
#include "dllapi.h"
#include "h_export.h"
#include "meta_api.h"
#include "entity_state.h"
#include "studio.h"

#include "bot.h"
#include "bot_func.h"
#include "waypoint.h"


#define MENU_NONE  0
#define MENU_1     1
#define MENU_2     2
#define MENU_3     3


#ifndef __linux__
extern HINSTANCE h_Library;
#else
extern void *h_Library;
#endif

extern enginefuncs_t g_engfuncs;
extern int debug_engine;
extern globalvars_t  *gpGlobals;
extern char g_argv[1024];
extern bool g_waypoint_on;
extern bool g_auto_waypoint;
extern bool g_path_waypoint;
extern int num_waypoints;  // number of waypoints currently in use
extern WAYPOINT waypoints[MAX_WAYPOINTS];
extern float wp_display_time[MAX_WAYPOINTS];
extern bot_t **pBots;
extern bool b_observer_mode;
extern bot_player_t *pBotData;

static FILE *fp;

DLL_FUNCTIONS gFunctionTable;
DLL_FUNCTIONS other_gFunctionTable;
DLL_GLOBAL const Vector g_vecZero = Vector(0,0,0);

int mod_id = 0;
int m_spriteTexture = 0;
int default_bot_skill = 2;
bool isFakeClientCommand = false;
int fake_arg_count;
float bot_check_time = 30.0;
int min_bots = -1;
int max_bots = -1;
int num_bots = 0;
int prev_num_bots = 0;
bool g_GameRules = FALSE;
edict_t *clients[32];
edict_t *listenserver_edict = NULL;
int g_menu_waypoint;
int g_menu_state = 0;

char team_names[MAX_TEAMS][MAX_TEAMNAME_LENGTH];
int num_teams = 0;
edict_t *pent_info_tfdetect = NULL;
edict_t *pent_info_ctfdetect = NULL;
edict_t *pent_item_tfgoal = NULL;
int max_team_players[4];  // for TFC
int team_class_limits[4];  // for TFC
int team_allies[4];  // TFC bit mapped allies BLUE, RED, YELLOW, and GREEN
int max_teams = 0;  // for TFC
FLAG_S flags[MAX_FLAGS];  // for TFC
int num_flags = 0;  // for TFC
int num_backpacks = 0;
BACKPACK_S backpacks[MAX_BACKPACKS];
char arg[256];

FILE *bot_cfg_fp = NULL;
bool need_to_open_cfg = TRUE;
float bot_cfg_pause_time = 0.0;
float respawn_time = 0.0;
bool spawn_time_reset = FALSE;

bool bBaseLinesCreated = false;
bool bServerActivated = false;
bool bCanAddBots = false;

Game *pGame = NULL;

// TheFatal's method for calculating the msecval
int msecnum;
float msecdel;
float msecval;

cvar_t bot_skill = {"bot_skill", "3"};

cvar_t bot_count = {"bot_count", "11"};
cvar_t bot_shoot = {"bot_shoot", "1"};

cvar_t *developer;

// a fresh install of Natural Selection 3.2 will spam the console about the non-existence of this cvar
cvar_t sv_airmove = {"sv_airmove", "0"};

char *show_menu_1 =
   {"Waypoint Tags\n\n1. Team Specific\n2. Wait for Lift\n3. Door\n4. Sniper Spot\n5. More..."};
char *show_menu_2 =
   {"Waypoint Tags\n\n1. Team 1\n2. Team 2\n3. Team 3\n4. Team 4\n5. CANCEL"};
char *show_menu_2_flf =
   {"Waypoint Tags\n\n1. Attackers\n2. Defenders\n\n5. CANCEL"};
char *show_menu_3 =
   {"Waypoint Tags\n\n1. Flag Location\n2. Flag Goal Location\n\n5. CANCEL"};
char *show_menu_3_flf =
   {"Waypoint Tags\n\n1. Capture Point\n2. Defend Point\n3. Prone\n\n5. CANCEL"};

void GameDLLInit( void )
{
	CVAR_REGISTER(&sv_airmove);
	CVAR_REGISTER(&bot_skill);
	CVAR_REGISTER(&bot_count);
	CVAR_REGISTER(&bot_shoot);

	switch( mod_id )
	{
	case VALVE_DLL:
		bot_count.value = 11;
		break;
	case GEARBOX_DLL:
		bot_count.value = 11;
		break;
	case DOD_DLL:
		bot_count.value = 15;
		break;
	case TFC_DLL:
		bot_count.value = 15;
		break;
	case REWOLF_DLL:
		bot_count.value = 11;
		break;
	case NS_DLL:
		bot_count.value = 15;
		break;
	case HUNGER_DLL:
		bot_count.value = 11;
		break;
	case SHIP_DLL:
		bot_count.value = 7;
		break;
	}

	developer = CVAR_GET_POINTER("developer");

	for( int i = 0; i < MAX_PLAYERS; i++ )
	{
		clients[i] = NULL;
	}

	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnGameInit)();
}

int DispatchSpawn( edict_t *pent )
{
	int index;

	char *pClassname = (char *)STRING(pent->v.classname);

  if (debug_engine)
  {
     fp=fopen("bot.txt","a");
     fprintf(fp, "DispatchSpawn: %p %s\n",pent,pClassname);
     if (pent->v.model != 0)
        fprintf(fp, " model=%s\n",STRING(pent->v.model));
     fclose(fp);
  }

  if (strcmp(pClassname, "worldspawn") == 0)
  {
     // do level initialization stuff here...

     WaypointInit();
     WaypointLoad(NULL);

     pent_info_tfdetect = NULL;
     pent_info_ctfdetect = NULL;
     pent_item_tfgoal = NULL;

     for (index=0; index < 4; index++)
     {
        max_team_players[index] = 0;  // no player limit
        team_class_limits[index] = 0;  // no class limits
        team_allies[index] = 0;
     }

     max_teams = 0;
     num_flags = 0;

     for (index=0; index < MAX_FLAGS; index++)
     {
        flags[index].edict = NULL;
        flags[index].team_no = 0;  // any team unless specified
     }

     num_backpacks = 0;

     for (index=0; index < MAX_BACKPACKS; index++)
     {
        backpacks[index].edict = NULL;
        backpacks[index].armor = 0;
        backpacks[index].health = 0;
        backpacks[index].ammo = 0;
        backpacks[index].team = 0;  // any team unless specified
     }

     PRECACHE_SOUND("weapons/xbow_hit1.wav");      // waypoint add
     PRECACHE_SOUND("weapons/mine_activate.wav");  // waypoint delete
     PRECACHE_SOUND("common/wpn_hudoff.wav");      // path add/delete start
     PRECACHE_SOUND("common/wpn_hudon.wav");       // path add/delete done
     PRECACHE_SOUND("common/wpn_moveselect.wav");  // path add/delete cancel
     PRECACHE_SOUND("common/wpn_denyselect.wav");  // path add/delete error
	 PRECACHE_MODEL("models/mechgibs.mdl");

     m_spriteTexture = PRECACHE_MODEL( "sprites/lgtning.spr");

     g_GameRules = TRUE;

     memset(team_names, 0, sizeof(team_names));
     num_teams = 0;

     bot_cfg_pause_time = 0.0;
     respawn_time = 0.0;
     spawn_time_reset = FALSE;

     prev_num_bots = num_bots;
     num_bots = 0;

     bot_check_time = gpGlobals->time + 30.0;
  }

  if( g_bIsMMPlugin )
	  RETURN_META_VALUE( MRES_IGNORED, 0 );

   int result = (*other_gFunctionTable.pfnSpawn)(pent);

   // solves the bots unable to see through certain types of glass bug.
   // MAPPERS: NEVER EVER ALLOW A TRANSPARENT ENTITY TO WEAR THE FL_WORLDBRUSH FLAG !!!

   if( pent->v.rendermode == kRenderTransTexture )
	   pent->v.flags &= ~FL_WORLDBRUSH;  // clear the FL_WORLDBRUSH flag out of transparent ents

   return result;
}

int DispatchSpawn_Post( edict_t * pent )
{
	// solves the bots unable to see through certain types of glass bug.
	// MAPPERS: NEVER EVER ALLOW A TRANSPARENT ENTITY TO WEAR THE FL_WORLDBRUSH FLAG !!!

	if( pent->v.rendermode == kRenderTransTexture )
		pent->v.flags &= ~FL_WORLDBRUSH;  // clear the FL_WORLDBRUSH flag out of transparent ents

	RETURN_META_VALUE( MRES_IGNORED, 0 );
}

void DispatchThink( edict_t *pent )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnThink)(pent);
}

void DispatchUse( edict_t *pentUsed, edict_t *pentOther )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnUse)(pentUsed, pentOther);
}

void DispatchTouch( edict_t *pentTouched, edict_t *pentOther )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnTouch)(pentTouched, pentOther);
}

void DispatchBlocked( edict_t *pentBlocked, edict_t *pentOther )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnBlocked)(pentBlocked, pentOther);
}

void DispatchKeyValue( edict_t *pentKeyvalue, KeyValueData *pkvd )
{
   static edict_t *temp_pent;
   static int flag_index;

   if (mod_id == TFC_DLL)
   {
      if (pentKeyvalue == pent_info_tfdetect)
      {
         if (strcmp(pkvd->szKeyName, "ammo_medikit") == 0)  // max BLUE players
            max_team_players[0] = atoi(pkvd->szValue);
         else if (strcmp(pkvd->szKeyName, "ammo_detpack") == 0)  // max RED players
            max_team_players[1] = atoi(pkvd->szValue);
         else if (strcmp(pkvd->szKeyName, "maxammo_medikit") == 0)  // max YELLOW players
            max_team_players[2] = atoi(pkvd->szValue);
         else if (strcmp(pkvd->szKeyName, "maxammo_detpack") == 0)  // max GREEN players
            max_team_players[3] = atoi(pkvd->szValue);

         else if (strcmp(pkvd->szKeyName, "maxammo_shells") == 0)  // BLUE class limits
            team_class_limits[0] = atoi(pkvd->szValue);
         else if (strcmp(pkvd->szKeyName, "maxammo_nails") == 0)  // RED class limits
            team_class_limits[1] = atoi(pkvd->szValue);
         else if (strcmp(pkvd->szKeyName, "maxammo_rockets") == 0)  // YELLOW class limits
            team_class_limits[2] = atoi(pkvd->szValue);
         else if (strcmp(pkvd->szKeyName, "maxammo_cells") == 0)  // GREEN class limits
            team_class_limits[3] = atoi(pkvd->szValue);

         else if (strcmp(pkvd->szKeyName, "team1_allies") == 0)  // BLUE allies
            team_allies[0] = atoi(pkvd->szValue);
         else if (strcmp(pkvd->szKeyName, "team2_allies") == 0)  // RED allies
            team_allies[1] = atoi(pkvd->szValue);
         else if (strcmp(pkvd->szKeyName, "team3_allies") == 0)  // YELLOW allies
            team_allies[2] = atoi(pkvd->szValue);
         else if (strcmp(pkvd->szKeyName, "team4_allies") == 0)  // GREEN allies
            team_allies[3] = atoi(pkvd->szValue);
      }
      else if (pent_info_tfdetect == NULL)
      {
         if ((strcmp(pkvd->szKeyName, "classname") == 0) && (strcmp(pkvd->szValue, "info_tfdetect") == 0))
         {
            pent_info_tfdetect = pentKeyvalue;
         }
      }

      if (pentKeyvalue == pent_item_tfgoal)
      {
         if (strcmp(pkvd->szKeyName, "team_no") == 0)
            flags[flag_index].team_no = atoi(pkvd->szValue);

         if ((strcmp(pkvd->szKeyName, "mdl") == 0) &&
             ((strcmp(pkvd->szValue, "models/flag.mdl") == 0) ||
              (strcmp(pkvd->szValue, "models/keycard.mdl") == 0) ||
              (strcmp(pkvd->szValue, "models/ball.mdl") == 0)))
         {
            flags[flag_index].mdl_match = TRUE;
            num_flags++;
         }
      }
      else if (pent_item_tfgoal == NULL)
      {
         if ((strcmp(pkvd->szKeyName, "classname") == 0) && (strcmp(pkvd->szValue, "item_tfgoal") == 0))
         {
            if (num_flags < MAX_FLAGS)
            {
               pent_item_tfgoal = pentKeyvalue;

               flags[num_flags].mdl_match = FALSE;
               flags[num_flags].team_no = 0;  // any team unless specified
               flags[num_flags].edict = pentKeyvalue;

               flag_index = num_flags;  // in case the mdl comes before team_no
            }
         }
      }
      else
      {
         pent_item_tfgoal = NULL;  // reset for non-flag item_tfgoal's
      }

      if ((strcmp(pkvd->szKeyName, "classname") == 0) &&
          ((strcmp(pkvd->szValue, "info_player_teamspawn") == 0) ||
           (strcmp(pkvd->szValue, "i_p_t") == 0)))
      {
         temp_pent = pentKeyvalue;
      }
      else if (pentKeyvalue == temp_pent)
      {
         if (strcmp(pkvd->szKeyName, "team_no") == 0)
         {
            int value = atoi(pkvd->szValue);

            if (value > max_teams)
               max_teams = value;
         }
      }
   }
   else if (mod_id == GEARBOX_DLL)
   {
      if (pent_info_ctfdetect == NULL)
      {
         if ((strcmp(pkvd->szKeyName, "classname") == 0) && (strcmp(pkvd->szValue, "info_ctfdetect") == 0))
         {
            pent_info_ctfdetect = pentKeyvalue;
         }
      }
   }

   if( g_bIsMMPlugin )
	   RETURN_META( MRES_IGNORED );

   (*other_gFunctionTable.pfnKeyValue)(pentKeyvalue, pkvd);
}

void DispatchSave( edict_t *pent, SAVERESTOREDATA *pSaveData )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnSave)(pent, pSaveData);
}

int DispatchRestore( edict_t *pent, SAVERESTOREDATA *pSaveData, int globalEntity )
{
	if( g_bIsMMPlugin )
		RETURN_META_VALUE( MRES_IGNORED, 0 );

	return (*other_gFunctionTable.pfnRestore)(pent, pSaveData, globalEntity);
}

void DispatchObjectCollisionBox( edict_t *pent )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnSetAbsBox)(pent);
}

void SaveWriteFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnSaveWriteFields)(pSaveData, pname, pBaseData, pFields, fieldCount);
}

void SaveReadFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnSaveReadFields)(pSaveData, pname, pBaseData, pFields, fieldCount);
}

void SaveGlobalState( SAVERESTOREDATA *pSaveData )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnSaveGlobalState)(pSaveData);
}

void RestoreGlobalState( SAVERESTOREDATA *pSaveData )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnRestoreGlobalState)(pSaveData);
}

void ResetGlobalState( void )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnResetGlobalState)();
}

BOOL ClientConnect( edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128] )
{
	int i;
	int count = 0;

	if( debug_engine ) { fp = fopen( "bot.txt", "a" ); fprintf( fp, "ClientConnect: pent=%p name=%s\n", pEntity, pszName ); fclose( fp ); }

	// check if this client is the listen server client
	if( strcmp( pszAddress, "loopback" ) == 0 )
	{
		// save the edict of the listen server client...
		listenserver_edict = pEntity;
	}

	// check if this is NOT a bot joining the server...
	if( strcmp( pszAddress, "127.0.0.1" ) != 0 )
	{
		// don't try to add bots for 60 seconds, give client time to get added
		bot_check_time = gpGlobals->time + 60.0;

		for( i = 0; i < MAX_PLAYERS; i++ )
		{
			if( pBots[i]->is_used )  // count the number of bots in use
				count++;
		}

		// if there are currently more than the minimum number of bots running
		// then kick one of the bots off the server...
		if( (count > min_bots) && (min_bots != -1) )
		{
			for( i = 0; i < MAX_PLAYERS; i++ )
			{
				if( pBots[i]->is_used )  // is this slot used?
				{
					char cmd[80];

					sprintf( cmd, "kick \"%s\"\n", pBots[i]->name );

					SERVER_COMMAND( cmd );  // kick the bot using (kick "name")

					break;
				}
			}
		}
	}

	if( g_bIsMMPlugin )
		RETURN_META_VALUE( MRES_IGNORED, 0 );

	return (*other_gFunctionTable.pfnClientConnect)(pEntity, pszName, pszAddress, szRejectReason);
}

void ClientDisconnect( edict_t *pEntity )
{
	UTIL_LogDPrintf( "ClientDisconnect: pEntity=%x\n", pEntity );

	int i = 0;
	while( (i < MAX_PLAYERS) && (clients[i] != pEntity) )
		i++;

	if( i < MAX_PLAYERS )
		clients[i] = NULL;

	for( i = 0; i < MAX_PLAYERS; i++ )
	{
		if( pBots && pBots[i] && pBots[i]->pEdict == pEntity )
		{
			pBots[i]->SetKicked();
			// TODO: experiment in kicking all bots at the end of each map
			// don't put this in SetKicked because we only want to try setting this when all
			// bots are kicked so that the total player count doesn't get out of sync with
			// which bots have is_used set to true - this all really needs to be simplified
			pBots[i]->is_used = false;
			break;
		}
	}

	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnClientDisconnect)(pEntity);
}

void ClientKill( edict_t *pEntity )
{
	UTIL_LogDPrintf("ClientKill: pEntity=%x\n", pEntity);

	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnClientKill)(pEntity);
}

void ClientPutInServer( edict_t *pEntity )
{
	UTIL_LogDPrintf("ClientPutInServer: pEntity=%x\n", pEntity);

	NewActiveClient( pEntity );

	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnClientPutInServer)(pEntity);
}

void ClientCommand( edict_t *pEntity )
{
	const char *pcmd = CMD_ARGV(0);
	const char *arg1 = CMD_ARGV(1);
	const char *arg2 = CMD_ARGV(2);
	const char *arg3 = CMD_ARGV(3);
	const char *arg4 = CMD_ARGV(4);

	UTIL_LogDPrintf("ClientCommand: pEntity=%x, pcmd=%s", pEntity, pcmd);
	if ((arg1 != NULL) && (*arg1 != 0))
		UTIL_LogDPrintf(", arg1=%s", arg1);
	if ((arg2 != NULL) && (*arg2 != 0))
		UTIL_LogDPrintf(", arg2=%s", arg2);
	if ((arg3 != NULL) && (*arg3 != 0))
		UTIL_LogDPrintf(", arg3=%s", arg3);
	if ((arg4 != NULL) && (*arg4 != 0))
		UTIL_LogDPrintf(", arg4=%s", arg4);
	UTIL_LogDPrintf("\n");

   // only allow custom commands if NOT dedicated server and
   // client sending command is the listen server client...

   if (!IS_DEDICATED_SERVER() && (pEntity == listenserver_edict))
   {
      char msg[80];

      if (FStrEq(pcmd, "addbot"))
      {
         BotCreate( pEntity, arg1, arg2, arg3, arg4 );

         bot_check_time = gpGlobals->time + 5.0;

		 if( g_bIsMMPlugin )
			 RETURN_META( MRES_SUPERCEDE );

         return;
      }
	  // seems to be like a bots ignore real players setting?
      else if (FStrEq(pcmd, "observer"))
      {
         if ((arg1 != NULL) && (*arg1 != 0))
         {
            int temp = atoi(arg1);
            if (temp)
               b_observer_mode = TRUE;
            else
               b_observer_mode = FALSE;
         }

         if (b_observer_mode)
            ClientPrint(pEntity, HUD_PRINTNOTIFY, "observer mode ENABLED\n");
         else
            ClientPrint(pEntity, HUD_PRINTNOTIFY, "observer mode DISABLED\n");

		 if( g_bIsMMPlugin )
			 RETURN_META( MRES_SUPERCEDE );

         return;
      }
      else if (FStrEq(pcmd, "botskill"))
      {
         if ((arg1 != NULL) && (*arg1 != 0))
         {
            int temp = atoi(arg1);

            if ((temp < 1) || (temp > 5))
               ClientPrint(pEntity, HUD_PRINTNOTIFY, "invalid botskill value!\n");
            else
               default_bot_skill = temp;
         }

         sprintf(msg, "botskill is %d\n", default_bot_skill);
         ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

		 if( g_bIsMMPlugin )
			 RETURN_META( MRES_SUPERCEDE );

         return;
      }
      else if (FStrEq(pcmd, "debug_engine"))
      {
         debug_engine = 1;

         ClientPrint(pEntity, HUD_PRINTNOTIFY, "debug_engine enabled!\n");

		 if( g_bIsMMPlugin )
			 RETURN_META( MRES_SUPERCEDE );

         return;
      }
      else if (FStrEq(pcmd, "waypoint"))
      {
         if (FStrEq(arg1, "on"))
         {
            g_waypoint_on = TRUE;

            ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints are ON\n");
         }
         else if (FStrEq(arg1, "off"))
         {
            g_waypoint_on = FALSE;

            ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints are OFF\n");
         }
         else if (FStrEq(arg1, "add"))
         {
            if (!g_waypoint_on)
               g_waypoint_on = TRUE;  // turn waypoints on if off

            WaypointAdd(pEntity);
         }
         else if (FStrEq(arg1, "delete"))
         {
            if (!g_waypoint_on)
               g_waypoint_on = TRUE;  // turn waypoints on if off

            WaypointDelete(pEntity);
         }
         else if (FStrEq(arg1, "save"))
         {
            WaypointSave();

            ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints saved\n");
         }
         else if (FStrEq(arg1, "load"))
         {
            if (WaypointLoad(pEntity))
               ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints loaded\n");
         }
         else if (FStrEq(arg1, "menu"))
         {
            int index;

			if( num_waypoints < 1 )
			{
				if( g_bIsMMPlugin )
					RETURN_META( MRES_SUPERCEDE );

				return;
			}

            index = WaypointFindNearest(pEntity, 50.0, -1);

			if( index == -1 )
			{
				if( g_bIsMMPlugin )
					RETURN_META( MRES_SUPERCEDE );

				return;
			}

            g_menu_waypoint = index;
            g_menu_state = MENU_1;

            UTIL_ShowMenu(pEntity, 0x1F, -1, FALSE, show_menu_1);
         }
         else if (FStrEq(arg1, "info"))
         {
            WaypointPrintInfo(pEntity);
         }
         else
         {
            if (g_waypoint_on)
               ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints are ON\n");
            else
               ClientPrint(pEntity, HUD_PRINTNOTIFY, "waypoints are OFF\n");
         }

		 if( g_bIsMMPlugin )
			 RETURN_META( MRES_SUPERCEDE );

         return;
      }
      else if (FStrEq(pcmd, "autowaypoint"))
      {
         if (FStrEq(arg1, "on"))
         {
            g_auto_waypoint = TRUE;
            g_waypoint_on = TRUE;  // turn this on just in case
         }
         else if (FStrEq(arg1, "off"))
         {
            g_auto_waypoint = FALSE;
         }

         if (g_auto_waypoint)
            sprintf(msg, "autowaypoint is ON\n");
         else
            sprintf(msg, "autowaypoint is OFF\n");

         ClientPrint(pEntity, HUD_PRINTNOTIFY, msg);

		 if( g_bIsMMPlugin )
			 RETURN_META( MRES_SUPERCEDE );

         return;
      }
      else if (FStrEq(pcmd, "pathwaypoint"))
      {
         if (FStrEq(arg1, "on"))
         {
            g_path_waypoint = TRUE;
            g_waypoint_on = TRUE;  // turn this on just in case

            ClientPrint(pEntity, HUD_PRINTNOTIFY, "pathwaypoint is ON\n");
         }
         else if (FStrEq(arg1, "off"))
         {
            g_path_waypoint = FALSE;

            ClientPrint(pEntity, HUD_PRINTNOTIFY, "pathwaypoint is OFF\n");
         }
         else if (FStrEq(arg1, "create1"))
         {
            WaypointCreatePath(pEntity, 1);
         }
         else if (FStrEq(arg1, "create2"))
         {
            WaypointCreatePath(pEntity, 2);
         }
         else if (FStrEq(arg1, "remove1"))
         {
            WaypointRemovePath(pEntity, 1);
         }
         else if (FStrEq(arg1, "remove2"))
         {
            WaypointRemovePath(pEntity, 2);
         }

		 if( g_bIsMMPlugin )
			 RETURN_META( MRES_SUPERCEDE );

         return;
      }
      else if (FStrEq(pcmd, "menuselect") && (g_menu_state != MENU_NONE))
      {
         if (g_menu_state == MENU_1)  // main menu...
         {
            if (FStrEq(arg1, "1"))  // team specific...
            {
               g_menu_state = MENU_2;  // display team specific menu...

               UTIL_ShowMenu(pEntity, 0x1F, -1, FALSE, show_menu_2);

			   if( g_bIsMMPlugin )
				   RETURN_META( MRES_SUPERCEDE );

               return;
            }
            else if (FStrEq(arg1, "2"))  // wait for lift...
            {
               if (waypoints[g_menu_waypoint].flags & W_FL_LIFT)
                  waypoints[g_menu_waypoint].flags &= ~W_FL_LIFT;  // off
               else
                  waypoints[g_menu_waypoint].flags |= W_FL_LIFT;  // on
            }
            else if (FStrEq(arg1, "3"))  // door waypoint
            {
               if (waypoints[g_menu_waypoint].flags & W_FL_DOOR)
                  waypoints[g_menu_waypoint].flags &= ~W_FL_DOOR;  // off
               else
                  waypoints[g_menu_waypoint].flags |= W_FL_DOOR;  // on
            }
            else if (FStrEq(arg1, "4"))  // sniper spot
            {
               if (waypoints[g_menu_waypoint].flags & W_FL_SNIPER)
                  waypoints[g_menu_waypoint].flags &= ~W_FL_SNIPER;  // off
               else
               {
                  waypoints[g_menu_waypoint].flags |= W_FL_SNIPER;  // on

                  // set the aiming waypoint...

                  WaypointAddAiming(pEntity);
               }
            }
            else if (FStrEq(arg1, "5"))  // more...
            {
               g_menu_state = MENU_3;

               UTIL_ShowMenu(pEntity, 0x13, -1, FALSE, show_menu_3);

			   if( g_bIsMMPlugin )
				   RETURN_META( MRES_SUPERCEDE );

               return;
            }
         }
         else if (g_menu_state == MENU_2)  // team specific menu
         {
            if (waypoints[g_menu_waypoint].flags & W_FL_TEAM_SPECIFIC)
            {
               waypoints[g_menu_waypoint].flags &= ~W_FL_TEAM;
               waypoints[g_menu_waypoint].flags &= ~W_FL_TEAM_SPECIFIC; // off
            }
            else
            {
               int team = atoi(arg1);

               team--;  // make 0 to 3

               // this is kind of a kludge (team bits MUST be LSB!!!)
               waypoints[g_menu_waypoint].flags |= team;
               waypoints[g_menu_waypoint].flags |= W_FL_TEAM_SPECIFIC; // on
            }
         }
         else if (g_menu_state == MENU_3)  // second menu...
         {
            if (FStrEq(arg1, "1"))  // flag location
            {
               if (waypoints[g_menu_waypoint].flags & W_FL_TFC_FLAG)
                  waypoints[g_menu_waypoint].flags &= ~W_FL_TFC_FLAG;  // off
               else
                  waypoints[g_menu_waypoint].flags |= W_FL_TFC_FLAG;  // on
            }
            else if (FStrEq(arg1, "2"))  // flag goal
            {
               if (waypoints[g_menu_waypoint].flags & W_FL_TFC_FLAG_GOAL)
                  waypoints[g_menu_waypoint].flags &= ~W_FL_TFC_FLAG_GOAL;  // off
               else
                  waypoints[g_menu_waypoint].flags |= W_FL_TFC_FLAG_GOAL;  // on
            }
         }

         g_menu_state = MENU_NONE;

		 if( g_bIsMMPlugin )
			 RETURN_META( MRES_SUPERCEDE );

         return;
      }
      else if (FStrEq(pcmd, "search"))
      {
         edict_t *pent = NULL;
         float radius = 50;
         char str[80];

         ClientPrint(pEntity, HUD_PRINTCONSOLE, "searching...\n");

         while ((pent = UTIL_FindEntityInSphere( pent, pEntity->v.origin, radius )) != NULL)
         {
            sprintf(str, "Found %s at %5.2f %5.2f %5.2f\n", STRING(pent->v.classname),
                       pent->v.origin.x, pent->v.origin.y, pent->v.origin.z);
            ClientPrint(pEntity, HUD_PRINTCONSOLE, str);
         }

		 if( g_bIsMMPlugin )
			 RETURN_META( MRES_SUPERCEDE );

         return;
      }
		else if (FStrEq(pcmd, "player_info") && DEBUG_CODE)
		{
			int playerIndex = atoi( arg1 );

			edict_t *player = g_engfuncs.pfnPEntityOfEntIndex( playerIndex );

			if( !player )
			{
				ALERT( at_console, "Player not found\n" );

				if( g_bIsMMPlugin )
					RETURN_META( MRES_SUPERCEDE );

				return;
			}

			bot_t *pBot = NULL;

			if( player->v.flags & FL_FAKECLIENT )
			{
				pBot = UTIL_GetBotPointer( player );
				// ALERT( at_console, "%d\n", pBot->GetLightLevel() );
			}

			ALERT( at_console, "Light level: %d\n", player->v.light_level );
			ALERT( at_console, "Team: %d\n", player->v.team );
			ALERT( at_console, "Class: %d\n", player->v.playerclass );
			ALERT( at_console, "Speed: %d\n", player->v.speed );

			if( mod_id == VALVE_DLL )
			{
			}
			else if( mod_id == DOD_DLL )
			{
				if( player->v.team == DODBot::TEAM_ALLIES )
				{
					ALERT( at_console, "Team: Allies\n" );
				}
				else if( player->v.team == DODBot::TEAM_AXIS )
				{
					ALERT( at_console, "Team: Axis\n" );
				}

				if( pBot )
				{
					ALERT( at_console, "Capturing: %d\n", ((DODBot *)pBot)->bCapturing );
				}

#if 0
				for( int i = 0; i < 32; i++ )
				{
					if( player->v.weapons & (1<<i) )
					{
						ALERT( at_console, "weapon %d\n", i );
					}
				}
#endif
			}
			else if( mod_id == NS_DLL )
			{
				extern float UTIL_GetExperience( edict_t *player );

				if( player->v.team == NSBot::TEAM_MARINE )
				{
					ALERT( at_console, "Team: Marine\n" );
				}
				else if( player->v.team == NSBot::TEAM_ALIEN )
				{
					ALERT( at_console, "Team: Alien\n" );
				}
				ALERT( at_console, "Resources: %f\n", player->v.vuser4.z / 100.0 );
				ALERT( at_console, "Experience: %f\n", UTIL_GetExperience( player ) );

#if 0
				if( player->v.team == NSBot::TEAM_MARINE )
				{
					if( player->v.iuser4 & MASK_UPGRADE_1 )
					{
						ALERT( at_console, "Marine Weapons 1\n" );
					}
					if( player->v.iuser4 & MASK_UPGRADE_4 )
					{
						ALERT( at_console, "Marine Armor 1\n" );
					}
				}
				else if( player->v.team == NSBot::TEAM_ALIEN )
				{
					if( player->v.iuser4 & MASK_UPGRADE_1 )
					{
						ALERT( at_console, "Carapace\n" );
					}
					if( player->v.iuser4 & MASK_UPGRADE_2 )
					{
						ALERT( at_console, "Regeneration\n" );
					}
					if( player->v.iuser4 & MASK_UPGRADE_4 )
					{
						ALERT( at_console, "Celerity\n" );
					}
				}
#endif
			}

			if( g_bIsMMPlugin )
				RETURN_META( MRES_SUPERCEDE );

			return;
		}
   }

   if( g_bIsMMPlugin )
	   RETURN_META( MRES_IGNORED );

   (*other_gFunctionTable.pfnClientCommand)(pEntity);
}

void ClientUserInfoChanged( edict_t *pEntity, char *infobuffer )
{
	UTIL_LogDPrintf("ClientUserInfoChanged: pEntity=%x infobuffer=%s\n", pEntity, infobuffer);

	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnClientUserInfoChanged)(pEntity, infobuffer);
}

void ServerActivate( edict_t *pEdictList, int edictCount, int clientMax )
{
	// make sure it's cleaned up (just in case)
	if( pGame )
	{
		delete pGame;
		pGame = NULL;
	}
	if( pBotData )
	{
		for( int i = 0; i < MAX_PLAYERS; i++ )
		{
			pBotData[i].bIsUsed = false;
		}
	}
	if( pBots )
	{
		for( int i = 0; i < MAX_PLAYERS; i++ )
		{
			delete pBots[i];
			pBots[i] = NULL;
		}
		pBots = NULL;
	}

	pBots = new bot_t*[MAX_PLAYERS];

	if( mod_id == VALVE_DLL )
	{
		pGame = new Game();

		for( int i = 0; i < MAX_PLAYERS; i++ )
		{
			pBots[i] = new HalfLifeBot();
		}
	}
	else if( mod_id == GEARBOX_DLL )
	{
		pGame = new GearboxGame();

		for( int i = 0; i < MAX_PLAYERS; i++ )
		{
			pBots[i] = new OpposingForceBot();
		}
	}
	else if( mod_id == DECAY_DLL )
	{
		pGame = new DecayGame();

		for( int i = 0; i < MAX_PLAYERS; i++ )
		{
			pBots[i] = new HalfLifeBot();
		}
	}
	else if( mod_id == CSTRIKE_DLL || mod_id == CZERO_DLL )
	{
		pGame = new CStrikeGame();

		for( int i = 0; i < MAX_PLAYERS; i++ )
		{
			pBots[i] = new CStrikeBot();
		}
	}
	else if( mod_id == DOD_DLL )
	{
		pGame = new DODGame();

		for( int i = 0; i < MAX_PLAYERS; i++ )
		{
			pBots[i] = new DODBot();
		}
	}
	else if( mod_id == TFC_DLL )
	{
		pGame = new TFCGame();

		for( int i = 0; i < MAX_PLAYERS; i++ )
		{
			pBots[i] = new TFCBot();
		}
	}
	else if( mod_id == REWOLF_DLL )
	{
		pGame = new Game();

		for( int i = 0; i < MAX_PLAYERS; i++ )
		{
			pBots[i] = new GunmanBot();
		}
	}
	else if( mod_id == NS_DLL )
	{
		pGame = new NSGame();

		for( int i = 0; i < MAX_PLAYERS; i++ )
		{
			pBots[i] = new NSBot();
		}
	}
	else if( mod_id == SHIP_DLL )
	{
		pGame = new Game();

		for( int i = 0; i < MAX_PLAYERS; i++ )
		{
			pBots[i] = new ShipBot();
		}
	}
	else
	{
		pGame = new Game();

		for( int i = 0; i < MAX_PLAYERS; i++ )
		{
			pBots[i] = new bot_t();
		}
	}

	bServerActivated = true;
	bCanAddBots = false;

	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnServerActivate)(pEdictList, edictCount, clientMax);
}

void ServerDeactivate( void )
{
	CleanupGameAndBots();

	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnServerDeactivate)();
}

void PlayerPreThink( edict_t *pEntity )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnPlayerPreThink)(pEntity);
}

void PlayerPostThink( edict_t *pEntity )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnPlayerPostThink)(pEntity);
}

bool g_bInGame = false;
int g_iCountDown = 0;
float g_fCountDownTime = 0.0;

void StartFrame( void )
{
	edict_t *pPlayer;
	static float check_server_cmd = 0.0;
	static int i, index, player_index, bot_index;
	static float previous_time = -1.0;
	int count;

	if( !g_bInGame )
	{
		if( mod_id == NS_DLL )
		{
			// if the Countdown message has been sent, make a note of the time
			if( g_iCountDown && !g_fCountDownTime )
			{
				g_fCountDownTime = gpGlobals->time;
			}

			// if the Countdown time (plus a bit) has passed, we're in-game
			if( g_iCountDown && g_fCountDownTime + (float)g_iCountDown + 1.0 < gpGlobals->time )
			{
				g_bInGame = true;
			}
		}
	}

	// if a new map has started then (MUST BE FIRST IN StartFrame)...
	if( (gpGlobals->time + 0.1) < previous_time )
	{
		check_server_cmd = 0.0;  // reset at start of map

		msecnum = 0;
		msecdel = 0;
		msecval = 0;

		count = 0;

		// mark the bots as needing to be respawned...
		for( index = 0; index < MAX_PLAYERS; index++ )
		{
			if( count >= prev_num_bots )
			{
				pBots[index]->is_used = FALSE;
				pBots[index]->respawn_state = 0;
				pBots[index]->kick_time = 0.0;
			}

			if( pBots[index]->is_used )  // is this slot used?
			{
				pBots[index]->respawn_state = RESPAWN_NEED_TO_RESPAWN;
				count++;
			}

			// check for any bots that were very recently kicked...
			if( (pBots[index]->kick_time + 5.0) > previous_time )
			{
				pBots[index]->respawn_state = RESPAWN_NEED_TO_RESPAWN;
				count++;
			}
			else
				pBots[index]->kick_time = 0.0;  // reset to prevent false spawns later
		}

		// set the respawn time
		if( IS_DEDICATED_SERVER() )
			respawn_time = gpGlobals->time + 5.0;
		else
			respawn_time = gpGlobals->time + 20.0;

		bot_check_time = gpGlobals->time + 30.0;
	}

	// adjust the millisecond delay based on the frame rate interval...
	if( msecdel <= gpGlobals->time )
	{
		msecdel = gpGlobals->time + 0.5;
		if( msecnum > 0 )
			msecval = 450.0 / msecnum;
		msecnum = 0;
	}
	else
		msecnum++;

	if( msecval < 1 )    // don't allow msec to be less than 1...
		msecval = 1;

	if( msecval > 100 )  // ...or greater than 100
		msecval = 100;

	count = 0;

	for( bot_index = 0; bot_index < gpGlobals->maxClients; bot_index++ )
	{
		// is this slot used AND not respawning
		if( !pBots[bot_index]->IsKicked() && pBots[bot_index]->is_used && pBots[bot_index]->respawn_state == RESPAWN_IDLE )
		{
			BotThink( pBots[bot_index] );

			count++;
		}
		else if( pBots[bot_index]->IsKicked() )
		{
			// TODO: is_used is set to by IsKicked - probably don't touch iBotCount - adjusting things based on it would be more complicated
		}
	}

	if( count > num_bots )
		num_bots = count;

	for( player_index = 1; player_index <= gpGlobals->maxClients; player_index++ )
	{
		pPlayer = INDEXENT( player_index );

		if( pPlayer && !pPlayer->free )
		{
			if( (g_waypoint_on) && FBitSet( pPlayer->v.flags, FL_CLIENT ) )
			{
				// ALERT( at_console, "%f\n", pPlayer->v.maxspeed );
				WaypointThink( pPlayer );
			}
		}
	}

	if( g_GameRules )
	{
		if( !IS_DEDICATED_SERVER() && !spawn_time_reset )
		{
			if( listenserver_edict != NULL )
			{
				if( IsAlive( listenserver_edict ) )
				{
					spawn_time_reset = TRUE;

					if( respawn_time >= 1.0 )
						respawn_time = min( respawn_time, gpGlobals->time + (float)1.0 );
				}
			}
		}
	}

	// check if time to see if a bot needs to be created...
	if( bot_check_time < gpGlobals->time )
	{
		int count = 0;

		bot_check_time = gpGlobals->time + 5.0;

		for( i = 0; i < MAX_PLAYERS; i++ )
		{
			if( clients[i] != NULL )
				count++;
		}

		// if there are currently less than the maximum number of "players"
		// then add another bot using the default skill level...
		if( (count < max_bots) && (max_bots != -1) )
		{
			BotCreate( NULL, NULL, NULL, NULL, NULL );
		}
	}

	previous_time = gpGlobals->time;

	if( bBaseLinesCreated )
	{
		bCanAddBots = true;
		bBaseLinesCreated = false;
	}

	if( pGame->CanAddBots() && bCanAddBots && GetBotCount() < bot_count.value )
	{
		BotCreate( NULL, NULL, NULL, NULL, NULL );
	}

	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnStartFrame)();
}

void ParmsNewLevel( void )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnParmsNewLevel)();
}

void ParmsChangeLevel( void )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnParmsChangeLevel)();
}

const char *GetGameDescription( void )
{
	if( g_bIsMMPlugin )
		RETURN_META_VALUE( MRES_IGNORED, 0 );

	return (*other_gFunctionTable.pfnGetGameDescription)();
}

void PlayerCustomization( edict_t *pEntity, customization_t *pCust )
{
	UTIL_LogDPrintf("PlayerCustomization: pEntity=%x pCust=%x\n", pEntity, pCust);

	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnPlayerCustomization)(pEntity, pCust);
}

void SpectatorConnect( edict_t *pEntity )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnSpectatorConnect)(pEntity);
}

void SpectatorDisconnect( edict_t *pEntity )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnSpectatorDisconnect)(pEntity);
}

void SpectatorThink( edict_t *pEntity )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnSpectatorThink)(pEntity);
}

void Sys_Error( const char *error_string )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnSys_Error)(error_string);
}

void PM_Move( struct playermove_s *ppmove, int server )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnPM_Move)(ppmove, server);
}

void PM_Init( struct playermove_s *ppmove )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnPM_Init)(ppmove);
}

char PM_FindTextureType( char *name )
{
	if( g_bIsMMPlugin )
		RETURN_META_VALUE( MRES_IGNORED, 0 );

	return (*other_gFunctionTable.pfnPM_FindTextureType)(name);
}

void SetupVisibility( edict_t *pViewEntity, edict_t *pClient, unsigned char **pvs, unsigned char **pas )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnSetupVisibility)(pViewEntity, pClient, pvs, pas);
}

void UpdateClientData( const struct edict_s *ent, int sendweapons, struct clientdata_s *cd )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnUpdateClientData)(ent, sendweapons, cd);
}

int AddToFullPack( struct entity_state_s *state, int e, edict_t *ent, edict_t *host, int hostflags, int player, unsigned char *pSet )
{
	if( g_bIsMMPlugin )
		RETURN_META_VALUE( MRES_IGNORED, 0 );

	return (*other_gFunctionTable.pfnAddToFullPack)(state, e, ent, host, hostflags, player, pSet);
}

void CreateBaseline( int player, int eindex, struct entity_state_s *baseline, struct edict_s *entity, int playermodelindex, vec3_t player_mins, vec3_t player_maxs )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnCreateBaseline)(player, eindex, baseline, entity, playermodelindex, player_mins, player_maxs);
}

void RegisterEncoders( void )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnRegisterEncoders)();
}

int GetWeaponData( struct edict_s *player, struct weapon_data_s *info )
{
	if( g_bIsMMPlugin )
		RETURN_META_VALUE( MRES_IGNORED, 0 );

	return (*other_gFunctionTable.pfnGetWeaponData)(player, info);
}

void CmdStart( const edict_t *player, const struct usercmd_s *cmd, unsigned int random_seed )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnCmdStart)(player, cmd, random_seed);
}

void CmdEnd ( const edict_t *player )
{
	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnCmdEnd)(player);
}

int ConnectionlessPacket( const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size )
{
	if( g_bIsMMPlugin )
		RETURN_META_VALUE( MRES_IGNORED, 0 );

	return (*other_gFunctionTable.pfnConnectionlessPacket)(net_from, args, response_buffer, response_buffer_size);
}

int GetHullBounds( int hullnumber, float *mins, float *maxs )
{
	if( g_bIsMMPlugin )
		RETURN_META_VALUE( MRES_IGNORED, 0 );

	return (*other_gFunctionTable.pfnGetHullBounds)(hullnumber, mins, maxs);
}

void CreateInstancedBaselines( void )
{
	if( bServerActivated )
	{
		bBaseLinesCreated = true;
		bServerActivated = false;
	}

	if( g_bIsMMPlugin )
		RETURN_META( MRES_IGNORED );

	(*other_gFunctionTable.pfnCreateInstancedBaselines)();
}

int InconsistentFile( const edict_t *player, const char *filename, char *disconnect_message )
{
	UTIL_LogDPrintf("InconsistentFile: player=%x filename=%s disconnect_message=%s\n", player, filename, disconnect_message);

	if( g_bIsMMPlugin )
		RETURN_META_VALUE( MRES_IGNORED, 0 );

	return (*other_gFunctionTable.pfnInconsistentFile)(player, filename, disconnect_message);
}

int AllowLagCompensation( void )
{
	if( g_bIsMMPlugin )
		RETURN_META_VALUE( MRES_IGNORED, 0 );

	return (*other_gFunctionTable.pfnAllowLagCompensation)();
}

void FakeClientCommand( edict_t *pFakeClient, const char *fmt, ... )
{
	// the purpose of this function is to provide fakeclients (bots) with the same client
	// command-scripting advantages (putting multiple commands in one line between semicolons)
	// as real players. It is an improved version of botman's FakeClientCommand, in which you
	// supply directly the whole string as if you were typing it in the bot's "console". It
	// is supposed to work exactly like the pfnClientCommand (server-sided client command).

	va_list argptr;
	static char command[256];
	int length, fieldstart, fieldstop, i, index, stringindex = 0;

	if( FNullEnt( pFakeClient ) )
		return;                   // reliability check

								  // concatenate all the arguments in one string
	va_start( argptr, fmt );
	_vsnprintf( command, sizeof( command ), fmt, argptr );
	va_end( argptr );

	if( command == NULL || *command == '\0' )
		return;                   // if nothing in the command buffer, return

	isFakeClientCommand = true;  // set the "fakeclient command" flag
	length = strlen( command );    // get the total length of the command string

								   // process all individual commands (separated by a semicolon) one each a time
	while( stringindex < length )
	{
		fieldstart = stringindex; // save field start position (first character)
		while( stringindex < length && command[stringindex] != ';' )
			stringindex++;         // reach end of field
		if( command[stringindex - 1] == '\n' )
			fieldstop = stringindex - 2;   // discard any trailing '\n' if needed
		else
			fieldstop = stringindex - 1;   // save field stop position (last character before semicolon or end)
		for( i = fieldstart; i <= fieldstop; i++ )
			g_argv[i - fieldstart] = command[i];   // store the field value in the g_argv global string
		g_argv[i - fieldstart] = 0;       // terminate the string
		stringindex++;            // move the overall string index one step further to bypass the semicolon

		index = 0;
		fake_arg_count = 0;       // let's now parse that command and count the different arguments

								  // count the number of arguments
		while( index < i - fieldstart )
		{
			while( index < i - fieldstart && g_argv[index] == ' ' )
				index++;            // ignore spaces

									// is this field a group of words between quotes or a single word ?
			if( g_argv[index] == '"' )
			{
				index++;            // move one step further to bypass the quote
				while( index < i - fieldstart && g_argv[index] != '"' )
					index++;         // reach end of field
				index++;            // move one step further to bypass the quote
			}
			else
				while( index < i - fieldstart && g_argv[index] != ' ' )
					index++;         // this is a single word, so reach the end of field

			fake_arg_count++;      // we have processed one argument more
		}

		// tell now the MOD DLL to execute this ClientCommand...
		MDLL_ClientCommand(pFakeClient);
	}

	g_argv[0] = 0;               // when it's done, reset the g_argv field
	isFakeClientCommand = false; // reset the "fakeclient command" flag
	fake_arg_count = 0;          // and the argument count
}

const char *GetArg( const char *command, int arg_number )
{
	// the purpose of this function is to provide fakeclients (bots) with the same Cmd_Argv
	// convenience the engine provides to real clients. This way the handling of real client
	// commands and bot client commands is exactly the same, just have a look in engine.cpp
	// for the hooking of pfnCmd_Argc, pfnCmd_Args and pfnCmd_Argv, which redirects the call
	// either to the actual engine functions (when the caller is a real client), either on
	// our function here, which does the same thing, when the caller is a bot.

	int length, i, index = 0, arg_count = 0, fieldstart, fieldstop;

	arg[0] = 0;                  // reset arg
	length = strlen( command );    // get length of command

								   // while we have not reached end of line
	while( index < length && arg_count <= arg_number )
	{
		while( index < length && command[index] == ' ' )
			index++;               // ignore spaces

								   // is this field multi-word between quotes or single word?
		if( command[index] == '"' )
		{
			index++;               // move one step further to bypass the quote
			fieldstart = index;    // save field start position
			while( index < length && command[index] != '"' )
				index++;            // reach end of field
			fieldstop = index - 1; // save field stop position
			index++;               // move one step further to bypass the quote
		}
		else
		{
			fieldstart = index;    // save field start position
			while( index < length && command[index] != ' ' )
				index++;            // reach end of field
			fieldstop = index - 1; // save field stop position
		}

		// is this argument we just processed the wanted one?
		if( arg_count == arg_number )
		{
			for( i = fieldstart; i <= fieldstop; i++ )
				arg[i - fieldstart] = command[i];   // store the field value in a string
			arg[i - fieldstart] = 0;       // terminate the string
		}

		arg_count++;              // we have processed one argument more
	}

	return arg;                  // returns the wanted argument
}

const char *Cmd_Args( void )
{
	// is this a bot issuing that client command?
	if( isFakeClientCommand )
	{
		// is it a "say" or "say_team" client command?
		if( strncmp( "say ", g_argv, 4 ) == 0 )
		{
			if( g_bIsMMPlugin )
				RETURN_META_VALUE( MRES_SUPERCEDE, g_argv + 4 );
			return g_argv + 4;     // skip the "say" bot client command
		}
		else if( strncmp( "say_team ", g_argv, 9 ) == 0 )
		{
			if( g_bIsMMPlugin )
				RETURN_META_VALUE( MRES_SUPERCEDE, g_argv + 9 );
			return g_argv + 9;     // skip the "say_team" bot client command
		}

		if( g_bIsMMPlugin )
			RETURN_META_VALUE( MRES_SUPERCEDE, g_argv );
		return g_argv;            // else return the whole bot client command string we know
	}

	if( g_bIsMMPlugin )
		RETURN_META_VALUE( MRES_IGNORED, 0 );

	return CMD_ARGS();	// ask the client command string to the engine
}

const char *Cmd_Argv( int argc )
{
	// is this a bot issuing that client command ?
	if( isFakeClientCommand )
	{
		if( g_bIsMMPlugin )
			RETURN_META_VALUE( MRES_SUPERCEDE, GetArg( g_argv, argc ) );

		return GetArg( g_argv, argc );      // if so, then return the wanted argument we know
	}

	if( g_bIsMMPlugin )
		RETURN_META_VALUE( MRES_IGNORED, 0 );
	
	return CMD_ARGV(argc);
}

int Cmd_Argc( void )
{
	// is this a bot issuing that client command ?
	if( isFakeClientCommand )
	{
		if( g_bIsMMPlugin )
			RETURN_META_VALUE( MRES_SUPERCEDE, fake_arg_count );
		return fake_arg_count;    // if so, then return the argument count we know
	}

	if( g_bIsMMPlugin )
		RETURN_META_VALUE( MRES_IGNORED, 0 );
	
	return CMD_ARGC();
}
