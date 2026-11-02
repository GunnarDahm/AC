package net.cubers.assaultcube;

import org.libsdl.app.SDLActivity;

/** Runs the game; SetupActivity has already unpacked the data by the time this starts. */
public class AssaultCubeActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    @Override
    protected String[] getArguments() {
        // --android=<dir>: game data in <dir>/game, profile (saved settings, demos, screenshots) in <dir>/home
        // --init: keep resolution and other init settings in the profile, like the desktop launcher scripts do
        return new String[] { "--android=" + SetupActivity.getBaseDir(this).getAbsolutePath(), "--init" };
    }
}
