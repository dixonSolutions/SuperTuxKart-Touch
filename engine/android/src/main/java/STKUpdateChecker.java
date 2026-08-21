package org.supertuxkart.stk_dbg;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageInstaller;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;

/**
 * On-launch update check against the project's GitHub Releases.
 *
 * A sideloaded build has no store to update it, so the app watches its own
 * release feed. It never installs silently: Android puts its own confirmation
 * in front of every package install, which is where that decision belongs.
 *
 * Nothing here touches game data. An update is an ordinary same-signature
 * upgrade, so karts, progress, achievements and settings survive it, and the
 * APK is streamed straight into the install session rather than parked in
 * storage.
 *
 * Strings are literal English rather than resources: make.sh regenerates res/
 * on every build from the .po files, and threading four more strings through
 * that machinery is not worth it for a dialog most players see once.
 */
public class STKUpdateChecker
{
    private static final String TAG = "STKUpdateChecker";

    private static final String RELEASES_URL =
        "https://api.github.com/repos/dixonSolutions/SuperTuxKart-Touch/releases/latest";

    private static final String PREFS = "stk_updates";
    private static final String PREF_ENABLED = "check_on_launch";
    private static final String PREF_SKIPPED_TAG = "skipped_tag";

    private static final String INSTALL_ACTION =
        "org.supertuxkart.stk_dbg.INSTALL_STATUS";

    private final Activity m_activity;
    private Update m_pending_update;

    public STKUpdateChecker(Activity activity)
    {
        m_activity = activity;
    }

    /**
     * Checks in the background and, if a newer build exists, offers it. Returns
     * immediately; the game carries on loading either way, and a failed check
     * is never allowed to stand between the player and the game.
     */
    public void checkInBackground()
    {
        if (!prefs().getBoolean(PREF_ENABLED, true))
            return;

        new Thread(new Runnable()
        {
            @Override
            public void run()
            {
                final Update update = findUpdate();
                if (update == null)
                    return;
                m_activity.runOnUiThread(new Runnable()
                {
                    @Override
                    public void run() { offer(update); }
                });
            }
        }, "stk-update-check").start();
    }

    private static class Update
    {
        final String version;
        final String url;
        final long size;

        Update(String version, String url, long size)
        {
            this.version = version;
            this.url = url;
            this.size = size;
        }
    }

    private Update findUpdate()
    {
        try
        {
            JSONObject release = new JSONObject(fetch(RELEASES_URL));
            if (release.optBoolean("draft") || release.optBoolean("prerelease"))
                return null;

            String tag = release.optString("tag_name", "");
            String version = tag.startsWith("v") ? tag.substring(1) : tag;
            if (version.isEmpty())
                return null;
            if (compareVersions(version, installedVersion()) <= 0)
                return null;
            if (version.equals(prefs().getString(PREF_SKIPPED_TAG, null)))
                return null;

            JSONArray assets = release.optJSONArray("assets");
            if (assets == null)
                return null;
            for (String abi : Build.SUPPORTED_ABIS)
            {
                for (int i = 0; i < assets.length(); i++)
                {
                    JSONObject asset = assets.getJSONObject(i);
                    String name = asset.optString("name", "");
                    if (name.endsWith(".apk") && name.contains(abi))
                    {
                        return new Update(version,
                            asset.getString("browser_download_url"),
                            asset.optLong("size", -1));
                    }
                }
            }
            Log.i(TAG, "Release " + version + " has no build for " +
                Build.SUPPORTED_ABIS[0]);
        }
        catch (Exception e)
        {
            Log.i(TAG, "Update check skipped: " + e);
        }
        return null;
    }

    private void offer(final Update update)
    {
        if (m_activity.isFinishing())
            return;
        new AlertDialog.Builder(m_activity)
            .setTitle("SuperTuxKart Touch " + update.version)
            .setMessage("You have " + installedVersion() +
                ". Updating keeps your progress, karts and settings.")
            .setPositiveButton("Update", new DialogInterface.OnClickListener()
            {
                @Override
                public void onClick(DialogInterface dialog, int which)
                {
                    startInstall(update);
                }
            })
            .setNegativeButton("Not now", null)
            .setNeutralButton("Skip this version", new DialogInterface.OnClickListener()
            {
                @Override
                public void onClick(DialogInterface dialog, int which)
                {
                    prefs().edit()
                        .putString(PREF_SKIPPED_TAG, update.version).apply();
                }
            })
            .show();
    }

    private void startInstall(final Update update)
    {
        if (!canInstallPackages())
        {
            m_pending_update = update;
            requestInstallPermission();
            return;
        }
        m_pending_update = null;
        new Thread(new Runnable()
        {
            @Override
            public void run()
            {
                try
                {
                    install(update);
                }
                catch (Exception e)
                {
                    Log.w(TAG, "Update install failed", e);
                }
            }
        }, "stk-update-install").start();
    }

    void onResume()
    {
        if (m_pending_update != null && canInstallPackages())
            startInstall(m_pending_update);
    }

    private void install(Update update) throws IOException
    {
        PackageInstaller installer =
            m_activity.getPackageManager().getPackageInstaller();
        PackageInstaller.SessionParams params = new PackageInstaller.SessionParams(
            PackageInstaller.SessionParams.MODE_FULL_INSTALL);
        if (update.size > 0)
            params.setSize(update.size);

        int session_id = installer.createSession(params);
        PackageInstaller.Session session = installer.openSession(session_id);
        try
        {
            HttpURLConnection connection =
                (HttpURLConnection)new URL(update.url).openConnection();
            connection.setConnectTimeout(30000);
            connection.setReadTimeout(60000);
            connection.setInstanceFollowRedirects(true);
            try
            {
                int code = connection.getResponseCode();
                if (code != HttpURLConnection.HTTP_OK)
                    throw new IOException("HTTP " + code + " downloading update");

                long total = update.size > 0 ? update.size :
                    connection.getContentLength();
                InputStream in = connection.getInputStream();
                OutputStream out = session.openWrite("payload", 0, total);
                try
                {
                    byte[] buffer = new byte[64 * 1024];
                    int read;
                    while ((read = in.read(buffer)) > 0)
                        out.write(buffer, 0, read);
                    session.fsync(out);
                }
                finally
                {
                    out.close();
                    in.close();
                }
            }
            finally
            {
                connection.disconnect();
            }
            session.commit(installStatusIntent().getIntentSender());
        }
        finally
        {
            session.close();
        }
    }

    private boolean canInstallPackages()
    {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O)
            return true;
        return m_activity.getPackageManager().canRequestPackageInstalls();
    }

    private void requestInstallPermission()
    {
        try
        {
            m_activity.startActivity(new Intent(
                Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                Uri.parse("package:" + m_activity.getPackageName())));
        }
        catch (Exception e)
        {
            Log.w(TAG, "No settings screen for install permission", e);
        }
    }

    /**
     * The installer answers by broadcast. The reply that matters is
     * STATUS_PENDING_USER_ACTION, which carries the system's confirm dialog for
     * us to launch — without starting it the session just sits there.
     */
    private PendingIntent installStatusIntent()
    {
        Context application_context = m_activity.getApplicationContext();
        BroadcastReceiver receiver = new BroadcastReceiver()
        {
            @Override
            public void onReceive(Context context, Intent intent)
            {
                int status = intent.getIntExtra(PackageInstaller.EXTRA_STATUS,
                    PackageInstaller.STATUS_FAILURE);
                if (status == PackageInstaller.STATUS_PENDING_USER_ACTION)
                {
                    Intent confirm = intent.getParcelableExtra(Intent.EXTRA_INTENT);
                    if (confirm != null)
                    {
                        confirm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                        context.startActivity(confirm);
                    }
                    return;
                }
                if (status != PackageInstaller.STATUS_SUCCESS)
                {
                    Log.w(TAG, "Install failed (" + status + "): " +
                        intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE));
                }
                try
                {
                    context.unregisterReceiver(this);
                }
                catch (IllegalArgumentException ignored)
                {
                    // Already gone; the process may be restarting for the upgrade.
                }
            }
        };
        IntentFilter filter = new IntentFilter(INSTALL_ACTION);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU)
        {
            application_context.registerReceiver(receiver, filter,
                Context.RECEIVER_NOT_EXPORTED);
        }
        else
        {
            application_context.registerReceiver(receiver, filter);
        }

        int flags = PendingIntent.FLAG_UPDATE_CURRENT;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
            flags |= PendingIntent.FLAG_MUTABLE;
        Intent status = new Intent(INSTALL_ACTION);
        status.setPackage(m_activity.getPackageName());
        return PendingIntent.getBroadcast(application_context, 0, status, flags);
    }

    private SharedPreferences prefs()
    {
        return m_activity.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    private String installedVersion()
    {
        try
        {
            return m_activity.getPackageManager().getPackageInfo(
                m_activity.getPackageName(), 0).versionName;
        }
        catch (Exception e)
        {
            return "0";
        }
    }

    /** Numeric dotted-version compare; non-numeric parts sort as 0. */
    static int compareVersions(String a, String b)
    {
        String[] left = a.split("\\.");
        String[] right = b.split("\\.");
        int parts = Math.max(left.length, right.length);
        for (int i = 0; i < parts; i++)
        {
            int l = i < left.length ? number(left[i]) : 0;
            int r = i < right.length ? number(right[i]) : 0;
            if (l != r)
                return l < r ? -1 : 1;
        }
        return 0;
    }

    private static int number(String part)
    {
        int end = 0;
        while (end < part.length() && Character.isDigit(part.charAt(end)))
            end++;
        if (end == 0)
            return 0;
        try
        {
            return Integer.parseInt(part.substring(0, end));
        }
        catch (NumberFormatException e)
        {
            return 0;
        }
    }

    private static String fetch(String url) throws IOException
    {
        HttpURLConnection connection =
            (HttpURLConnection)new URL(url).openConnection();
        try
        {
            connection.setRequestProperty("Accept", "application/vnd.github+json");
            connection.setRequestProperty("User-Agent", "SuperTuxKartTouch");
            connection.setConnectTimeout(10000);
            connection.setReadTimeout(15000);
            int code = connection.getResponseCode();
            if (code != HttpURLConnection.HTTP_OK)
                throw new IOException("HTTP " + code + " for " + url);

            InputStream in = connection.getInputStream();
            try
            {
                ByteArrayOutputStream buffer = new ByteArrayOutputStream();
                byte[] chunk = new byte[8192];
                int read;
                while ((read = in.read(chunk)) > 0)
                    buffer.write(chunk, 0, read);
                return buffer.toString(StandardCharsets.UTF_8.name());
            }
            finally
            {
                in.close();
            }
        }
        finally
        {
            connection.disconnect();
        }
    }
}
