#include "qr_jpeg.h"

#include <setjmp.h>

#define RGB_CHANNELS 3

/* libjpeg 压缩错误处理：默认 error_exit 会直接 exit() 整个进程，
 * 磁盘写满/路径不可写等异常将导致快递柜程序崩溃退出。
 * 这里改为 longjmp 回到 setjmp 点，由调用方清理资源并返回错误码 */
typedef struct
{
    struct jpeg_error_mgr pub;
    jmp_buf jmp_buf;
} qr_jpeg_err_mgr_t;

static void qr_jpeg_error_exit(j_common_ptr cinfo)
{
    qr_jpeg_err_mgr_t *err = (qr_jpeg_err_mgr_t *)cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(err->jmp_buf, 1);
}

static int write_qr_jpeg(const char *out_path, QRcode *qrcode,
                         int pixel_size, int margin, int quality)
{
    int qr_w, img_size, buf_size, row_stride;
    unsigned char *buf = NULL;
    unsigned char *rotated = NULL;
    FILE *fp = NULL;
    struct jpeg_compress_struct cinfo;
    qr_jpeg_err_mgr_t jerr;

    if (out_path == NULL || qrcode == NULL ||
        pixel_size < 1 || margin < 0 || quality < 0)
    {
        return -1;
    }

    qr_w = qrcode->width;
    img_size = (qr_w + margin * 2) * pixel_size;
    buf_size = img_size * img_size * RGB_CHANNELS;

    buf = malloc(buf_size);
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
                        int idx = (row * img_size + start_x + px) * RGB_CHANNELS;
                        buf[idx + 0] = 0x00;
                        buf[idx + 1] = 0x00;
                        buf[idx + 2] = 0x00;
                    }
                }
            }
        }
    }

    rotated = malloc(buf_size);
    if (rotated == NULL)
    {
        free(buf);
        return -1;
    }
    /* 顺时针旋转 90°，适配屏幕显示方向 */
    for (int y = 0; y < img_size; y++)
    {
        for (int x = 0; x < img_size; x++)
        {
            int src_idx = (x * img_size + (img_size - 1 - y)) * RGB_CHANNELS;
            int dst_idx = (y * img_size + x) * RGB_CHANNELS;
            rotated[dst_idx + 0] = buf[src_idx + 0];
            rotated[dst_idx + 1] = buf[src_idx + 1];
            rotated[dst_idx + 2] = buf[src_idx + 2];
        }
    }

    fp = fopen(out_path, "wb");
    if (!fp)
    {
        perror("fopen jpg");
        free(buf);
        free(rotated);
        return -1;
    }

    /* 先清零 cinfo，避免 setjmp 后错误路径对未初始化结构调用 jpeg_destroy_compress */
    memset(&cinfo, 0, sizeof(cinfo));

    /* setjmp 之后 buf/rotated/fp 指针不再被重新赋值，longjmp 后取值仍确定 */
    if (setjmp(jerr.jmp_buf) != 0)
    {
        jpeg_destroy_compress(&cinfo);
        fclose(fp);
        free(buf);
        free(rotated);
        return -1;
    }

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = qr_jpeg_error_exit;
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);

    cinfo.image_width      = img_size;
    cinfo.image_height     = img_size;
    cinfo.input_components = RGB_CHANNELS;
    cinfo.in_color_space   = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    row_stride = img_size * RGB_CHANNELS;
    while (cinfo.next_scanline < cinfo.image_height)
    {
        JSAMPROW row_ptr = &rotated[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, &row_ptr, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(fp);
    free(rotated);
    free(buf);
    return 0;
}

int generate_qr(const char *content, const char *out_path,
                int pixel_size, int margin, int quality)
{
    QRcode *qr_obj;

    if (content == NULL || out_path == NULL)
    {
        return -1;
    }

    qr_obj = QRcode_encodeString(content, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!qr_obj)
    {
        fprintf(stderr, "QR encode failed\n");
        return -1;
    }

    int ret = write_qr_jpeg(out_path, qr_obj, pixel_size, margin, quality);
    QRcode_free(qr_obj);
    return ret;
}
