#ifndef _QR_JPEG_H_
#define _QR_JPEG_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <qrencode.h>
#include <jpeglib.h>

int generate_qr(const char *content, const char *out_path,
                int pixel_size, int margin, int quality);

#endif