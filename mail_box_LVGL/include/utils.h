#ifndef UTILS_H_
#define UTILS_H_

/**************************************************************************
 *
 *   @brief : 生成随机验证码字符串
 *   @arg   : code    输出缓冲区
 *   @arg   : length  验证码长度 (1-6)
 *
 *   @retval: 无
 *
 ***************************************************************************/
void generate_random_code(char *code, int length);

/* 安全的字符串拷贝，确保目标缓冲区以 '\0' 结尾 */
void safe_strcpy(char *dst, size_t dst_size, const char *src);

/* 储物柜类型前缀：索引 1/2/3 对应小(A)/中(B)/大(C) 柜，0 无前缀 */
extern const char *LOCKER_PREFIXES[4];

#endif