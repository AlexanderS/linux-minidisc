#ifndef LIBNETMD_COMMON_H
#define LIBNETMD_COMMON_H

#include <usb.h>

/**
   Typedef that nearly all netmd_* functions use to identify the USB connection
   with the minidisc player.
*/
typedef usb_dev_handle*	netmd_dev_handle;

/**
  Function to exchange command/response buffer with minidisc player.

  @param dev device handle
  @param cmd buffer with the command, that should be send to the player
  @param cmdlen length of the command
  @param rsp buffer where the response should be written to
  @return number of bytes received if >0, or error if <0
*/
int netmd_exch_message(netmd_dev_handle *dev, unsigned char *cmd,
                       const size_t cmdlen, unsigned char *rsp);

#endif
