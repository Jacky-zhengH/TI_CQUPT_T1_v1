#ifndef __APP_PROCESS_H
#define __APP_PROCESS_H

//=======================================
// 函数声明
//=======================================
void HMI_Process_Init(void);
void HMI_Send_Cmd(const char *cmd_string);
void Debug_printf(const char *text, ...);
void App_Main_Process_Poll(void);

#endif
