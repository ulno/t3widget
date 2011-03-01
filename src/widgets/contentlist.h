/* Copyright (C) 2010 G.P. Halkes
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
#ifndef CONTENTLIST_H
#define CONTENTLIST_H

#include <string>
#include <vector>
using namespace std;

class string_list_t {
	public:
		virtual size_t get_length(void) const = 0;
		virtual const string *get_name(int idx) const = 0;
};

class file_list_t : public string_list_t {
	public:
		virtual bool is_dir(int idx) const = 0;
};

class file_name_list_t : public file_list_t {
	private:
		class file_name_entry_t {
			public:
				string name;
				bool is_dir;
				file_name_entry_t(void) : is_dir(false) {}
				file_name_entry_t(const char *_name, bool _isDir) : name(_name), is_dir(_isDir) {}
		};

		vector<file_name_entry_t> files;

		static bool sort_entries(file_name_entry_t first, file_name_entry_t second);

	public:
		virtual size_t get_length(void) const;
		virtual const string *get_name(int idx) const;
		virtual bool is_dir(int idx) const;
		void load_directory(string *dirName);
		size_t get_index(const string *name) const;
		file_name_list_t &operator=(const file_name_list_t& a);

		static string get_working_directory(void);
		static string get_directory(const char *directory);
		static bool is_dir(const string *current_dir, const char *name);

		class file_name_list_view_t : public file_list_t {
			private:
				file_name_list_t *base;
				vector<size_t> items;
			public:
				file_name_list_view_t(file_name_list_t *_base, bool showHidden, const string *filter);
				file_name_list_view_t(file_name_list_t *_base, const string *start);
				virtual size_t get_length(void) const;
				virtual const string *get_name(int idx) const;
				virtual bool is_dir(int idx) const;
				size_t get_index(const string *name) const;
		};
};

class multi_string_list_i {
	public:
		virtual size_t get_length(void) const = 0;
		virtual int get_columns(void) const = 0;
		virtual const string *get_item(int idx, int column) const = 0;
};

class multi_string_list_t : public multi_string_list_i {
	private:
		int columns;
		vector<const string *> *data;
	public:
		multi_string_list_t(int _columns);
		virtual size_t get_length(void) const;
		virtual int get_columns(void) const;
		virtual const string *get_item(int idx, int column) const;
		void add_line(const string *first, ...);
};

#endif
