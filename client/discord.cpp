#include "build.h"

// a1ba: keep it win32 only for now
#if XASH_64BIT && XASH_WIN32

// OpenAG
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_set>
#include <time.h>

#include <discord-rpc/include/discord_rpc.h>

#include "hud.h"
#include "r_local.h"
#include "r_world.h"
#include "utils.h"

#include "discord.h"

using namespace std::literals;

namespace discord_integration
{
	namespace
	{
		// From Discord developer dashboard.
		constexpr const char CLIENT_ID[] = "1224041387654385695";

		// This seems to be consistent across PCs.
		constexpr const char STEAM_APP_ID[] = "10";

		// For tracking if we're in-game.
		bool updated_client_data = false;

		// Possible game states.
		enum class game_state
		{
			NOT_PLAYING = 0,
			PLAYING,
			SPECTATING,
		};

		// Class that handles tracking state changes.
		class DiscordState {
		public:
			DiscordState()
				: cur_state( game_state::NOT_PLAYING )
				, gamemode()
				, player_count( 0 )
				, player_limit( 0 )
				, dirty( true )
			{
				update_presence();
			};

			inline void set_game_state( game_state new_game_state )
			{
				if( cur_state != new_game_state )
				{
					cur_state = new_game_state;

					if( new_game_state == game_state::NOT_PLAYING )
					{
						gamemode.clear();
					}
					else
					{
						refresh_player_stats();
					}

					dirty = true;
				}
			}

			inline game_state get_game_state() const
			{
				return cur_state;
			}

			inline void set_gamemode( std::string new_gamemode )
			{
				if( gamemode != new_gamemode )
				{
					gamemode = std::move( new_gamemode );
					dirty = true;
				}
			}

			inline void set_player_count( int new_player_count )
			{
				if( player_count != new_player_count )
				{
					player_count = new_player_count;
					dirty = true;
				}
			}

			inline void set_player_limit( int new_player_limit )
			{
				if( player_limit != new_player_limit )
				{
					player_limit = new_player_limit;
					dirty = true;
				}
			}

			inline void refresh_player_stats()
			{
				set_player_count( get_player_count() );
				set_player_limit( gEngfuncs.GetMaxClients() );
			}


			inline void update_presence_if_dirty()
			{
				if( dirty )
					update_presence();
			}

		protected:
			void update_presence()
			{
				dirty = false;

				DiscordRichPresence presence{};

				presence.largeImageKey = "gameicon";
				presence.smallImageKey = "gameicon";
				presence.largeImageText = "https://moddb.com/mods/diffusion";
				presence.smallImageText = "";

				static int64_t gametime = time( 0 );
				presence.startTimestamp = gametime;

				if( cur_state != game_state::NOT_PLAYING )
				{
					char map[256];
					map[0] = '\0';
					char ServerName[256];
					char *szServerName;
					if( gEngfuncs.GetMaxClients() <= 1 )
					{
						ServerName[0] = '\0';
						szServerName = gHUD.m_szServerName;
					}
					else
					{
						Q_sprintf( ServerName, "Server: " );
						szServerName = gHUD.m_szServerName;
						size_t map_len = strlen( map );
						get_map_name( map + map_len, ARRAYSIZED( map ) - map_len );
					}

					Q_strncat( ServerName, szServerName, sizeof( ServerName ));
					remove_color_characters( ServerName );
					presence.details = ServerName;
					presence.state = map;
					if( gEngfuncs.GetMaxClients() <= 1 )
					{
						presence.partySize = 0;
						presence.partyMax = 0;
					}
					else
					{
						presence.partySize = player_count;
						presence.partyMax = player_limit;
					}
				}
				else
					presence.state = "In Menus";

				Discord_UpdatePresence( &presence );
			}

			game_state cur_state;
			std::string gamemode;
			int player_count;
			int player_limit;

			// Flag indicating there are some changes not sent to Discord yet.
			bool dirty;
		};

		// Pointer so the constructor doesn't run too early.
		std::unique_ptr<DiscordState> discord_state;

		// Time of the last update.
		float last_update_time;

		void handle_ready( const DiscordUser * )
		{
			gEngfuncs.Con_Printf( "Connected to Discord.\n" );
		}

		void handle_errored( int error_code, const char *message )
		{
			gEngfuncs.Con_Printf( "Discord error (%d): %s\n", error_code, message );
		}

		void handle_disconnected( int error_code, const char *message )
		{
			gEngfuncs.Con_Printf( "Disconnected from Discord (%d): %s\n", error_code, message );
		}
	}

	void initialize()
	{
		DiscordEventHandlers handlers{};

		Discord_Initialize( CLIENT_ID, &handlers, 1, STEAM_APP_ID );

		discord_state = std::make_unique<DiscordState>();

		Discord_RunCallbacks();

		last_update_time = gEngfuncs.GetClientTime();
	}

	void shutdown()
	{
		discord_state.reset();
		Discord_Shutdown();
	}

	void set_spectating( bool spectating )
	{
		if( spectating )
			discord_state->set_game_state( game_state::SPECTATING );
		else
			discord_state->set_game_state( game_state::PLAYING );
	}


	void set_gamemode( std::string gamemode )
	{
		discord_state->set_gamemode( std::move( gamemode ) );
	}

	void on_update_client_data()
	{
		updated_client_data = true;
	}

	void on_frame()
	{
		if( !updated_client_data )
			discord_state->set_game_state( game_state::NOT_PLAYING );
		else if( discord_state->get_game_state() == game_state::NOT_PLAYING )
			// Only set this if we weren't playing, otherwise we might overwrite some other state.
			discord_state->set_game_state( game_state::PLAYING );

		updated_client_data = false;

		// Check player counts for updates every now and then.
		const auto current_time = gEngfuncs.GetClientTime();
		if( last_update_time < current_time || last_update_time - current_time > 5 )
		{
			last_update_time = current_time;
			on_player_count_update();
			discord_state->update_presence_if_dirty();
		}

		Discord_RunCallbacks();
	}

	void on_player_count_update()
	{
		discord_state->refresh_player_stats();
	}
}

#endif // 64-bit only
