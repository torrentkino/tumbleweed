/*
Copyright 2010 Aiko Barz

This file is part of torrentkino.

torrentkino is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

torrentkino is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with torrentkino.  If not, see <http://www.gnu.org/licenses/>.
*/

void send_tcp( TCP_NODE *n );
void send_cork_start( TCP_NODE *n );
void send_data( TCP_NODE *n );
void send_mem( TCP_NODE *n, ITEM *item_r );
void send_file( TCP_NODE *n, ITEM *item_r );
void send_cork_stop( TCP_NODE *n );
