/* 产线 CLI */
#include "isp_cli.h"
#include "app_logic.h"
#include "log.h"
#include "isp_config.h"
#include "isp_logname.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ISP_VERSION_STR
#define ISP_VERSION_STR "0.6"
#endif

static FILE *g_cli_log;
static volatile LONG g_cli_cancel;
static int g_cli_console;

static void cli_attach_console(void)
{
    if (g_cli_console) return;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h && h != INVALID_HANDLE_VALUE) {
        g_cli_console = 1;
        return;
    }
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    g_cli_console = 1;
}
static void cli_print(const char *s)
{
    size_t n = strlen(s);
    if (n == 0) return;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h && h != INVALID_HANDLE_VALUE) {
        DWORD w = 0;
        WriteFile(h, s, (DWORD)n, &w, NULL);
        FlushFileBuffers(h);
    } else if (g_cli_console && stdout) {
        fwrite(s, 1, n, stdout);
        fflush(stdout);
    }
    if (g_cli_log) {
        fwrite(s, 1, n, g_cli_log);
        fflush(g_cli_log);
    }
}
static void cli_log_sink(int level, const char *msg)
{
    const char *t = "";
    char line[1200];
    switch (level) {
    case LOG_SUCCESS: t = "[OK] "; break;
    case LOG_WARN:    t = "[!] "; break;
    case LOG_ERROR:   t = "[ERR] "; break;
    case LOG_CMD:     t = ">> "; break;
    case LOG_TRACE:   t = "... "; break;
    default: break;
    }
    snprintf(line, sizeof(line), "%s%s\r\n", t, msg ? msg : "");
    /* CMD 与主界面操作日志一致：不打印固件读写/协议明细 */
    if (level == LOG_CMD || level == LOG_TRACE) {
        if (g_cli_log) {
            fputs(line, g_cli_log);
            fflush(g_cli_log);
        }
        return;
    }
    cli_print(line);
}

static void cli_progress(int pct, const char *st)
{
    char line[256];
    snprintf(line, sizeof(line), "\r[%3d%%] %s                    ", pct, st ? st : "");
    cli_print(line);
    if (pct >= 100)
        cli_print("\r\n");
    if (g_cli_log) {
        fprintf(g_cli_log, "[%d%%] %s\n", pct, st ? st : "");
        fflush(g_cli_log);
    }
}
/* 取下一个参数：按空白分词；值可用英文双引号包裹以支持含空格路径（引号会被剥除） */
static bool next_tok(char **pp, char *out, int outlen)
{
    char *p = *pp;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) { *pp = p; return false; }
    int o = 0;
    if (*p == '"') {
        p++;
        while (*p && *p != '"') {
            if (o < outlen - 1) out[o++] = *p;
            p++;
        }
        if (*p == '"') p++;
    } else {
        while (*p && *p != ' ' && *p != '\t') {
            if (o < outlen - 1) out[o++] = *p;
            p++;
        }
    }
    out[o] = 0;
    *pp = p;
    return true;
}

static int arg_match(const char *a, const char *name)
{
    return a && name && strcmp(a, name) == 0;
}

static bool has_cli_args(LPSTR cmd)
{
    if (!cmd || !*cmd) return false;
    /* 含任一 CLI 参数即进入命令行模式：长选项（--xxx）或短选项 -p/-f/-b/-m
       （后接空格、引号或直接连值，如 -pCOM8） */
    if (strstr(cmd, "--") != NULL) return true;
    for (const char *q = cmd; (q = strchr(q, '-')) != NULL; q++) {
        char k = q[1];
        if ((k == 'p' || k == 'f' || k == 'b' || k == 'm') && q[2] != '-')
            return true;
    }
    return false;
}

bool isp_cli_run(LPSTR cmd_line)
{
    if (!has_cli_args(cmd_line))
        return false;

    cli_attach_console();

    char port[32] = {0};
    char file[MAX_PATH] = {0};
    int baud = 0, mode = -1, delay = -1;
    int verify = -1, run_after = -1;
    int opt_read = 0;
    uint32_t bin_addr = 0x08000000u;
    char log_path[MAX_PATH];
    char exe_dir[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, exe_dir, MAX_PATH);
    char *slash = strrchr(exe_dir, '\\');
    if (slash) *(slash + 1) = 0;
    {
        char stamp[32];
        char hdr[256];
        isp_log_stamp(stamp, sizeof(stamp));
        snprintf(log_path, sizeof(log_path), "%sISP_CLI_v%s_%s.log",
                 exe_dir, ISP_VERSION_STR, stamp);
        g_cli_log = fopen(log_path, "w");
        if (!g_cli_log) {
            snprintf(log_path, sizeof(log_path), "ISP_CLI_v%s_%s.log", ISP_VERSION_STR, stamp);
            g_cli_log = fopen(log_path, "w");
        }
        if (g_cli_log) {
            isp_log_header(hdr, sizeof(hdr));
            fputs(hdr, g_cli_log);
            fflush(g_cli_log);
            HANDLE ho = GetStdHandle(STD_OUTPUT_HANDLE);
            if (ho && ho != INVALID_HANDLE_VALUE) {
                DWORD ww = 0;
                WriteFile(ho, hdr, (DWORD)strlen(hdr), &ww, NULL);
            }
        }
    }

    log_set_sink(cli_log_sink);
    isp_cfg_t cfg;
    isp_cfg_load(&cfg);

    /* 参数解析：key value 空格分隔；值可加双引号支持空格路径；短选项支持连值（如 -pCOM8） */
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", cmd_line ? cmd_line : "");
    char tok[MAX_PATH], val[MAX_PATH];
    char *p = buf;
    while (next_tok(&p, tok, sizeof(tok))) {
        char k = (tok[0] == '-' && tok[1]) ? tok[1] : 0;
        if (k && tok[2]) {
            /* 短选项连值形式：-pCOM8 */
            if (k == 'p') { snprintf(port, sizeof(port), "%s", tok + 2); continue; }
            if (k == 'f') { snprintf(file, sizeof(file), "%s", tok + 2); continue; }
            if (k == 'b') { baud = atoi(tok + 2); continue; }
            if (k == 'm') { mode = atoi(tok + 2); continue; }
        }
        if (arg_match(tok, "--port") || arg_match(tok, "-p")) {
            if (next_tok(&p, val, sizeof(val))) snprintf(port, sizeof(port), "%s", val);
        } else if (arg_match(tok, "--file") || arg_match(tok, "-f")) {
            if (next_tok(&p, val, sizeof(val))) snprintf(file, sizeof(file), "%s", val);
        } else if (arg_match(tok, "--baud") || arg_match(tok, "-b")) {
            if (next_tok(&p, val, sizeof(val))) baud = atoi(val);
        } else if (arg_match(tok, "--mode") || arg_match(tok, "-m")) {
            if (next_tok(&p, val, sizeof(val))) mode = atoi(val);
        } else if (arg_match(tok, "--delay")) {
            if (next_tok(&p, val, sizeof(val))) delay = atoi(val);
        } else if (arg_match(tok, "--bin-addr")) {
            if (next_tok(&p, val, sizeof(val))) bin_addr = (uint32_t)strtoul(val, NULL, 0);
        } else if (arg_match(tok, "--verify")) {
            verify = 1;
        } else if (arg_match(tok, "--no-verify")) {
            verify = 0;
        } else if (arg_match(tok, "--run")) {
            run_after = 1;
        } else if (arg_match(tok, "--no-run")) {
            run_after = 0;
        } else if (arg_match(tok, "--opt-read")) {
            opt_read = 1;
        } else if (arg_match(tok, "--help") || arg_match(tok, "-h")) {
            char help[1200];
            snprintf(help, sizeof(help),
                "用法: ISP_Downloader_C.exe --port COM8 --file firmware.hex [选项]\r\n"
                "  --port/-p   串口\r\n"
                "  --file/-f   HEX/BIN 路径 (含空格用英文双引号包裹)\r\n"
                "  --baud/-b   波特率 (默认配置或115200)\r\n"
                "  --mode/-m   DTR/RTS 模式 0-16\r\n"
                "  --delay     步骤延时 ms\r\n"
                "  --verify    下载时逐页校验\r\n"
                "  --no-verify 下载时不逐页校验\r\n"
                "  --run       下载后运行\r\n"
                "  --no-run    下载后不运行\r\n"
                "  --bin-addr  BIN 起始地址 (默认 0x08000000)\r\n"
                "日志: %s\r\n", log_path);
            cli_print(help);
            if (g_cli_log) {
                fputs(help, g_cli_log);
                fclose(g_cli_log);
                g_cli_log = NULL;
            }
            ExitProcess(0);
            return true;
        }
    }

    if (port[0] == 0) snprintf(port, sizeof(port), "%s", cfg.port);
    if (file[0] == 0) snprintf(file, sizeof(file), "%s", cfg.file);
    if (baud <= 0) baud = cfg.baud > 0 ? cfg.baud : 115200;
    if (mode < 0) mode = cfg.mode;
    if (delay <= 0) delay = cfg.delay_ms;
    if (verify < 0) verify = cfg.verify_dl;
    if (run_after < 0) run_after = cfg.run_after;

    if (g_cli_log) {
        fprintf(g_cli_log, "STM32 ISP CLI v" ISP_VERSION_STR "\n");
        fprintf(g_cli_log, "Port=%s Baud=%d Mode=%d Delay=%d\n", port, baud, mode, delay);
        fprintf(g_cli_log, "File=%s verify=%d run=%d\n", file, verify, run_after);
        fflush(g_cli_log);
    }
    {
        char hdr[256];
        snprintf(hdr, sizeof(hdr),
                 "STM32 ISP CLI v%s  Port=%s Baud=%d Mode=%d Delay=%d\r\nFile=%s verify=%d run=%d\r\n",
                 ISP_VERSION_STR, port, baud, mode, delay, file[0] ? file : "(none)",
                 verify < 0 ? 1 : verify, run_after < 0 ? 1 : run_after);
        cli_print(hdr);
    }

    if (!port[0]) {
        cli_log_sink(LOG_ERROR, "参数不足: 需要 --port (如 COM8)");
        if (g_cli_log) { fclose(g_cli_log); g_cli_log = NULL; }
        ExitProcess(2);
        return true;
    }

    logic_set_cancel_ptr(&g_cli_cancel);
    logic_set_progress_sink(cli_progress);
    logic_set_mode(mode, delay);

    int rc = 2;
    if (!logic_open_serial(port, baud)) {
        cli_log_sink(LOG_ERROR, "打开串口失败");
        rc = 1;
    } else if (opt_read && !file[0]) {
        /* 仅连接 + 读选项字节（调试用） */
        if (logic_ensure_bl()) {
            uint8_t ob[16] = {0};
            if (logic_read_option_bytes(ob)) {
                char line[128];
                snprintf(line, sizeof(line),
                         "OPT @ 0x%08X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                         (unsigned)logic_option_bytes_addr(),
                         ob[0],ob[1],ob[2],ob[3],ob[4],ob[5],ob[6],ob[7],
                         ob[8],ob[9],ob[10],ob[11],ob[12],ob[13],ob[14],ob[15]);
                cli_log_sink(LOG_SUCCESS, line);
                rc = 0;
            } else {
                cli_log_sink(LOG_ERROR, "读选项字节失败");
                rc = 1;
            }
        } else {
            rc = 1;
        }
        logic_close_serial();
    } else {
        char err[256] = {0};
        if (!file[0]) {
            cli_log_sink(LOG_ERROR, "参数不足: --file 或 --opt-read");
            rc = 2;
        } else if (!logic_load_firmware(file, bin_addr, err, sizeof(err))) {
            cli_log_sink(LOG_ERROR, err[0] ? err : "固件加载失败");
            rc = 1;
        } else {
            download_opts_t opt;
            opt.verify_each_page = verify < 0 ? 1 : (verify ? 1 : 0);
            opt.run_after = run_after < 0 ? 1 : (run_after ? 1 : 0);
            if (logic_download(&opt)) {
                cli_log_sink(LOG_SUCCESS, "下载成功");
                rc = 0;
            } else {
                cli_log_sink(LOG_ERROR, "下载失败");
                rc = 1;
            }
        }
        logic_close_serial();
    }

    if (g_cli_log) {
        fprintf(g_cli_log, "EXIT=%d\n", rc);
        fclose(g_cli_log);
        g_cli_log = NULL;
    }
    if (rc == 0)
        cli_print("\r\n[OK] Download finished. Exit=0\r\n");
    else
        cli_print("\r\n[ERR] Download failed. Exit=1\r\n");
    ExitProcess((UINT)rc);
    return true;
}
