

void S_Init( void );
void S_Shutdown( void );

// if origin is NULL, the sound will be dynamically sourced from the entity
void S_MuteSound(int entityNum, int entchannel);
void S_StartSound( const vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx );
void S_StartLocalSound( sfxHandle_t sfx, int channelNum );

void S_RestartMusic( void );
void S_StartBackgroundTrack( const char *intro, const char *loop, qboolean bReturnWithoutStarting );
void S_StopBackgroundTrack( void );

// cinematics and voice-over-network will send raw samples
// 1.0 volume will be direct output of source samples
void S_RawSamples (int samples, int rate, int width, int channels,
				   const byte *data, float volume);

// stop all sounds and the background track
void S_StopAllSounds( void );

// all continuous looping sounds must be added before calling S_Update
void S_ClearLoopingSounds( qboolean killall );
void S_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
void S_AddRealLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
void S_StopLoopingSound(int entityNum );

// recompute the reletive volumes for all running sounds
// reletive to the given entityNum / orientation
void S_Respatialize( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater );

// let the sound system know where an entity currently is
void S_UpdateEntityPosition( int entityNum, const vec3_t origin );

void S_Update( void );

void S_DisableSounds( void );

void S_BeginRegistration( void );

// RegisterSound will allways return a valid sample, even if it
// has to create a placeholder.  This prevents continuous filesystem
// checks for missing files
sfxHandle_t	S_RegisterSound( const char *name );

void S_DisplayFreeMemory(void);

void S_ClearSoundBuffer( void );

void S_Activate(qboolean activate);
