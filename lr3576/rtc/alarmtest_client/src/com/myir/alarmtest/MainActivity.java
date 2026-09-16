package com.myir.alarmtest;

import android.app.Activity;
import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

public class MainActivity extends Activity {
    static final String TAG = "AlarmTest";
    static final String ACTION_FIRE = "com.myir.alarmtest.FIRE";

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        int delaySec = 60;
        try { delaySec = Integer.parseInt(getIntent().getStringExtra("delay")); } catch (Exception e) {}
        long trigger = System.currentTimeMillis() + delaySec * 1000L;
        Intent i = new Intent(ACTION_FIRE).setPackage(getPackageName());
        PendingIntent pi = PendingIntent.getBroadcast(this, 0, i,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        AlarmManager am = (AlarmManager) getSystemService(Context.ALARM_SERVICE);
        am.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, trigger, pi);
        Log.i(TAG, "ALARM_SET delay=" + delaySec + " trigger=" + trigger
                + " now=" + System.currentTimeMillis());
        finish();
    }
}
