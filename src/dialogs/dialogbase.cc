/* Copyright (C) 2011-2012 G.P. Halkes
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
#include "dialogbase.h"
#include "colorscheme.h"
#include "main.h"
#include "dialogs/dialog.h"
#include "dialogs/mainwindow.h"
#include "internal.h"

using namespace std;
namespace t3_widget {

dialog_base_list_t dialog_base_t::dialog_base_list;

dummy_widget_t *dialog_base_t::dummy;
sigc::connection dialog_base_t::init_connected = connect_on_init(sigc::ptr_fun(dialog_base_t::init));

void dialog_base_t::init(bool _init) {
	if (_init){
		if (dummy == NULL)
			dummy = new dummy_widget_t();
	} else {
		if (dummy != NULL)
			delete dummy;
	}
}

dialog_base_t::dialog_base_t(int height, int width, bool has_shadow) : redraw(true) {
	if ((window = t3_win_new(NULL, height, width, 0, 0, 0)) == NULL)
		throw bad_alloc();
	if (has_shadow) {
		if ((shadow_window = t3_win_new(NULL, height, width, 1, 1, 1)) == NULL)
			throw bad_alloc();
	}
	t3_win_set_anchor(shadow_window, window, 0);
	dialog_base_list.push_back(this);
	t3_win_set_restrict(window, NULL);
}

/** Create a new ::dialog_base_t.

    This constructor should only be called by ::main_window_base_t (through ::dialog_t).
*/
dialog_base_t::dialog_base_t(void) : redraw(false) {
	dialog_base_list.push_back(this);
}

dialog_base_t::~dialog_base_t() {
	for (dialog_base_list_t::iterator iter = dialog_base_list.begin(); iter != dialog_base_list.end(); iter++) {
		if ((*iter) == this) {
			dialog_base_list.erase(iter);
			break;
		}
	}
	for (widgets_t::iterator widget_iter = widgets.begin(); widget_iter != widgets.end(); widget_iter++) {
		if (*widget_iter != dummy)
			delete *widget_iter;
	}
}


void dialog_base_t::set_position(optint top, optint left) {
	if (!top.is_valid())
		top = t3_win_get_y(window);
	if (!left.is_valid())
		left = t3_win_get_x(window);

	t3_win_move(window, top, left);
}

bool dialog_base_t::set_size(optint height, optint width) {
	bool result = true;

	redraw = true;
	if (!height.is_valid())
		height = t3_win_get_height(window);
	if (!width.is_valid())
		width = t3_win_get_width(window);

	result &= (t3_win_resize(window, height, width) == 0);
	result &= (t3_win_resize(shadow_window, height, width) == 0);
	return result;
}


void dialog_base_t::update_contents(void) {
	if (redraw) {
		int i, x;

		redraw = false;
		t3_win_set_default_attrs(window, attributes.dialog);
		t3_win_set_default_attrs(shadow_window, attributes.shadow);

		/* Just clear the whole thing and redraw */
		t3_win_set_paint(window, 0, 0);
		t3_win_clrtobot(window);

		t3_win_box(window, 0, 0, t3_win_get_height(window), t3_win_get_width(window), 0);

		x = t3_win_get_width(shadow_window) - 1;
		for (i = t3_win_get_height(shadow_window) - 1; i > 0; i--) {
			t3_win_set_paint(shadow_window, i - 1, x);
			t3_win_addch(shadow_window, ' ', 0);
		}
		t3_win_set_paint(shadow_window, t3_win_get_height(shadow_window) - 1, 0);
		t3_win_addchrep(shadow_window, ' ', 0, t3_win_get_width(shadow_window));
	}

	for (widgets_t::iterator iter = widgets.begin(); iter != widgets.end(); iter++)
		(*iter)->update_contents();
}

void dialog_base_t::set_focus(focus_t focus) {
	(*current_widget)->set_focus(focus);
}

void dialog_base_t::show(void) {
	for (current_widget = widgets.begin();
		current_widget != widgets.end() && !(*current_widget)->accepts_focus();
		current_widget++)
	{}

	if (current_widget == widgets.end()) {
		widgets.push_front(dummy);
		current_widget = widgets.begin();
	}

	t3_win_show(window);
	if (shadow_window != NULL)
		t3_win_show(shadow_window);
}

void dialog_base_t::hide(void) {
	t3_win_hide(window);
	if (shadow_window != NULL)
		t3_win_hide(shadow_window);
	if (widgets.front() == dummy)
		widgets.pop_front();
}

void dialog_base_t::focus_next(void) {
	(*current_widget)->set_focus(window_component_t::FOCUS_OUT);
	do {
		current_widget++;
		if (current_widget == widgets.end())
			current_widget = widgets.begin();
	} while (!(*current_widget)->accepts_focus());

	(*current_widget)->set_focus(window_component_t::FOCUS_IN_FWD);
}

void dialog_base_t::focus_previous(void) {
	(*current_widget)->set_focus(window_component_t::FOCUS_OUT);

	do {
		if (current_widget == widgets.begin())
			current_widget = widgets.end();

		current_widget--;
	} while (!(*current_widget)->accepts_focus());

	(*current_widget)->set_focus(window_component_t::FOCUS_IN_BCK);
}

void dialog_base_t::set_child_focus(window_component_t *target) {
	widget_t *target_widget = dynamic_cast<widget_t *>(target);
	if (target_widget == NULL || !target_widget->accepts_focus())
		return;

	for (widgets_t::iterator iter = widgets.begin(); iter != widgets.end(); iter++) {
		if (*iter == target) {
			if (*current_widget != *iter) {
				(*current_widget)->set_focus(window_component_t::FOCUS_OUT);
				current_widget = iter;
				(*current_widget)->set_focus(window_component_t::FOCUS_SET);
			}
			return;
		} else {
			container_t *container = dynamic_cast<container_t *>(*iter);
			if (container != NULL && container->is_child(target)) {
				if (*current_widget != *iter) {
					(*current_widget)->set_focus(window_component_t::FOCUS_OUT);
					current_widget = iter;
				}
				container->set_child_focus(target);
				return;
			}
		}
	}
}

bool dialog_base_t::is_child(window_component_t *widget) {
	for (widgets_t::iterator iter = widgets.begin(); iter != widgets.end(); iter++) {
		if (*iter == widget) {
			return true;
		} else {
			container_t *container = dynamic_cast<container_t *>(*iter);
			if (container != NULL && container->is_child(widget))
				return true;
		}
	}
	return false;
}

void dialog_base_t::push_back(widget_t *widget) {
	if (!set_widget_parent(widget))
		return;
	if (widgets.size() > 0 && widgets.front() == dummy)
		widgets.pop_front();
	widgets.push_back(widget);
}

void dialog_base_t::force_redraw(void) {
	redraw = true;
	for (widgets_t::iterator iter = widgets.begin(); iter != widgets.end(); iter++)
		(*iter)->force_redraw();
}

void dialog_base_t::center_over(window_component_t *center) {
	t3_win_set_anchor(window, center->get_base_window(), T3_PARENT(T3_ANCHOR_CENTER) | T3_CHILD(T3_ANCHOR_CENTER));
	t3_win_move(window, 0, 0);
}

void dialog_base_t::force_redraw_all(void) {
	for (dialog_base_list_t::iterator iter = dialog_base_list.begin(); iter != dialog_base_list.end(); iter++)
		(*iter)->force_redraw();
}

}; // namespace