package com.myir.alarmtest;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

public class AlarmReceiver extends BroadcastReceiver {
    static final String TAG = "AlarmTest";
    @Override
    public void onReceive(Context c, Intent i) {
        long now = System.currentTimeMillis();
        Log.i(TAG, "ALARM_FIRED at=" + now);
        try {
            java.io.FileWriter f = new java.io.FileWriter("/data/data/com.myir.alarmtest/fired.txt", true);
            f.write("fired=" + now + "\n");
            f.close();
        } catch (Exception e) {
            Log.e(TAG, "write failed: " + e);
        }
    }
}
