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
import java.util.concurrent.atomic.AtomicBoolean;

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
    /** Set on whichever thread starts an install, read by onResume() on the UI
     *  thread, so it has to be published across both. */
    private volatile Update m_pending_update;
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
        if (request.equals("auto-on") || request.equals("auto-off"))
        {
            setAutoInstall(m_activity, request.equals("auto-on"));
            // Republish so line 8 carries the new preference at once; the
            // screen reads its checkbox back from there. Only that line
            // changes: a preference is no reason to forget a release we have
            // already found, or to unlock the buttons mid-download.
            m_bridge.republish(installed);
        }
        else if (request.equals("skip"))
        {
            CheckResult target = targetUpdate(installed);
            if (target.m_update == null)
            {
                publishFindings(installed, target);
                return;
            }
            skipVersion(m_activity, target.m_update.version);
            m_last_seen = null;
            // Skip stays pressable while we wait for the install permission --
            // that state is not busy -- and onResume() installs whatever is
            // left here as soon as the permission arrives. Forgetting it is
            // what makes the skip stick.
            m_pending_update = null;
            m_bridge.publishUpToDate(installed);
        }
        else if (request.equals("check"))
        {
            m_bridge.publishChecking(installed);
            publishFindings(installed, findUpdate());
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

    /**
     * The release Install and Skip are acting on.
     *
     * The screen enables both from the published status, which the launch check
     * may have written before the player ever reached Options -- and on a
     * launch where that check failed, or where the status file is left over
     * from a previous run, the field is empty when the press arrives. Look
     * again rather than acting on nothing and then reporting success.
     */
    private CheckResult targetUpdate(String installed)
    {
        Update update = m_last_seen;
        if (update != null)
            return new CheckResult(update, false);

        m_bridge.publishChecking(installed);
        CheckResult result = findUpdate();
        m_last_seen = result.m_update;
        return result;
    }

    /**
     * Publish what a check learned.
     *
     * A check that could not reach the feed has learned nothing, and saying
     * "up to date" on its behalf both misinforms the player and drops the
     * release the last successful check found -- which is the one thing on this
     * screen they might have been about to install.
     */
    private void publishFindings(String installed, CheckResult result)
    {
        if (result.m_update != null)
        {
            m_last_seen = result.m_update;
            m_bridge.publishAvailable(installed, result.m_update.version);
            return;
        }
        if (result.m_failed)
        {
            m_bridge.publishCheckFailed(installed,
                m_last_seen == null ? null : m_last_seen.version);
            return;
        }
        m_last_seen = null;
        m_bridge.publishUpToDate(installed);
    }

    private void installFromMenu(String installed)
    {
        CheckResult target = targetUpdate(installed);
        Update update = target.m_update;
        if (update == null)
        {
            publishFindings(installed, target);
            return;
        }
        if (!canInstallPackages())
        {
            // onResume() is what carries an install across the trip to the
            // settings screen, and it only retries an update left pending
            // here. Without this the player grants the permission, comes back,
            // and nothing happens.
            m_pending_update = update;
            requestInstallPermission();
            m_bridge.publishNeedsPermission(installed, update.version);
            return;
        }
        m_pending_update = null;
        runInstall(update, installed);
    }

    /** Install, and say so the whole way through, from either entry point. */
    private void runInstall(final Update update, final String installed)
    {
        // The failure callback below is a broadcast receiver, so it can land
        // before install() has returned -- at once, if launching the confirm
        // dialog throws. Publishing `installing` over it would put back the
        // very state the callback exists to clear.
        final AtomicBoolean failed = new AtomicBoolean();
        try
        {
            install(update, installed, new Runnable()
            {
                @Override
                public void run()
                {
                    // Dismissed at the system prompt, or refused outright.
                    // Without this the status file stays on `installing` and
                    // the Updates screen spends the rest of the session greyed
                    // out behind a bar that never finishes.
                    //
                    // Not publishError: that writes no latest version and
                    // nothing behind, and the screen gates Install and Skip on
                    // exactly that -- so cancelling the confirm dialog, which
                    // is a thing players do on purpose, would take away the
                    // button that starts it again.
                    failed.set(true);
                    m_bridge.publishInstallFailed(installed, update.version,
                        "Update failed. Try again later.");
                }
            });
            if (!failed.get())
                m_bridge.publishInstalling(installed, update.version);
        }
        catch (IOException | RuntimeException e)
        {
            // Same reasoning as the callback above: a download that broke off
            // is a reason to offer the retry, not to take it away.
            Log.w(TAG, "Update install failed", e);
            m_bridge.publishInstallFailed(installed, update.version,
                String.valueOf(e.getMessage()));
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

                final CheckResult result = findUpdate();
                final Update update = result.m_update;
                // Publishes "up to date" only for a check that got that answer;
                // one that never completed is published as the failure it was.
                publishFindings(installed, result);
                if (update == null)
                    return;
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

    /**
     * One check's answer: the release, and whether we got to ask at all.
     *
     * Carried back per call rather than left in a field. The launch check and
     * the request service both run findUpdate() on their own threads -- a
     * request file left over from a previous run is served within a second of
     * startup, while the launch check is still in flight -- and a shared flag
     * lets the successful one clear the failing one's, which then publishes
     * "up to date" and drops the release the other just found.
     */
    private static class CheckResult
    {
        final Update m_update;
        /** True when the check never reached the feed: offline, bad response. */
        final boolean m_failed;

        CheckResult(Update update, boolean failed)
        {
            m_update = update;
            m_failed = failed;
        }
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

    /**
     * @return the newer release, or a null one when up to date, skipped,
     *         offline, or the feed has no build for this device's ABI. Never
     *         throws: a failed check must not stand between the player and the
     *         game. {@link CheckResult#m_failed} tells the null that means
     *         "nothing to install" from the one that means "could not ask".
     */
    private CheckResult findUpdate()
    {
        try
        {
            return new CheckResult(queryFeed(), false);
        }
        catch (Exception e)
        {
            // Never rethrown: a failed update check must not stand between the
            // player and the game. It is reported instead, so a caller does not
            // mistake it for "there is nothing newer".
            Log.i(TAG, "Update check skipped: " + e);
            return new CheckResult(null, true);
        }
    }

    /**
     * Ask the release feed.
     *
     * @return the newer release, or null when the feed's answer is that there
     *         is nothing to install -- newest build, skipped by the player, or
     *         no build for this device's ABI.
     * @throws Exception when the feed could not be reached or made sense of.
     *         Thrown rather than returned as another null, because the two mean
     *         opposite things to the screen: one is an answer, the other is the
     *         absence of one.
     */
    private Update queryFeed() throws Exception
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
                    // The status file still says `available` from the check a
                    // moment ago. Left there, the Updates screen would spend
                    // the session offering to install the release just
                    // declined here.
                    m_last_seen = null;
                    m_bridge.publishUpToDate(installedVersion());
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
            public void run() { runInstall(update, installedVersion()); }
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

    /**
     * @param on_failure run when the install does not happen -- the system
     *        prompt dismissed, the session rejected. The commit below returns
     *        as soon as the session is handed over, not when the player has
     *        confirmed it, so this callback is the only thing that ever learns
     *        the difference.
     */
    private void install(Update update, String installed, Runnable on_failure)
        throws IOException
    {
        // Checked again at the point of use, not only where the URL was picked:
        // these bytes go straight into a PackageInstaller session, so this is the
        // difference between updating this app and installing an arbitrary APK.
        if (!isTrustedApkUrl(update.url))
            throw new IOException("refusing to install an untrusted APK URL");

        // Claim the busy state before the first byte moves. The Updates screen
        // gates Install and Skip on what is published here, so a download that
        // says nothing leaves the screen looking idle -- and one tap away from
        // starting a second one -- for its entire length.
        m_bridge.publishDownloading(installed, update.version, 0);

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
                    long written = 0;
                    // Publish only when the whole number changes: each one is
                    // a file write and a rename, and the screen reads it once
                    // a second.
                    int published = 0;
                    int read;
                    while ((read = in.read(buffer)) > 0)
                    {
                        out.write(buffer, 0, read);
                        written += read;
                        int percent = total > 0 ?
                            (int)(written * 100 / total) : 0;
                        if (percent != published)
                        {
                            published = percent;
                            m_bridge.publishDownloading(installed,
                                update.version, percent);
                        }
                    }
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
            session.commit(installStatusIntent(on_failure).getIntentSender());
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
    private PendingIntent installStatusIntent(final Runnable on_failure)
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
                        try
                        {
                            confirm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                            context.startActivity(confirm);
                            return;
                        }
                        catch (RuntimeException e)
                        {
                            // A receiver that throws takes the process with it,
                            // and the install is not happening either way.
                            Log.w(TAG, "Could not open install confirmation", e);
                        }
                    }
                    finish(context, true);
                    return;
                }
                if (status != PackageInstaller.STATUS_SUCCESS)
                {
                    Log.w(TAG, "Install failed (" + status + "): " +
                        intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE));
                }
                finish(context, status != PackageInstaller.STATUS_SUCCESS);
            }

            private void finish(Context context, boolean failed)
            {
                try
                {
                    context.unregisterReceiver(this);
                }
                catch (IllegalArgumentException ignored)
                {
                    // Already gone; the process may be restarting for the upgrade.
                }
                if (failed && on_failure != null)
                    on_failure.run();
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
