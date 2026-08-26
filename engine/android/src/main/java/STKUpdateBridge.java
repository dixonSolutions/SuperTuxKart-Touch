package org.supertuxkart.stk_dbg;

import android.content.Context;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.BufferedReader;
import java.io.FileReader;
import java.nio.charset.StandardCharsets;

/**
 * The game's half of the updater.
 *
 * {@link STKUpdateChecker} can only ask its question once, at launch, in a
 * dialog that is gone before anyone is racing. Everything a player wants
 * afterwards -- which build am I on, how far behind, check again, stop asking
 * -- lives in Options → Updates, and Options is C++.
 *
 * Rather than reach across that with JNI, the two halves pass line-based files
 * through the app's files directory. This class owns {@code update-status.txt}
 * and only writes it; the C++ side owns {@code update-request.txt} and only
 * writes that. Neither reads its own file back, so there is no shared state to
 * race over -- a torn read costs one stale second and the screen re-reads on a
 * timer.
 *
 * The directory is {@code getFilesDir()} rather than STK's config directory on
 * purpose: the config path is assembled in assets_android.cpp from HOME plus
 * .config/supertuxkart, and duplicating that derivation here would be a second
 * copy to get wrong. Both sides name this one by construction -- C++ reaches it
 * through SDL's internal storage path, which is the same directory.
 *
 * The file format is documented once, in {@code src/utils/touch_update_status.hpp}.
 * Xonotic Touch runs the identical contract; keep all three in step.
 */
public class STKUpdateBridge
{
    private static final String TAG = "STKUpdateBridge";

    /** Bump only alongside the C++ reader's TouchUpdate::FORMAT. */
    private static final int FORMAT = 1;

    public static final String STATE_IDLE = "idle";
    public static final String STATE_CHECKING = "checking";
    public static final String STATE_UPTODATE = "uptodate";
    public static final String STATE_AVAILABLE = "available";
    public static final String STATE_DOWNLOADING = "downloading";
    public static final String STATE_INSTALLING = "installing";
    public static final String STATE_NEEDS_PERMISSION = "needs-permission";
    public static final String STATE_ERROR = "error";

    private final Context m_context;
    private final File m_status_file;
    private final File m_request_file;

    public STKUpdateBridge(Context context)
    {
        m_context = context.getApplicationContext();
        File dir = m_context.getFilesDir();
        m_status_file = new File(dir, "update-status.txt");
        m_request_file = new File(dir, "update-request.txt");
    }

    /** A request the game left for us, or null. Consumed: read once, then gone. */
    public String takeRequest()
    {
        if (!m_request_file.isFile())
            return null;

        String token = null;
        BufferedReader reader = null;
        try
        {
            reader = new BufferedReader(new FileReader(m_request_file));
            String line = reader.readLine();
            if (line != null)
                token = line.trim();
        }
        catch (IOException | RuntimeException e)
        {
            Log.w(TAG, "Could not read update request", e);
        }
        finally
        {
            if (reader != null)
            {
                try { reader.close(); } catch (IOException ignored) { }
            }
        }

        // Delete even on a failed read: a request we cannot parse would
        // otherwise be retried on every poll for the life of the install.
        if (!m_request_file.delete())
            Log.w(TAG, "Could not clear " + m_request_file);

        return (token == null || token.isEmpty()) ? null : token;
    }

    public void publishIdle(String installed)
    {
        publish(STATE_IDLE, installed, "", 0, 0, "");
    }

    public void publishChecking(String installed)
    {
        publish(STATE_CHECKING, installed, "", 0, 0, "");
    }

    public void publishUpToDate(String installed)
    {
        publish(STATE_UPTODATE, installed, installed, 0, 0,
                "This is the newest build.");
    }

    public void publishAvailable(String installed, String latest)
    {
        publish(STATE_AVAILABLE, installed, latest,
                versionsBehind(installed, latest), 0,
                "SuperTuxKart Touch " + latest + " is available.");
    }

    public void publishDownloading(String installed, String latest, int percent)
    {
        publish(STATE_DOWNLOADING, installed, latest,
                versionsBehind(installed, latest), percent, "");
    }

    public void publishInstalling(String installed, String latest)
    {
        publish(STATE_INSTALLING, installed, latest,
                versionsBehind(installed, latest), 100,
                "Confirm the install prompt to finish.");
    }

    public void publishNeedsPermission(String installed, String latest)
    {
        publish(STATE_NEEDS_PERMISSION, installed, latest,
                versionsBehind(installed, latest), 100,
                "Allow installing apps from SuperTuxKart to finish.");
    }

    public void publishError(String installed, String message)
    {
        publish(STATE_ERROR, installed, "", 0, 0, message);
    }

    /**
     * How many releases behind the installed build is.
     *
     * Tags are X.Y.Z and only the patch component moves between builds, so the
     * patch delta is the release count. Anything that does not fit that shape --
     * a locally built APK, a dirty version string -- falls back to 1: "there is
     * an update" is still true, and a wrong count is worse than a vague one.
     */
    public static int versionsBehind(String installed, String latest)
    {
        String[] a = installed.split("\\.");
        String[] b = latest.split("\\.");
        if (a.length != 3 || b.length != 3)
            return STKUpdateChecker.compareVersions(latest, installed) > 0 ? 1 : 0;
        try
        {
            if (!a[0].equals(b[0]) || !a[1].equals(b[1]))
                return STKUpdateChecker.compareVersions(latest, installed) > 0 ? 1 : 0;
            return Math.max(0, Integer.parseInt(b[2].trim()) -
                               Integer.parseInt(a[2].trim()));
        }
        catch (NumberFormatException e)
        {
            return STKUpdateChecker.compareVersions(latest, installed) > 0 ? 1 : 0;
        }
    }

    private void publish(String state, String installed, String latest,
                         int behind, int percent, String message)
    {
        StringBuilder out = new StringBuilder()
            .append(FORMAT).append('\n')
            .append(state).append('\n')
            .append(nullSafe(installed)).append('\n')
            .append(nullSafe(latest)).append('\n')
            .append(Math.max(0, behind)).append('\n')
            .append(Math.max(0, Math.min(100, percent))).append('\n')
            .append(oneLine(message)).append('\n')
            .append(STKUpdateChecker.isAutoInstall(m_context) ? "on" : "off").append('\n');

        // Write and rename: the game polls this file on its own clock, and a
        // half-written one would be read as a truncated record rather than
        // simply being missed.
        File tmp = new File(m_status_file.getPath() + ".tmp");
        OutputStreamWriter writer = null;
        try
        {
            writer = new OutputStreamWriter(new FileOutputStream(tmp),
                                            StandardCharsets.UTF_8);
            writer.write(out.toString());
            writer.close();
            writer = null;
            if (!tmp.renameTo(m_status_file))
            {
                Log.w(TAG, "Could not replace " + m_status_file);
                //noinspection ResultOfMethodCallIgnored
                tmp.delete();
            }
        }
        catch (IOException | RuntimeException e)
        {
            Log.w(TAG, "Could not publish update status", e);
        }
        finally
        {
            if (writer != null)
            {
                try { writer.close(); } catch (IOException ignored) { }
            }
        }
    }

    private static String nullSafe(String s)
    {
        return s == null ? "" : s.trim();
    }

    /** The format is line-based, so a newline in a message would shift every field after it. */
    private static String oneLine(String s)
    {
        return nullSafe(s).replace('\n', ' ').replace('\r', ' ');
    }
}
