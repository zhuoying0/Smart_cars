#ifndef _PARAM_H_
#define _PARAM_H_

#include "zf_common_typedef.h"

// 定义系统参数结构体
typedef struct {
    int   base_speed;
    float Kp;
    float Ki;
    float Kd;
    float gkd;
} SystemParameters;

/**
 * @brief 初始化系统参数为默认值
 */
void param_init(void);

/**
 * @brief 获取当前系统参数的只读指针
 * @return const SystemParameters* 指向全局参数
 */
const SystemParameters* param_get(void);

/**
 * @brief 使用新的参数更新系统参数
 * @param new_params 包含新参数的结构体指针
 */
void param_update(const SystemParameters* new_params);

#endif // _PARAM_H_