
#include "server.h"

#include <mv_setup.h>

#include "../qcommon/q_shared.h"

/*
Ghoul2 Insert Start
*/

#include "../ghoul2/G2.h"


#ifdef G2_COLLISION_ENABLED
#if !defined (MINIHEAP_H_INC)
#include "../qcommon/MiniHeap.h"
#endif
#endif

#include "../qcommon/strip.h"

/*
===============
SV_SetConfigstring

===============
*/
void SV_SetConfigstring (int index, const char *val) {
	int		len, i;
	int		maxChunkSize = MAX_STRING_CHARS - 24;
	client_t	*client;

	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error (ERR_DROP, "SV_SetConfigstring: bad index %i", index);
	}

	if ( !val ) {
		val = "";
	}

	// don't bother broadcasting an update if no change
	if ( !strcmp( val, sv.configstrings[ index ] ) ) {
		return;
	}

	// change the string in sv
	Z_Free( (void *)sv.configstrings[index] );
	sv.configstrings[index] = CopyString( val );

	// send it to all the clients if we aren't
	// spawning a new server
	if ( sv.state == SS_GAME || sv.restarting ) {

		// send the data to all relevent clients
		for (i = 0, client = svs.clients; i < sv_maxclients->integer ; i++, client++) {
			if ( client->state < CS_PRIMED ) {
				continue;
			}
			// do not always send server info to all clients
			if ( index == CS_SERVERINFO && client->gentity && (client->gentity->r.svFlags & SVF_NOSERVERINFO) ) {
				continue;
			}

			len = (int)strlen( val );
			if( len >= maxChunkSize ) {
				int		sent = 0;
				int		remaining = len;
				char	*cmd;
				char	buf[MAX_STRING_CHARS];

				while (remaining > 0 ) {
					if ( sent == 0 ) {
						cmd = "bcs0";
					}
					else if( remaining < maxChunkSize ) {
						cmd = "bcs2";
					}
					else {
						cmd = "bcs1";
					}
					Q_strncpyz( buf, &val[sent], maxChunkSize );

					SV_SendServerCommand( client, "%s %i \"%s\"\n", cmd, index, buf );

					sent += (maxChunkSize - 1);
					remaining -= (maxChunkSize - 1);
				}
			} else {
				// standard cs, just send it
				SV_SendServerCommand( client, "cs %i \"%s\"\n", index, val );
			}
		}
	}
}



/*
===============
SV_GetConfigstring

===============
*/
void SV_GetConfigstring( int index, char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetConfigstring: bufferSize == %i", bufferSize );
	}
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error (ERR_DROP, "SV_GetConfigstring: bad index %i", index);
	}
	if ( !sv.configstrings[index] ) {
		buffer[0] = 0;
		return;
	}

	Q_strncpyz( buffer, sv.configstrings[index], bufferSize );
}


/*
================
SV_AddConfigstring

================
*/
int SV_AddConfigstring (const char *name, int start, int max)
{
	int		i;

	if (!name || !name[0])
	{
		return 0;
	}

	if (name[0] == '/' || name[0] == '\\')
	{
#ifdef DEBUG
		Com_DPrintf( "WARNING: Leading slash on '%s'\n", name);
#endif
		name++;

		if (!name[0])
		{
			return 0;
		}
	}

	for (i=1 ; i < max ; i++)
	{
		if (sv.configstrings[start+i][0] == 0)
		{	// Didn't find it
			SV_SetConfigstring ((start+i), name);
			break;
		}
		else if (!Q_stricmp(sv.configstrings[start+i], name))
		{
			return i;
		}
	}

	return 0;

}

/*
===============
SV_SetUserinfo

===============
*/
void SV_SetUserinfo( int index, const char *val ) {
	if ( index < 0 || index >= sv_maxclients->integer ) {
		Com_Error (ERR_DROP, "SV_SetUserinfo: bad index %i", index);
	}

	if ( !val ) {
		val = "";
	}

	Q_strncpyz( svs.clients[index].userinfo, val, sizeof( svs.clients[ index ].userinfo ) );
	Q_strncpyz( svs.clients[index].name, Info_ValueForKey( val, "name" ), sizeof(svs.clients[index].name) );
}



/*
===============
SV_GetUserinfo

===============
*/
void SV_GetUserinfo( int index, char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetUserinfo: bufferSize == %i", bufferSize );
	}
	if ( index < 0 || index >= sv_maxclients->integer ) {
		Com_Error (ERR_DROP, "SV_GetUserinfo: bad index %i", index);
	}
	Q_strncpyz( buffer, svs.clients[ index ].userinfo, bufferSize );
}


/*
================
SV_CreateBaseline

Entity baselines are used to compress non-delta messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
void SV_CreateBaseline( void ) {
	sharedEntity_t *svent;
	int				entnum;

	for ( entnum = 0; entnum < sv.num_entities ; entnum++ ) {
		svent = SV_GentityNum(entnum);
		if (!svent->r.linked) {
			continue;
		}
		svent->s.number = entnum;

		//
		// take current state as baseline
		//
		sv.svEntities[entnum].baseline = svent->s;
	}
}


/*
===============
SV_BoundMaxClients

===============
*/
void SV_BoundMaxClients( int minimum ) {
	// get the current maxclients value
	Cvar_Get( "sv_maxclients", "8", 0 );

	sv_maxclients->modified = qfalse;

	if ( sv_maxclients->integer < minimum ) {
		Cvar_Set( "sv_maxclients", va("%i", minimum) );
	} else if ( sv_maxclients->integer > MAX_CLIENTS ) {
		Cvar_Set( "sv_maxclients", va("%i", MAX_CLIENTS) );
	}
}


/*
===============
SV_Startup

Called when a host starts a map when it wasn't running
one before.  Successive map or map_restart commands will
NOT cause this to be called, unless the game is exited to
the menu system first.
===============
*/
void SV_Startup( void ) {
	if ( svs.initialized ) {
		Com_Error( ERR_FATAL, "SV_Startup: svs.initialized" );
	}
	SV_BoundMaxClients( 1 );

	svs.clients = (struct client_s *)Z_Malloc (sizeof(client_t) * sv_maxclients->integer, TAG_CLIENTS, qtrue );
	if ( com_dedicated->integer ) {
		svs.numSnapshotEntities = sv_maxclients->integer * PACKET_BACKUP * 64;
		Cvar_Set( "r_ghoul2animsmooth", "0");
		Cvar_Set( "r_ghoul2unsqashaftersmooth", "0");

	} else {
		// we don't need nearly as many when playing locally
		svs.numSnapshotEntities = sv_maxclients->integer * 4 * 64;
	}
	svs.initialized = qtrue;

	Cvar_Set( "sv_running", "1" );

}

/*
==================
SV_ChangeMaxClients
==================
*/
void SV_ChangeMaxClients( void ) {
	int		oldMaxClients;
	int		i;
	client_t	*oldClients;
	int		count;

	// get the highest client number in use
	count = 0;
	for ( i = 0 ; i < sv_maxclients->integer ; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			if (i > count)
				count = i;
		}
	}
	count++;

	oldMaxClients = sv_maxclients->integer;
	// never go below the highest client number in use
	SV_BoundMaxClients( count );
	// if still the same
	if ( sv_maxclients->integer == oldMaxClients ) {
		return;
	}

	oldClients = (struct client_s *)Hunk_AllocateTempMemory( count * sizeof(client_t) );
	// copy the clients to hunk memory
	for ( i = 0 ; i < count ; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			oldClients[i] = svs.clients[i];
		}
		else {
			Com_Memset(&oldClients[i], 0, sizeof(client_t));
		}
	}

	// free old clients arrays
	Z_Free( svs.clients );

	// allocate new clients
	svs.clients = (struct client_s *)Z_Malloc ( sv_maxclients->integer * sizeof(client_t), TAG_CLIENTS, qtrue );
	Com_Memset( svs.clients, 0, sv_maxclients->integer * sizeof(client_t) );

	// copy the clients over
	for ( i = 0 ; i < count ; i++ ) {
		if ( oldClients[i].state >= CS_CONNECTED ) {
			svs.clients[i] = oldClients[i];
		}
	}

	// free the old clients on the hunk
	Hunk_FreeTempMemory( oldClients );

	// allocate new snapshot entities
	if ( com_dedicated->integer ) {
		svs.numSnapshotEntities = sv_maxclients->integer * PACKET_BACKUP * 64;
	} else {
		// we don't need nearly as many when playing locally
		svs.numSnapshotEntities = sv_maxclients->integer * 4 * 64;
	}
}

/*
================
SV_ClearServer
================
*/
void SV_ClearServer(void) {
	int i;

	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( sv.configstrings[i] ) {
			Z_Free( (void *)sv.configstrings[i] );
		}
	}

//	CM_ClearMap();

	Com_Memset (&sv, 0, sizeof(sv));
}

/*
================
SV_TouchCGame

Touch the cgame.vm and ui.qvm so that a client can load it if it's in a seperate pk3
================
*/
void SV_TouchCGame(void) {
	fileHandle_t	f;

	FS_FOpenFileRead( "vm/cgame.qvm", &f, qfalse );
	if ( f ) {
		FS_FCloseFile( f );
	}

	FS_FOpenFileRead( "vm/ui.qvm", &f, qfalse);
	if (f) {
		FS_FCloseFile(f);
	}
}

void SV_SendMapChange(void)
{
	int		i;

	if (svs.clients)
	{
		for (i=0 ; i<sv_maxclients->integer ; i++)
		{
			if (svs.clients[i].state >= CS_CONNECTED)
			{
				if ( svs.clients[i].netchan.remoteAddress.type != NA_BOT )
				{
					SV_SendClientMapChange( &svs.clients[i] ) ;
				}
			}
		}
	}
}

/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.
This is NOT called for map_restart
================
*/
//#ifdef G2_COLLISION_ENABLED
//extern CMiniHeap *G2VertSpaceServer;
//#define G2_VERT_SPACE_SERVER_SIZE 256
//#endif

//extern void RE_RegisterMedia_LevelLoadBegin(const char *psMapName, ForceReload_e eForceReload);
void SV_SpawnServer( char *server, qboolean killBots, ForceReload_e eForceReload ) {
	int			i;
	int			checksum;
	qboolean	isBot;
	char		systemInfo[16384];
	const char	*p;
	qboolean	resetTime;

	Com_Printf("------ Server Initialization ------\n");
	Com_Printf("Server: %s\n", server);

	SV_SendMapChange();

	re->RegisterMedia_LevelLoadBegin(server, eForceReload);

	// shut down the existing game if it is running
	SV_ShutdownGameProgs();

/*
Ghoul2 Insert Start
*/
 	// de allocate the snapshot entities
	if (svs.snapshotEntities)
	{
		delete[] svs.snapshotEntities;
		svs.snapshotEntities = NULL;
	}
/*
Ghoul2 Insert End
*/

	SV_SendMapChange();

	// if not running a dedicated server CL_MapLoading will connect the client to the server
	// also print some status stuff
	CL_MapLoading();

#ifndef DEDICATED
	// make sure all the client stuff is unloaded
	CL_ShutdownAll( qfalse );
#endif

	CM_ClearMap();

	// clear the whole hunk because we're (re)loading the server
	Hunk_Clear();

/*
Ghoul2 Insert Start
*/
	// clear out those shaders, images and Models as long as this
	// isnt a dedicated server.
	if ( !com_dedicated->integer )
	{
#ifndef DEDICATED
		// ????????????????????
		/*R_InitImages();

		R_InitShaders();

		R_ModelInit();*/
#endif
	}
	else
	{
		re->SVModelInit();

#ifdef G2_COLLISION_ENABLED
		//if (!G2VertSpaceServer)
		//{
		//	G2VertSpaceServer = new CMiniHeap(G2_VERT_SPACE_SERVER_SIZE * 1024);
		//}
#endif
	}

	SV_SendMapChange();

	// init client structures and svs.numSnapshotEntities
	if ( !Cvar_VariableValue("sv_running") ) {
		SV_Startup();
	} else {
		// check for maxclients change
		if ( sv_maxclients->modified ) {
			SV_ChangeMaxClients();
		}
	}

	SV_SendMapChange();

	// clear pak references
	FS_ClearPakReferences(0);

/*
Ghoul2 Insert Start
*/
	// allocate the snapshot entities on the hunk
//	svs.snapshotEntities = (struct entityState_s *)Hunk_Alloc( sizeof(entityState_t)*svs.numSnapshotEntities, h_high );
	svs.nextSnapshotEntities = 0;

	// allocate the snapshot entities
	svs.snapshotEntities = new entityState_s[svs.numSnapshotEntities];
	// we CAN afford to do this here, since we know the STL vectors in Ghoul2 are empty
	memset(svs.snapshotEntities, 0, sizeof(entityState_t)*svs.numSnapshotEntities);

/*
Ghoul2 Insert End
*/

	// toggle the server bit so clients can detect that a
	// server has changed
	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	// set nextmap to the same map, but it may be overriden
	// by the game startup or another console command
	Cvar_Set( "nextmap", "map_restart 0");
//	Cvar_Set( "nextmap", va("map %s", server) );

	for (i = 0; i < sv_maxclients->integer; i++) {
		// save when the server started for each client already connected
		if (svs.clients[i].state >= CS_CONNECTED) {
			svs.clients[i].oldServerTime = sv.time;
		}
	}

	// close all filehandles before FS_Restart
	for (i = 0; i < sv_maxclients->integer; i++) {
		SV_CloseDownload( &svs.clients[i] );
	}

	// check sv.resetServerTime before clearing sv struct
	if (sv.resetServerTime) {
		resetTime = (qboolean)(sv.resetServerTime == 1);
	} else if (mv_resetServerTime->integer == 1) {
		resetTime = (qboolean)(sv_gametype->integer != GT_TOURNAMENT);
	} else {
		resetTime = (qboolean)(mv_resetServerTime->integer == 2);
	}

	// wipe the entire per-level structure
	SV_ClearServer();
	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		sv.configstrings[i] = CopyString("");
	}

	if (!resetTime) {
		// Keep old game module time as original engine did
		sv.time = svs.time;
	}

	// decide which serverversion to host
	mv_serverversion = Cvar_Get("mv_serverversion", "1.04", CVAR_ARCHIVE | CVAR_LATCH | CVAR_GLOBAL);
	if (FS_AllPath_Base_FileExists("assets5.pk3") && (!strcmp(mv_serverversion->string, "auto") || !strcmp(mv_serverversion->string, "1.04"))) {
		Com_Printf("serverversion set to 1.04\n");
		MV_SetCurrentGameversion(VERSION_1_04);
	}
	else if (FS_AllPath_Base_FileExists("assets2.pk3") && (!strcmp(mv_serverversion->string, "auto") || !strcmp(mv_serverversion->string, "1.03"))) {
		Com_Printf("serverversion set to 1.03\n");
		MV_SetCurrentGameversion(VERSION_1_03);
	} else {
		Com_Printf("serverversion set to 1.02\n");
		MV_SetCurrentGameversion(VERSION_1_02);
	}

	Cvar_Set("protocol", va("%i", MV_GetCurrentProtocol()));

	// make sure we are not paused
	Cvar_Set("cl_paused", "0");

	// get a new checksum feed and restart the file system
	srand(Com_Milliseconds());
	sv.checksumFeed = ( ((int) rand() << 16) ^ rand() ) ^ Com_Milliseconds();

	FS_PureServerSetReferencedPaks("", "");
	FS_Restart( sv.checksumFeed );

	CM_LoadMap( va("maps/%s.bsp", server), qfalse, &checksum );

	SV_SendMapChange();

	// set serverinfo visible name
	Cvar_Set( "mapname", server );

	Cvar_Set( "sv_mapChecksum", va("%i",checksum) );

	// serverid should be different each time
	sv.serverId = com_frameTime;
	sv.restartedServerId = sv.serverId;
	Cvar_Set( "sv_serverid", va("%i", sv.serverId ) );

	// media configstring setting should be done during
	// the loading stage, so connected clients don't have
	// to load during actual gameplay
	sv.state = SS_LOADING;

	// load and spawn all other entities
	SV_InitGameProgs();

	// If the game module didn't announce it can handle it we want to abort now
	if ( CM_NumInlineModels() > MAX_SUBMODELS && !sv.submodelBypass ) {
		Com_Error( ERR_DROP, "MAX_SUBMODELS exceeded (game module doesn't support submodel bypass)" );
	}

	// don't allow a map_restart if game is modified
	sv_gametype->modified = qfalse;

	// run a few frames to allow everything to settle
	for ( i = 0 ;i < 3 ; i++ ) {
		VM_Call( gvm, GAME_RUN_FRAME, sv.time );
		SV_BotFrame( sv.time );
		sv.time += 100;
		svs.time += 100;
	}

	// create a baseline for more efficient communications
	SV_CreateBaseline ();

	for (i=0 ; i<sv_maxclients->integer ; i++) {
		// send the new gamestate to all connected clients
		if (svs.clients[i].state >= CS_CONNECTED) {
			const char	*denied;

			if ( svs.clients[i].netchan.remoteAddress.type == NA_BOT ) {
				if ( killBots ) {
					SV_DropClient( &svs.clients[i], "" );
					continue;
				}
				isBot = qtrue;
			}
			else {
				isBot = qfalse;
			}

			// connect the client again
			denied = (const char *)VM_ExplicitArgString( gvm, VM_Call( gvm, GAME_CLIENT_CONNECT, i, qfalse, isBot ) );	// firstTime = qfalse
			if ( denied ) {
				// this generally shouldn't happen, because the client
				// was connected before the level change
				SV_DropClient( &svs.clients[i], denied );
			} else {
				if( !isBot ) {
					// when we get the next packet from a connected client,
					// the new gamestate will be sent
					svs.clients[i].state = CS_CONNECTED;
				}
				else {
					client_t		*client;
					sharedEntity_t	*ent;

					client = &svs.clients[i];
					client->state = CS_ACTIVE;
					ent = SV_GentityNum( i );
					ent->s.number = i;
					client->gentity = ent;

					client->deltaMessage = -1;
					client->nextSnapshotTime = svs.time;	// generate a snapshot immediately

					VM_Call( gvm, GAME_CLIENT_BEGIN, i );
				}
			}
		}
	}

	// run another frame to allow things to look at all the players
	VM_Call( gvm, GAME_RUN_FRAME, sv.time );
	SV_BotFrame( sv.time );
	sv.time += 100;
	svs.time += 100;

	svs.hibernation.disableUntil = svs.time + 10000;

	// if a dedicated server we need to touch the cgame and ui because it could be in a
	// seperate pk3 file and the client will need to load the latest cgame.qvm
	if (com_dedicated->integer) {
		SV_TouchCGame();
	}

	if ( sv_pure->integer ) {
		// the server sends these to the clients so they will only
		// load pk3s also loaded at the server
		p = FS_LoadedPakChecksums();
		Cvar_Set( "sv_paks", p );
		if (strlen(p) == 0) {
			Com_Printf( "WARNING: sv_pure set but no PK3 files loaded\n" );
		}
		p = FS_LoadedPakNames();
		Cvar_Set( "sv_pakNames", p );
	}
	else {
		Cvar_Set( "sv_paks", "" );
		Cvar_Set( "sv_pakNames", "" );
	}
	// the server sends these to the clients so they can figure
	// out which pk3s should be auto-downloaded
	p = FS_ReferencedPakChecksums();
	Cvar_Set( "sv_referencedPaks", p );
	p = FS_ReferencedPakNames();
	Cvar_Set( "sv_referencedPakNames", p );

	// save systeminfo and serverinfo strings
	Q_strncpyz( systemInfo, Cvar_InfoString_Big( CVAR_SYSTEMINFO ), sizeof( systemInfo ) );
	cvar_modifiedFlags &= ~CVAR_SYSTEMINFO;
	SV_SetConfigstring( CS_SYSTEMINFO, systemInfo );

	SV_SetConfigstring( CS_SERVERINFO, Cvar_InfoString( CVAR_SERVERINFO ) );
	cvar_modifiedFlags &= ~CVAR_SERVERINFO;

	// any media configstring setting now should issue a warning
	// and any configstring changes should be reliably transmitted
	// to all clients
	sv.state = SS_GAME;

	// send a heartbeat now so the master will get up to date info
	SV_Heartbeat_f();

	Hunk_SetMark();

	/* MrE: 2000-09-13: now called in CL_DownloadsComplete
	// don't call when running dedicated
	if ( !com_dedicated->integer ) {
		// note that this is called after setting the hunk mark with Hunk_SetMark
		CL_StartHunkUsers();
	}
	*/

	if (!mv_httpdownloads || mv_httpdownloads->modified || mv_httpserverport->modified) {
		NET_HTTP_StopServer();
	}

	// here because latched
	mv_httpdownloads = Cvar_Get("mv_httpdownloads", "0", CVAR_SERVERINFO | CVAR_ARCHIVE | CVAR_LATCH);
	mv_httpserverport = Cvar_Get("mv_httpserverport", "0", CVAR_ARCHIVE | CVAR_LATCH);

	if (mv_httpdownloads->integer) {
		if (!Q_stricmpn(mv_httpserverport->string, "http://", strlen("http://"))) {
			Com_Printf("HTTP Downloads: redirecting to %s\n", mv_httpserverport->string);
		} else {
			sv.http_port = NET_HTTP_StartServer(mv_httpserverport->integer);
			// allow connected clients to use HTTP server
			for (i = 0; i < sv_maxclients->integer; i++) {
				if (svs.clients[i].state >= CS_CONNECTED) {
					NET_HTTP_AllowClient(i, svs.clients[i].netchan.remoteAddress);
				}
			}
		}
	}

	SVC_LoadWhitelist();

	Com_Printf ("-----------------------------------\n");
}

#ifdef DEDICATED

#define G2_VERT_SPACE_SERVER_SIZE 256
CMiniHeap *G2VertSpaceServer = NULL;
CMiniHeap CMiniHeap_singleton(G2_VERT_SPACE_SERVER_SIZE * 1024);


/*
================
CL_RefPrintf

DLL glue
================
*/
void QDECL SV_RefPrintf( int print_level, const char *fmt, ...) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	if ( print_level == PRINT_ALL ) {
		Com_Printf ("%s", msg);
	} else if ( print_level == PRINT_WARNING ) {
		Com_Printf (S_COLOR_YELLOW "%s", msg);		// yellow
	} else if ( print_level == PRINT_DEVELOPER ) {
		Com_DPrintf (S_COLOR_RED "%s", msg);		// red
	}
}

qboolean Com_TheHunkMarkHasBeenMade(void);

//qcommon/vm.cpp
extern vm_t *currentVM;

//qcommon/cm_load.cpp
extern void *gpvCachedMapDiskImage;
extern qboolean gbUsingCachedMapDataRightNow;

//static char *GetSharedMemory( void ) { return sv.mSharedMemory; }
static vm_t *GetCurrentVM( void ) { return currentVM; }
static void *CM_GetCachedMapDiskImage( void ) { return gpvCachedMapDiskImage; }
static void CM_SetCachedMapDiskImage( void *ptr ) { gpvCachedMapDiskImage = ptr; }
static void CM_SetUsingCache( qboolean usingCache ) { gbUsingCachedMapDataRightNow = usingCache; }

//server stuff D:
extern void SV_GetConfigstring( int index, char *buffer, int bufferSize );
extern void SV_SetConfigstring( int index, const char *val );

static CMiniHeap *GetG2VertSpaceServer( void ) {
	return G2VertSpaceServer;
}

refexport_t	*re = NULL;

static void SV_InitRef( void ) {
	static refimport_t ri;
	refexport_t *ret;

	Com_Printf( "----- Initializing Renderer ----\n" );

	memset( &ri, 0, sizeof( ri ) );

	//set up the import table
	ri.Printf = SV_RefPrintf;
	ri.Error = Com_Error;
	ri.OPrintf = Com_OPrintf;
	ri.Milliseconds = Sys_Milliseconds2; //FIXME: unix+mac need this
	ri.Hunk_AllocateTempMemory = Hunk_AllocateTempMemory;
	ri.Hunk_FreeTempMemory = Hunk_FreeTempMemory;
//	ri.Hunk_Alloc = Hunk_Alloc;
	ri.Hunk_MemoryRemaining = Hunk_MemoryRemaining;
	ri.Z_Malloc = Z_Malloc;
	ri.Z_Free = Z_Free;
	ri.Z_MemSize = Z_MemSize;
	ri.Z_MorphMallocTag = Z_MorphMallocTag;
//	ri.Cmd_ExecuteString = Cmd_ExecuteString;
	ri.Cmd_Argc = Cmd_Argc;
	ri.Cmd_Argv = Cmd_Argv;
	ri.Cmd_ArgsBuffer = Cmd_ArgsBuffer;
	ri.Cmd_AddCommand = Cmd_AddCommand;
	ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
	ri.Cvar_Set = Cvar_Set;
	ri.Cvar_Get = Cvar_Get;
	ri.Cvar_VariableStringBuffer = Cvar_VariableStringBuffer;
	ri.Cvar_VariableString = Cvar_VariableString;
	ri.Cvar_VariableValue = Cvar_VariableValue;
	ri.Cvar_VariableIntegerValue = Cvar_VariableIntegerValue;
//	ri.Sys_LowPhysicalMemory = Sys_LowPhysicalMemory;
//	ri.SE_GetString = SE_GetString;
	ri.FS_FreeFile = FS_FreeFile;
	ri.FS_FreeFileList = FS_FreeFileList;
//	ri.FS_Read = FS_Read;
	ri.FS_ReadFile = FS_ReadFile;
	ri.FS_FCloseFile = FS_FCloseFile_RI;
	ri.FS_FOpenFileRead = FS_FOpenFileRead_RI;
	ri.FS_FOpenFileWrite = FS_FOpenFileWrite_RI;
//	ri.FS_FOpenFileByMode = FS_FOpenFileByMode;
	ri.FS_FileExists = FS_FileExists;
	ri.FS_FileIsInPAK = FS_FileIsInPAK;
	ri.FS_ListFiles = FS_ListFiles;
//	ri.FS_Write = FS_Write;
	ri.FS_WriteFile = FS_WriteFile;
	ri.CM_BoxTrace = CM_BoxTrace;
	ri.CM_DrawDebugSurface = CM_DrawDebugSurface;
//	ri.CM_CullWorldBox = CM_CullWorldBox;
//	ri.CM_TerrainPatchIterate = CM_TerrainPatchIterate;
//	ri.CM_RegisterTerrain = CM_RegisterTerrain;
//	ri.CM_ShutdownTerrain = CM_ShutdownTerrain;
	ri.CM_ClusterPVS = CM_ClusterPVS;
	ri.CM_LeafArea = CM_LeafArea;
	ri.CM_LeafCluster = CM_LeafCluster;
	ri.CM_PointLeafnum = CM_PointLeafnum;
	ri.CM_PointContents = CM_PointContents;
//	ri.Com_TheHunkMarkHasBeenMade = Com_TheHunkMarkHasBeenMade;
//	ri.S_RestartMusic = S_RestartMusic;
//	ri.SND_RegisterAudio_LevelLoadEnd = SND_RegisterAudio_LevelLoadEnd;
//	ri.CIN_RunCinematic = CIN_RunCinematic;
//	ri.CIN_PlayCinematic = CIN_PlayCinematic;
//	ri.CIN_UploadCinematic = CIN_UploadCinematic;
//	ri.CL_WriteAVIVideoFrame = CL_WriteAVIVideoFrame;

	// g2 data access
//	ri.GetSharedMemory = GetSharedMemory;

	// (c)g vm callbacks
//	ri.GetCurrentVM = GetCurrentVM;
//	ri.CGVMLoaded = CGVMLoaded;
//	ri.CGVM_RagCallback = CGVM_RagCallback;

	// ugly win32 backend
	ri.CM_GetCachedMapDiskImage = CM_GetCachedMapDiskImage;
	ri.CM_SetCachedMapDiskImage = CM_SetCachedMapDiskImage;
	ri.CM_SetUsingCache = CM_SetUsingCache;

	//FIXME: Might have to do something about this...
	ri.GetG2VertSpaceServer = GetG2VertSpaceServer;
	G2VertSpaceServer = &CMiniHeap_singleton;

	ret = GetRefAPI( REF_API_VERSION, &ri );

//	Com_Printf( "-------------------------------\n");

	if ( !ret ) {
		Com_Error (ERR_FATAL, "Couldn't initialize refresh" );
	}

	re = ret;
}
#endif

/*
===============
SV_Init

Only called at main exe startup, not for each game
===============
*/
void SV_BotInitBotLib(void);

void SV_Init (void) {
	SV_AddOperatorCommands ();

	Cvar_Get("JK2MV", JK2MV_VERSION, CVAR_SERVERINFO | CVAR_ROM);

	mv_serverversion = Cvar_Get("mv_serverversion", "1.04", CVAR_ARCHIVE | CVAR_LATCH);

	mv_fixnamecrash = Cvar_Get("mv_fixnamecrash", "1", CVAR_ARCHIVE);
	mv_fixforcecrash = Cvar_Get("mv_fixforcecrash", "1", CVAR_ARCHIVE);
	mv_fixgalaking = Cvar_Get("mv_fixgalaking", "1", CVAR_ARCHIVE);
	mv_fixbrokenmodels = Cvar_Get("mv_fixbrokenmodels", "1", CVAR_ARCHIVE);
	mv_fixturretcrash = Cvar_Get("mv_fixturretcrash", "1", CVAR_ARCHIVE);
	mv_blockchargejump = Cvar_Get("mv_blockchargejump", "1", CVAR_ARCHIVE);
	mv_blockspeedhack = Cvar_Get("mv_blockspeedhack", "1", CVAR_ARCHIVE);
	mv_fixsaberstealing = Cvar_Get("mv_fixsaberstealing", "1", CVAR_ARCHIVE);
	mv_fixplayerghosting = Cvar_Get("mv_fixplayerghosting", "1", CVAR_ARCHIVE);

	mv_resetServerTime = Cvar_Get("mv_resetServerTime", "1", CVAR_ARCHIVE);

	// serverinfo vars
	Cvar_Get ("dmflags", "0", CVAR_SERVERINFO);
	Cvar_Get ("fraglimit", "20", CVAR_SERVERINFO);
	Cvar_Get ("timelimit", "0", CVAR_SERVERINFO);

	// Get these to establish them and to make sure they have a default before the menus decide to stomp them.
	Cvar_Get ("g_maxHolocronCarry", "3", CVAR_SERVERINFO);
	Cvar_Get ("g_privateDuel", "1", CVAR_SERVERINFO );
	Cvar_Get ("g_saberLocking", "1", CVAR_SERVERINFO );
	Cvar_Get ("g_maxForceRank", "6", CVAR_SERVERINFO );
	Cvar_Get ("duel_fraglimit", "10", CVAR_SERVERINFO);
	Cvar_Get ("g_forceBasedTeams", "0", CVAR_SERVERINFO);
	Cvar_Get ("g_duelWeaponDisable", "1", CVAR_SERVERINFO);

	sv_gametype = Cvar_Get ("g_gametype", "0", CVAR_SERVERINFO | CVAR_LATCH );
	sv_needpass = Cvar_Get ("g_needpass", "0", CVAR_SERVERINFO | CVAR_ROM );
	Cvar_Get ("sv_keywords", "", CVAR_SERVERINFO);
	sv_mapname = Cvar_Get ("mapname", "nomap", CVAR_SERVERINFO | CVAR_ROM);
	sv_privateClients = Cvar_Get ("sv_privateClients", "0", CVAR_SERVERINFO);
	sv_hostname = Cvar_Get ("sv_hostname", "noname", CVAR_SERVERINFO | CVAR_ARCHIVE );
	sv_maxclients = Cvar_Get ("sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH);
	sv_minSnaps = Cvar_Get("sv_minSnaps", "1", CVAR_ARCHIVE);                        // jk2ded hardcoded min: 1
	sv_maxSnaps = Cvar_Get("sv_maxSnaps", "30", CVAR_ARCHIVE);                       // jk2ded hardcoded max: 30
	sv_enforceSnaps = Cvar_Get("sv_enforceSnaps", "0", CVAR_ARCHIVE);                // 0: users choice (limited by min/max snaps); 1: sv_fps (limited by min/max snaps)	
	sv_minRate = Cvar_Get("sv_minRate", "1000", CVAR_ARCHIVE | CVAR_SERVERINFO );    // jk2ded hardcoded min: 1000
	sv_maxRate = Cvar_Get ("sv_maxRate", "90000", CVAR_ARCHIVE | CVAR_SERVERINFO );  // jk2ded hardcoded max: 90000
	sv_maxOOBRate = Cvar_Get ("sv_maxOOBRate", "20", CVAR_ARCHIVE | CVAR_GLOBAL );
	sv_minPing = Cvar_Get ("sv_minPing", "0", CVAR_ARCHIVE | CVAR_SERVERINFO );
	sv_maxPing = Cvar_Get ("sv_maxPing", "0", CVAR_ARCHIVE | CVAR_SERVERINFO );
	sv_floodProtect = Cvar_Get ("sv_floodProtect", "3", CVAR_ARCHIVE | CVAR_SERVERINFO );
	sv_allowAnonymous = Cvar_Get ("sv_allowAnonymous", "0", CVAR_SERVERINFO);

	// systeminfo
	Cvar_Get ("sv_cheats", "0", CVAR_SYSTEMINFO | CVAR_ROM );
	sv_serverid = Cvar_Get ("sv_serverid", "0", CVAR_SYSTEMINFO | CVAR_ROM );
#ifndef DLL_ONLY // bk010216 - for DLL-only servers
	sv_pure = Cvar_Get ("sv_pure", "0", CVAR_SYSTEMINFO );
#else
	sv_pure = Cvar_Get ("sv_pure", "0", CVAR_SYSTEMINFO | CVAR_INIT | CVAR_ROM );
#endif
	Cvar_Get ("sv_paks", "", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get ("sv_pakNames", "", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get ("sv_referencedPaks", "", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get ("sv_referencedPakNames", "", CVAR_SYSTEMINFO | CVAR_ROM );

	// The mv_cs_remaps systeminfo cvar can be used by game module mods to
	// announce the index of the configstring used for mvremaps. Registering the
	// cvar here is not actually required. We just register it here to ensure
	// the right flags are set on it.
	Cvar_Get ("mv_cs_remaps", "", CVAR_SYSTEMINFO | CVAR_ROM | CVAR_INTERNAL );

	// server vars
	sv_rconPassword = Cvar_Get ("rconPassword", "", CVAR_TEMP );
	sv_privatePassword = Cvar_Get ("sv_privatePassword", "", CVAR_TEMP );
	sv_fps = Cvar_Get ("sv_fps", "20", CVAR_TEMP );
	sv_timeout = Cvar_Get ("sv_timeout", "200", CVAR_TEMP );
	sv_zombietime = Cvar_Get ("sv_zombietime", "2", CVAR_TEMP );
	Cvar_Get ("nextmap", "", CVAR_TEMP );

	sv_allowDownload = Cvar_Get ("sv_allowDownload", "0", CVAR_SERVERINFO);
	sv_master[0] = Cvar_Get("sv_master1", "masterjk2.ravensoft.com", CVAR_ROM);
	Cvar_Set("sv_master1", "masterjk2.ravensoft.com");
	sv_master[1] = Cvar_Get("sv_master2", "master.jk2mv.org", CVAR_ROM); // multimaster
	Cvar_Set("sv_master2", "master.jk2mv.org");
	sv_master[2] = Cvar_Get("sv_master3", "master.jkhub.org", CVAR_ROM);
	Cvar_Set("sv_master3", "master.jkhub.org");
	sv_master[3] = Cvar_Get("sv_master4", "", CVAR_ARCHIVE | CVAR_GLOBAL);
	sv_master[4] = Cvar_Get("sv_master5", "", CVAR_ARCHIVE | CVAR_GLOBAL);
	sv_master[5] = Cvar_Get("sv_master6", "", CVAR_ARCHIVE | CVAR_GLOBAL);
	sv_master[6] = Cvar_Get("sv_master7", "", CVAR_ARCHIVE | CVAR_GLOBAL);
	sv_master[7] = Cvar_Get("sv_master8", "", CVAR_ARCHIVE | CVAR_GLOBAL);
	sv_reconnectlimit = Cvar_Get ("sv_reconnectlimit", "3", 0);
	sv_showloss = Cvar_Get ("sv_showloss", "0", 0);
	sv_padPackets = Cvar_Get ("sv_padPackets", "0", 0);
	sv_killserver = Cvar_Get ("sv_killserver", "0", 0);
	sv_mapChecksum = Cvar_Get ("sv_mapChecksum", "", CVAR_ROM);

//	sv_debugserver = Cvar_Get ("sv_debugserver", "0", 0);

	sv_hibernateFps = Cvar_Get("sv_hibernateFps", "4", CVAR_ARCHIVE | CVAR_GLOBAL);
	mv_apiConnectionless = Cvar_Get("mv_apiConnectionless", "1", CVAR_ARCHIVE | CVAR_INIT | CVAR_VM_NOWRITE);
	sv_pingFix = Cvar_Get("sv_pingFix", "1", CVAR_ARCHIVE);
	sv_autoWhitelist = Cvar_Get("sv_autoWhitelist", "1", CVAR_ARCHIVE | CVAR_GLOBAL);
	sv_dynamicSnapshots = Cvar_Get("sv_dynamicSnapshots", "1", CVAR_ARCHIVE);

	SP_Register("str_server",SP_REGISTER_REQUIRED);

	// initialize bot cvars so they are listed and can be set before loading the botlib
	SV_BotInitCvars();

	// init the botlib here because we need the pre-compiler in the UI
	SV_BotInitBotLib();

	SVC_LoadWhitelist();

	// Only allocated once, no point in moving it around and fragmenting
	// create a heap for Ghoul2 to use for game side model vertex transforms used in collision detection
#ifdef DEDICATED
	SV_InitRef();
#endif
}


/*
==================
SV_FinalMessage

Used by SV_Shutdown to send a final message to all
connected clients before the server goes down.  The messages are sent immediately,
not just stuck on the outgoing message list, because the server is going
to totally exit after returning from this function.
==================
*/
void SV_FinalMessage( char *message ) {
	int			i, j;
	client_t	*cl;

	// send it twice, ignoring rate
	for ( j = 0 ; j < 2 ; j++ ) {
		for (i=0, cl = svs.clients ; i < sv_maxclients->integer ; i++, cl++) {
			if (cl->state >= CS_CONNECTED) {
				// don't send a disconnect to a local client
				if ( cl->netchan.remoteAddress.type != NA_LOOPBACK ) {
					SV_SendServerCommand( cl, "print \"%s\n\"", message );
					SV_SendServerCommand( cl, "disconnect" );
				}
				// force a snapshot to be sent
				cl->nextSnapshotTime = -1;
				SV_SendClientSnapshot( cl );
			}
		}
	}
}


/*
================
SV_Shutdown

Called when each game quits,
before Sys_Quit or Sys_Error
================
*/
void SV_Shutdown( char *finalmsg )
{
	if ( !com_sv_running || !com_sv_running->integer )
	{
		return;
	}

	Com_Printf( "----- Server Shutdown (%s) -----\n", finalmsg );

	if ( svs.clients && !com_errorEntered ) {
		SV_FinalMessage( finalmsg );
	}

	SV_RemoveOperatorCommands();
	SV_MasterShutdown();
	SV_ShutdownGameProgs();
/*
Ghoul2 Insert Start
*/
 	// de allocate the snapshot entities
	if (svs.snapshotEntities)
	{
		delete[] svs.snapshotEntities;
		svs.snapshotEntities = NULL;
	}

#ifdef G2_COLLISION_ENABLED
	if ( com_dedicated->integer && G2VertSpaceServer)
	{
		delete G2VertSpaceServer;
		G2VertSpaceServer = 0;
	}
#endif

	// free current level
	SV_ClearServer();

	// free server static data
	if ( svs.clients ) {
		Z_Free( svs.clients );
	}
	Com_Memset( &svs, 0, sizeof( svs ) );

	Cvar_Set( "sv_running", "0" );
	Cvar_Set("ui_singlePlayerActive", "0");

	Com_Printf( "---------------------------\n" );

	// disconnect any local clients
	CL_Disconnect( qfalse );

	NET_HTTP_StopServer();
	sv.http_port = 0;
}

