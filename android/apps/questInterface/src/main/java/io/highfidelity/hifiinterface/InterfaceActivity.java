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

import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.Vibrator;
import android.view.WindowManager;

import org.qtproject.qt5.android.bindings.QtActivity;

import io.highfidelity.hifiinterface.receiver.HeadsetStateReceiver;
import io.highfidelity.utils.HifiUtils;

public class InterfaceActivity extends QtActivity {
    private HeadsetStateReceiver headsetStateReceiver;

    private native void nativeOnCreate(AssetManager assetManager);
    private native void nativeOnDestroy();
    private native void nativeOnPause();
    private native void nativeOnResume();



    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.isLoading = true;
        super.keepInterfaceRunning = true;
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        HifiUtils.upackAssets(getAssets(), getCacheDir().getAbsolutePath());
        nativeOnCreate(getResources().getAssets());
        //headsetStateReceiver = new HeadsetStateReceiver();
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (!super.isLoading) {
            nativeOnPause();
        }
        //unregisterReceiver(headsetStateReceiver);
    }

    @Override
    protected void onResume() {
        super.onResume();
        nativeOnResume();
        //registerReceiver(headsetStateReceiver);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        nativeOnDestroy();
    }
}
