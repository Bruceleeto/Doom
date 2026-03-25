/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "sys/platform.h"
#include "idlib/hashing/CRC32.h"
#include "idlib/LangDict.h"
#include "framework/async/AsyncNetwork.h"
#include "framework/Console.h"
#include "framework/Game.h"
#include "framework/EventLoop.h"
#include "renderer/ModelManager.h"

#include "framework/Session_local.h"

#if defined(__AROS__)
#define CDKEY_FILEPATH CDKEY_FILE
#define XPKEY_FILEPATH XPKEY_FILE
#else
#define CDKEY_FILEPATH "../" BASE_GAMEDIR "/" CDKEY_FILE
#define XPKEY_FILEPATH "../" BASE_GAMEDIR "/" XPKEY_FILE
#endif

idCVar	idSessionLocal::com_showAngles( "com_showAngles", "0", CVAR_SYSTEM | CVAR_BOOL, "" );
idCVar	idSessionLocal::com_minTics( "com_minTics", "1", CVAR_SYSTEM, "" );
idCVar	idSessionLocal::com_showTics( "com_showTics", "0", CVAR_SYSTEM | CVAR_BOOL, "" );
idCVar	idSessionLocal::com_fixedTic( "com_fixedTic", "0", CVAR_SYSTEM | CVAR_INTEGER | CVAR_ARCHIVE, "", -1, 10 );
idCVar	idSessionLocal::com_skipGameDraw( "com_skipGameDraw", "0", CVAR_SYSTEM | CVAR_BOOL, "" );
idCVar	idSessionLocal::com_aviDemoSamples( "com_aviDemoSamples", "16", CVAR_SYSTEM, "" );
idCVar	idSessionLocal::com_aviDemoWidth( "com_aviDemoWidth", "256", CVAR_SYSTEM, "" );
idCVar	idSessionLocal::com_aviDemoHeight( "com_aviDemoHeight", "256", CVAR_SYSTEM, "" );
idCVar	idSessionLocal::com_aviDemoTics( "com_aviDemoTics", "2", CVAR_SYSTEM | CVAR_INTEGER, "", 1, 60 );
idCVar	idSessionLocal::com_wipeSeconds( "com_wipeSeconds", "1", CVAR_SYSTEM, "" );
idCVar	idSessionLocal::com_guid( "com_guid", "", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_ROM, "" );

idCVar	idSessionLocal::com_numQuicksaves( "com_numQuicksaves", "4", CVAR_SYSTEM|CVAR_ARCHIVE|CVAR_INTEGER,
                                           "number of quicksaves to keep before overwriting the oldest", 1, 99 );
idCVar	idSessionLocal::com_disableAutoSaves( "com_disableAutoSaves", "0", CVAR_SYSTEM|CVAR_ARCHIVE|CVAR_BOOL,
                                              "Don't create Autosaves when entering a new map" );

idSessionLocal		sessLocal;
idSession			*session = &sessLocal;

// these must be kept up to date with window Levelshot in guis/mainmenu.gui
const int PREVIEW_X = 211;
const int PREVIEW_Y = 31;
const int PREVIEW_WIDTH = 398;
const int PREVIEW_HEIGHT = 298;

void RandomizeStack( void ) {
	// attempt to force uninitialized stack memory bugs
	int		bytes = 4000000;
	byte	*buf = (byte *)_alloca( bytes );

	int	fill = rand()&255;
	for ( int i = 0 ; i < bytes ; i++ ) {
		buf[i] = fill;
	}
}

/*
=================
Session_RescanSI_f
=================
*/
void Session_RescanSI_f( const idCmdArgs &args ) {
	sessLocal.mapSpawnData.serverInfo = *cvarSystem->MoveCVarsToDict( CVAR_SERVERINFO );
#ifndef NONET
	if ( game && idAsyncNetwork::server.IsActive() ) {
		game->SetServerInfo( sessLocal.mapSpawnData.serverInfo );
	}
#endif
}

#ifndef	ID_DEDICATED
/*
==================
Session_Map_f

Restart the server on a different map
==================
*/
static void Session_Map_f( const idCmdArgs &args ) {
	idStr		map, string;
	findFile_t	ff;
	idCmdArgs	rl_args;

	map = args.Argv(1);
	if ( !map.Length() ) {
		// DG: if the map command is called without any arguments, print the current map
		// TODO: could check whether we're currently in a game, otherwise the last loaded
		//       map is printed.. but OTOH, who cares
		const char* curmap = sessLocal.mapSpawnData.serverInfo.GetString( "si_map" );
		if ( curmap[0] != '\0' ) {
			common->Printf( "Current Map: %s\n", curmap );
		}
		return;
	}
	map.StripFileExtension();

	// make sure the level exists before trying to change, so that
	// a typo at the server console won't end the game
	// handle addon packs through reloadEngine
	sprintf( string, "maps/%s.map", map.c_str() );
	ff = fileSystem->FindFile( string, true );
	switch ( ff ) {
	case FIND_NO:
		common->Printf( "Can't find map %s\n", string.c_str() );
		return;
	case FIND_ADDON:
		common->Printf( "map %s is in an addon pak - reloading\n", string.c_str() );
		rl_args.AppendArg( "map" );
		rl_args.AppendArg( map );
		cmdSystem->SetupReloadEngine( rl_args );
		return;
	default:
		break;
	}

	cvarSystem->SetCVarBool( "developer", false );
	sessLocal.StartNewGame( map, true );
}

/*
==================
Session_DevMap_f

Restart the server on a different map in developer mode
==================
*/
static void Session_DevMap_f( const idCmdArgs &args ) {
	idStr map, string;
	findFile_t	ff;
	idCmdArgs	rl_args;

	map = args.Argv(1);
	if ( !map.Length() ) {
		return;
	}
	map.StripFileExtension();

	// make sure the level exists before trying to change, so that
	// a typo at the server console won't end the game
	// handle addon packs through reloadEngine
	sprintf( string, "maps/%s.map", map.c_str() );
	ff = fileSystem->FindFile( string, true );
	switch ( ff ) {
	case FIND_NO:
		common->Printf( "Can't find map %s\n", string.c_str() );
		return;
	case FIND_ADDON:
		common->Printf( "map %s is in an addon pak - reloading\n", string.c_str() );
		rl_args.AppendArg( "devmap" );
		rl_args.AppendArg( map );
		cmdSystem->SetupReloadEngine( rl_args );
		return;
	default:
		break;
	}

	cvarSystem->SetCVarBool( "developer", true );
	sessLocal.StartNewGame( map, true );
}

/*
==================
Session_TestMap_f
==================
*/
static void Session_TestMap_f( const idCmdArgs &args ) {
	idStr map, string;

	map = args.Argv(1);
	if ( !map.Length() ) {
		return;
	}
	map.StripFileExtension();

	cmdSystem->BufferCommandText( CMD_EXEC_NOW, "disconnect" );

	sprintf( string, "dmap maps/%s.map", map.c_str() );
	cmdSystem->BufferCommandText( CMD_EXEC_NOW, string );

	sprintf( string, "devmap %s", map.c_str() );
	cmdSystem->BufferCommandText( CMD_EXEC_NOW, string );
}
#endif

/*
==================
Sess_WritePrecache_f
==================
*/
static void Sess_WritePrecache_f( const idCmdArgs &args ) {
	if ( args.Argc() != 2 ) {
		common->Printf( "USAGE: writePrecache <execFile>\n" );
		return;
	}
	idStr	str = args.Argv(1);
	str.DefaultFileExtension( ".cfg" );
	idFile *f = fileSystem->OpenFileWrite( str, "fs_configpath" );
	declManager->WritePrecacheCommands( f );
	renderModelManager->WritePrecacheCommands( f );
	uiManager->WritePrecacheCommands( f );

	fileSystem->CloseFile( f );
}

/*
===============
idSessionLocal::MaybeWaitOnCDKey
===============
*/
bool idSessionLocal::MaybeWaitOnCDKey( void ) {
	if ( authEmitTimeout > 0 ) {
		authWaitBox = true;
		sessLocal.MessageBox( MSG_WAIT, common->GetLanguageDict()->GetString( "#str_07191" ), NULL, true, NULL, NULL, true );
		return true;
	}
	return false;
}

/*
===================
Session_PromptKey_f
===================
*/
static void Session_PromptKey_f( const idCmdArgs &args ) {
	const char	*retkey;
	bool		valid[ 2 ];
	static bool recursed = false;

	if ( recursed ) {
		common->Warning( "promptKey recursed - aborted" );
		return;
	}
	recursed = true;

	do {
		// in case we're already waiting for an auth to come back to us ( may happen exceptionally )
		if ( sessLocal.MaybeWaitOnCDKey() ) {
			if ( sessLocal.CDKeysAreValid( true ) ) {
				recursed = false;
				return;
			}
		}
		// the auth server may have replied and set an error message, otherwise use a default
		const char *prompt_msg = sessLocal.GetAuthMsg();
		if ( prompt_msg[ 0 ] == '\0' ) {
			prompt_msg = common->GetLanguageDict()->GetString( "#str_04308" );
		}
		retkey = sessLocal.MessageBox( MSG_CDKEY, prompt_msg, common->GetLanguageDict()->GetString( "#str_04305" ), true, NULL, NULL, true );
		if ( retkey ) {
			if ( sessLocal.CheckKey( retkey, false, valid ) ) {
				// if all went right, then we may have sent an auth request to the master ( unless the prompt is used during a net connect )
				bool canExit = true;
				if ( sessLocal.MaybeWaitOnCDKey() ) {
					// wait on auth reply, and got denied, prompt again
					if ( !sessLocal.CDKeysAreValid( true ) ) {
						// server says key is invalid - MaybeWaitOnCDKey was interrupted by a CDKeysAuthReply call, which has set the right error message
						// the invalid keys have also been cleared in the process
						sessLocal.MessageBox( MSG_OK, sessLocal.GetAuthMsg(), common->GetLanguageDict()->GetString( "#str_04310" ), true, NULL, NULL, true );
						canExit = false;
					}
				}
				if ( canExit ) {
					// make sure that's saved on file
					sessLocal.WriteCDKey();
					sessLocal.MessageBox( MSG_OK, common->GetLanguageDict()->GetString( "#str_04307" ), common->GetLanguageDict()->GetString( "#str_04305" ), true, NULL, NULL, true );
					break;
				}
			} else {
				// offline check sees key invalid
				// build a message about keys being wrong. do not attempt to change the current key state though
				// ( the keys may be valid, but user would have clicked on the dialog anyway, that kind of thing )
				idStr msg;
				idAsyncNetwork::BuildInvalidKeyMsg( msg, valid );
				sessLocal.MessageBox( MSG_OK, msg, common->GetLanguageDict()->GetString( "#str_04310" ), true, NULL, NULL, true );
			}
		} else if ( args.Argc() == 2 && idStr::Icmp( args.Argv(1), "force" ) == 0 ) {
			// cancelled in force mode
			cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "quit\n" );
			cmdSystem->ExecuteCommandBuffer();
		}
	} while ( retkey );
	recursed = false;
}

/*
===============================================================================

SESSION LOCAL

===============================================================================
*/

/*
===============
idSessionLocal::Clear
===============
*/
void idSessionLocal::Clear() {

	insideUpdateScreen = false;
	insideExecuteMapChange = false;

	loadingSaveGame = false;
	savegameFile = NULL;
	savegameVersion = 0;

	currentMapName.Clear();
	aviDemoShortName.Clear();
	msgFireBack[ 0 ].Clear();
	msgFireBack[ 1 ].Clear();

	timeHitch = 0;

	rw = NULL;
	sw = NULL;
	menuSoundWorld = NULL;
	readDemo = NULL;
	writeDemo = NULL;
	renderdemoVersion = 0;

	syncNextGameFrame = false;
	mapSpawned = false;
	guiActive = NULL;
	aviCaptureMode = false;
	waitingOnBind = false;
	lastPacifierTime = 0;

	msgRunning = false;
	guiMsgRestore = NULL;
	msgIgnoreButtons = false;

	bytesNeededForMapLoad = 0;

#if ID_CONSOLE_LOCK
	emptyDrawCount = 0;
#endif
	ClearWipe();

	loadGameList.Clear();
	modsList.Clear();

	authEmitTimeout = 0;
	authWaitBox = false;

	authMsg.Clear();
}

/*
===============
idSessionLocal::idSessionLocal
===============
*/
idSessionLocal::idSessionLocal() {
	guiInGame = guiMainMenu = guiIntro \
		= guiRestartMenu = guiLoading = guiGameOver = guiActive \
		= guiTest = guiMsg = guiMsgRestore = guiTakeNotes = NULL;

	menuSoundWorld = NULL;

	demoversion=false;

	Clear();
}

/*
===============
idSessionLocal::~idSessionLocal
===============
*/
idSessionLocal::~idSessionLocal() {
}

/*
===============
idSessionLocal::Stop

called on errors and game exits
===============
*/
void idSessionLocal::Stop() {
	ClearWipe();

	// clear mapSpawned and demo playing flags
	UnloadMap();

#ifndef NONET
	// disconnect async client
	idAsyncNetwork::client.DisconnectFromServer();

	// kill async server
	idAsyncNetwork::server.Kill();
#endif

	if ( sw ) {
		sw->StopAllSounds();
	}

	insideUpdateScreen = false;
	insideExecuteMapChange = false;

	// drop all guis
	SetGUI( NULL, NULL );
}

/*
===============
idSessionLocal::Shutdown
===============
*/
void idSessionLocal::Shutdown() {
	int i;

	if ( aviCaptureMode ) {
		EndAVICapture();
	}

	Stop();

	if ( rw ) {
		delete rw;
		rw = NULL;
	}

	if ( sw ) {
		delete sw;
		sw = NULL;
	}

	if ( menuSoundWorld ) {
		delete menuSoundWorld;
		menuSoundWorld = NULL;
	}

	mapSpawnData.serverInfo.Clear();
	mapSpawnData.syncedCVars.Clear();
	for ( i = 0; i < MAX_ASYNC_CLIENTS; i++ ) {
		mapSpawnData.userInfo[i].Clear();
		mapSpawnData.persistentPlayerInfo[i].Clear();
	}

	if ( guiMainMenu_MapList != NULL ) {
		guiMainMenu_MapList->Shutdown();
		uiManager->FreeListGUI( guiMainMenu_MapList );
		guiMainMenu_MapList = NULL;
	}

	Clear();
}

/*
===============
idSessionLocal::IsMultiplayer
===============
*/
bool	idSessionLocal::IsMultiplayer() {
#ifndef NONET
	return idAsyncNetwork::IsActive();
#else
	return false;
#endif
}

/*
================
idSessionLocal::StartWipe

Draws and captures the current state, then starts a wipe with that image
================
*/
void idSessionLocal::StartWipe( const char *_wipeMaterial, bool hold ) {
	console->Close();

	// render the current screen into a texture for the wipe model
	renderSystem->CropRenderSize( 640, 480, true );

	Draw();

	renderSystem->CaptureRenderToImage( "_scratch");
	renderSystem->UnCrop();

	wipeMaterial = declManager->FindMaterial( _wipeMaterial, false );

	wipeStartTime = Sys_Milliseconds();
	wipeStopTime = wipeStartTime + com_wipeSeconds.GetFloat() * 1000.0f;
	wipeHold = hold;
}

/*
================
idSessionLocal::CompleteWipe
================
*/
void idSessionLocal::CompleteWipe() {
	if ( com_ticNumber == 0 ) {
		// if the tic counting hasn't started, we would hang here
		wipeStopTime = 0;
		UpdateScreen( true );
		return;
	}
	while ( Sys_Milliseconds() < wipeStopTime ) {
#if ID_CONSOLE_LOCK
		emptyDrawCount = 0;
#endif
		UpdateScreen( true );
	}
}


/*
================
idSessionLocal::ShowLoadingGui
================
*/
void idSessionLocal::ShowLoadingGui() {
	if ( com_ticNumber == 0 ) {
		return;
	}
	console->Close();

	// introduced in D3XP code. don't think it actually fixes anything, but doesn't hurt either
#if 1
	// Try and prevent the while loop from being skipped over (long hitch on the main thread?)
	int stop = Sys_Milliseconds() + 1000;
	int force = 10;
	while ( Sys_Milliseconds() < stop || force-- > 0 ) {
		Com_UpdateFrameTime(); // DG: put updating com_frameTime into a function
		session->Frame();
		session->UpdateScreen( false );
	}
#else
	int stop = com_ticNumber + 1000.0f / USERCMD_MSEC * 1.0f;
	while ( com_ticNumber < stop ) {
		com_frameTime = com_ticNumber * USERCMD_MSEC;
		session->Frame();
		session->UpdateScreen( false );
	}
#endif
}



/*
================
idSessionLocal::ClearWipe
================
*/
void idSessionLocal::ClearWipe( void ) {
	wipeHold = false;
	wipeStopTime = 0;
	wipeStartTime = 16;
}

/*
================
Session_TestGUI_f
================
*/
static void Session_TestGUI_f( const idCmdArgs &args ) {
	sessLocal.TestGUI( args.Argv(1) );
}

/*
================
idSessionLocal::TestGUI
================
*/
void idSessionLocal::TestGUI( const char *guiName ) {
	if ( guiName && *guiName ) {
		guiTest = uiManager->FindGui( guiName, true, false, true );
	} else {
		guiTest = NULL;
	}
}

/*
================
FindUnusedFileName
================
*/
static idStr FindUnusedFileName( const char *format ) {
	int i;
	char	filename[1024];

	for ( i = 0 ; i < 999 ; i++ ) {
		sprintf( filename, format, i );
		int len = fileSystem->ReadFile( filename, NULL, NULL );
		if ( len <= 0 ) {
			return filename;	// file doesn't exist
		}
	}

	return filename;
}

#ifndef	ID_DEDICATED
/*
================
Session_AVIGame_f
================
*/
static void Session_AVIGame_f( const idCmdArgs &args ) {
	sessLocal.AVIGame( args.Argv(1) );
}

#endif

/*
================
Session_Disconnect_f
================
*/
static void Session_Disconnect_f( const idCmdArgs &args ) {
	sessLocal.Stop();
	sessLocal.StartMenu();
	if ( soundSystem ) {
		soundSystem->SetMute( false );
	}
}



/*
================
idSessionLocal::BeginAVICapture
================
*/
void idSessionLocal::BeginAVICapture( const char *demoName ) {
	idStr name = demoName;
	name.ExtractFileBase( aviDemoShortName );
	aviCaptureMode = true;
	aviDemoFrameCount = 0;
	aviTicStart = 0;
	sw->AVIOpen( va( "demos/%s/", aviDemoShortName.c_str() ), aviDemoShortName.c_str() );
}

/*
================
idSessionLocal::EndAVICapture
================
*/
void idSessionLocal::EndAVICapture() {
	if ( !aviCaptureMode ) {
		return;
	}

	sw->AVIClose();

	// write a .roqParam file so the demo can be converted to a roq file
	idFile *f = fileSystem->OpenFileWrite( va( "demos/%s/%s.roqParam",
		aviDemoShortName.c_str(), aviDemoShortName.c_str() ) );
	f->Printf( "INPUT_DIR demos/%s\n", aviDemoShortName.c_str() );
	f->Printf( "FILENAME demos/%s/%s.RoQ\n", aviDemoShortName.c_str(), aviDemoShortName.c_str() );
	f->Printf( "\nINPUT\n" );
	f->Printf( "%s_*.tga [00000-%05i]\n", aviDemoShortName.c_str(), (int)( aviDemoFrameCount-1 ) );
	f->Printf( "END_INPUT\n" );
	delete f;

	common->Printf( "captured %i frames for %s.\n", ( int )aviDemoFrameCount, aviDemoShortName.c_str() );

	aviCaptureMode = false;
}


/*
================
idSessionLocal::AVIGame

Start AVI recording the current game session
================
*/
void idSessionLocal::AVIGame( const char *demoName ) {
	if ( aviCaptureMode ) {
		EndAVICapture();
		return;
	}

	if ( !mapSpawned ) {
		common->Printf( "No map spawned.\n" );
	}

	if ( !demoName || !demoName[0] ) {
		idStr filename = FindUnusedFileName( "demos/game%03i.game" );
		demoName = filename.c_str();

		// write a one byte stub .game file just so the FindUnusedFileName works,
		fileSystem->WriteFile( demoName, demoName, 1 );
	}

	BeginAVICapture( demoName ) ;
}


/*
===============
idSessionLocal::StartNewGame
===============
*/
void idSessionLocal::StartNewGame( const char *mapName, bool devmap ) {
#ifdef	ID_DEDICATED
	common->Printf( "Dedicated servers cannot start singleplayer games.\n" );
	return;
#else
#if ID_ENFORCE_KEY
	// strict check. don't let a game start without a definitive answer
	if ( !CDKeysAreValid( true ) ) {
		bool prompt = true;
		if ( MaybeWaitOnCDKey() ) {
			// check again, maybe we just needed more time
			if ( CDKeysAreValid( true ) ) {
				// can continue directly
				prompt = false;
			}
		}
		if ( prompt ) {
			cmdSystem->BufferCommandText( CMD_EXEC_NOW, "promptKey force" );
			cmdSystem->ExecuteCommandBuffer();
		}
	}
#endif
#ifndef NONET
	if ( idAsyncNetwork::server.IsActive() ) {
		common->Printf("Server running, use si_map / serverMapRestart\n");
		return;
	}
	if ( idAsyncNetwork::client.IsActive() ) {
		common->Printf("Client running, disconnect from server first\n");
		return;
	}
#endif

	// clear the userInfo so the player starts out with the defaults
	mapSpawnData.userInfo[0].Clear();
	mapSpawnData.persistentPlayerInfo[0].Clear();
	mapSpawnData.userInfo[0] = *cvarSystem->MoveCVarsToDict( CVAR_USERINFO );

	mapSpawnData.serverInfo.Clear();
	mapSpawnData.serverInfo = *cvarSystem->MoveCVarsToDict( CVAR_SERVERINFO );
	mapSpawnData.serverInfo.Set( "si_gameType", "singleplayer" );

	// set the devmap key so any play testing items will be given at
	// spawn time to set approximately the right weapons and ammo
	if(devmap) {
		mapSpawnData.serverInfo.Set( "devmap", "1" );
	}

	mapSpawnData.syncedCVars.Clear();
	mapSpawnData.syncedCVars = *cvarSystem->MoveCVarsToDict( CVAR_NETWORKSYNC );

	MoveToNewMap( mapName );
#endif
}

/*
===============
idSessionLocal::GetAutoSaveName
===============
*/
idStr idSessionLocal::GetAutoSaveName( const char *mapName ) const {
	const idDecl *mapDecl = declManager->FindType( DECL_MAPDEF, mapName, false );
	const idDeclEntityDef *mapDef = static_cast<const idDeclEntityDef *>( mapDecl );
	if ( mapDef ) {
		mapName = common->GetLanguageDict()->GetString( mapDef->dict.GetString( "name", mapName ) );
	}
	// Fixme: Localization
	return va( "^3AutoSave:^0 %s", mapName );
}

/*
===============
idSessionLocal::MoveToNewMap

Leaves the existing userinfo and serverinfo
===============
*/
void idSessionLocal::MoveToNewMap( const char *mapName ) {
	mapSpawnData.serverInfo.Set( "si_map", mapName );

	ExecuteMapChange();

	if ( !com_disableAutoSaves.GetBool() && !mapSpawnData.serverInfo.GetBool("devmap") ) {
		// Autosave at the beginning of the level - DG: unless disabled with "com_disableAutoSaves 1"

		// DG: set an explicit savename to avoid problems with autosave names
		//     (they were translated which caused problems like all alpha labs parts
		//      getting the same filename in spanish, probably because the strings contained
		//      dots and everything behind them was cut off as "file extension".. see #305)
		idStr saveFileName = "Autosave_";
		saveFileName += mapName;
		SaveGame( GetAutoSaveName( mapName ), true, saveFileName );
	}

	SetGUI( NULL, NULL );
}

/*
===============
idSessionLocal::UnloadMap

Performs cleanup that needs to happen between maps, or when a
game is exited.
Exits with mapSpawned = false
===============
*/
void idSessionLocal::UnloadMap() {
	// end the current map in the game
	if ( game ) {
		game->MapShutdown();
	}

	mapSpawned = false;

	// DG: that state needs to be reset now
	Sys_SetInteractiveIngameGuiActive( false, NULL );
}

/*
===============
idSessionLocal::LoadLoadingGui
===============
*/
void idSessionLocal::LoadLoadingGui( const char *mapName ) {
	// load / program a gui to stay up on the screen while loading
	idStr stripped = mapName;
	stripped.StripFileExtension();
	stripped.StripPath();

	char guiMap[ MAX_STRING_CHARS ];
	idStr::Copynz( guiMap, va( "guis/map/%s.gui", stripped.c_str() ), MAX_STRING_CHARS );
	// give the gamecode a chance to override
	game->GetMapLoadingGUI( guiMap );

	if ( uiManager->CheckGui( guiMap ) ) {
		guiLoading = uiManager->FindGui( guiMap, true, false, true );
	} else {
		guiLoading = uiManager->FindGui( "guis/map/loading.gui", true, false, true );
	}
	guiLoading->SetStateFloat( "map_loading", 0.0f );
}

/*
===============
idSessionLocal::GetBytesNeededForMapLoad
===============
*/
int idSessionLocal::GetBytesNeededForMapLoad( const char *mapName ) {
	const idDecl *mapDecl = declManager->FindType( DECL_MAPDEF, mapName, false );
	const idDeclEntityDef *mapDef = static_cast<const idDeclEntityDef *>( mapDecl );
	if ( mapDef ) {
		return mapDef->dict.GetInt( va("size%d", Max( 0, com_machineSpec.GetInteger() ) ) );
	} else {
		if ( com_machineSpec.GetInteger() < 2 ) {
			return 200 * 1024 * 1024;
		} else {
			return 400 * 1024 * 1024;
		}
	}
}

/*
===============
idSessionLocal::SetBytesNeededForMapLoad
===============
*/
void idSessionLocal::SetBytesNeededForMapLoad( const char *mapName, int bytesNeeded ) {
	idDecl *mapDecl = const_cast<idDecl *>(declManager->FindType( DECL_MAPDEF, mapName, false ));
	idDeclEntityDef *mapDef = static_cast<idDeclEntityDef *>( mapDecl );

	if ( com_updateLoadSize.GetBool() && mapDef ) {
		// we assume that if com_updateLoadSize is true then the file is writable

		mapDef->dict.SetInt( va("size%d", com_machineSpec.GetInteger()), bytesNeeded );

		idStr declText = "\nmapDef ";
		declText += mapDef->GetName();
		declText += " {\n";
		for (int i=0; i<mapDef->dict.GetNumKeyVals(); i++) {
			const idKeyValue *kv = mapDef->dict.GetKeyVal( i );
			if ( kv && (kv->GetKey().Cmp("classname") != 0 ) ) {
				declText += "\t\"" + kv->GetKey() + "\"\t\t\"" + kv->GetValue() + "\"\n";
			}
		}
		declText += "}";
		mapDef->SetText( declText );
		mapDef->ReplaceSourceFileText();
	}
}

/*
===============
idSessionLocal::ExecuteMapChange

Performs the initialization of a game based on mapSpawnData, used for both single
player and multiplayer, but not for renderDemos, which don't
create a game at all.
Exits with mapSpawned = true
===============
*/
void idSessionLocal::ExecuteMapChange( bool noFadeWipe ) {
	int		i;
	bool	reloadingSameMap;

	// close console and remove any prints from the notify lines
	console->Close();

	if ( IsMultiplayer() ) {
		// make sure the mp GUI isn't up, or when players get back in the
		// map, mpGame's menu and the gui will be out of sync.
		SetGUI( NULL, NULL );
	}

	// mute sound
	soundSystem->SetMute( true );

	// clear all menu sounds
	menuSoundWorld->ClearAllSoundEmitters();

	// unpause the game sound world
	// NOTE: we UnPause again later down. not sure this is needed
	if ( sw->IsPaused() ) {
		sw->UnPause();
	}

	if ( !noFadeWipe ) {
		// capture the current screen and start a wipe
		StartWipe( "wipeMaterial", true );

		// immediately complete the wipe to fade out the level transition
		// run the wipe to completion
		CompleteWipe();
	}

	// extract the map name from serverinfo
	idStr mapString = mapSpawnData.serverInfo.GetString( "si_map" );

	idStr fullMapName = "maps/";
	fullMapName += mapString;
	fullMapName.StripFileExtension();

	// shut down the existing game if it is running
	UnloadMap();

	// don't do the deferred caching if we are reloading the same map
	if ( fullMapName == currentMapName ) {
		reloadingSameMap = true;
	} else {
		reloadingSameMap = false;
		currentMapName = fullMapName;
	}

	// note which media we are going to need to load
	if ( !reloadingSameMap ) {
		declManager->BeginLevelLoad();
		renderSystem->BeginLevelLoad();
		soundSystem->BeginLevelLoad();
	}

	uiManager->BeginLevelLoad();
	uiManager->Reload( true );

	// set the loading gui that we will wipe to
	LoadLoadingGui( mapString );

	// cause prints to force screen updates as a pacifier,
	// and draw the loading gui instead of game draws
	insideExecuteMapChange = true;

	// if this works out we will probably want all the sizes in a def file although this solution will
	// work for new maps etc. after the first load. we can also drop the sizes into the default.cfg
	fileSystem->ResetReadCount();
	if ( !reloadingSameMap  ) {
		bytesNeededForMapLoad = GetBytesNeededForMapLoad( mapString.c_str() );
	} else {
		bytesNeededForMapLoad = 30 * 1024 * 1024;
	}

	ClearWipe();

	// let the loading gui spin for 1 second to animate out
	ShowLoadingGui();

	// note any warning prints that happen during the load process
	common->ClearWarnings( mapString );

	// release the mouse cursor
	// before we do this potentially long operation
	Sys_GrabMouseCursor( false );

#ifndef NONET
	// if net play, we get the number of clients during mapSpawnInfo processing
	if ( !idAsyncNetwork::IsActive() ) {
		numClients = 1;
	}
#else
	numClients = 1;
#endif

	int start = Sys_Milliseconds();

	common->Printf( "----- Map Initialization -----\n" );
	common->Printf( "Map: %s\n", mapString.c_str() );

	// let the renderSystem load all the geometry
	if ( !rw->InitFromMap( fullMapName ) ) {
		common->Error( "couldn't load %s", fullMapName.c_str() );
	}

	// for the synchronous networking we needed to roll the angles over from
	// level to level, but now we can just clear everything
	usercmdGen->InitForNewMap();
	memset( &mapSpawnData.mapSpawnUsercmd, 0, sizeof( mapSpawnData.mapSpawnUsercmd ) );

	// set the user info
	for ( i = 0; i < numClients; i++ ) {
#ifndef NONET
		game->SetUserInfo( i, mapSpawnData.userInfo[i], idAsyncNetwork::client.IsActive(), false );
#else
		game->SetUserInfo( i, mapSpawnData.userInfo[i], false, false );
#endif
		game->SetPersistentPlayerInfo( i, mapSpawnData.persistentPlayerInfo[i] );
	}

	// load and spawn all other entities ( from a savegame possibly )
	if ( loadingSaveGame && savegameFile ) {
		if ( game->InitFromSaveGame( fullMapName + ".map", rw, sw, savegameFile ) == false ) {
			// If the loadgame failed, restart the map with the player persistent data
			loadingSaveGame = false;
			fileSystem->CloseFile( savegameFile );
			savegameFile = NULL;

			common->Warning( "WARNING: Loading savegame failed, will restart the map with the player persistent data!" );

			game->SetServerInfo( mapSpawnData.serverInfo );
#ifndef NONET
			game->InitFromNewMap( fullMapName + ".map", rw, sw, idAsyncNetwork::server.IsActive(), idAsyncNetwork::client.IsActive(), Sys_Milliseconds() );
#else
			game->InitFromNewMap( fullMapName + ".map", rw, sw, false, false, Sys_Milliseconds() );
#endif
		}
	} else {
		game->SetServerInfo( mapSpawnData.serverInfo );
#ifndef NONET
		game->InitFromNewMap( fullMapName + ".map", rw, sw, idAsyncNetwork::server.IsActive(), idAsyncNetwork::client.IsActive(), Sys_Milliseconds() );
#else
		game->InitFromNewMap( fullMapName + ".map", rw, sw, false, false, Sys_Milliseconds() );
#endif
	}

#ifndef NONET
	if ( !idAsyncNetwork::IsActive() && !loadingSaveGame ) {
#else
	if ( !loadingSaveGame ) {
#endif
		// spawn players
		for ( i = 0; i < numClients; i++ ) {
			game->SpawnPlayer( i );
		}
	}

	// actually purge/load the media
	if ( !reloadingSameMap ) {
		renderSystem->EndLevelLoad();
		soundSystem->EndLevelLoad( mapString.c_str() );
		declManager->EndLevelLoad();
		SetBytesNeededForMapLoad( mapString.c_str(), fileSystem->GetReadCount() );
	}
	uiManager->EndLevelLoad();

#ifndef NONET
	if ( !idAsyncNetwork::IsActive() && !loadingSaveGame ) {
#else
	if ( !loadingSaveGame ) {
#endif
		// run a few frames to allow everything to settle
		for ( i = 0; i < 10; i++ ) {
			game->RunFrame( mapSpawnData.mapSpawnUsercmd );
		}
	}

	int	msec = Sys_Milliseconds() - start;
	common->Printf( "%6d msec to load %s\n", msec, mapString.c_str() );

	// let the renderSystem generate interactions now that everything is spawned
	rw->GenerateAllInteractions();

	common->PrintWarnings();

	if ( guiLoading && bytesNeededForMapLoad ) {
		float pct = guiLoading->State().GetFloat( "map_loading" );
		if ( pct < 0.0f ) {
			pct = 0.0f;
		}
		while ( pct < 1.0f ) {
			guiLoading->SetStateFloat( "map_loading", pct );
			guiLoading->StateChanged( com_frameTime );
			Sys_GenerateEvents();
			UpdateScreen();
			pct += 0.05f;
		}
	}

	// capture the current screen and start a wipe
	StartWipe( "wipe2Material" );

	usercmdGen->Clear();

	// don't bother spinning over all the tics we spent loading
	lastGameTic = latchedTicNumber = com_ticNumber;

	// remove any prints from the notify lines
	console->ClearNotifyLines();

	// stop drawing the laoding screen
	insideExecuteMapChange = false;

	Sys_SetPhysicalWorkMemory( -1, -1 );

	// set the game sound world for playback
	soundSystem->SetPlayingSoundWorld( sw );

	// when loading a save game the sound is paused
	if ( sw->IsPaused() ) {
		// unpause the game sound world
		sw->UnPause();
	}

	// restart entity sound playback
	soundSystem->SetMute( false );

	// we are valid for game draws now
	mapSpawned = true;
	Sys_ClearEvents();
}

/*
===============
LoadGame_f
===============
*/
void LoadGame_f( const idCmdArgs &args ) {
	console->Close();
	if ( args.Argc() < 2 || idStr::Icmp(args.Argv(1), "quick" ) == 0 ) {
		sessLocal.QuickLoad();
	} else {
		sessLocal.LoadGame( args.Argv(1) );
	}
}

/*
===============
SaveGame_f
===============
*/
void SaveGame_f( const idCmdArgs &args ) {
	if ( args.Argc() < 2 || idStr::Icmp( args.Argv(1), "quick" ) == 0 ) {
		sessLocal.QuickSave();
	} else {
		if ( sessLocal.SaveGame( args.Argv(1) ) ) {
			common->Printf( "Saved %s\n", args.Argv(1) );
		}
	}
}

/*
===============
TakeViewNotes_f
===============
*/
void TakeViewNotes_f( const idCmdArgs &args ) {
	const char *p = ( args.Argc() > 1 ) ? args.Argv( 1 ) : "";
	sessLocal.TakeNotes( p );
}

/*
===============
TakeViewNotes2_f
===============
*/
void TakeViewNotes2_f( const idCmdArgs &args ) {
	const char *p = ( args.Argc() > 1 ) ? args.Argv( 1 ) : "";
	sessLocal.TakeNotes( p, true );
}

/*
===============
idSessionLocal::TakeNotes
===============
*/
void idSessionLocal::TakeNotes( const char *p, bool extended ) {
	if ( !mapSpawned ) {
		common->Printf( "No map loaded!\n" );
		return;
	}

	if ( extended ) {
		guiTakeNotes = uiManager->FindGui( "guis/takeNotes2.gui", true, false, true );

#if 0
		const char *people[] = {
			"Nobody", "Adam", "Brandon", "David", "PHook", "Jay", "Jake",
				"PatJ", "Brett", "Ted", "Darin", "Brian", "Sean"
		};
#else
		const char *people[] = {
			"Tim", "Kenneth", "Robert",
			"Matt", "Mal", "Jerry", "Steve", "Pat",
			"Xian", "Ed", "Fred", "James", "Eric", "Andy", "Seneca", "Patrick", "Kevin",
			"MrElusive", "Jim", "Brian", "John", "Adrian", "Nobody"
		};
#endif
		const int numPeople = sizeof( people ) / sizeof( people[0] );

		idListGUI * guiList_people = uiManager->AllocListGUI();
		guiList_people->Config( guiTakeNotes, "person" );
		for ( int i = 0; i < numPeople; i++ ) {
			guiList_people->Push( people[i] );
		}
		uiManager->FreeListGUI( guiList_people );

	} else {
		guiTakeNotes = uiManager->FindGui( "guis/takeNotes.gui", true, false, true );
	}

	SetGUI( guiTakeNotes, NULL );
	guiActive->SetStateString( "note", "" );
	guiActive->SetStateString( "notefile", p );
	guiActive->SetStateBool( "extended", extended );
	guiActive->Activate( true, com_frameTime );
}

/*
===============
Session_Hitch_f
===============
*/
void Session_Hitch_f( const idCmdArgs &args ) {
	idSoundWorld *sw = soundSystem->GetPlayingSoundWorld();
	if ( sw ) {
		soundSystem->SetMute(true);
		sw->Pause();
		Sys_EnterCriticalSection();
	}
	if ( args.Argc() == 2 ) {
		Sys_Sleep( atoi(args.Argv(1)) );
	} else {
		Sys_Sleep( 100 );
	}
	if ( sw ) {
		Sys_LeaveCriticalSection();
		sw->UnPause();
		soundSystem->SetMute(false);
	}
}

/*
===============
idSessionLocal::ScrubSaveGameFileName

Turns a bad file name into a good one or your money back
===============
*/
void idSessionLocal::ScrubSaveGameFileName( idStr &saveFileName ) const {
	int i;
	idStr inFileName;

	inFileName = saveFileName;
	inFileName.RemoveColors();
	inFileName.StripFileExtension();

	saveFileName.Clear();

	int len = inFileName.Length();
	for ( i = 0; i < len; i++ ) {
		if ( strchr( "',.~!@#$%^&*()[]{}<>\\|/=?+;:-\'\"", inFileName[i] ) ) {
			// random junk
			saveFileName += '_';
		} else if ( (const unsigned char)inFileName[i] >= 128 ) {
			// high ascii chars
			saveFileName += '_';
		} else if ( inFileName[i] == ' ' ) {
			saveFileName += '_';
		} else {
			saveFileName += inFileName[i];
		}
	}
}

/*
===============
idSessionLocal::SaveGame
===============
*/
bool idSessionLocal::SaveGame( const char *saveName, bool autosave, const char* saveFileName ) {
#ifdef	ID_DEDICATED
	common->Printf( "Dedicated servers cannot save games.\n" );
	return false;
#else
	int i;
	idStr previewFile, descriptionFile, mapName;
	// DG: support setting an explicit savename to avoid problems with autosave names
	idStr gameFile = (saveFileName != NULL) ? saveFileName : saveName;

	if ( !mapSpawned ) {
		common->Printf( "Not playing a game.\n" );
		return false;
	}

	if ( IsMultiplayer() ) {
		common->Printf( "Can't save during net play.\n" );
		return false;
	}

	if ( game->GetPersistentPlayerInfo( 0 ).GetInt( "health" ) <= 0 ) {
		MessageBox( MSG_OK, common->GetLanguageDict()->GetString ( "#str_04311" ), common->GetLanguageDict()->GetString ( "#str_04312" ), true );
		common->Printf( "You must be alive to save the game\n" );
		return false;
	}

	if ( Sys_GetDriveFreeSpace( cvarSystem->GetCVarString( "fs_savepath" ) ) < 25 ) {
		MessageBox( MSG_OK, common->GetLanguageDict()->GetString ( "#str_04313" ), common->GetLanguageDict()->GetString ( "#str_04314" ), true );
		common->Printf( "Not enough drive space to save the game\n" );
		return false;
	}

	idSoundWorld *pauseWorld = soundSystem->GetPlayingSoundWorld();
	if ( pauseWorld ) {
		pauseWorld->Pause();
		soundSystem->SetPlayingSoundWorld( NULL );
	}

	// setup up filenames and paths
	ScrubSaveGameFileName( gameFile );

	gameFile = "savegames/" + gameFile;
	gameFile.SetFileExtension( ".save" );

	previewFile = gameFile;
	previewFile.SetFileExtension( ".tga" );

	descriptionFile = gameFile;
	descriptionFile.SetFileExtension( ".txt" );

	// Open savegame file
	idFile *fileOut = fileSystem->OpenFileWrite( gameFile );
	if ( fileOut == NULL ) {
		common->Warning( "Failed to open save file '%s'\n", gameFile.c_str() );
		if ( pauseWorld ) {
			soundSystem->SetPlayingSoundWorld( pauseWorld );
			pauseWorld->UnPause();
		}
		return false;
	}

	// Write SaveGame Header:
	// Game Name / Version / Map Name / Persistant Player Info

	// game
	const char *gamename = GAME_NAME;
	fileOut->WriteString( gamename );

	// version
	fileOut->WriteInt( SAVEGAME_VERSION );

	// map
	mapName = mapSpawnData.serverInfo.GetString( "si_map" );
	fileOut->WriteString( mapName );

	// persistent player info
	for ( i = 0; i < MAX_ASYNC_CLIENTS; i++ ) {
		mapSpawnData.persistentPlayerInfo[i] = game->GetPersistentPlayerInfo( i );
		mapSpawnData.persistentPlayerInfo[i].WriteToFileHandle( fileOut );
	}

	// let the game save its state
	game->SaveGame( fileOut );

	// close the sava game file
	fileSystem->CloseFile( fileOut );

	// Write screenshot
	if ( !autosave ) {
		renderSystem->CropRenderSize( 320, 240, false );
		game->Draw( 0 );
		renderSystem->CaptureRenderToFile( previewFile, true );
		renderSystem->UnCrop();
	}

	// Write description, which is just a text file with
	// the unclean save name on line 1, map name on line 2, screenshot on line 3
	idFile *fileDesc = fileSystem->OpenFileWrite( descriptionFile );
	if ( fileDesc == NULL ) {
		common->Warning( "Failed to open description file '%s'\n", descriptionFile.c_str() );
		if ( pauseWorld ) {
			soundSystem->SetPlayingSoundWorld( pauseWorld );
			pauseWorld->UnPause();
		}
		return false;
	}

	idStr description = saveName;
	description.Replace( "\\", "\\\\" );
	description.Replace( "\"", "\\\"" );

	const idDeclEntityDef *mapDef = static_cast<const idDeclEntityDef *>(declManager->FindType( DECL_MAPDEF, mapName, false ));
	if ( mapDef ) {
		mapName = common->GetLanguageDict()->GetString( mapDef->dict.GetString( "name", mapName ) );
	}

	fileDesc->Printf( "\"%s\"\n", description.c_str() );
	fileDesc->Printf( "\"%s\"\n", mapName.c_str());

	if ( autosave ) {
		idStr sshot = mapSpawnData.serverInfo.GetString( "si_map" );
		sshot.StripPath();
		sshot.StripFileExtension();
		fileDesc->Printf( "\"guis/assets/autosave/%s\"\n", sshot.c_str() );
	} else {
		fileDesc->Printf( "\"\"\n" );
	}

	fileSystem->CloseFile( fileDesc );

	if ( pauseWorld ) {
		soundSystem->SetPlayingSoundWorld( pauseWorld );
		pauseWorld->UnPause();
	}

	syncNextGameFrame = true;


	return true;
#endif
}

/*
===============
idSessionLocal::LoadGame
===============
*/
bool idSessionLocal::LoadGame( const char *saveName ) {
#ifdef	ID_DEDICATED
	common->Printf( "Dedicated servers cannot load games.\n" );
	return false;
#else
	int i;
	idStr in, loadFile, saveMap, gamename;

	if ( IsMultiplayer() ) {
		common->Printf( "Can't load during net play.\n" );
		return false;
	}

	//Hide the dialog box if it is up.
	StopBox();

	loadFile = saveName;
	ScrubSaveGameFileName( loadFile );
	loadFile.SetFileExtension( ".save" );

	in = "savegames/";
	in += loadFile;

	// Open savegame file
	// only allow loads from the game directory because we don't want a base game to load
	idStr game = cvarSystem->GetCVarString( "fs_game" );
	savegameFile = fileSystem->OpenFileRead( in, true, game.Length() ? game : NULL );

	if ( savegameFile == NULL ) {
		common->Warning( "Couldn't open savegame file %s", in.c_str() );
		return false;
	}

	loadingSaveGame = true;

	// Read in save game header
	// Game Name / Version / Map Name / Persistant Player Info

	// game
	savegameFile->ReadString( gamename );

	// if this isn't a savegame for the correct game, abort loadgame
	if ( ! (gamename == GAME_NAME || gamename == "DOOM 3") ) {
		common->Warning( "Attempted to load an invalid savegame: %s", in.c_str() );

		loadingSaveGame = false;
		fileSystem->CloseFile( savegameFile );
		savegameFile = NULL;
		return false;
	}

	// version
	savegameFile->ReadInt( savegameVersion );

	// map
	savegameFile->ReadString( saveMap );

	// persistent player info
	for ( i = 0; i < MAX_ASYNC_CLIENTS; i++ ) {
		mapSpawnData.persistentPlayerInfo[i].ReadFromFileHandle( savegameFile );
	}

	// check the version, if it doesn't match, cancel the loadgame,
	// but still load the map with the persistant playerInfo from the header
	// so that the player doesn't lose too much progress.
	if ( savegameVersion < 16 || savegameVersion > SAVEGAME_VERSION ) { // dhewm3 supports savegames with v16 - v18
		common->Warning( "Savegame Version mismatch: aborting loadgame and starting level with persistent data" );
		loadingSaveGame = false;
		fileSystem->CloseFile( savegameFile );
		savegameFile = NULL;
	}

	common->DPrintf( "loading a v%d savegame\n", savegameVersion );

	if ( saveMap.Length() > 0 ) {

		// Start loading map
		mapSpawnData.serverInfo.Clear();

		mapSpawnData.serverInfo = *cvarSystem->MoveCVarsToDict( CVAR_SERVERINFO );
		mapSpawnData.serverInfo.Set( "si_gameType", "singleplayer" );

		mapSpawnData.serverInfo.Set( "si_map", saveMap );

		mapSpawnData.syncedCVars.Clear();
		mapSpawnData.syncedCVars = *cvarSystem->MoveCVarsToDict( CVAR_NETWORKSYNC );

		mapSpawnData.mapSpawnUsercmd[0] = usercmdGen->TicCmd( latchedTicNumber );
		// make sure no buttons are pressed
		mapSpawnData.mapSpawnUsercmd[0].buttons = 0;

		ExecuteMapChange();

		SetGUI( NULL, NULL );
	}

	if ( loadingSaveGame ) {
		fileSystem->CloseFile( savegameFile );
		loadingSaveGame = false;
		savegameFile = NULL;
	}

	return true;
#endif
}

bool idSessionLocal::QuickSave()
{
	idStr saveName = common->GetLanguageDict()->GetString( "#str_07178" );

	idStr saveFilePathBase = saveName;
	ScrubSaveGameFileName( saveFilePathBase );
	saveFilePathBase = "savegames/" + saveFilePathBase;

	const char* game = cvarSystem->GetCVarString( "fs_game" );
	if ( game != NULL && game[0] == '\0' ) {
		game = NULL;
	}

	const int maxNum = com_numQuicksaves.GetInteger();
	int indexToUse = 1;
	ID_TIME_T oldestTime = 0;
	for( int i = 1; i <= maxNum; ++i ) {
		idStr saveFilePath = saveFilePathBase;
		if ( i > 1 ) {
			// the first one is just called "QuickSave" without a number, like before.
			// the others are called "QuickSave2" "QuickSave3" etc
			saveFilePath += i;
		}
		saveFilePath.SetFileExtension( ".save" );

		idFile *f = fileSystem->OpenFileRead( saveFilePath, true, game );
		if ( f == NULL ) {
			// this savegame doesn't exist yet => we can use this index for the name
			indexToUse = i;
			break;
		} else {
			ID_TIME_T ts = f->Timestamp();
			assert( ts != 0 );
			if ( ts < oldestTime || oldestTime == 0 ) {
				// this is the oldest quicksave we found so far => a candidate to be overwritten
				indexToUse = i;
				oldestTime = ts;
			}
			delete f;
		}
	}

	if ( indexToUse > 1 ) {
		saveName += indexToUse;
	}

	if ( SaveGame( saveName ) ) {
		common->Printf( "%s\n", saveName.c_str() );
		return true;
	}
	return false;
}

bool idSessionLocal::QuickLoad()
{
	idStr saveName = common->GetLanguageDict()->GetString( "#str_07178" );

	idStr saveFilePathBase = saveName;
	ScrubSaveGameFileName( saveFilePathBase );
	saveFilePathBase = "savegames/" + saveFilePathBase;

	const char* game = cvarSystem->GetCVarString( "fs_game" );
	if ( game != NULL && game[0] == '\0' ) {
		game = NULL;
	}

	// find the newest QuickSave (or QuickSave2, QuickSave3, ...)
	const int maxNum = com_numQuicksaves.GetInteger();
	int indexToUse = 1;
	ID_TIME_T newestTime = 0;
	for( int i = 1; i <= maxNum; ++i ) {
		idStr saveFilePath = saveFilePathBase;
		if ( i > 1 ) {
			// the first one is just called "QuickSave" without a number, like before.
			// the others are called "QuickSave2" "QuickSave3" etc
			saveFilePath += i;
		}
		saveFilePath.SetFileExtension( ".save" );

		idFile *f = fileSystem->OpenFileRead( saveFilePath, true, game );
		if ( f != NULL ) {
			ID_TIME_T ts = f->Timestamp();
			assert( ts != 0 );
			if ( ts > newestTime ) {
				indexToUse = i;
				newestTime = ts;
			}
			delete f;
		}
	}

	if ( indexToUse > 1 ) {
		saveName += indexToUse;
	}

	return sessLocal.LoadGame( saveName );
}

/*
===============
idSessionLocal::ProcessEvent
===============
*/
bool idSessionLocal::ProcessEvent( const sysEvent_t *event ) {
	// hitting escape anywhere brings up the menu
	// DG: but shift-escape should bring up console instead so ignore that
	if ( !guiActive && event->evType == SE_KEY && event->evValue2 == 1
			&& event->evValue == K_ESCAPE && !idKeyInput::IsDown( K_SHIFT ) ) {
		console->Close();
		if ( game ) {
			idUserInterface	*gui = NULL;
			escReply_t		op;
			op = game->HandleESC( &gui );
			if ( op == ESC_IGNORE ) {
				return true;
			} else if ( op == ESC_GUI ) {
				SetGUI( gui, NULL );
				return true;
			}
		}
		StartMenu();
		return true;
	}

	// let the pull-down console take it if desired
	if ( console->ProcessEvent( event, false ) ) {
		return true;
	}

	// if we are testing a GUI, send all events to it
	if ( guiTest ) {
		// hitting escape exits the testgui
		if ( event->evType == SE_KEY && event->evValue2 == 1 && event->evValue == K_ESCAPE ) {
			guiTest = NULL;
			return true;
		}

		static const char *cmd;
		cmd = guiTest->HandleEvent( event, com_frameTime );
		if ( cmd && cmd[0] ) {
			common->Printf( "testGui event returned: '%s'\n", cmd );
		}
		return true;
	}

	// menus / etc
	if ( guiActive ) {
		MenuEvent( event );
		return true;
	}

	// if we aren't in a game, force the console to take it
	if ( !mapSpawned ) {
		console->ProcessEvent( event, true );
		return true;
	}

	// in game, exec bindings for all key downs
	if ( event->evType == SE_KEY && event->evValue2 == 1 ) {
		idKeyInput::ExecKeyBinding( event->evValue );
		return true;
	}

	return false;
}

/*
===============
idSessionLocal::DrawWipeModel

Draw the fade material over everything that has been drawn
===============
*/
void	idSessionLocal::DrawWipeModel() {
	unsigned now = Sys_Milliseconds();

	if (  wipeStartTime >= wipeStopTime ) {
		return;
	}

	if ( !wipeHold && now > wipeStopTime ) {
		return;
	}

	float fade = ( float )( now - wipeStartTime ) / ( wipeStopTime - wipeStartTime );
	renderSystem->SetColor4( 1, 1, 1, fade );
	renderSystem->DrawStretchPic( 0, 0, 640, 480, 0, 0, 1, 1, wipeMaterial );
}

/*
===============
idSessionLocal::PacifierUpdate
===============
*/
void idSessionLocal::PacifierUpdate() {
	if ( !insideExecuteMapChange ) {
		return;
	}

	// never do pacifier screen updates while inside the
	// drawing code, or we can have various recursive problems
	if ( insideUpdateScreen ) {
		return;
	}

	int	time = eventLoop->Milliseconds();

	if ( time - lastPacifierTime < 100 ) {
		return;
	}
	lastPacifierTime = time;

	if ( guiLoading && bytesNeededForMapLoad ) {
		float n = fileSystem->GetReadCount();
		float pct = ( n / bytesNeededForMapLoad );
		// pct = idMath::ClampFloat( 0.0f, 100.0f, pct );
		guiLoading->SetStateFloat( "map_loading", pct );
		guiLoading->StateChanged( com_frameTime );
	}

	Sys_GenerateEvents();

	UpdateScreen();

#ifndef NONET
	idAsyncNetwork::client.PacifierUpdate();
	idAsyncNetwork::server.PacifierUpdate();
#endif
}

/*
===============
idSessionLocal::Draw
===============
*/
void idSessionLocal::Draw() {
	D3P_ScopedCPUSample(Session_Draw);
	bool fullConsole = false;

	if ( insideExecuteMapChange ) {
		if ( guiLoading ) {
			guiLoading->Redraw( com_frameTime );
		}
		if ( guiActive == guiMsg ) {
			guiMsg->Redraw( com_frameTime );
		}
	} else if ( guiTest ) {
		// if testing a gui, clear the screen and draw it
		// clear the background, in case the tested gui is transparent
		// NOTE that you can't use this for aviGame recording, it will tick at real com_frameTime between screenshots..
		renderSystem->SetColor( colorBlack );
		renderSystem->DrawStretchPic( 0, 0, 640, 480, 0, 0, 1, 1, declManager->FindMaterial( "_white" ) );
		guiTest->Redraw( com_frameTime );
	} else if ( guiActive && !guiActive->State().GetBool( "gameDraw" ) ) {

		// draw the frozen gui in the background
		if ( guiActive == guiMsg && guiMsgRestore ) {
			guiMsgRestore->Redraw( com_frameTime );
		}

		// draw the menus full screen
		if ( guiActive == guiTakeNotes && !com_skipGameDraw.GetBool() ) {
			game->Draw( GetLocalClientNum() );
		}

		guiActive->Redraw( com_frameTime );
	} else if ( mapSpawned ) {
		bool gameDraw = false;
		// normal drawing for both single and multi player
		if ( !com_skipGameDraw.GetBool() && GetLocalClientNum() >= 0 ) {
			// draw the game view
			int	start = Sys_Milliseconds();
			gameDraw = game->Draw( GetLocalClientNum() );
			int end = Sys_Milliseconds();
			time_gameDraw += ( end - start );	// note time used for com_speeds
		}
		if ( !gameDraw ) {
			renderSystem->SetColor( colorBlack );
			renderSystem->DrawStretchPic( 0, 0, 640, 480, 0, 0, 1, 1, declManager->FindMaterial( "_white" ) );
		}
	} else {
#if ID_CONSOLE_LOCK
		if ( com_allowConsole.GetBool() ) {
			console->Draw( true );
		} else {
			emptyDrawCount++;
			if ( emptyDrawCount > 5 ) {
				// it's best if you can avoid triggering the watchgod by doing the right thing somewhere else
				assert( false );
				common->Warning( "idSession: triggering mainmenu watchdog" );
				emptyDrawCount = 0;
				StartMenu();
			}
			renderSystem->SetColor4( 0, 0, 0, 1 );
			renderSystem->DrawStretchPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, 1, 1, declManager->FindMaterial( "_white" ) );
		}
#else
		// draw the console full screen - this should only ever happen in developer builds
		console->Draw( true );
#endif
		fullConsole = true;
	}

#if ID_CONSOLE_LOCK
	if ( !fullConsole && emptyDrawCount ) {
		common->DPrintf( "idSession: %d empty frame draws\n", emptyDrawCount );
		emptyDrawCount = 0;
	}
	fullConsole = false;
#endif

	// draw the wipe material on top of this if it hasn't completed yet
	DrawWipeModel();

	// draw the half console / notify console on top of everything
	if ( !fullConsole ) {
		console->Draw( false );
	}
}

/*
===============
idSessionLocal::UpdateScreen
===============
*/
void idSessionLocal::UpdateScreen( bool outOfSequence ) {
	D3P_ScopedCPUSample(Session_UpdateScreen);
#ifdef _WIN32

	if ( com_editors ) {
		if ( !Sys_IsWindowVisible() ) {
			return;
		}
	}
#endif

	if ( insideUpdateScreen ) {
		return;
//		common->FatalError( "idSessionLocal::UpdateScreen: recursively called" );
	}

	insideUpdateScreen = true;

	// if this is a long-operation update and we are in windowed mode,
	// release the mouse capture back to the desktop
	if ( outOfSequence ) {
		Sys_GrabMouseCursor( false );
	}

	D3P_BeginCPUSample(Render_BeginFrame);
	renderSystem->BeginFrame( renderSystem->GetScreenWidth(), renderSystem->GetScreenHeight() );
	D3P_EndCPUSample(Render_BeginFrame);

	// draw everything
	Draw();

	D3P_BeginCPUSample(Render_EndFrame);
	if ( com_speeds.GetBool() ) {
		renderSystem->EndFrame( &time_frontend, &time_backend );
	} else {
		renderSystem->EndFrame( NULL, NULL );
	}
	D3P_EndCPUSample(Render_EndFrame);

	insideUpdateScreen = false;
}

/*
===============
idSessionLocal::Frame
===============
*/
#ifndef NOAUDIO
extern bool CheckOpenALDeviceAndRecoverIfNeeded();
#endif
extern int g_screenshotFormat;
void idSessionLocal::Frame() {
	D3P_ScopedCPUSample(Session_Frame);

	if ( com_asyncSound.GetInteger() == 0 ) {
		soundSystem->AsyncUpdateWrite( Sys_Milliseconds() );
	}

#ifndef NOAUDIO
	// DG: periodically check if sound device is still there and try to reset it if not
	//     (calling this from idSoundSystem::AsyncUpdate(), which runs in a separate thread
	//      by default, causes a deadlock when calling idCommon->Warning())
	CheckOpenALDeviceAndRecoverIfNeeded();
#endif

	// Editors that completely take over the game
	if ( com_editorActive && ( com_editors & ( EDITOR_RADIANT | EDITOR_GUI ) ) ) {
		return;
	}

#if 0 // handled via Sys_GenerateEvents() -> handleMouseGrab()
	// if the console is down, we don't need to hold
	// the mouse cursor
	if ( console->Active() || com_editorActive ) {
		Sys_GrabMouseCursor( false );
	} else {
		Sys_GrabMouseCursor( true );
	}
#endif

	// save the screenshot and audio from the last draw if needed
	if ( aviCaptureMode ) {
		idStr	name;

		name = va("demos/%s/%s_%05i.tga", aviDemoShortName.c_str(), aviDemoShortName.c_str(), aviTicStart );

		float ratio = 30.0f / ( 1000.0f / USERCMD_MSEC / com_aviDemoTics.GetInteger() );
		aviDemoFrameCount += ratio;
		if ( aviTicStart + 1 != ( int )aviDemoFrameCount ) {
			// skipped frames so write them out
			int c = aviDemoFrameCount - aviTicStart;
			while ( c-- ) {
				g_screenshotFormat = 0;
				renderSystem->TakeScreenshot( com_aviDemoWidth.GetInteger(), com_aviDemoHeight.GetInteger(), name, com_aviDemoSamples.GetInteger(), NULL );
				name = va("demos/%s/%s_%05i.tga", aviDemoShortName.c_str(), aviDemoShortName.c_str(), ++aviTicStart );
			}
		}
		aviTicStart = aviDemoFrameCount;

		// remove any printed lines at the top before taking the screenshot
		console->ClearNotifyLines();

		// this will call Draw, possibly multiple times if com_aviDemoSamples is > 1
		g_screenshotFormat = 0;
		renderSystem->TakeScreenshot( com_aviDemoWidth.GetInteger(), com_aviDemoHeight.GetInteger(), name, com_aviDemoSamples.GetInteger(), NULL );
	}

	// at startup, we may be backwards
	if ( latchedTicNumber > com_ticNumber ) {
		latchedTicNumber = com_ticNumber;
	}

	// se how many tics we should have before continuing
	int	minTic = latchedTicNumber + 1;
	if ( com_minTics.GetInteger() > 1 ) {
		minTic = lastGameTic + com_minTics.GetInteger();
	}

	// fixedTic lets us run a forced number of usercmd each frame without timing
	if ( com_fixedTic.GetInteger() ) {
		minTic = latchedTicNumber;
	}

	while( 1 ) {
		latchedTicNumber = com_ticNumber;
		if ( latchedTicNumber >= minTic ) {
			break;
		}
		D3P_ScopedCPUSample(WaitForNextFrameTime);
		Com_WaitForNextTicStart();
	}

	if ( authEmitTimeout ) {
		// waiting for a game auth
		if ( Sys_Milliseconds() > authEmitTimeout ) {
			// expired with no reply
			// means that if a firewall is blocking the master, we will let through
			common->DPrintf( "no reply from auth\n" );
			if ( authWaitBox ) {
				// close the wait box
				StopBox();
				authWaitBox = false;
			}
			if ( cdkey_state == CDKEY_CHECKING ) {
				cdkey_state = CDKEY_OK;
			}
			if ( xpkey_state == CDKEY_CHECKING ) {
				xpkey_state = CDKEY_OK;
			}
			// maintain this empty as it's set by auth denials
			authMsg.Empty();
			authEmitTimeout = 0;
			SetCDKeyGuiVars();
		}
	}

	// send frame and mouse events to active guis
	GuiFrameEvents();

	//------------ single player game tics --------------

	if ( !mapSpawned || guiActive ) {
		// early exit, won't do RunGameTic .. but still need to update mouse position for GUIs
		usercmdGen->GetDirectUsercmd();
	}

	if ( !mapSpawned ) {
		return;
	}

	if ( guiActive ) {
		lastGameTic = latchedTicNumber;
		return;
	}

#ifndef NONET
	// in message box / GUIFrame, idSessionLocal::Frame is used for GUI interactivity
	// but we early exit to avoid running game frames
	if ( idAsyncNetwork::IsActive() ) {
		return;
	}
#endif

	// check for user info changes
	if ( cvarSystem->GetModifiedFlags() & CVAR_USERINFO ) {
		mapSpawnData.userInfo[0] = *cvarSystem->MoveCVarsToDict( CVAR_USERINFO );
		game->SetUserInfo( 0, mapSpawnData.userInfo[0], false, false );
		cvarSystem->ClearModifiedFlags( CVAR_USERINFO );
	}

	// see how many usercmds we are going to run
	int	numCmdsToRun = latchedTicNumber - lastGameTic;

	// don't let a long onDemand sound load unsync everything
	if ( timeHitch ) {
		int	skip = timeHitch / USERCMD_MSEC;
		lastGameTic += skip;
		numCmdsToRun -= skip;
		timeHitch = 0;
	}

	// don't get too far behind after a hitch
	if ( numCmdsToRun > 10 ) {
		lastGameTic = latchedTicNumber - 10;
	}

	if ( com_fixedTic.GetInteger() > 0 ) {
		// this may cause commands run in a previous frame to
		// be run again if we are going at above the real time rate
		lastGameTic = latchedTicNumber - com_fixedTic.GetInteger();
	} else if (	aviCaptureMode ) {
		lastGameTic = latchedTicNumber - com_aviDemoTics.GetInteger();
	}

	// force only one game frame update this frame.  the game code requests this after skipping cinematics
	// so we come back immediately after the cinematic is done instead of a few frames later which can
	// cause sounds played right after the cinematic to not play.
	if ( syncNextGameFrame ) {
		lastGameTic = latchedTicNumber - 1;
		syncNextGameFrame = false;
	}

	// create client commands, which will be sent directly
	// to the game
	if ( com_showTics.GetBool() ) {
		common->Printf( "%i ", latchedTicNumber - lastGameTic );
	}

	int	gameTicsToRun = latchedTicNumber - lastGameTic;
	int i;
	for ( i = 0 ; i < gameTicsToRun ; i++ ) {
		RunGameTic();
		if ( !mapSpawned ) {
			// exited game play
			break;
		}
		if ( syncNextGameFrame ) {
			// long game frame, so break out and continue executing as if there was no hitch
			break;
		}
	}
}

/*
================
idSessionLocal::RunGameTic
================
*/
void idSessionLocal::RunGameTic() {
	D3P_ScopedCPUSample(Session_RunGameTic);
	usercmd_t	cmd;

	// get a locally created command
	cmd = usercmdGen->GetDirectUsercmd();
	lastGameTic++;

	// run the game logic every player move
	int	start = Sys_Milliseconds();
	gameReturn_t	ret = game->RunFrame( &cmd );

	int end = Sys_Milliseconds();
	time_gameFrame += end - start;	// note time used for com_speeds

	syncNextGameFrame = ret.syncNextGameFrame;

	if ( ret.sessionCommand[0] ) {
		idCmdArgs args;

		args.TokenizeString( ret.sessionCommand, false );

		if ( !idStr::Icmp( args.Argv(0), "map" ) ) {
			// get current player states
			for ( int i = 0 ; i < numClients ; i++ ) {
				mapSpawnData.persistentPlayerInfo[i] = game->GetPersistentPlayerInfo( i );
			}
			// clear the devmap key on serverinfo, so player spawns
			// won't get the map testing items
			mapSpawnData.serverInfo.Delete( "devmap" );

			// go to the next map
			MoveToNewMap( args.Argv(1) );
		} else if ( !idStr::Icmp( args.Argv(0), "devmap" ) ) {
			mapSpawnData.serverInfo.Set( "devmap", "1" );
			MoveToNewMap( args.Argv(1) );
		} else if ( !idStr::Icmp( args.Argv(0), "died" ) ) {
			// restart on the same map
			UnloadMap();
			SetGUI(guiRestartMenu, NULL);
		} else if ( !idStr::Icmp( args.Argv(0), "disconnect" ) ) {
			cmdSystem->BufferCommandText( CMD_EXEC_INSERT, "disconnect" );
		}
	}
}

/*
===============
idSessionLocal::Init

Called in an orderly fashion at system startup,
so commands, cvars, files, etc are all available
===============
*/
void idSessionLocal::Init() {

	common->Printf( "----- Initializing Session -----\n" );

	cmdSystem->AddCommand( "writePrecache", Sess_WritePrecache_f, CMD_FL_SYSTEM|CMD_FL_CHEAT, "writes precache commands" );

#ifndef	ID_DEDICATED
	cmdSystem->AddCommand( "map", Session_Map_f, CMD_FL_SYSTEM, "loads a map", idCmdSystem::ArgCompletion_MapName );
	cmdSystem->AddCommand( "devmap", Session_DevMap_f, CMD_FL_SYSTEM, "loads a map in developer mode", idCmdSystem::ArgCompletion_MapName );
	cmdSystem->AddCommand( "testmap", Session_TestMap_f, CMD_FL_SYSTEM, "tests a map", idCmdSystem::ArgCompletion_MapName );

	cmdSystem->AddCommand( "aviGame", Session_AVIGame_f, CMD_FL_SYSTEM, "writes AVIs for the current game" );
#endif

	cmdSystem->AddCommand( "disconnect", Session_Disconnect_f, CMD_FL_SYSTEM, "disconnects from a game" );

	cmdSystem->AddCommand( "testGUI", Session_TestGUI_f, CMD_FL_SYSTEM, "tests a gui" );

#ifndef	ID_DEDICATED
	cmdSystem->AddCommand( "saveGame", SaveGame_f, CMD_FL_SYSTEM|CMD_FL_CHEAT, "saves a game" );
	cmdSystem->AddCommand( "loadGame", LoadGame_f, CMD_FL_SYSTEM|CMD_FL_CHEAT, "loads a game", idCmdSystem::ArgCompletion_SaveGame );
#endif

	cmdSystem->AddCommand( "takeViewNotes", TakeViewNotes_f, CMD_FL_SYSTEM, "take notes about the current map from the current view" );
	cmdSystem->AddCommand( "takeViewNotes2", TakeViewNotes2_f, CMD_FL_SYSTEM, "extended take view notes" );

	cmdSystem->AddCommand( "rescanSI", Session_RescanSI_f, CMD_FL_SYSTEM, "internal - rescan serverinfo cvars and tell game" );

	cmdSystem->AddCommand( "promptKey", Session_PromptKey_f, CMD_FL_SYSTEM, "prompt and sets the CD Key" );

	cmdSystem->AddCommand( "hitch", Session_Hitch_f, CMD_FL_SYSTEM|CMD_FL_CHEAT, "hitches the game" );

	// the same idRenderWorld will be used for all games
	// and demos, insuring that level specific models
	// will be freed
	rw = renderSystem->AllocRenderWorld();
	sw = soundSystem->AllocSoundWorld( rw );

	menuSoundWorld = soundSystem->AllocSoundWorld( rw );

	// we have a single instance of the main menu
	guiMainMenu = uiManager->FindGui( "guis/mainmenu.gui", true, false, true );
	if (!guiMainMenu) {
		guiMainMenu = uiManager->FindGui( "guis/demo_mainmenu.gui", true, false, true );
		demoversion = (guiMainMenu != NULL);
	}
	guiMainMenu_MapList = uiManager->AllocListGUI();
	guiMainMenu_MapList->Config( guiMainMenu, "mapList" );
#ifndef NONET
	idAsyncNetwork::client.serverList.GUIConfig( guiMainMenu, "serverList" );
#endif
	guiRestartMenu = uiManager->FindGui( "guis/restart.gui", true, false, true );
	guiGameOver = uiManager->FindGui( "guis/gameover.gui", true, false, true );
	guiMsg = uiManager->FindGui( "guis/msg.gui", true, false, true );
	guiTakeNotes = uiManager->FindGui( "guis/takeNotes.gui", true, false, true );
	guiIntro = uiManager->FindGui( "guis/intro.gui", true, false, true );

	whiteMaterial = declManager->FindMaterial( "_white" );

	guiInGame = NULL;
	guiTest = NULL;

	guiActive = NULL;
	guiHandle = NULL;

	ReadCDKey();
}

/*
===============
idSessionLocal::GetLocalClientNum
===============
*/
int idSessionLocal::GetLocalClientNum() {
#ifndef NONET
	if ( idAsyncNetwork::client.IsActive() ) {
		return idAsyncNetwork::client.GetLocalClientNum();
	} else if ( idAsyncNetwork::server.IsActive() ) {
		if ( idAsyncNetwork::serverDedicated.GetInteger() == 0 ) {
			return 0;
		} else if ( idAsyncNetwork::server.IsClientInGame( idAsyncNetwork::serverDrawClient.GetInteger() ) ) {
			return idAsyncNetwork::serverDrawClient.GetInteger();
		} else {
			return -1;
		}
	} else {
		return 0;
	}
#else
	return 0;
#endif
}

/*
===============
idSessionLocal::SetPlayingSoundWorld
===============
*/
void idSessionLocal::SetPlayingSoundWorld() {
	if ( guiActive && ( guiActive == guiMainMenu || guiActive == guiIntro || guiActive == guiLoading || ( guiActive == guiMsg && !mapSpawned ) ) ) {
		soundSystem->SetPlayingSoundWorld( menuSoundWorld );
	} else {
		soundSystem->SetPlayingSoundWorld( sw );
	}
}

/*
===============
idSessionLocal::TimeHitch

this is used by the sound system when an OnDemand sound is loaded, so the game action
doesn't advance and get things out of sync
===============
*/
void idSessionLocal::TimeHitch( int msec ) {
	timeHitch += msec;
}

/*
=================
idSessionLocal::ReadCDKey
=================
*/
void idSessionLocal::ReadCDKey( void ) {
	idStr filename;
	idFile *f;
	char buffer[32];

	cdkey_state = CDKEY_UNKNOWN;

	filename = CDKEY_FILEPATH;
	f = fileSystem->OpenExplicitFileRead( fileSystem->RelativePathToOSPath( filename, "fs_configpath" ) );

	// try the install path, which is where the cd installer and steam put it
	if ( !f )
		f = fileSystem->OpenExplicitFileRead( fileSystem->RelativePathToOSPath( filename, "fs_basepath" ) );

	if ( !f ) {
		common->Printf( "Couldn't read %s.\n", filename.c_str() );
		cdkey[ 0 ] = '\0';
	} else {
		memset( buffer, 0, sizeof(buffer) );
		f->Read( buffer, CDKEY_BUF_LEN - 1 );
		fileSystem->CloseFile( f );
		idStr::Copynz( cdkey, buffer, CDKEY_BUF_LEN );
	}

	xpkey_state = CDKEY_UNKNOWN;

	filename = XPKEY_FILEPATH;
	f = fileSystem->OpenExplicitFileRead( fileSystem->RelativePathToOSPath( filename, "fs_configpath" ) );

	// try the install path, which is where the cd installer and steam put it
	if ( !f )
		f = fileSystem->OpenExplicitFileRead( fileSystem->RelativePathToOSPath( filename, "fs_basepath" ) );

	if ( !f ) {
		common->Printf( "Couldn't read %s.\n", filename.c_str() );
		xpkey[ 0 ] = '\0';
	} else {
		memset( buffer, 0, sizeof(buffer) );
		f->Read( buffer, CDKEY_BUF_LEN - 1 );
		fileSystem->CloseFile( f );
		idStr::Copynz( xpkey, buffer, CDKEY_BUF_LEN );
	}
}

/*
================
idSessionLocal::WriteCDKey
================
*/
void idSessionLocal::WriteCDKey( void ) {
	idStr filename;
	idFile *f;
	const char *OSPath;

	filename = CDKEY_FILEPATH;
	// OpenFileWrite advertises creating directories to the path if needed, but that won't work with a '..' in the path
	// occasionally on windows, but mostly on Linux and OSX, the fs_configpath/base may not exist in full
	OSPath = fileSystem->BuildOSPath( cvarSystem->GetCVarString( "fs_configpath" ), BASE_GAMEDIR, CDKEY_FILE );
	fileSystem->CreateOSPath( OSPath );
	f = fileSystem->OpenFileWrite( filename, "fs_configpath" );
	if ( !f ) {
		common->Printf( "Couldn't write %s.\n", filename.c_str() );
		return;
	}
	f->Printf( "%s%s", cdkey, CDKEY_TEXT );
	fileSystem->CloseFile( f );

	filename = XPKEY_FILEPATH;
	f = fileSystem->OpenFileWrite( filename, "fs_configpath" );
	if ( !f ) {
		common->Printf( "Couldn't write %s.\n", filename.c_str() );
		return;
	}
	f->Printf( "%s%s", xpkey, CDKEY_TEXT );
	fileSystem->CloseFile( f );
}

/*
===============
idSessionLocal::ClearKey
===============
*/
void idSessionLocal::ClearCDKey( bool valid[ 2 ] ) {
	if ( !valid[ 0 ] ) {
		memset( cdkey, 0, CDKEY_BUF_LEN );
		cdkey_state = CDKEY_UNKNOWN;
	} else if ( cdkey_state == CDKEY_CHECKING ) {
		// if a key was in checking and not explicitely asked for clearing, put it back to ok
		cdkey_state = CDKEY_OK;
	}
	if ( !valid[ 1 ] ) {
		memset( xpkey, 0, CDKEY_BUF_LEN );
		xpkey_state = CDKEY_UNKNOWN;
	} else if ( xpkey_state == CDKEY_CHECKING ) {
		xpkey_state = CDKEY_OK;
	}
	WriteCDKey( );
}

/*
================
idSessionLocal::GetCDKey
================
*/
const char *idSessionLocal::GetCDKey( bool xp ) {
	if ( !xp ) {
		return cdkey;
	}
	if ( xpkey_state == CDKEY_OK || xpkey_state == CDKEY_CHECKING ) {
		return xpkey;
	}
	return NULL;
}

// digits to letters table
#define CDKEY_DIGITS "TWSBJCGD7PA23RLH"

/*
===============
idSessionLocal::EmitGameAuth
we toggled some key state to CDKEY_CHECKING. send a standalone auth packet to validate
===============
*/
void idSessionLocal::EmitGameAuth( void ) {
	// make sure the auth reply is empty, we use it to indicate an auth reply
	authMsg.Empty();
#ifndef NONET
	if ( idAsyncNetwork::client.SendAuthCheck( cdkey_state == CDKEY_CHECKING ? cdkey : NULL, xpkey_state == CDKEY_CHECKING ? xpkey : NULL ) ) {
		authEmitTimeout = Sys_Milliseconds() + CDKEY_AUTH_TIMEOUT;
		common->DPrintf( "authing with the master..\n" );
	} else {
#endif
		// net is not available
		common->DPrintf( "sendAuthCheck failed\n" );
		if ( cdkey_state == CDKEY_CHECKING ) {
			cdkey_state = CDKEY_OK;
		}
		if ( xpkey_state == CDKEY_CHECKING ) {
			xpkey_state = CDKEY_OK;
		}
#ifndef NONET
	}
#endif
}

/*
================
idSessionLocal::CheckKey
the function will only modify keys to _OK or _CHECKING if the offline checks are passed
if the function returns false, the offline checks failed, and offline_valid holds which keys are bad
================
*/
bool idSessionLocal::CheckKey( const char *key, bool netConnect, bool offline_valid[ 2 ] ) {
	char lkey[ 2 ][ CDKEY_BUF_LEN ];
	char l_chk[ 2 ][ 3 ];
	char s_chk[ 3 ];
	int imax,i_key;
	unsigned int checksum, chk8;
	bool edited_key[ 2 ];

	// make sure have a right input string
	assert( strlen( key ) == ( CDKEY_BUF_LEN - 1 ) * 2 + 4 + 3 + 4 );

	edited_key[ 0 ] = ( key[0] == '1' );
	idStr::Copynz( lkey[0], key + 2, CDKEY_BUF_LEN );
	idStr::ToUpper( lkey[0] );
	idStr::Copynz( l_chk[0], key + CDKEY_BUF_LEN + 2, 3 );
	idStr::ToUpper( l_chk[0] );
	edited_key[ 1 ] = ( key[ CDKEY_BUF_LEN + 2 + 3 ] == '1' );
	idStr::Copynz( lkey[1], key + CDKEY_BUF_LEN + 7, CDKEY_BUF_LEN );
	idStr::ToUpper( lkey[1] );
	idStr::Copynz( l_chk[1], key + CDKEY_BUF_LEN * 2 + 7, 3 );
	idStr::ToUpper( l_chk[1] );

	if ( fileSystem->HasD3XP() ) {
		imax = 2;
	} else {
		imax = 1;
	}
	offline_valid[ 0 ] = offline_valid[ 1 ] = true;
	for( i_key = 0; i_key < imax; i_key++ ) {
		// check that the characters are from the valid set
		int i;
		for ( i = 0; i < CDKEY_BUF_LEN - 1; i++ ) {
			if ( !strchr( CDKEY_DIGITS, lkey[i_key][i] ) ) {
				offline_valid[ i_key ] = false;
				continue;
			}
		}

		if ( edited_key[ i_key ] ) {
			// verify the checksum for edited keys only
			checksum = CRC32_BlockChecksum( lkey[i_key], CDKEY_BUF_LEN - 1 );
			chk8 = ( checksum & 0xff ) ^ ( ( ( checksum & 0xff00 ) >> 8 ) ^ ( ( ( checksum & 0xff0000 ) >> 16 ) ^ ( ( checksum & 0xff000000 ) >> 24 ) ) );
			idStr::snPrintf( s_chk, 3, "%02X", chk8 );
			if ( idStr::Icmp( l_chk[i_key], s_chk ) != 0 ) {
				offline_valid[ i_key ] = false;
				continue;
			}
		}
	}

	if ( !offline_valid[ 0 ] || !offline_valid[1] ) {
		return false;
	}

	// offline checks passed, we'll return true and optionally emit key check requests
	// the function should only modify the key states if the offline checks passed successfully

	// set the keys, don't send a game auth if we are net connecting
	idStr::Copynz( cdkey, lkey[0], CDKEY_BUF_LEN );
	netConnect ? cdkey_state = CDKEY_OK : cdkey_state = CDKEY_CHECKING;
	if ( fileSystem->HasD3XP() ) {
		idStr::Copynz( xpkey, lkey[1], CDKEY_BUF_LEN );
		netConnect ? xpkey_state = CDKEY_OK : xpkey_state = CDKEY_CHECKING;
	} else {
		xpkey_state = CDKEY_NA;
	}
	if ( !netConnect ) {
		EmitGameAuth();
	}
	SetCDKeyGuiVars();

	return true;
}

/*
===============
idSessionLocal::CDKeysAreValid
checking that the key is present and uses only valid characters
if d3xp is installed, check for a valid xpkey as well
emit an auth packet to the master if possible and needed
===============
*/
bool idSessionLocal::CDKeysAreValid( bool strict ) {
	int i;
	bool emitAuth = false;

	if ( cdkey_state == CDKEY_UNKNOWN ) {
		if ( strlen( cdkey ) != CDKEY_BUF_LEN - 1 ) {
			cdkey_state = CDKEY_INVALID;
		} else {
			for ( i = 0; i < CDKEY_BUF_LEN-1; i++ ) {
				if ( !strchr( CDKEY_DIGITS, cdkey[i] ) ) {
					cdkey_state = CDKEY_INVALID;
					break;
				}
			}
		}
		if ( cdkey_state == CDKEY_UNKNOWN ) {
			cdkey_state = CDKEY_CHECKING;
			emitAuth = true;
		}
	}
	if ( xpkey_state == CDKEY_UNKNOWN ) {
		if ( fileSystem->HasD3XP() ) {
			if ( strlen( xpkey ) != CDKEY_BUF_LEN -1 ) {
				xpkey_state = CDKEY_INVALID;
			} else {
				for ( i = 0; i < CDKEY_BUF_LEN-1; i++ ) {
					if ( !strchr( CDKEY_DIGITS, xpkey[i] ) ) {
						xpkey_state = CDKEY_INVALID;
					}
				}
			}
			if ( xpkey_state == CDKEY_UNKNOWN ) {
				xpkey_state = CDKEY_CHECKING;
				emitAuth = true;
			}
		} else {
			xpkey_state = CDKEY_NA;
		}
	}
	if ( emitAuth ) {
		EmitGameAuth();
	}
	// make sure to keep the mainmenu gui up to date in case we made state changes
	SetCDKeyGuiVars();
	if ( strict ) {
		return cdkey_state == CDKEY_OK && ( xpkey_state == CDKEY_OK || xpkey_state == CDKEY_NA );
	} else {
		return ( cdkey_state == CDKEY_OK || cdkey_state == CDKEY_CHECKING ) && ( xpkey_state == CDKEY_OK || xpkey_state == CDKEY_CHECKING || xpkey_state == CDKEY_NA );
	}
}

/*
===============
idSessionLocal::WaitingForGameAuth
===============
*/
bool idSessionLocal::WaitingForGameAuth( void ) {
	return authEmitTimeout != 0;
}

/*
===============
idSessionLocal::CDKeysAuthReply
===============
*/
void idSessionLocal::CDKeysAuthReply( bool valid, const char *auth_msg ) {
	//assert( authEmitTimeout > 0 );
	if ( authWaitBox ) {
		// close the wait box
		StopBox();
		authWaitBox = false;
	}
	if ( !valid ) {
		common->DPrintf( "auth key is invalid\n" );
		authMsg = auth_msg;
		if ( cdkey_state == CDKEY_CHECKING ) {
			cdkey_state = CDKEY_INVALID;
		}
		if ( xpkey_state == CDKEY_CHECKING ) {
			xpkey_state = CDKEY_INVALID;
		}
	} else {
		common->DPrintf( "client is authed in\n" );
		if ( cdkey_state == CDKEY_CHECKING ) {
			cdkey_state = CDKEY_OK;
		}
		if ( xpkey_state == CDKEY_CHECKING ) {
			xpkey_state = CDKEY_OK;
		}
	}
	authEmitTimeout = 0;
	SetCDKeyGuiVars();
}

/*
===============
idSessionLocal::GetCurrentMapName
===============
*/
const char *idSessionLocal::GetCurrentMapName() {
	return currentMapName.c_str();
}

/*
===============
idSessionLocal::GetSaveGameVersion
===============
*/
int idSessionLocal::GetSaveGameVersion( void ) {
	return savegameVersion;
}

/*
===============
idSessionLocal::GetAuthMsg
===============
*/
const char *idSessionLocal::GetAuthMsg( void ) {
	return authMsg.c_str();
}
