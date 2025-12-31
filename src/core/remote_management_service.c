/*********************************Copyright (c)**********************************
**
**                        许昌智能继电器股份有限公司
**                              www.xjpmf.com
**
**-----------------------------------文件信息-----------------------------------
**  项目名称    ：PMF406
**  文    件    ：remote_management_service.c
**  作    者    ：李鹏博
**  版    本    ：1.00
**  创建日期    ：2025-09-04
**  描    述    ：远程管理线程
**
**-----------------------------------历史版本-----------------------------------
**  作    者    ：
**  版    本    ：
**  修改日期    ：
**  描    述    ：
**
******************************************************************************/
#define REMOTE_MANAGEMENT_SEVICE_PRIVATE_GLOBALS

/******************************************************************************
**C库头文件
******************************************************************************/
#include "../../inc/private/inc_private.h"
/******************************************************************************
**底层驱动平台头文件
******************************************************************************/

/******************************************************************************
**基础平台头文件
******************************************************************************/

/******************************************************************************
**应用头文件
******************************************************************************/

/******************************************************************************
**宏定义
******************************************************************************/

/******************************************************************************
**枚举定义
******************************************************************************/

/******************************************************************************
**结构类型定义
******************************************************************************/

/******************************************************************************
**静态变量定义
******************************************************************************/


/******************************************************************************
**API函数实现
******************************************************************************/

/******************************************************************************
  End File
******************************************************************************/

#if 0

#endif

/*
** OTA升级相关函数实现
*/
//  判断是否成功
int system_ok(int status)
{
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
// 判断是否应该使用wr命令
int exec_cmd_auto_wr(const char *cmd)
{
    int status;
    char buf[1024];

    status = system(cmd);
    if (system_ok(status))
        return 0;

    snprintf(buf, sizeof(buf), "wr %s", cmd);
    printf("[AUTO_WR] fallback: %s\n", buf);

    status = system(buf);
    if (system_ok(status))
        return 0;

    return -1;
}

// MD5 文件计算函数 (OpenSSL 实现)
int md5_file(const char *filePath, char *md5_out)
{
    unsigned char c[MD5_DIGEST_LENGTH];
    FILE *inFile = NULL;
    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];
    int i;

    inFile = fopen(filePath, "rb");
    if (inFile == NULL) {
        printf("Error: Cannot open file %s for MD5 calculation.\n", filePath);
        return -1;
    }

    MD5_Init(&mdContext);

    while ((bytes = fread(data, 1, 1024, inFile)) != 0) {
        MD5_Update(&mdContext, data, bytes);
    }

    MD5_Final(c, &mdContext);

    fclose(inFile);

    // 将结果转换为 32 位小写十六进制字符串
    for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(&md5_out[i * 2], "%02x", c[i]);
    }
    md5_out[MAX_MD5_LEN - 1] = '\0';

    return 0;
}
// SHA-1 文件计算函数 (OpenSSL 实现)
int sha1_file(const char *filePath, char *sha1_out)
{
    unsigned char c[SHA_DIGEST_LENGTH];
    FILE *inFile = NULL;
    SHA_CTX shaContext;
    int bytes;
    unsigned char data[1024];
    int i;

    inFile = fopen(filePath, "rb");
    if (inFile == NULL) {
        printf("Error: Cannot open file %s for SHA-1 calculation.\n", filePath);
        return -1;
    }

    SHA1_Init(&shaContext);

    while ((bytes = fread(data, 1, 1024, inFile)) != 0) {
        SHA1_Update(&shaContext, data, bytes);
    }

    SHA1_Final(c, &shaContext);

    fclose(inFile);

    // 将结果转换为 40 位小写十六进制字符串
    for (i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(&sha1_out[i * 2], "%02x", c[i]);
    }
    sha1_out[MAX_SHA1_LEN - 1] = '\0';

    return 0;
}
// SHA-256 文件计算函数 (OpenSSL 实现)
int sha256_file(const char *filePath, char *sha256_out)
{
    unsigned char c[SHA256_DIGEST_LENGTH];
    FILE *inFile = NULL;
    SHA256_CTX sha256Context;
    int bytes;
    unsigned char data[1024];
    int i;

    inFile = fopen(filePath, "rb");
    if (inFile == NULL) {
        printf("Error: Cannot open file %s for SHA-256 calculation.\n", filePath);
        return -1;
    }

    SHA256_Init(&sha256Context);

    while ((bytes = fread(data, 1, 1024, inFile)) != 0) {
        SHA256_Update(&sha256Context, data, bytes);
    }

    SHA256_Final(c, &sha256Context);

    fclose(inFile);

    // 将结果转换为 64 位小写十六进制字符串
    for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(&sha256_out[i * 2], "%02x", c[i]);
    }
    sha256_out[MAX_SHA256_LEN - 1] = '\0';

    return 0;
}
// HTTP 文件下载函数 (系统命令实现)
int http_download_file(const char *url, const char *savePath){
    if (!url || !savePath) return -1;

    // 创建目录(如果不存在)
    if (access(OTA_DOWNLOAD_DIR, F_OK) != 0) {
        mkdir(OTA_DOWNLOAD_DIR, 0755);
    }

    // 检查 wget 是否存在
    if (access("/usr/bin/wget", X_OK) != 0 &&
        access("/bin/wget", X_OK) != 0 &&
        access("/usr/sbin/wget", X_OK) != 0 &&
        access("/sbin/wget", X_OK) != 0) {

        printf("Error: wget command not found.\n");
        return -1;
    }

    // 删除旧文件
    unlink(savePath);

    // ----------- 🔥 构造新的 URL（https → http） -----------
    char fixed_url[512] = {0};

    if (strncmp(url, "https://", 8) == 0) {
        // 替换为 http://
        snprintf(fixed_url, sizeof(fixed_url), "http://%s", url + 8);
    } else {
        // 若不是 https，原样使用
        snprintf(fixed_url, sizeof(fixed_url), "%s", url);
    }

    // -------------------------------------------------------

    char cmd[768];
    snprintf(cmd, sizeof(cmd),
             "wget -q -O \"%s\" \"%s\"",
             savePath, fixed_url);

    printf("Downloading via wget: %s\n", cmd);

    // ⚠️ 不再信任返回值，只负责执行
    exec_cmd_auto_wr(cmd);

    // ---------- 🔒 结果校验（关键） ----------
    struct stat st;
    if (stat(savePath, &st) == 0 && st.st_size > 0) {
        printf("Download success: %s (size=%ld)\n",
               savePath, (long)st.st_size);
        return 0;
    }

    // 校验失败
    printf("Download failed: file not valid\n");
    unlink(savePath);
    return -1;
}
// ZIP 解压函数 (使用 system/unzip 实现强制覆盖到指定目录)
int unzip_file(const char *zipPath, const char *destDir) {
    char cmd[MAX_PATH_LEN * 2];
    int ret;

    // 1. 如果解压目录存在，先删除
    if (access(destDir, F_OK) == 0) {
        snprintf(cmd, sizeof(cmd), "rm -rf %s", destDir);
        ret = exec_cmd_auto_wr(cmd);
        if (ret != 0) {
            printf("Error: Failed to remove existing directory %s\n", destDir);
            return -1;
        }
    }

    // 2. 创建解压目录
    if (mkdir(destDir, 0755) != 0) {
        printf("Error: Failed to create directory %s, errno=%d\n", destDir, errno);
        return -1;
    }

    // 2. 使用 unzip -o 强制覆盖解压到指定目录
    snprintf(cmd, sizeof(cmd), "unzip -o %s -d %s", zipPath, destDir);

    printf("Executing unzip command: %s (Forced Overwrite)\n", cmd);

    ret = exec_cmd_auto_wr(cmd);

    if (ret != 0) {
        printf("Error: Unzip of %s failed, system returned %d.\n", zipPath, ret);
        return -1;
    }

    return 0;
}
// 文件大小获取 (POSIX stat 实现)
long get_file_size(const char *filePath) {
    struct stat st;
    if (stat(filePath, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

// 判断文件名是否在备份列表中 
int is_file_in_backup_list(const char *filename) {
    FILE *fp = fopen(BACKUP_LIST_FILE, "r");
    if (!fp) return 0; // 默认不备份

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        // 去掉换行符
        line[strcspn(line, "\r\n")] = 0;
        if (strcmp(filename, line) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

// 备份文件到 BACKUP_DIR（不检查是否已存在，直接覆盖） 
void backup_file_if_needed(const char *file_path)
{
    struct stat st;
    if (stat(file_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        printf("[SKIP] not a regular file: %s\n", file_path);
        return;
    }

    const char *filename = strrchr(file_path, '/');
    filename = filename ? filename + 1 : file_path;

    if (!is_file_in_backup_list(filename))
        return;

    if (mkdir(BACKUP_DIR, 0755) != 0 && errno != EEXIST) {
        printf("[ERROR] mkdir failed: %s\n", BACKUP_DIR);
        return;
    }

    char backup_path[512];
    snprintf(backup_path, sizeof(backup_path),
             "%s/%s", BACKUP_DIR, filename);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "cp -f \"%s\" \"%s\"", file_path, backup_path);

    if (exec_cmd_auto_wr(cmd) == 0) {
        printf("[BACKUP] %s -> %s\n", file_path, backup_path);
    } else {
        printf("[ERROR] backup failed: %s\n", file_path);
    }
}


// ----------------------------------------------------------------------
// OTA 重启状态文件 (upgrade.txt) 操作函数
// ----------------------------------------------------------------------

/**
 * @brief 写入重启状态文件 (version 和 id)
 */
int write_ota_reboot_status(const char *version, const char *id, const char *ota) {
    char status_path[MAX_PATH_LEN];
    snprintf(status_path, sizeof(status_path), "%s%s", OTA_DOWNLOAD_DIR, OTA_STATUS_FILE);

    // "w" 模式会创建或截断文件
    FILE *f = fopen(status_path, "w");
    if (f) {
        // 格式: version\nid
        fprintf(f, "%s\n", version);
        fprintf(f, "%s\n", id);
        fprintf(f, "%s\n", ota);
        fclose(f);
        return 0;
    }
    printf("Error: Failed to create OTA status file %s.\n", status_path);
    return -1;
}

/**
 * @brief 检查升级状态文件，读取 version 和 id
 */
ota_reboot_status_t *check_ota_finish_status(void) {
    static ota_reboot_status_t status = {0};
    char status_path[MAX_PATH_LEN];
    snprintf(status_path, sizeof(status_path), "%s%s", OTA_DOWNLOAD_DIR, OTA_STATUS_FILE);

    FILE *f = fopen(status_path, "r");
    if (f) {
            if (fgets(status.version, sizeof(status.version), f) != NULL &&
            fgets(status.id, sizeof(status.id), f) != NULL &&
            fgets(status.ota, sizeof(status.ota), f) != NULL)
        {
            status.version[strcspn(status.version, "\n")] = 0;
            status.id[strcspn(status.id, "\n")] = 0;
            status.ota[strcspn(status.ota, "\n")] = 0;
            fclose(f);
            return &status;
        }
        fclose(f);
    }
    return NULL;
}

/**
 * @brief 清除升级状态文件
 */
void clear_ota_finish_status(void) {
    char status_path[MAX_PATH_LEN];
    snprintf(status_path, sizeof(status_path), "%s%s", OTA_DOWNLOAD_DIR, OTA_STATUS_FILE);

    if (remove(status_path) != 0) {
        printf("Warning: Failed to delete OTA status file %s.\n", status_path);
    }
}

