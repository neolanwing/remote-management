// 引入 OTA 基础平台头文件
#include "../inc/private/inc_private.h"

// 声明来自 OTA 模块的函数
extern void remote_management_thread_init(void);

// 程序运行标志
volatile int g_ota_service_running = 1;


// 捕获 Ctrl+C / systemd 停止信号
void handle_signal(int sig)
{
    if (sig == SIGINT || sig == SIGTERM)
    {
        printf("OTA service received signal %d, exiting...\n", sig);
        g_ota_service_running = 0;
    }
}

int main(void)
{
    // 行缓冲和无缓冲设置
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("=== PMF406 OTA Service Starting ===\n");

    // 捕获退出信号
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    // 406id初始化
    system_init ("/tmp/pmf406.db");
    sleep(5);
    // 启动 OTA 线程
    remote_management_thread_init();

    // 主循环保持运行
    while (g_ota_service_running)
    {
        printf("[main] OTA service heartbeat...\n");
        sleep(30);
    }

    printf("=== PMF406 OTA Service Stopped ===\n");
    return 0;
}
