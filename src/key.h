/* Copyright (C) 2011-2013 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef T3_WIDGET_KEYS_H
#define T3_WIDGET_KEYS_H

#include <climits>
#include <t3widget/widget_api.h>

namespace t3_widget {

#if INT_MAX < 2147483647L
/** Integer type holding a single key symbol. */
typedef long key_t;
#else
typedef int key_t;
#endif

enum {
  EKEY_FIRST_SPECIAL = 0x110000,
};

/** List of special keys and masks generated by the key handling code. */
enum {
  EKEY_IGNORE = (key_t)-1,

  EKEY_END = EKEY_FIRST_SPECIAL,
  EKEY_HOME,
  EKEY_PGUP,
  EKEY_PGDN,
  EKEY_LEFT,
  EKEY_RIGHT,
  EKEY_UP,
  EKEY_DOWN,
  EKEY_DEL,
  EKEY_INS,
  EKEY_BS,
  EKEY_NL,
  EKEY_KP_CENTER,

  EKEY_KP_END,
  EKEY_KP_HOME,
  EKEY_KP_PGUP,
  EKEY_KP_PGDN,
  EKEY_KP_LEFT,
  EKEY_KP_RIGHT,
  EKEY_KP_UP,
  EKEY_KP_DOWN,
  EKEY_KP_DEL,
  EKEY_KP_INS,
  EKEY_KP_NL,
  EKEY_KP_DIV,
  EKEY_KP_MUL,
  EKEY_KP_PLUS,
  EKEY_KP_MINUS,

  EKEY_F1 = 0x110100,
  EKEY_F2,
  EKEY_F3,
  EKEY_F4,
  EKEY_F5,
  EKEY_F6,
  EKEY_F7,
  EKEY_F8,
  EKEY_F9,
  EKEY_F10,
  EKEY_F11,
  EKEY_F12,
  EKEY_F13,
  EKEY_F14,
  EKEY_F15,
  EKEY_F16,
  EKEY_F17,
  EKEY_F18,
  EKEY_F19,
  EKEY_F20,
  EKEY_F21,
  EKEY_F22,
  EKEY_F23,
  EKEY_F24,
  EKEY_F25,
  EKEY_F26,
  EKEY_F27,
  EKEY_F28,
  EKEY_F29,
  EKEY_F30,
  EKEY_F31,
  EKEY_F32,
  EKEY_F33,
  EKEY_F34,
  EKEY_F35,
  EKEY_F36,

  /* Make sure the synthetic keys are out of the way of future aditions. */
  /** Key symbol indicating the terminal was resized. */
  EKEY_RESIZE = 0x111000,
  /** Key symbol used when a widget has indicated that a pressed key is a hot key.
      Testing whether a key is a hot key for a widget is performed using the
          widget_t::is_hotkey function. If that returns @c true, the
          widget_t::process_key is called with this value.
  */
  EKEY_HOTKEY,
  /** Key symbol indicating that the ::signal_update function was called and the @c
     update_notification signal should be emitted. */
  EKEY_EXTERNAL_UPDATE,
  /** Key symbol indicating that the @c terminal_settings_changed signal should be emitted. */
  EKEY_UPDATE_TERMINAL,
  /** Key symbol indicating that there is a new #mouse_event_t available in the mouse events queue.
     */
  EKEY_MOUSE_EVENT,
  /** Key symbol indicating that the async_safe_exit_main_loop function was called.
      The exit code is encoded in the @c key_t value, which means that all values up
      to EXIT_MAIN_LOOP + 256 cause the program to exit. */
  EKEY_EXIT_MAIN_LOOP,
  /** Following text was pasted. Text runs until @c EKEY_PASTE_END. */
  EKEY_PASTE_START = EKEY_EXIT_MAIN_LOOP + 256,
  /** Pasted text stops. */
  EKEY_PASTE_END,

  /** Symbolic name for the escape key. */
  EKEY_ESC = 27,
  /** Value indicating the shift key was pressed with a key press, used as bit-field. */
  EKEY_SHIFT = 0x40000000,
  /** Value indicating the meta key was pressed with a key press, used as bit-field. */
  EKEY_META = 0x20000000,
  /** Value indicating the control key was pressed with a key press, used as bit-field. */
  EKEY_CTRL = 0x10000000,
  /** Value indicating the key was inserted using #insert_protected_key, used as bit-field. */
  EKEY_PROTECT = 0x08000000,
  /** Bit mask to retrieve the base key symbol. */
  EKEY_KEY_MASK = 0x1fffff
};

/** Retrieve a key from the input queue. */
T3_WIDGET_API key_t read_key();
/** Set the timeout for handling escape sequences.

    The value of the @p msec parameter can have the following values:
    @li 0 to disable the timeout completely. To generate #EKEY_ESC, the user
        needs to press the escape key twice.
    @li a positive value to accept the keys within @p msec milliseconds as part
        of an escape sequence. Pressing escape once and waiting for the timeout
        to pass will result in #EKEY_ESC.
    @li a positive value to accept the keys within @p msec milliseconds as part
        of an escape sequence. Pressing escape once and waiting for the timeout
        to pass will result in the escape key press being ignored. To generate
        #EKEY_ESC, the user needs to press the escape key twice.
*/
T3_WIDGET_API void set_key_timeout(int msec);

/** Get the timeout for handling escape sequences.

    See set_key_timeout for details about the return value.
*/
T3_WIDGET_API int get_key_timeout();

/** Request the generation of the @c update_notification signal.

    This function is part of the multi-threading support of libt3widget.
    Updates to widgets are only allowed from the thread running the #main_loop
    function. The signal_update function can be called from any thread and will
    cause an @c update_notification signal to be generated on the thread
    running the #main_loop function.
*/
T3_WIDGET_API void signal_update();

};  // namespace
#endif
