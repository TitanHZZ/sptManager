/*
 * This file belongs to sptManager.
 *
 * This file only contains source code built for the linux platform...
 */

// basic platform check for no error reports on windows
#ifdef __linux__

// #include <iostream>
#include <unistd.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include "playerctl/playerctl.h"
#include "wmctrl/wmctrl.h"

// the main loop for the follow command
static GMainLoop *main_loop = NULL;
// the manager of all the players we connect to
static PlayerctlPlayerManager *manager = NULL;
// json config file
static nlohmann::json config_j = NULL;
// keep track of when to start spotify music
static gboolean play_spt = FALSE;

static void exit_func(GList **available_players, PlayerctlPlayerManager **manager) {
    // clean the available players list and players manager
    if (*available_players != NULL) g_list_free(*available_players);
    if (*manager != NULL)           g_object_unref(*manager);
}

static void manage_ad() {
    // 'c' is to close spotify on the first call coming from the metadata callback
    // 'i' is an invalid option (will not do anything to the spotify window) but without it,
    // spt will not start playing after the "managed_player_execute_command(player, "play");" call in player_appeared_callback
    spt_window_action(!play_spt ? 'c' : 'i');
    sleep((int) config_j["timings"]["timeToWaitAfterClosingSpotify"] / 1000);

    // create a new child process
    // NOTE: spotify will not close if the main process closes unless we send a sigterm to the main process
    if (!play_spt) // needed for the second call (from player_vanished callback)
    {
        if (fork() == 0)
        {
            // close child process after spotify terminates
            const std::string sptExecuteCmd = static_cast<std::string>(config_j["generalConfiguration"]["SpotifyInstallationDir"]) + "spotify 2&> temp.log";
            exit(system(sptExecuteCmd.c_str()));
        }

        play_spt = TRUE;
    }
}

static gboolean playercmd_play(PlayerctlPlayer *player) {
    GError *error = NULL;
    gboolean can_play = FALSE;

    g_object_get(player, "can-play", &can_play, NULL);
    if (!can_play) {
        return FALSE;
    }

    playerctl_player_play(player, &error);
    if (error) {
        return FALSE;
    }

    return TRUE;
}

static gboolean playercmd_metadata(PlayerctlPlayer *player) {
    GError *error = NULL;
    gboolean can_play = FALSE;

    g_object_get(player, "can-play", &can_play, NULL);
    if (!can_play) {
        return FALSE;
    }

    // get only the title from the metadata
    gchar *data = playerctl_player_get_title(player, &error);
    if (error) {
        return FALSE;
    }

    if (data != NULL) {
        // std::cout << data << std::endl;
        if (!g_strcmp0(data, "Advertisement"))
        {
            manage_ad();
        } else if (!g_strcmp0(data, "")) {
            // get the playback status (aka 'Playing', 'Paused', ...) of spotify
            PlayerctlPlaybackStatus status = (PlayerctlPlaybackStatus) 0;
            g_object_get(player, "playback-status", &status, NULL);

            // if status is '0', spotify is playing
            if (!status)
            {
                manage_ad();
            }
        }

        g_free(data);
        return TRUE;
    }

    return FALSE;
}

static void managed_player_execute_command(PlayerctlPlayer *player, const gchar *player_cmd = (gchar *) "metadata") {
    if (player == NULL) return;

    // basic command checker
    switch (g_strcmp0(player_cmd, "metadata")) {
        // command is 'metadata'
        case 0:
            playercmd_metadata(player);
            break;
        // command is 'play'
        case 3:
            playercmd_play(player);
            break;
        default:
            break;
    }
}

static void managed_player_updates_properties_callback(PlayerctlPlayer *player, gpointer data) {
    // this function is called when a player updates its properties
    playerctl_player_manager_move_player_to_top(manager, player);
    managed_player_execute_command(player);
}

static void name_appeared_callback(PlayerctlPlayerManager *manager, PlayerctlPlayerName *name, gpointer *data) {
    // this fucntion is called when a new name (aka spotify of any other appears) and creates a player struct for the actual player app
    // make sure we only create a player manager for spotify
    if (g_strcmp0(name->name, "spotify") != 0) return;

    GError *error = NULL;
    PlayerctlPlayer *player = playerctl_player_new_from_name(name, &error);
    if (error != NULL) {
        g_clear_error(&error);
        g_main_loop_quit(main_loop);
        return;
    }

    playerctl_player_manager_manage_player(manager, player);
    g_object_unref(player);
}

static void player_appeared_callback(PlayerctlPlayerManager *manager, PlayerctlPlayer *player, gpointer *data) {
    // this function is called when a new player is created for a new name that appeared
    // init managed player
    g_signal_connect(G_OBJECT(player), (gchar *) "metadata", G_CALLBACK(managed_player_updates_properties_callback), NULL);

    if (play_spt)
    {
        // an AD was detected, so now play spotify again
        play_spt = FALSE;
        sleep((int) config_j["timings"]["timeToWaitAfterSpotifySpotifyOpened"] / 1000);

        if (config_j["generalConfiguration"]["maximizeSpotify"])
        {
            spt_window_action('M'); // maximize spt
        } else if (config_j["generalConfiguration"]["minimizeSpotify"]) {
            spt_window_action('m'); // minimize spt
        }

        sleep((int) config_j["timings"]["timeToWaitBeforePlayingTheMediaInSpotify"] / 1000);
        managed_player_execute_command(player, "play"); //play spt
    }
}

static void player_vanished_callback(PlayerctlPlayerManager *manager, PlayerctlPlayer *player, gpointer *data) {
    GError *error = NULL;

    // for some reason this funcion call is needed
    // or spt will not start playing after the "managed_player_execute_command(player, "play");" call in player_appeared_callback
    playercmd_metadata(player);
    if (error != NULL) {
        g_clear_error(&error);
        g_main_loop_quit(main_loop);
    }
}

static void get_initial_metadata (GList *available_players) {
    // this code is only needed when spotify is already running when sptManager starts
    GError *error = NULL;
    GList *lst = NULL;
    PlayerctlPlayer *player = NULL;
    for (lst = available_players; lst != NULL; lst = lst->next) {
        PlayerctlPlayerName *name = (PlayerctlPlayerName*) (lst->data);

        // basic check to make sure that the only player we control is spotify
        if (!g_strcmp0(name->name, "spotify"))
        {
            player = playerctl_player_new_from_name(name, &error);
            if (error != NULL) {
                exit_func(&available_players, &manager);
                exit(1);
            }

            // init managed player
            playerctl_player_manager_manage_player(manager, player);
            g_signal_connect(G_OBJECT(player), (gchar *) "metadata", G_CALLBACK(managed_player_updates_properties_callback), NULL);
            break;
        }
    }

    managed_player_execute_command(player);
    if (player != NULL) g_object_unref(player);
}

int main(int argc, char* argv[]) {
    // read the json config file
    std::ifstream config_f("config.json");
    config_j = nlohmann::json::parse(config_f);

    GError *error = NULL;
    GList *available_players = NULL;

    // the manager of all the players to connect to
    manager = playerctl_player_manager_new(&error);
    if (error != NULL) {
        exit_func(&available_players, &manager);
        return 1;
    }

    // get all the available players
    g_object_get(manager, "player-names", &available_players, NULL);

    get_initial_metadata(available_players);

    // callbacks for player actions
    g_signal_connect(PLAYERCTL_PLAYER_MANAGER(manager), "name-appeared", G_CALLBACK(name_appeared_callback), NULL);
    g_signal_connect(PLAYERCTL_PLAYER_MANAGER(manager), "player-appeared", G_CALLBACK(player_appeared_callback), NULL);
    g_signal_connect(PLAYERCTL_PLAYER_MANAGER(manager), "player-vanished", G_CALLBACK(player_vanished_callback), NULL);

    main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(main_loop);
    g_main_loop_unref(main_loop);

    return 0;
}

#endif // ifdef __linux__
