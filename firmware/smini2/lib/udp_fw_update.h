/* -*- c++ -*- */
/*
 * Copyright 2010 Ettus Research LLC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "net_common.h"

#define SMINI2_UDP_UPDATE_PORT 49154

typedef enum {
  SMINI2_FW_UPDATE_ID_WAT = ' ',

  SMINI2_FW_UPDATE_ID_OHAI_LOL = 'a',
  SMINI2_FW_UPDATE_ID_OHAI_OMG = 'A',

  SMINI2_FW_UPDATE_ID_WATS_TEH_FLASH_INFO_LOL = 'f',
  SMINI2_FW_UPDATE_ID_HERES_TEH_FLASH_INFO_OMG = 'F',

  SMINI2_FW_UPDATE_ID_ERASE_TEH_FLASHES_LOL = 'e',
  SMINI2_FW_UPDATE_ID_ERASING_TEH_FLASHES_OMG = 'E',

  SMINI2_FW_UPDATE_ID_R_U_DONE_ERASING_LOL = 'd',
  SMINI2_FW_UPDATE_ID_IM_DONE_ERASING_OMG = 'D',
  SMINI2_FW_UPDATE_ID_NOPE_NOT_DONE_ERASING_OMG = 'B',

  SMINI2_FW_UPDATE_ID_WRITE_TEH_FLASHES_LOL = 'w',
  SMINI2_FW_UPDATE_ID_WROTE_TEH_FLASHES_OMG = 'W',

  SMINI2_FW_UPDATE_ID_READ_TEH_FLASHES_LOL = 'r',
  SMINI2_FW_UPDATE_ID_KK_READ_TEH_FLASHES_OMG = 'R',

  SMINI2_FW_UPDATE_ID_RESET_MAH_COMPUTORZ_LOL = 's',
  SMINI2_FW_UPDATE_ID_RESETTIN_TEH_COMPUTORZ_OMG = 'S',

  SMINI2_FW_UPDATE_ID_I_CAN_HAS_HW_REV_LOL = 'v',
  SMINI2_FW_UPDATE_ID_HERES_TEH_HW_REV_OMG = 'V',

  SMINI2_FW_UPDATE_ID_KTHXBAI = '~'

} smini2_fw_update_id_t;

typedef struct {
  uint32_t proto_ver;
  uint32_t id;
  uint32_t seq;
  union {
    uint32_t ip_addr;
    uint32_t hw_rev;
    struct {
      uint32_t flash_addr;
      uint32_t length;
      uint8_t  data[256];
    } flash_args;
    struct {
      uint32_t sector_size_bytes;
      uint32_t memory_size_bytes;
    } flash_info_args;
  } data;
} smini2_fw_update_data_t;

void handle_udp_fw_update_packet(struct socket_address src, struct socket_address dst,
                                 unsigned char *payload, int payload_len);
