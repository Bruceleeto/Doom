/*
 * snd_stub.cpp — No-op sound system for NOAUDIO builds.
 * Provides stub implementations of idSoundSystem, idSoundWorld,
 * idSoundEmitter, and idSoundShader so the engine compiles and
 * runs without OpenAL or any audio processing.
 */

#include "sys/platform.h"
#include "framework/Common.h"
#include "framework/CmdSystem.h"
#include "framework/CVarSystem.h"
#include "framework/FileSystem.h"
#include "framework/DeclManager.h"
#include "sound/sound.h"

// =========================================================================
// Stub idSoundEmitter
// =========================================================================

class idSoundEmitterStub : public idSoundEmitter {
public:
	int idx;
	idSoundEmitterStub( void ) : idx(0) {}
	virtual void	Free( bool immediate ) {}
	virtual void	UpdateEmitter( const idVec3 &origin, int listenerId, const soundShaderParms_t *parms ) {}
	virtual int		StartSound( const idSoundShader *shader, const s_channelType channel, float diversity = 0, int shaderFlags = 0, bool allowSlow = true ) { return 0; }
	virtual void	ModifySound( const s_channelType channel, const soundShaderParms_t *parms ) {}
	virtual void	StopSound( const s_channelType channel ) {}
	virtual void	FadeSound( const s_channelType channel, float to, float over ) {}
	virtual bool	CurrentlyPlaying( void ) const { return false; }
	virtual float	CurrentAmplitude( void ) { return 0.0f; }
	virtual int		Index( void ) const { return idx; }
};

// =========================================================================
// Stub idSoundWorld
// =========================================================================

class idSoundWorldStub : public idSoundWorld {
public:
	idList<idSoundEmitterStub *> emitters;

	idSoundWorldStub( void ) {}
	virtual ~idSoundWorldStub( void ) {
		for ( int i = 0; i < emitters.Num(); i++ ) {
			delete emitters[i];
		}
		emitters.Clear();
	}

	virtual void			ClearAllSoundEmitters( void ) {
		for ( int i = 0; i < emitters.Num(); i++ ) {
			delete emitters[i];
		}
		emitters.Clear();
	}
	virtual void			StopAllSounds( void ) {}
	virtual idSoundEmitter *AllocSoundEmitter( void ) {
		idSoundEmitterStub *e = new idSoundEmitterStub();
		e->idx = emitters.Append( e ) + 1;
		return e;
	}
	virtual idSoundEmitter *EmitterForIndex( int index ) {
		if ( index <= 0 || index > emitters.Num() ) {
			return NULL;
		}
		return emitters[ index - 1 ];
	}
	virtual float			CurrentShakeAmplitudeForPosition( const int time, const idVec3 &listenerPosition ) { return 0.0f; }
	virtual void			PlaceListener( const idVec3 &origin, const idMat3 &axis, const int listenerId, const int gameTime, const idStr &areaName ) {}
	virtual void			FadeSoundClasses( const int soundClass, const float to, const float over ) {}
	virtual void			PlayShaderDirectly( const char *name, int channel = -1 ) {}
	virtual void			StartWritingDemo( idDemoFile *demo ) {}
	virtual void			StopWritingDemo( void ) {}
	virtual void			ProcessDemoCommand( idDemoFile *demo ) {}
	virtual void			Pause( void ) {}
	virtual void			UnPause( void ) {}
	virtual bool			IsPaused( void ) { return false; }
	virtual void			AVIOpen( const char *path, const char *name ) {}
	virtual void			AVIClose( void ) {}
	virtual void			WriteToSaveGame( idFile *savefile ) {}
	virtual void			ReadFromSaveGame( idFile *savefile ) {}
	virtual void			SetSlowmo( bool active ) {}
	virtual void			SetSlowmoSpeed( float speed ) {}
	virtual void			SetEnviroSuit( bool active ) {}
};

// =========================================================================
// Stub idSoundSystem
// =========================================================================

class idSoundSystemStub : public idSoundSystem {
public:
	virtual void			Init( void ) { common->Printf( "Sound disabled (NOAUDIO)\n" ); }
	virtual void			Shutdown( void ) {}
	virtual bool			InitHW( void ) { return false; }
	virtual bool			ShutdownHW( void ) { return false; }
	virtual int				AsyncUpdate( int time ) { return 0; }
	virtual int				AsyncUpdateWrite( int time ) { return 0; }
	virtual void			SetMute( bool mute ) {}
	virtual cinData_t		ImageForTime( const int milliseconds, const bool waveform ) { cinData_t d = {}; return d; }
	virtual int				GetSoundDecoderInfo( int index, soundDecoderInfo_t &decoderInfo ) { return -1; }
	virtual idSoundWorld *	AllocSoundWorld( idRenderWorld *rw ) { return new idSoundWorldStub(); }
	virtual void			SetPlayingSoundWorld( idSoundWorld *soundWorld ) {}
	virtual idSoundWorld *	GetPlayingSoundWorld( void ) { return NULL; }
	virtual void			BeginLevelLoad( void ) {}
	virtual void			EndLevelLoad( const char *mapString ) { common->Printf( "sound: no audio loaded (NOAUDIO)\n" ); }
	virtual int				AsyncMix( int soundTime, float *mixBuffer ) { return 0; }
	virtual void			PrintMemInfo( MemInfo_t *mi ) {}
	virtual int				IsEFXAvailable( void ) { return -1; }
};

static idSoundSystemStub soundSystemStub;
idSoundSystem *soundSystem = &soundSystemStub;

// =========================================================================
// idSoundShader — must still work as a decl, just skip sample loading
// =========================================================================

void idSoundShader::Init( void ) {
	desc = "<no description>";
	errorDuringParse = false;
	onDemand = false;
	numEntries = 0;
	numLeadins = 0;
	leadinVolume = 0;
	altSound = NULL;
}

idSoundShader::idSoundShader( void ) { Init(); }
idSoundShader::~idSoundShader( void ) {}

size_t idSoundShader::Size( void ) const { return sizeof( idSoundShader ); }

void idSoundShader::FreeData( void ) {
	numEntries = 0;
	numLeadins = 0;
}

bool idSoundShader::SetDefaultText( void ) {
	idStr wavname = GetName();
	wavname.DefaultFileExtension( ".wav" );
	char generated[2048];
	idStr::snPrintf( generated, sizeof( generated ),
		"sound %s // IMPLICITLY GENERATED\n{\n%s\n}\n", GetName(), wavname.c_str() );
	SetText( generated );
	return true;
}

const char *idSoundShader::DefaultDefinition( void ) const {
	return "{\n\t_default.wav\n}";
}

bool idSoundShader::Parse( const char *text, const int textLength ) {
	idLexer src;
	src.LoadMemory( text, textLength, GetFileName(), GetLineNum() );
	src.SetFlags( DECL_LEXER_FLAGS );
	src.SkipUntilString( "{" );

	errorDuringParse = false;
	if ( !ParseShader( src ) || errorDuringParse ) {
		MakeDefault();
		return false;
	}
	return true;
}

bool idSoundShader::ParseShader( idLexer &src ) {
	idToken token;

	parms.minDistance = 1;
	parms.maxDistance = 10;
	parms.volume = 1;
	parms.shakes = 0;
	parms.soundShaderFlags = 0;
	parms.soundClass = 0;
	speakerMask = 0;
	altSound = NULL;

	for ( int i = 0; i < SOUND_MAX_LIST_WAVS; i++ ) {
		leadins[i] = NULL;
		entries[i] = NULL;
	}
	numEntries = 0;
	numLeadins = 0;

	while ( 1 ) {
		if ( !src.ExpectAnyToken( &token ) ) {
			return false;
		}
		if ( token == "}" ) {
			break;
		}
		// parse all the keywords but skip sample loading
		else if ( !token.Icmp( "description" ) )	{ src.ReadTokenOnLine( &token ); desc = token.c_str(); }
		else if ( !token.Icmp( "mindistance" ) )	{ parms.minDistance = src.ParseFloat(); }
		else if ( !token.Icmp( "maxdistance" ) )	{ parms.maxDistance = src.ParseFloat(); }
		else if ( !token.Icmp( "volume" ) )			{ parms.volume = src.ParseFloat(); }
		else if ( !token.Icmp( "leadinVolume" ) )	{ leadinVolume = src.ParseFloat(); }
		else if ( !token.Icmp( "minSamples" ) )		{ src.ParseInt(); }
		else if ( !token.Icmp( "shakes" ) ) {
			src.ExpectAnyToken( &token );
			if ( token.type == TT_NUMBER ) { parms.shakes = token.GetFloatValue(); }
			else { src.UnreadToken( &token ); parms.shakes = 1.0f; }
		}
		else if ( !token.Icmp( "reverb" ) )			{ src.ParseFloat(); src.ExpectTokenString( "," ); src.ParseFloat(); }
		else if ( !token.Icmp( "soundClass" ) ) {
			parms.soundClass = src.ParseInt();
			if ( parms.soundClass < 0 || parms.soundClass >= SOUND_MAX_CLASSES ) {
				src.Warning( "SoundClass out of range" );
				return false;
			}
		}
		else if ( !token.Icmp( "altSound" ) )		{ src.ExpectAnyToken( &token ); altSound = declManager->FindSound( token.c_str() ); }
		else if ( !token.Icmp( "ordered" ) )		{}
		else if ( !token.Icmp( "no_dups" ) )		{ parms.soundShaderFlags |= SSF_NO_DUPS; }
		else if ( !token.Icmp( "no_flicker" ) )		{ parms.soundShaderFlags |= SSF_NO_FLICKER; }
		else if ( !token.Icmp( "plain" ) )			{}
		else if ( !token.Icmp( "looping" ) )		{ parms.soundShaderFlags |= SSF_LOOPING; }
		else if ( !token.Icmp( "no_occlusion" ) )	{ parms.soundShaderFlags |= SSF_NO_OCCLUSION; }
		else if ( !token.Icmp( "private" ) )		{ parms.soundShaderFlags |= SSF_PRIVATE_SOUND; }
		else if ( !token.Icmp( "antiPrivate" ) )	{ parms.soundShaderFlags |= SSF_ANTI_PRIVATE_SOUND; }
		else if ( !token.Icmp( "playonce" ) )		{ parms.soundShaderFlags |= SSF_PLAY_ONCE; }
		else if ( !token.Icmp( "global" ) )			{ parms.soundShaderFlags |= SSF_GLOBAL; }
		else if ( !token.Icmp( "unclamped" ) )		{ parms.soundShaderFlags |= SSF_UNCLAMPED; }
		else if ( !token.Icmp( "omnidirectional" ) ){ parms.soundShaderFlags |= SSF_OMNIDIRECTIONAL; }
		else if ( !token.Icmp( "onDemand" ) )		{}
		else if ( !token.Icmp( "mask_center" ) )	{ speakerMask |= 1<<SPEAKER_CENTER; }
		else if ( !token.Icmp( "mask_left" ) )		{ speakerMask |= 1<<SPEAKER_LEFT; }
		else if ( !token.Icmp( "mask_right" ) )		{ speakerMask |= 1<<SPEAKER_RIGHT; }
		else if ( !token.Icmp( "mask_backright" ) )	{ speakerMask |= 1<<SPEAKER_BACKRIGHT; }
		else if ( !token.Icmp( "mask_backleft" ) )	{ speakerMask |= 1<<SPEAKER_BACKLEFT; }
		else if ( !token.Icmp( "mask_lfe" ) )		{ speakerMask |= 1<<SPEAKER_LFE; }
		else if ( !token.Icmp( "leadin" ) )			{ src.ReadToken( &token ); /* skip sample */ }
		else if ( token.Find( ".wav", false ) != -1 || token.Find( ".ogg", false ) != -1 ) {
			/* skip sample — don't load anything */
		}
		else {
			src.Warning( "unknown token '%s'", token.c_str() );
			return false;
		}
	}

	return true;
}

bool idSoundShader::CheckShakesAndOgg( void ) const { return false; }

void idSoundShader::List( void ) const {
	common->Printf( "%4i: %s\n", Index(), GetName() );
}

const idSoundShader *idSoundShader::GetAltSound( void ) const { return altSound; }
float idSoundShader::GetMinDistance( void ) const { return parms.minDistance; }
float idSoundShader::GetMaxDistance( void ) const { return parms.maxDistance; }
const char *idSoundShader::GetDescription( void ) const { return desc; }
bool idSoundShader::HasDefaultSound( void ) const { return false; }
const soundShaderParms_t *idSoundShader::GetParms( void ) const { return &parms; }
int idSoundShader::GetNumSounds( void ) const { return 0; }
const char *idSoundShader::GetSound( int index ) const { return ""; }
