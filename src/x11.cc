/* Copyright (C) 2011 G.P. Halkes
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
#include <stdlib.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <cstring>
#include <string>
#include <pthread.h>
#include <errno.h>
#include <list>

#ifdef HAS_SELECT_H
#include <sys/select.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#include "log.h"
#include "ptr.h"
#include "x11.h"

// FIXME: remove incr_sends on long periods of inactivity

using namespace std;

namespace t3_widget {

enum clipboard_action_t {
	ACTION_NONE,
	CONVERT_CLIPBOARD,
	CONVERT_PRIMARY,
	CLAIM_CLIPBOARD,
	CLAIM_PRIMARY,
	RELEASE_SELECTIONS
};

struct incr_send_data_t {
	Window window;
	linked_ptr<string> data;
	Atom property;
	size_t offset;

	incr_send_data_t(Window _window, linked_ptr<string> _data, Atom _property) : window(_window),
		data(_data), property(_property), offset(0) {}
};
typedef list<incr_send_data_t> incr_send_list_t;
static incr_send_list_t incr_sends;

static clipboard_action_t action;
static bool conversion_succeeded;

/* Use CurrentTime as "Invalid" value, as it will never be returned by anything. */
static Time clipboard_owner_since = CurrentTime,
	primary_owner_since = CurrentTime;
static Time conversion_started_at;

static Display *display;
static Window root, window;

static size_t max_data;

static bool receive_incr;

static pthread_t x11_event_thread;
static pthread_mutex_t clipboard_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t clipboard_signal = PTHREAD_COND_INITIALIZER;

static bool x11_initialized;
static bool x11_error;
static bool end_connection;

static const char *atom_names[] = {
	"CLIPBOARD",
	"PRIMARY",
	"TARGETS",
	"TIMESTAMP",
	"MULTIPLE",
	"UTF8_STRING",
	"GDK_SELECTION", // Use same name as GDK to save on atoms
	"INCR",
	"ATOM",
	"ATOM_PAIR",
};
enum {
	CLIPBOARD,
	PRIMARY,
	TARGETS,
	TIMESTAMP,
	MULTIPLE,
	UTF8_STRING,
	GDK_SELECTION,
	INCR,
	ATOM,
	ATOM_PAIR,
};
#define ATOM_COUNT (sizeof(atom_names) / sizeof(atom_names[0]))
static Atom atoms[ATOM_COUNT];


#define DATA_BLOCK_SIZE 4000
static string retrieved_data;


static long retrieve_data(void) {
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop = NULL;
	unsigned long offset = 0;

	do {
		if (XGetWindowProperty(display, window, atoms[GDK_SELECTION], offset,
			DATA_BLOCK_SIZE, False, AnyPropertyType, &actual_type, &actual_format,
			&nitems, &bytes_after, &prop) != Success)
		{
			retrieved_data.clear();
			return -1;
		} else {
			retrieved_data.append((char *) prop, nitems);
			offset += nitems / 4;
			XFree(prop);
		}
		prop = NULL;
	} while (bytes_after);
	return offset;
}

static void claim(Time *since, Atom selection) {
	XSetSelectionOwner(display, selection, window, *since);
	if (XGetSelectionOwner(display, selection) != window)
		*since = CurrentTime;
	pthread_cond_signal(&clipboard_signal);
}

static bool send_selection(Window requestor, Atom target, Atom property, linked_ptr<string> data, Time since) {
	if (target == atoms[TARGETS]) {
		XChangeProperty(display, requestor, property, atoms[ATOM], 32, PropModeReplace, (unsigned char *) &atoms[TARGETS], 4);
		return true;
	} else if (target == atoms[TIMESTAMP]) {
		XChangeProperty(display, requestor, property, atoms[TIMESTAMP], 32, PropModeReplace, (unsigned char *) &since, 1);
		return true;
	} else if (target == atoms[UTF8_STRING]) {
		if (data->size() > max_data) {
			XChangeProperty(display, requestor, property, atoms[UTF8_STRING], 8, PropModeReplace,
				(unsigned char *) data->data(), data->size());
		} else {
			long size = data->size();
			incr_sends.push_back(incr_send_data_t(requestor, data, property));
			XSelectInput(display, requestor, PropertyChangeMask);
			XChangeProperty(display, requestor, property, atoms[INCR], 32, PropModeReplace, (unsigned char *) &size, 1);
		}
		return true;
	} else if (target == atoms[MULTIPLE]) {
		Atom actual_type, *requested_conversions;
		int actual_format;
		unsigned long nitems, bytes_after, i;

		if (XGetWindowProperty(display, requestor, property, 0, 100, False, atoms[ATOM_PAIR],
				&actual_type, &actual_format, &nitems, &bytes_after, (unsigned char **) &requested_conversions) != Success ||
				bytes_after != 0 || nitems & 1)
			return false;

		for (i = 0; i < nitems; i += 2) {
			if (requested_conversions[i] == atoms[MULTIPLE] || !send_selection(requestor, requested_conversions[i],
					requested_conversions[i + 1], data, since))
				requested_conversions[i + 1] = None;
		}
		XChangeProperty(display, requestor, property, atoms[ATOM_PAIR], 32, PropModeReplace,
			(unsigned char *) requested_conversions, nitems);
		return true;
	} else {
		return false;
	}
}

static void handle_property_notify(XEvent &event) {
	lprintf("Handling PropertyNotify\n");
	if (event.xproperty.window == window) {
		if (event.xproperty.atom == XA_WM_NAME) {
			switch (action) {
				case CONVERT_CLIPBOARD:
				case CONVERT_PRIMARY:
					retrieved_data.clear();
					conversion_succeeded = false;
					conversion_started_at = event.xproperty.time;
					/* Make sure that the target property does not exist */
					XDeleteProperty(display, window, atoms[GDK_SELECTION]);
					XConvertSelection(display, atoms[action == CONVERT_CLIPBOARD ? CLIPBOARD : PRIMARY],
						atoms[UTF8_STRING], atoms[GDK_SELECTION], window, conversion_started_at);
					break;
				case CLAIM_CLIPBOARD:
					clipboard_owner_since = event.xproperty.time;
					claim(&clipboard_owner_since, atoms[CLIPBOARD]);
					break;
				case CLAIM_PRIMARY:
					primary_owner_since = event.xproperty.time;
					claim(&primary_owner_since, atoms[PRIMARY]);
					break;
				default:
					break;
			}
		} else if (event.xproperty.atom == atoms[GDK_SELECTION]) {
			if (receive_incr && event.xproperty.state == PropertyNewValue) {
				long result;
				if ((result = retrieve_data()) <= 0) {
					receive_incr = false;
					conversion_succeeded = result == 0;
					pthread_cond_signal(&clipboard_signal);
				}
				XDeleteProperty(display, window, atoms[GDK_SELECTION]);
			}
		}
	} else {
		if (event.xproperty.state != PropertyDelete || incr_sends.empty())
			return;

		for (incr_send_list_t::iterator iter = incr_sends.begin(); iter != incr_sends.end(); iter++) {
			if (iter->window == event.xproperty.window) {
				unsigned long size = iter->data->size() - iter->offset;
				if (size > max_data)
					size = max_data;
				XChangeProperty(display, iter->window, iter->property, atoms[UTF8_STRING], 8, PropModeReplace,
					(unsigned char *) iter->data->data() + iter->offset, size);
				if (size == 0) {
					XSelectInput(display, iter->window, 0);
					incr_sends.erase(iter);
				}
				iter->offset += size;
				break;
			}
		}
	}
}


static void *process_events(void *arg) {
	XEvent event;
	(void) arg;

	pthread_mutex_lock(&clipboard_lock);
	while (1) {
		if (!XPending(display)) {
			fd_set read_fds;

			FD_ZERO(&read_fds);
			FD_SET(ConnectionNumber(display), &read_fds);
			pthread_mutex_unlock(&clipboard_lock);
			select(ConnectionNumber(display) + 1, &read_fds, NULL, NULL, NULL);
			pthread_mutex_lock(&clipboard_lock);
		}
		if (x11_error) {
			pthread_mutex_unlock(&clipboard_lock);
			return NULL;
		}
		XNextEvent(display, &event);

		switch (event.type) {
			case PropertyNotify:
				handle_property_notify(event);
				break;

			case SelectionNotify: {
				/* Conversion failed. */
				if (event.xselection.property == None) {
					if (action == CONVERT_CLIPBOARD || action == CONVERT_PRIMARY)
						pthread_cond_signal(&clipboard_signal);
					break;
				}

				if (action != CONVERT_CLIPBOARD && action != CONVERT_PRIMARY) {
					XDeleteProperty(display, window, event.xselection.property);
					break;
				}

				if (event.xselection.property != atoms[GDK_SELECTION] ||
						event.xselection.time != conversion_started_at ||
						(action == CONVERT_CLIPBOARD && event.xselection.selection != atoms[CLIPBOARD]) ||
						(action == CONVERT_PRIMARY && event.xselection.selection != atoms[PRIMARY]) ||
						(event.xselection.target != atoms[UTF8_STRING] && event.xselection.target != atoms[INCR]))
				{
					XDeleteProperty(display, window, event.xselection.property);
					pthread_cond_signal(&clipboard_signal);
					break;
				}

				if (event.xselection.target == atoms[INCR]) {
					receive_incr = true;
				} else if (event.xselection.target == atoms[UTF8_STRING]) {
					if (retrieve_data() >= 0)
						conversion_succeeded = true;
					pthread_cond_signal(&clipboard_signal);
				} else {
					pthread_cond_signal(&clipboard_signal);
				}
				XDeleteProperty(display, window, atoms[GDK_SELECTION]);
				break;
			}
			case SelectionClear:
				if (event.xselectionclear.selection == atoms[CLIPBOARD]) {
					clipboard_data = NULL;
					clipboard_owner_since = CurrentTime;
				} else if (event.xselectionclear.selection == atoms[PRIMARY]) {
					primary_data = NULL;
					primary_owner_since = CurrentTime;
				}
				if (action == RELEASE_SELECTIONS && clipboard_owner_since == CurrentTime && primary_owner_since == CurrentTime)
					pthread_cond_signal(&clipboard_signal);
				break;

			case SelectionRequest: {
				XEvent reply;
				linked_ptr<string> data;
				Time since;

				reply.type = SelectionNotify;
				reply.xselection.requestor = event.xselectionrequest.requestor;
				reply.xselection.selection = event.xselectionrequest.selection;
				reply.xselection.target = event.xselectionrequest.target;
				if (event.xselectionrequest.target == atoms[MULTIPLE] && event.xselectionrequest.property == None) {
					reply.xselection.property = None;
				} else {
					reply.xselection.property = event.xselectionrequest.property == None ? event.xselectionrequest.target:
						event.xselectionrequest.property;
					if (event.xselectionrequest.selection == atoms[CLIPBOARD] && clipboard_owner_since != CurrentTime) {
						data = clipboard_data;
						since = clipboard_owner_since;
					} else if (event.xselectionrequest.selection == atoms[PRIMARY] && primary_owner_since != CurrentTime) {
						data = primary_data;
						since = primary_owner_since;
					} else {
						reply.xselection.property = None;
					}
				}

				if (reply.xselection.property != None && !send_selection(event.xselectionrequest.requestor,
						event.xselectionrequest.target, reply.xselection.property, data, since))
					reply.xselection.property = None;

				XSendEvent(display, event.xselectionrequest.requestor, False, 0, &reply);
				break;
			}
			case ClientMessage:
				lprintf("ClientMessage received: %d\n", end_connection);
				if (end_connection) {
					XCloseDisplay(display);
					pthread_mutex_unlock(&clipboard_lock);
					return NULL;
				}
				break;
		}
	}
	return NULL;
}

static int error_handler(Display *_display, XErrorEvent *error) {
#ifdef _T3_WIDGET_DEBUG
	char error_text[1024];
	XGetErrorText(_display, error->error_code, error_text, sizeof(error_text));

	lprintf("X11 error handler: %s\n", error_text);
#else
	(void) _display;
	(void) error;
#endif
	if (!x11_initialized)
		x11_error = true;
	return 0;
}

static int io_error_handler(Display *_display) {
	(void) _display;
	lprintf("X11 IO error\n");
	x11_error = true;
	return 0;
}

static void stop_x11(void) {
	void *retval;
	XEvent event;

	pthread_mutex_lock(&clipboard_lock);
	/* If x11_error has been set, the event handling thread will stop, or will
	   have stopped already. Also, if this is the case, the connection is broken,
	   which means we can't send anything anyway. Thus we skip the client message
	   if x11_error is set. */
	if (!x11_error) {
		end_connection = true;

		/* We need to wake up the other thread, such that it will close the connection.
		   Thus we send it a ClientMessage. */
		event.type = ClientMessage;
		event.xclient.window = window;
		event.xclient.format = 8;
		event.xclient.message_type = None;
		lprintf("Sending ClientMessage\n");
		XSendEvent(display, window, False, 0, &event);
		XFlush(display);
	}
	pthread_mutex_unlock(&clipboard_lock);
	pthread_join(x11_event_thread, &retval);
}

bool init_x11(void) {
	XEvent event;
	int black;
	char **atom_names_local = NULL;
	long max_request;
	size_t i;

	if ((atom_names_local = (char **) malloc(sizeof(char *) * ATOM_COUNT)) == NULL)
		return false;

	for (i = 0; i < ATOM_COUNT; i++) atom_names_local[i] = NULL;

	for (i = 0; i < ATOM_COUNT; i++) {
		if ((atom_names_local[i] = strdup(atom_names[i])) == NULL)
			goto error_exit;
	}

	// Make sure X11 errors don't abort the program
	XSetErrorHandler(error_handler);
	XSetIOErrorHandler(io_error_handler);

	if ((display = XOpenDisplay(NULL)) == NULL)
		goto error_exit;

	max_request = XMaxRequestSize(display);
	if (max_request > DATA_BLOCK_SIZE * 4 + 100)
		max_data = DATA_BLOCK_SIZE * 4;
	else
		max_data = max_request - 100;

	root = XDefaultRootWindow(display);
	black = BlackPixel(display, DefaultScreen(display));
	window = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, black, black);

	while (XPending(display))
		XNextEvent(display, &event);
	if (x11_error)
		goto error_exit;

	if (!XInternAtoms(display, atom_names_local, ATOM_COUNT, False, atoms))
		goto error_exit;

	for (i = 0; i < ATOM_COUNT; i++)
		free(atom_names_local[i]);
	free(atom_names_local);
	atom_names_local = NULL;

	XSelectInput(display, window, PropertyChangeMask);

	while (XPending(display))
		XNextEvent(display, &event);
	if (x11_error)
		goto error_exit;

	x11_initialized = true;
	pthread_create(&x11_event_thread, NULL, process_events, NULL);
	atexit(stop_x11);
	return true;

error_exit:
	if (atom_names_local != NULL) {
		for (i = 0; i < ATOM_COUNT; i++)
			free(atom_names_local[i]);
		free(atom_names_local);
	}
	if (display != NULL)
		XCloseDisplay(display);
	return false;
}

bool x11_working(void) {
	return x11_initialized && !x11_error;
}

static struct timespec timeout_time(int usec) {
	struct timeval timeval;
	struct timespec result;

	gettimeofday(&timeval, NULL);
	timeval.tv_sec += usec / 1000000;
	timeval.tv_usec += usec % 1000000;
	if (timeval.tv_usec > 1000000) {
		timeval.tv_sec++;
		timeval.tv_usec -= 1000000;
	}
	result.tv_sec = timeval.tv_sec;
	result.tv_nsec = (long) timeval.tv_usec * 1000;
	return result;
}

linked_ptr<string> get_x11_selection(bool clipboard) {
	struct timespec timeout = timeout_time(1000000);
	linked_ptr<string> result;

	if (!x11_working()) {
		lprintf("X11 not initialized\n");
		return clipboard ? clipboard_data : primary_data;
	}

	pthread_mutex_lock(&clipboard_lock);
	if ((clipboard && clipboard_owner_since == CurrentTime) || (!clipboard && primary_owner_since == CurrentTime)) {
		action = clipboard ? CONVERT_CLIPBOARD : CONVERT_PRIMARY;
		XChangeProperty(display, window, XA_WM_NAME, XA_STRING, 8, PropModeAppend, NULL, 0);
		XFlush(display);
		if (pthread_cond_timedwait(&clipboard_signal, &clipboard_lock, &timeout) != ETIMEDOUT &&
				conversion_succeeded)
			result = new string(retrieved_data);
		action = ACTION_NONE;
	} else {
		result = clipboard ? clipboard_data : primary_data;
	}
	pthread_mutex_unlock(&clipboard_lock);
	return result;
}

void claim_selection(bool clipboard, string *data) {
	struct timespec timeout = timeout_time(1000000);

	if (!x11_working()) {
		if (clipboard)
			clipboard_data = data;
		else
			primary_data = data;
		return;
	}

	pthread_mutex_lock(&clipboard_lock);
	if (clipboard) {
		action = CLAIM_CLIPBOARD;
		clipboard_data = data;
	} else {
		action = CLAIM_PRIMARY;
		primary_data = data;
	}
	XChangeProperty(display, window, XA_WM_NAME, XA_STRING, 8, PropModeAppend, NULL, 0);
	XFlush(display);
	pthread_cond_timedwait(&clipboard_signal, &clipboard_lock, &timeout);
	action = ACTION_NONE;
	pthread_mutex_unlock(&clipboard_lock);
}

void release_selections(void) {
	struct timespec timeout = timeout_time(1000000);

	if (!x11_working())
		return;

	pthread_mutex_lock(&clipboard_lock);
	if (clipboard_owner_since == CurrentTime && primary_owner_since == CurrentTime) {
		pthread_mutex_unlock(&clipboard_lock);
		return;
	}

	action = RELEASE_SELECTIONS;
	if (clipboard_owner_since != CurrentTime)
		XSetSelectionOwner(display, atoms[CLIPBOARD], None, CurrentTime);
	if (primary_owner_since != CurrentTime)
		XSetSelectionOwner(display, atoms[PRIMARY], None, CurrentTime);
	XFlush(display);
	pthread_cond_timedwait(&clipboard_signal, &clipboard_lock, &timeout);
	action = ACTION_NONE;
	pthread_mutex_unlock(&clipboard_lock);
}

}; // namespace