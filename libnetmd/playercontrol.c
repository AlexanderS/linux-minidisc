/* playercontrol.c
 *      Copyright (C) 2004, Bertrik Sikken
 *      Copyright (C) 2011, Alexander Sulfrian
 *
 * This file is part of libnetmd.
 *
 * libnetmd is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Libnetmd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include "playercontrol.h"
#include "utils.h"
#include "const.h"


static netmd_error netmd_playback_control(netmd_dev_handle* dev, unsigned char code)
{
    unsigned char request[] = {0x00, 0x18, 0xc3, 0xff, 0x00, 0x00,
                               0x00, 0x00};
    unsigned char buf[255];
    int size;

    request[4] = code;
    size = netmd_exch_message(dev, request, sizeof(request), buf);

    if (size < 1) {
        return NETMD_ERROR;
    }

    return NETMD_NO_ERROR;
}

netmd_error netmd_play(netmd_dev_handle *dev)
{
    return netmd_playback_control(dev, NETMD_PLAY);
}

netmd_error netmd_pause(netmd_dev_handle *dev)
{
    return netmd_playback_control(dev, NETMD_PAUSE);
}

netmd_error netmd_fast_forward(netmd_dev_handle *dev)
{
    return netmd_playback_control(dev, NETMD_FFORWARD);
}

netmd_error netmd_rewind(netmd_dev_handle *dev)
{
    return netmd_playback_control(dev, NETMD_REWIND);
}

netmd_error netmd_stop(netmd_dev_handle* dev)
{
    unsigned char request[] = {0x00, 0x18, 0xc5, 0xff, 0x00, 0x00,
                               0x00, 0x00};
    unsigned char buf[255];
    int size;

    size = netmd_exch_message(dev, request, sizeof(request), buf);

    if(size < 0) {
        return NETMD_ERROR;
    }

    return NETMD_NO_ERROR;
}

netmd_error netmd_set_playmode(netmd_dev_handle* dev, const uint16_t playmode)
{
    unsigned char request[] = {0x00, 0x18, 0xd1, 0xff, 0x01, 0x00,
                               0x00, 0x00, 0x88, 0x08, 0x00, 0x00,
                               0x00};
    unsigned char buf[255];

    request[10] = (unsigned char)(playmode >> 8) & 0xff;
    request[11] = (unsigned int)playmode & 0xff;

    /* TODO: error checking */
    netmd_exch_message(dev, request, sizeof(request), buf);

    return NETMD_NO_ERROR;
}

netmd_error netmd_set_track(netmd_dev_handle* dev, const uint16_t track)
{
    unsigned char request[] = {0x00, 0x18, 0x50, 0xff, 0x01, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x00};
    unsigned char buf[255];

    proper_to_bcd(track, request + 9, 2);

    /* TODO: error checking */
    netmd_exch_message(dev, request, sizeof(request), buf);

    return NETMD_NO_ERROR;
}

netmd_error netmd_change_track(netmd_dev_handle* dev, const uint16_t direction)
{
    unsigned char request[] = {0x00, 0x18, 0x50, 0xff, 0x10, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x00};
    unsigned char buf[255];

    request[9] = (unsigned char)(direction >> 8) & 0xff;
    request[10] = (unsigned char)direction & 0xff;

    /* TODO: error checking */
    netmd_exch_message(dev, request, sizeof(request), buf);

    return NETMD_NO_ERROR;
}

netmd_error netmd_get_track(netmd_dev_handle* dev, uint16_t *track)
{
    unsigned char request[] = {0x00, 0x18, 0x09, 0x80, 0x01, 0x04,
                               0x30, 0x88, 0x02, 0x00, 0x30, 0x88,
                               0x05, 0x00, 0x30, 0x00, 0x03, 0x00,
                               0x30, 0x00, 0x02, 0x00, 0xff, 0x00,
                               0x00, 0x00, 0x00, 0x00};
    unsigned char buf[255];

    /* TODO: error checking */
    netmd_exch_message(dev, request, 28, buf);
    *track = bcd_to_proper(buf + 35, 2) & 0xffff;

    return NETMD_NO_ERROR;
}

netmd_error netmd_track_next(netmd_dev_handle* dev)
{
    return netmd_change_track(dev, NETMD_TRACK_NEXT);
}

netmd_error netmd_track_previous(netmd_dev_handle* dev)
{
    return netmd_change_track(dev, NETMD_TRACK_PREVIOUS);
}

netmd_error netmd_track_restart(netmd_dev_handle* dev)
{
    return netmd_change_track(dev, NETMD_TRACK_RESTART);
}

netmd_error netmd_set_time(netmd_dev_handle* dev, const uint16_t track, const netmd_time* time)
{
    unsigned char request[] = {0x00, 0x18, 0x50, 0xff, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00};
    unsigned char buf[255];

    proper_to_bcd(track, request + 9, 2);
    proper_to_bcd(time->hour, request + 11, 1);
    proper_to_bcd(time->minute, request + 12, 1);
    proper_to_bcd(time->second, request + 13, 1);
    proper_to_bcd(time->frame, request + 14, 1);

    /* TODO: error checking */
    netmd_exch_message(dev, request, sizeof(request), buf);

    return NETMD_NO_ERROR;
}

const netmd_time* netmd_parse_time(unsigned char* src, netmd_time* time)
{
    time->hour = bcd_to_proper(src, 2) & 0xffff;
    time->minute = bcd_to_proper(src + 2, 1) & 0xff;
    time->second = bcd_to_proper(src + 3, 1) & 0xff;
    time->frame = bcd_to_proper(src + 4, 1) & 0xff;

    return time;
}

netmd_error netmd_get_position(netmd_dev_handle* dev, netmd_time* time)
{
    unsigned char request[] = {0x00, 0x18, 0x09, 0x80, 0x01, 0x04,
                               0x30, 0x88, 0x02, 0x00, 0x30, 0x88,
                               0x05, 0x00, 0x30, 0x00, 0x03, 0x00,
                               0x30, 0x00, 0x02, 0x00, 0xff, 0x00,
                               0x00, 0x00, 0x00, 0x00};
    unsigned char buf[255];

    /* TODO: error checking */
    netmd_exch_message(dev, request, sizeof(request), buf);
    time->hour = bcd_to_proper(buf + 37, 1) & 0xff;
    time->minute = bcd_to_proper(buf + 38, 1) & 0xff;
    time->second = bcd_to_proper(buf + 39, 1) & 0xff;
    time->frame = bcd_to_proper(buf + 40, 1) & 0xff;

    return NETMD_NO_ERROR;
}

netmd_error netmd_get_disc_capacity(netmd_dev_handle* dev, netmd_disc_capacity* capacity)
{
    unsigned char request[] = {0x00, 0x18, 0x06, 0x02, 0x10, 0x10,
                               0x00, 0x30, 0x80, 0x03, 0x00, 0xff,
                               0x00, 0x00, 0x00, 0x00, 0x00};
    unsigned char buf[255];

    /* TODO: error checking */
    netmd_exch_message(dev, request, sizeof(request), buf);
    netmd_parse_time(buf + 27, &capacity->recorded);
    netmd_parse_time(buf + 34, &capacity->total);
    netmd_parse_time(buf + 41, &capacity->available);

    return NETMD_NO_ERROR;
}
