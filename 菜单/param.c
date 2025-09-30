#include "param.h"

// 定义全局的系统参数变量
static SystemParameters g_params;

void param_init(void) {
    // 在这里设置你的默认参数
    g_params.base_speed = 150;
    g_params.Kp = 1.2f;
    g_params.Ki = 0.05f;
    g_params.Kd = 0.8f;
    g_params.gkd = 100.0f;
}

const SystemParameters* param_get(void) {
    return &g_params;
}

void param_update(const SystemParameters* new_params) {
    if (new_params) {
        g_params = *new_params;
    }
}