#include "qr_jpeg.h"

static int write_qr_jpeg(const char *out_path, QRcode *qrcode,
                         int pixel_size, int margin, int quality)
{
    int qr_w = qrcode->width;
    int img_size = (qr_w + margin * 2) * pixel_size;
    int buf_size = img_size * img_size * 3;

    unsigned char *buf = malloc(buf_size);
    if (!buf)
    {
        fprintf(stderr, "malloc buffer failed\n");
        return -1;
    }

    memset(buf, 0xFF, buf_size);

    for (int y = 0; y < qr_w; y++)
    {
        for (int x = 0; x < qr_w; x++)
        {
            if (qrcode->data[y * qr_w + x] & 0x01)
            {
                int start_x = (x + margin) * pixel_size;
                int start_y = (y + margin) * pixel_size;
                for (int py = 0; py < pixel_size; py++)
                {
                    int row = start_y + py;
                    for (int px = 0; px < pixel_size; px++)
                    {
                        int idx = (row * img_size + start_x + px) * 3;
                        buf[idx + 0] = 0x00;
                        buf[idx + 1] = 0x00;
                        buf[idx + 2] = 0x00;
                    }
                }
            }
        }
    }

    unsigned char *rotated = malloc(buf_size);
    if (rotated == NULL)
    {
        free(buf);
        return -1;
    }
    for (int y = 0; y < img_size; y++)
    {
        for (int x = 0; x < img_size; x++)
        {
            int src_idx = (x * img_size + (img_size - 1 - y)) * 3;
            int dst_idx = (y * img_size + x) * 3;
            rotated[dst_idx + 0] = buf[src_idx + 0];
            rotated[dst_idx + 1] = buf[src_idx + 1];
            rotated[dst_idx + 2] = buf[src_idx + 2];
        }
    }
    free(buf);
    buf = rotated;

    FILE *fp = fopen(out_path, "wb");
    if (!fp)
    {
        perror("fopen jpg");
        free(buf);
        return -1;
    }

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);

    cinfo.image_width      = img_size;
    cinfo.image_height     = img_size;
    cinfo.input_components = 3;
    cinfo.in_color_space   = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    int row_stride = img_size * 3;
    JSAMPROW row_ptr;
    while (cinfo.next_scanline < cinfo.image_height)
    {
        row_ptr = &buf[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, &row_ptr, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(fp);
    free(buf);
    return 0;
}

int generate_qr(const char *content, const char *out_path,
                int pixel_size, int margin, int quality)
{
    QRcode *qr_obj = QRcode_encodeString(content, 0, QR_ECLEVEL_L,
                                         QR_MODE_8, 1);
    if (!qr_obj)
    {
        fprintf(stderr, "QR encode failed\n");
        return -1;
    }

    int ret = write_qr_jpeg(out_path, qr_obj, pixel_size, margin, quality);
    QRcode_free(qr_obj);
    return ret;
}