/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-8-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

package com.rmsl.juce;

import static android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
import static android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
import static android.view.View.SYSTEM_UI_FLAG_LAYOUT_STABLE;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.PowerManager;
import android.view.View;
import java.io.File;

//==============================================================================
public class JuceActivity   extends Activity
{
    private native void appNewIntent (Intent intent);
    private native void appOnResume();

    private void setupDirectories()
    {
        try {
            // Internal files directory holds factory assets (SurgeXTData)
            android.system.Os.setenv("HOME", getFilesDir().getAbsolutePath(), true);

            // App-specific external storage holds user data (Surge XT/Patches/...)
            File extDir = getExternalFilesDir(null);
            if (extDir != null)
            {
                if (!extDir.exists())
                    extDir.mkdirs();

                File surgeExt = new File(extDir, "Surge XT");
                if (!surgeExt.exists())
                    surgeExt.mkdirs();

                android.system.Os.setenv("SURGE_EXTERNAL_DIR", extDir.getAbsolutePath(), true);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void initEdgeToEdge()
    {
        if (Build.VERSION.SDK_INT < 35)
        {
            View decorView = getWindow().getDecorView();

            final int flags = Build.VERSION.SDK_INT < 30
                    ? (  SYSTEM_UI_FLAG_LAYOUT_STABLE
                       | SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                       | SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN)
                    : 0;

            decorView.setSystemUiVisibility (decorView.getSystemUiVisibility() | flags);
        }

        if (30 <= Build.VERSION.SDK_INT)
            getWindow().setDecorFitsSystemWindows (false);

        if (29 <= Build.VERSION.SDK_INT)
        {
            if (Build.VERSION.SDK_INT < 35)
                getWindow().setStatusBarContrastEnforced (false);

            getWindow().setNavigationBarContrastEnforced (false);
        }
    }

    @Override
    protected void onCreate (Bundle savedInstanceState)
    {
        setupDirectories();

        Java.initialiseJUCE (getApplicationContext());
        initEdgeToEdge();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N)
        {
            PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
            if (pm != null && pm.isSustainedPerformanceModeSupported())
            {
                getWindow().setSustainedPerformanceMode(true);
            }
        }

        super.onCreate (savedInstanceState);
    }

    @Override
    protected void onNewIntent (Intent intent)
    {
        super.onNewIntent (intent);
        setIntent (intent);

        appNewIntent (intent);
    }

    @Override
    protected void onResume()
    {
        super.onResume();
        setupDirectories();
        appOnResume();
    }
}
