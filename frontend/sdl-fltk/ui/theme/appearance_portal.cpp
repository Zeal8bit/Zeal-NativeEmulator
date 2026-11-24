/*
 * SPDX-FileCopyrightText: 2026 Zeal 8-bit Computer <contact@zeal8bit.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "theme/system_appearance.h"
#include <cstring>
#include <dbus/dbus.h>

namespace zui::theme {

constexpr char SERVICE[] = "org.freedesktop.portal.Desktop";
constexpr char PATH[] = "/org/freedesktop/portal/desktop";
constexpr char SETTINGS[] = "org.freedesktop.portal.Settings";
const char* const GROUP = "org.freedesktop.appearance";
const char* const KEY = "color-scheme";

static DBusConnection* bus;
static bool initialized;
static bool light;

static void read_value(DBusMessageIter iter)
{
    // Read wraps the variant once more than SettingChanged.
    while (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_VARIANT) {
        DBusMessageIter child;
        dbus_message_iter_recurse(&iter, &child);
        iter = child;
    }
    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_UINT32) {
        dbus_uint32_t scheme;
        dbus_message_iter_get_basic(&iter, &scheme);
        // 0 = desktop default, 1 = prefer dark, 2 = prefer light.
        light = scheme == 0 || scheme == 2;
    }
}

static void connect_portal()
{
    bus = dbus_bus_get_private(DBUS_BUS_SESSION, nullptr);
    if (bus == nullptr) {
        return;
    }
    dbus_connection_set_exit_on_disconnect(bus, false);
    dbus_bus_add_match(bus, "type='signal',sender='org.freedesktop.portal.Desktop',"
        "interface='org.freedesktop.portal.Settings',member='SettingChanged',"
        "path='/org/freedesktop/portal/desktop',arg0='org.freedesktop.appearance',"
        "arg1='color-scheme'", nullptr);
    DBusMessage* request = dbus_message_new_method_call(SERVICE, PATH, SETTINGS, "Read");
    if (request == nullptr) {
        return;
    }
    if (dbus_message_append_args(request, DBUS_TYPE_STRING, &GROUP, DBUS_TYPE_STRING, &KEY,
                                 DBUS_TYPE_INVALID)) {
        DBusMessage* reply = dbus_connection_send_with_reply_and_block(bus, request, 500, nullptr);
        if (reply) {
            DBusMessageIter iter;
            if (dbus_message_iter_init(reply, &iter)) {
                read_value(iter);
            }
            dbus_message_unref(reply);
        }
    }
    dbus_message_unref(request);
}

bool system_uses_light_theme()
{
    if (!initialized) {
        initialized = true;
        connect_portal();
    }
    if (bus == nullptr) {
        return false;
    }
    dbus_connection_read_write(bus, 0);
    while (DBusMessage* message = dbus_connection_pop_message(bus)) {
        if (dbus_message_is_signal(message, SETTINGS, "SettingChanged") &&
            dbus_message_has_signature(message, "ssv")) {
            DBusMessageIter iter;
            dbus_message_iter_init(message, &iter);
            const char* group;
            const char* key;
            dbus_message_iter_get_basic(&iter, &group);
            dbus_message_iter_next(&iter);
            dbus_message_iter_get_basic(&iter, &key);
            dbus_message_iter_next(&iter);
            if (strcmp(group, GROUP) == 0 && strcmp(key, KEY) == 0) {
                read_value(iter);
            }
        }
        dbus_message_unref(message);
    }
    return light;
}

void close_system_appearance()
{
    if (bus) {
        dbus_connection_close(bus);
        dbus_connection_unref(bus);
        bus = nullptr;
    }
    initialized = false;
    light = false;
}

} // namespace zui::theme
