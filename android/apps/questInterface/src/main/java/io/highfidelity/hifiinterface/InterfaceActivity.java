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
import android.icu.util.Output;
import android.os.Bundle;
import android.os.Vibrator;
import android.view.WindowManager;

import org.qtproject.qt5.android.bindings.QtActivity;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import com.google.common.io.ByteStreams;
import com.google.common.io.Files;

import java.io.OutputStream;
import java.util.LinkedList;

import io.highfidelity.hifiinterface.receiver.HeadsetStateReceiver;

public class InterfaceActivity extends QtActivity {
    private Vibrator mVibrator;
    private HeadsetStateReceiver headsetStateReceiver;

    private native void nativeOnCreate(AssetManager assetManager);
    private native String getAssetTargetPath();
    private native void nativeOnDestroy();
    private native void nativeOnPause();
    private native void nativeOnResume();


//    void withAssetData(String filename, const std::function<void(off64_t, const void*)>& callback) {
//        auto asset = AAssetManager_open(g_assetManager, filename, AASSET_MODE_BUFFER);
//        if (!asset) {
//            throw std::runtime_error("Failed to open file");
//        }
//        auto buffer = AAsset_getBuffer(asset);
//        off64_t size = AAsset_getLength64(asset);
//        callback(size, buffer);
//        AAsset_close(asset);
//    }
//
//    QStringList readAssetLines(const char* filename) {
//        QStringList result;
//        withAssetData(filename, [&](off64_t size, const void* data){
//            QByteArray buffer = QByteArray::fromRawData((const char*)data, size);
//            {
//                QTextStream in(&buffer);
//                while (!in.atEnd()) {
//                    QString line = in.readLine();
//                    result << line;
//                }
//            }
//        });
//        return result;
//    }
//
//    void copyAsset(const char* sourceAsset, const QString& destFilename) {
//        withAssetData(sourceAsset, [&](off64_t size, const void* data){
//            QFile file(destFilename);
//            if (!file.open(QFile::ReadWrite | QIODevice::Truncate)) {
//                throw std::runtime_error("Unable to open output file for writing");
//            }
//            if (!file.resize(size)) {
//                throw std::runtime_error("Unable to resize output file");
//            }
//            if (size != 0) {
//                auto mapped = file.map(0, size);
//                if (!mapped) {
//                    throw std::runtime_error("Unable to map output file");
//                }
//                memcpy(mapped, data, size);
//            }
//            file.close();
//        });
//    }

    LinkedList<String> readAssetLines(String asset) throws IOException {
        LinkedList<String> assets = new LinkedList<>();
        InputStream is = getAssets().open(asset);
        BufferedReader in = new BufferedReader(new InputStreamReader(is, "UTF-8"));
        String line;
        while ((line=in.readLine()) != null) {
            assets.add(line);
        }
        in.close();
        return assets;
    }

    void copyAsset(String asset, String destFileName) throws IOException {
        try (InputStream is = getAssets().open(asset)) {
            try (OutputStream os = Files.asByteSink(new File(destFileName)).openStream()) {
                ByteStreams.copy(is, os);
            }
        }
    }

    void unpackAndroidAssets(String dest) {
        try {
            if (!dest.endsWith("/"))
                dest = dest + "/";
            LinkedList<String> assets = readAssetLines("cache_assets.txt");
            String dateStamp = assets.poll();
            String dateStampFilename = dest + dateStamp;
            File dateStampFile = new File(dateStampFilename);
            if (dateStampFile.exists()) {
                return;
            }
            for (String fileToCopy : assets) {
                String destFileName = dest  + fileToCopy;
                {
                    File destFile = new File(destFileName);
                    File destFolder = destFile.getParentFile();
                    if (!destFolder.exists()) {
                        destFolder.mkdirs();
                    }
                    if (destFile.exists()) {
                        destFile.delete();
                    }
                }
                copyAsset(fileToCopy, destFileName);
            }

            Files.write("touch".getBytes(), dateStampFile);
        } catch (IOException e){
            throw new RuntimeException(e);
        }
    }


    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.isLoading = true;
        super.keepInterfaceRunning = true;
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getApplicationContext().getFilesDir();
        unpackAndroidAssets(getAssetTargetPath());
        nativeOnCreate(getResources().getAssets());
        //startActivity(new Intent(this, SplashActivity.class));
        //mVibrator = (Vibrator) this.getSystemService(VIBRATOR_SERVICE);
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
