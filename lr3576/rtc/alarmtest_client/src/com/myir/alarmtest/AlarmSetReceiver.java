package com.myir.alarmtest;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.app.AlarmManager;
import android.app.PendingIntent;
import android.util.Log;

public class AlarmSetReceiver extends BroadcastReceiver {
    static final String TAG = "AlarmTest";
    @Override
    public void onReceive(Context c, Intent i) {
        int delaySec = Integer.parseInt(i.getStringExtra("delay"));
        long trigger = System.currentTimeMillis() + delaySec * 1000L;
        Intent alarm = new Intent("com.myir.alarmtest.FIRE").setPackage(c.getPackageName());
        PendingIntent pi = PendingIntent.getBroadcast(c, 0, alarm,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        AlarmManager am = (AlarmManager) c.getSystemService(Context.ALARM_SERVICE);
        am.setExactAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, trigger, pi);
        Log.i(TAG, "SET_VIA_BROADCAST delay=" + delaySec + " trigger=" + trigger);
    }
}
