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

#endif