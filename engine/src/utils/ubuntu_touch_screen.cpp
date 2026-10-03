//
//  SuperTuxKart Touch - Ubuntu Touch display wake lock
//  Copyright (C) 2026 SuperTuxKart-Touch contributors
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.

#include "utils/ubuntu_touch_screen.hpp"

#if defined(TOUCH_STK) && defined(__linux__) && !defined(ANDROID) && \
    !defined(IOS_STK)

#include "utils/log.hpp"

#include <cstdlib>
#include <dlfcn.h>

namespace UbuntuTouchScreen
{
namespace
{
// libdbus-1 is already in the process (SDL loads it), so talk to it through
// dlopen rather than adding a link-time dependency to every Linux touch build.
// Only the handful of entry points below are needed; their prototypes and the
// public DBusError layout have been ABI-stable since dbus 1.0.
struct StkDBusError
{
    const char *name;
    const char *message;
    unsigned int dummy1 : 1;
    unsigned int dummy2 : 1;
    unsigned int dummy3 : 1;
    unsigned int dummy4 : 1;
    unsigned int dummy5 : 1;
    void *padding1;
};

const int STK_DBUS_BUS_SYSTEM = 1;
const int STK_DBUS_TYPE_INVALID = 0;
const int STK_DBUS_TYPE_INT32 = 'i';

typedef void DBusConnection;
typedef void DBusMessage;

typedef void (*fn_error_init)(StkDBusError*);
typedef unsigned int (*fn_error_is_set)(const StkDBusError*);
typedef void (*fn_error_free)(StkDBusError*);
typedef DBusConnection* (*fn_bus_get)(int, StkDBusError*);
typedef DBusMessage* (*fn_new_method_call)(const char*, const char*,
                                           const char*, const char*);
typedef DBusMessage* (*fn_send_block)(DBusConnection*, DBusMessage*, int,
                                      StkDBusError*);
typedef unsigned int (*fn_get_args)(DBusMessage*, StkDBusError*, int, ...);
typedef unsigned int (*fn_append_args)(DBusMessage*, int, ...);
typedef void (*fn_message_unref)(DBusMessage*);

struct DBus
{
    fn_error_init error_init;
    fn_error_is_set error_is_set;
    fn_error_free error_free;
    fn_bus_get bus_get;
    fn_new_method_call new_method_call;
    fn_send_block send_block;
    fn_get_args get_args;
    fn_append_args append_args;
    fn_message_unref message_unref;
};

DBus g_dbus;
DBusConnection *g_system_bus = NULL;
// com.canonical.Unity.Screen hands out a request id; -1 means "nothing held".
int g_display_on_cookie = -1;

/** Only Lomiri runs a com.canonical.Unity.Screen, and only a confined click
 *  has org.freedesktop.ScreenSaver denied. packaging/start.sh sets this. */
bool isUbuntuTouchClick()
{
    const char *click = std::getenv("STK_TOUCH_CLICK");
    return click != NULL && *click != '\0';
}   // isUbuntuTouchClick

// ----------------------------------------------------------------------------
/** Resolves libdbus-1 and the system bus. Returns false if either is missing,
 *  in which case the caller silently gives up on the wake lock. */
bool connectSystemBus()
{
    if (g_system_bus != NULL)
        return true;

    void *lib = dlopen("libdbus-1.so.3", RTLD_LAZY | RTLD_LOCAL);
    if (lib == NULL)
    {
        Log::warn("UbuntuTouchScreen",
                  "libdbus-1.so.3 not available (%s) — the display may sleep "
                  "during a race.", dlerror());
        return false;
    }

    g_dbus.error_init = (fn_error_init)dlsym(lib, "dbus_error_init");
    g_dbus.error_is_set = (fn_error_is_set)dlsym(lib, "dbus_error_is_set");
    g_dbus.error_free = (fn_error_free)dlsym(lib, "dbus_error_free");
    g_dbus.bus_get = (fn_bus_get)dlsym(lib, "dbus_bus_get");
    g_dbus.new_method_call =
        (fn_new_method_call)dlsym(lib, "dbus_message_new_method_call");
    g_dbus.send_block = (fn_send_block)dlsym(lib,
        "dbus_connection_send_with_reply_and_block");
    g_dbus.get_args = (fn_get_args)dlsym(lib, "dbus_message_get_args");
    g_dbus.append_args =
        (fn_append_args)dlsym(lib, "dbus_message_append_args");
    g_dbus.message_unref = (fn_message_unref)dlsym(lib, "dbus_message_unref");

    if (!g_dbus.error_init || !g_dbus.error_is_set || !g_dbus.error_free ||
        !g_dbus.bus_get || !g_dbus.new_method_call || !g_dbus.send_block ||
        !g_dbus.get_args || !g_dbus.append_args || !g_dbus.message_unref)
    {
        Log::warn("UbuntuTouchScreen",
                  "libdbus-1 is missing expected symbols — the display may "
                  "sleep during a race.");
        dlclose(lib);
        return false;
    }

    StkDBusError error;
    g_dbus.error_init(&error);
    // A shared connection: it is owned by libdbus and lives for the process,
    // which is exactly how long the wake lock is held.
    g_system_bus = g_dbus.bus_get(STK_DBUS_BUS_SYSTEM, &error);
    if (g_system_bus == NULL)
    {
        Log::warn("UbuntuTouchScreen", "No system bus (%s) — the display may "
                  "sleep during a race.",
                  error.message ? error.message : "unknown error");
        g_dbus.error_free(&error);
        return false;
    }
    return true;
}   // connectSystemBus

// ----------------------------------------------------------------------------
/** Sends one com.canonical.Unity.Screen call, and returns the reply. */
DBusMessage* callUnityScreen(const char *method, StkDBusError *error,
                             int *int32_arg)
{
    // Initialised up front: every early return below leaves the caller free to
    // read and free the error.
    g_dbus.error_init(error);

    DBusMessage *call = g_dbus.new_method_call("com.canonical.Unity.Screen",
        "/com/canonical/Unity/Screen", "com.canonical.Unity.Screen", method);
    if (call == NULL)
        return NULL;
    if (int32_arg != NULL)
    {
        g_dbus.append_args(call, STK_DBUS_TYPE_INT32, int32_arg,
                           STK_DBUS_TYPE_INVALID);
    }

    DBusMessage *reply = g_dbus.send_block(g_system_bus, call,
                                           2000/*timeout ms*/, error);
    g_dbus.message_unref(call);
    return reply;
}   // callUnityScreen

}   // namespace

// ----------------------------------------------------------------------------
void keepDisplayOn()
{
    if (!isUbuntuTouchClick() || g_display_on_cookie != -1)
        return;
    if (!connectSystemBus())
        return;

    StkDBusError error;
    DBusMessage *reply = callUnityScreen("keepDisplayOn", &error, NULL);
    if (reply == NULL)
    {
        Log::warn("UbuntuTouchScreen", "keepDisplayOn failed (%s) — the "
                  "display may sleep during a race.",
                  g_dbus.error_is_set(&error) && error.message ?
                  error.message : "no reply");
        g_dbus.error_free(&error);
        return;
    }

    int cookie = -1;
    if (g_dbus.get_args(reply, &error, STK_DBUS_TYPE_INT32, &cookie,
                        STK_DBUS_TYPE_INVALID))
    {
        g_display_on_cookie = cookie;
        Log::info("UbuntuTouchScreen",
                  "Display kept on for supertuxkart (request %d).", cookie);
    }
    else
    {
        Log::warn("UbuntuTouchScreen",
                  "keepDisplayOn gave no request id (%s).",
                  g_dbus.error_is_set(&error) && error.message ?
                  error.message : "unexpected reply");
        g_dbus.error_free(&error);
    }
    g_dbus.message_unref(reply);
}   // keepDisplayOn

// ----------------------------------------------------------------------------
void releaseDisplay()
{
    if (g_display_on_cookie == -1 || g_system_bus == NULL)
        return;

    StkDBusError error;
    DBusMessage *reply = callUnityScreen("removeDisplayOnRequest", &error,
                                         &g_display_on_cookie);
    if (reply != NULL)
        g_dbus.message_unref(reply);
    else
        g_dbus.error_free(&error);
    g_display_on_cookie = -1;
}   // releaseDisplay

}   // namespace UbuntuTouchScreen

#else

namespace UbuntuTouchScreen
{
    void keepDisplayOn() {}
    void releaseDisplay() {}
}   // namespace UbuntuTouchScreen

#endif
