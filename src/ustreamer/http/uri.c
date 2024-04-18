/*****************************************************************************
#                                                                            #
#    uStreamer - Lightweight and fast MJPEG-HTTP streamer.                   #
#                                                                            #
#    Copyright (C) 2018-2022  Maxim Devaev <mdevaev@gmail.com>               #
#                                                                            #
#    This program is free software: you can redistribute it and/or modify    #
#    it under the terms of the GNU General Public License as published by    #
#    the Free Software Foundation, either version 3 of the License, or       #
#    (at your option) any later version.                                     #
#                                                                            #
#    This program is distributed in the hope that it will be useful,         #
#    but WITHOUT ANY WARRANTY; without even the implied warranty of          #
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           #
#    GNU General Public License for more details.                            #
#                                                                            #
#    You should have received a copy of the GNU General Public License       #
#    along with this program.  If not, see <https://www.gnu.org/licenses/>.  #
#                                                                            #
*****************************************************************************/


#include "uri.h"


bool us_uri_get_true(struct evkeyvalq *params, const char *key) {
	const char *value_str = evhttp_find_header(params, key);
	if (value_str != NULL) {
		if (
			value_str[0] == '1'
			|| !evutil_ascii_strcasecmp(value_str, "true")
			|| !evutil_ascii_strcasecmp(value_str, "yes")
		) {
			return true;
		}
	}
	return false;
}

char *us_uri_get_string(struct evkeyvalq *params, const char *key) {
	const char *const value_str = evhttp_find_header(params, key);
	if (value_str != NULL) {
		return evhttp_encode_uri(value_str);
	}
	return NULL;
}
