//
//  InterfaceActivity.java
//  android/app/src/main/java
//
//  Created by Stephen Birarda on 1/26/15.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

package io.highfidelity.hifiinterface;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.graphics.Point;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Vibrator;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.webkit.WebViewFragment;
import android.widget.FrameLayout;
import android.widget.SlidingDrawer;

import org.qtproject.qt5.android.QtLayout;
import org.qtproject.qt5.android.QtSurface;
import org.qtproject.qt5.android.bindings.QtActivity;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import io.highfidelity.hifiinterface.receiver.HeadsetStateReceiver;

public class InterfaceActivity extends QtActivity {
    private Vibrator mVibrator;
    private HeadsetStateReceiver headsetStateReceiver;

    private native long nativeOnCreate(AssetManager assetManager);
    private native void nativeOnDestroy();
    private native void nativeGotoUrl(String url);
    private native void nativeGoToUser(String username);
    private native void nativeBeforeEnterBackground();
    private native void nativeEnterBackground();
    private native void nativeEnterForeground();
    private native void nativeInitAfterAppLoaded();
    
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.isLoading = true;
        super.keepInterfaceRunning = true;
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        nativeOnCreate(getResources().getAssets());
        //startActivity(new Intent(this, SplashActivity.class));
        //mVibrator = (Vibrator) this.getSystemService(VIBRATOR_SERVICE);
        //headsetStateReceiver = new HeadsetStateReceiver();
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (!super.isLoading) {
            nativeEnterBackground();
        }
        //unregisterReceiver(headsetStateReceiver);
    }

    @Override
    protected void onResume() {
        super.onResume();
        nativeEnterForeground();
        //registerReceiver(headsetStateReceiver);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        nativeOnDestroy();
    }
}
