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

    /** Only release assets of this repo are ever downloaded or installed. */
    private static final String REPO = "dixonSolutions/SuperTuxKart-Touch";

    private static final String RELEASES_URL =
        "https://api.github.com/repos/" + REPO + "/releases/latest";

    /** Every download URL this repo's release assets can have starts with this. */
    private static final String DOWNLOAD_PREFIX =
        "https://github.com/" + REPO + "/";

    private static final String PREFS = "stk_updates";
    private static final String PREF_ENABLED = "check_on_launch";
    private static final String PREF_SKIPPED_TAG = "skipped_tag";
    /** Whether a release we find installs without stopping to ask first. */
    private static final String PREF_AUTO_INSTALL = "auto_install";

    private static final String INSTALL_ACTION =
        "org.supertuxkart.stk_dbg.INSTALL_STATUS";

    private final Activity m_activity;
    private Update m_pending_update;
    private final STKUpdateBridge m_bridge;
    /** Last release the check found, so Install and Skip know what they mean. */
    private volatile Update m_last_seen;
    private Thread m_service;

    public STKUpdateChecker(Activity activity)
    {
        m_activity = activity;
        m_bridge = new STKUpdateBridge(activity);
    }

    /**
     * Opt-out, so a player who never opens Options still gets fixes.
     *
     * Turning this off does not stop the check -- the Updates screen still has
     * to be able to say how far behind you are -- it stops the install
     * happening without being asked for. Android confirms every package
     * install either way, so even "automatic" is one tap, not none.
     */
    public static boolean isAutoInstall(Context context)
    {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .getBoolean(PREF_AUTO_INSTALL, true);
    }

    public static void setAutoInstall(Context context, boolean enabled)
    {
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
            .putBoolean(PREF_AUTO_INSTALL, enabled).apply();
    }

    /** Suppress one release by version, for the Updates screen's Skip button. */
    public static void skipVersion(Context context, String version)
    {
        if (version == null || version.isEmpty())
            return;
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
            .putString(PREF_SKIPPED_TAG, version).apply();
    }

    /**
     * Serve the Updates screen's requests for as long as the game runs.
     *
     * Without this every button on that screen would write a request file that
     * nothing ever reads: the launch-time check has long finished by the time
     * anyone can reach Options. A poll rather than a watch -- the writer is C++
     * using plain ofstream, and a second of latency on a button that starts a
     * 30 MB download does not justify a FileObserver.
     */
    public void startRequestService()
    {
        if (m_service != null && m_service.isAlive())
            return;

        m_service = new Thread(new Runnable()
        {
            @Override
            public void run()
            {
                while (!Thread.currentThread().isInterrupted())
                {
                    try
                    {
                        Thread.sleep(1000);
                    }
                    catch (InterruptedException e)
                    {
                        Thread.currentThread().interrupt();
                        return;
                    }
                    String request = m_bridge.takeRequest();
                    if (request != null)
                        serveRequest(request);
                }
            }
        }, "stk-update-service");
        m_service.setDaemon(true);
        m_service.start();
    }

    public void stopRequestService()
    {
        if (m_service != null)
        {
            m_service.interrupt();
            m_service = null;
        }
    }

    private void serveRequest(String request)
    {
        String installed = installedVersion();
        if (request.equals("auto-on"))
        {
            setAutoInstall(m_activity, true);
            // Republish so line 8 carries the new preference at once; the
            // screen reads its checkbox back from there.
            m_bridge.publishIdle(installed);
        }
        else if (request.equals("auto-off"))
        {
            setAutoInstall(m_activity, false);
            m_bridge.publishIdle(installed);
        }
        else if (request.equals("skip"))
        {
            skipVersion(m_activity, m_last_seen == null ? null : m_last_seen.version);
            m_last_seen = null;
            m_bridge.publishUpToDate(installed);
        }
        else if (request.equals("check"))
        {
            m_bridge.publishChecking(installed);
            m_last_seen = findUpdate();
            if (m_last_seen == null)
                m_bridge.publishUpToDate(installed);
            else
                m_bridge.publishAvailable(installed, m_last_seen.version);
        }
        else if (request.equals("install"))
        {
            installFromMenu(installed);
        }
        else
        {
            Log.w(TAG, "Ignoring unknown update request: " + request);
        }
    }

    private void installFromMenu(String installed)
    {
        Update update = m_last_seen;
        if (update == null)
        {
            // The screen can ask to install before anything has been found -- a
            // check that failed, or a status file left by a previous run. Look
            // again rather than refusing.
            m_bridge.publishChecking(installed);
            update = findUpdate();
            m_last_seen = update;
        }
        if (update == null)
        {
            m_bridge.publishUpToDate(installed);
            return;
        }
        final String latest = update.version;
        try
        {
            if (!canInstallPackages())
            {
                requestInstallPermission();
                m_bridge.publishNeedsPermission(installed, latest);
                return;
            }
            install(update);
            m_bridge.publishInstalling(installed, latest);
        }
        catch (IOException | RuntimeException e)
        {
            Log.w(TAG, "Update install failed", e);
            m_bridge.publishError(installed, String.valueOf(e.getMessage()));
        }
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
                final String installed = installedVersion();
                // Publish before asking. The Updates screen draws its version
                // from this file, and that has to be right even on the launches
                // where the check finds nothing or never returns.
                m_bridge.publishChecking(installed);

                final Update update = findUpdate();
                m_last_seen = update;
                if (update == null)
                {
                    m_bridge.publishUpToDate(installed);
                    return;
                }
                m_bridge.publishAvailable(installed, update.version);
                if (isAutoInstall(m_activity))
                {
                    // Taking the automatic path means not stopping to ask. The
                    // system installer still confirms, so this is not an
                    // unattended install -- just one fewer dialog before it.
                    installFromMenu(installed);
                    return;
                }
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
                    String url = asset.optString("browser_download_url", "");
                    if (!name.endsWith(".apk") || !name.contains(abi))
                        continue;
                    if (!isTrustedApkUrl(url))
                    {
                        Log.w(TAG, "Ignoring release asset with an untrusted URL: " + url);
                        continue;
                    }
                    return new Update(version, url, asset.optLong("size", -1));
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

    /**
     * True only for an `.apk` release asset served by GitHub for this repo.
     *
     * Anchored to a single prefix rather than asking whether the URL merely
     * *contains* the repo name. A substring is not a path boundary, so the
     * previous revision accepted exactly what it was written to stop: an owner
     * whose name ends with ours (evil-dixonSolutions/SuperTuxKart-Touch), a repo
     * whose name starts with ours (SuperTuxKart-Touch-fork), and any unrelated
     * repo burying the string further down its path. All three still start with
     * https://github.com/ and end in .apk, so both call sites took them.
     *
     * The URL arrives inside a TLS response from api.github.com, so this is
     * defence in depth rather than the only thing standing in the way — but what
     * it guards is a package install, and the guard costs one string comparison.
     */
    static boolean isTrustedApkUrl(String url)
    {
        return url != null
            && url.startsWith(DOWNLOAD_PREFIX)
            && url.endsWith(".apk")
            && !url.contains("..");
    }

    private void install(Update update) throws IOException
    {
        // Checked again at the point of use, not only where the URL was picked:
        // these bytes go straight into a PackageInstaller session, so this is the
        // difference between updating this app and installing an arbitrary APK.
        if (!isTrustedApkUrl(update.url))
            throw new IOException("refusing to install an untrusted APK URL");

        PackageInstaller installer =
            m_activity.getPackageManager().getPackageInstaller();
        PackageInstaller.SessionParams params = new PackageInstaller.SessionParams(
            PackageInstaller.SessionParams.MODE_FULL_INSTALL);
        if (update.size > 0)
            params.setSize(update.size);

        int session_id = installer.createSession(params);
        PackageInstaller.Session session = installer.openSession(session_id);
        boolean committed = false;
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
            committed = true;
        }
        finally
        {
            session.close();
            // close() only drops our handle. A session that never committed keeps
            // its staged bytes -- a whole partial APK -- until the system reaps
            // it, so a failed download would otherwise cost the player storage
            // every time it was retried.
            if (!committed)
            {
                try
                {
                    installer.abandonSession(session_id);
                }
                catch (RuntimeException e)
                {
                    Log.w(TAG, "Could not abandon install session", e);
                }
            }
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
